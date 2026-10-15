#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationControlSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "Uxr15ToggleTestHelpers.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "RHIGlobals.h"

// One actual PIE checks the shared mutation gate used by both Python and ROS2,
// independent IMU configuration, and UI dataset + truth + export + IMU together.
class FUxr16Integration final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Began=FPlatformTime::Seconds(),Next=0;
    int32 Phase=0,SensorRevision=0;
    AVirtualLidarSensorActor* Lidar=nullptr;
    AVirtualCameraSensorActor* Camera=nullptr;
    ASlabActor* Slab=nullptr;
    UVirtualImuComponent* Imu=nullptr;
    UVirtualDatasetSubsystem* Dataset=nullptr;
    UAutomationControlSubsystem* Control=nullptr;
    UVirtualSensorCaptureExportPanelWidget* Panel=nullptr;
    FTransform Before;
    FString Root()const{return FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-16/integration");}
    bool Finish(){if(Dataset)Dataset->Stop();if(Control)Control->SetApiEnabled(false);return true;}
public:
    explicit FUxr16Integration(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Began>180){Test->AddError(TEXT("UXR16 combined integration timeout"));return Finish();}
        UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)W=Context.World();
        if(!FinalAcceptanceViewportReady(W)||Now<Next)return false;
        if(Phase==0)
        {
            auto* Workspace=W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=Workspace->GetCoordinator();if(!C)return false;
            for(auto* A:C->GetSensorActors()){A->StopSensor();if(auto* L=Cast<AVirtualLidarSensorActor>(A))Lidar=L;if(auto* Cam=Cast<AVirtualCameraSensorActor>(A))Camera=Cam;}
            for(TActorIterator<ASlabActor> It(W);It;++It){It->StopSimulation();Slab=*It;break;}
            if(!Lidar||!Camera||!Slab){Test->AddError(TEXT("Combined fixture targets missing"));return true;}
            Before=Slab->GetActorTransform();Imu=UVirtualImuComponent::Attach(Lidar);SensorRevision=Lidar->GetConfigurationRevision();
            Control=W->GetSubsystem<UAutomationControlSubsystem>();Dataset=W->GetSubsystem<UVirtualDatasetSubsystem>();
            Panel=Cast<UVirtualSensorCaptureExportPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Data));
            IFileManager::Get().MakeDirectory(*Root(),true);
            FFileHelper::SaveStringToFile(TEXT("{\"schema\":\"ma0t10.job.v1\",\"result_path\":\"batch-result.json\",\"steps\":[{\"op\":\"wait\",\"seconds\":1}]}"),*(Root()/TEXT("batch.json")));
            Test->TestTrue(TEXT("batch admitted without a listener"),Control->StartJob(Root()/TEXT("batch.json"),false));
            auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("sensor_id"),Lidar->GetSensorId());int32 Code=0;
            Control->Dispatch(TEXT("POST"),TEXT("/v1/sensor/start"),Body,Code);
            Test->TestEqual(TEXT("ROS2/Python shared endpoint rejects mutation while batch owns it"),Code,409);
            Test->TestFalse(TEXT("rejected command did not start measurement"),Lidar->IsSensorRunning());
            Control->Dispatch(TEXT("GET"),TEXT("/v1/status"),nullptr,Code);Test->TestEqual(TEXT("status remains available during batch"),Code,200);
            auto Config=Imu->GetConfig();Config.RateHz=400;FString Error;
            Test->TestTrue(TEXT("independent IMU edit during API/batch work"),Imu->Configure(Config,Error));
            Test->TestEqual(TEXT("IMU generation does not change parent sensor revision"),Lidar->GetConfigurationRevision(),SensorRevision);
            Next=Now+1.5;Phase=1;return false;
        }
        if(Phase==1)
        {
            const auto Status=Control->GetStatus();if(Status->GetBoolField(TEXT("job_active")))return false;
            Test->TestTrue(TEXT("batch completes with independent IMU change"),Status->GetBoolField(TEXT("success")));
            Test->TestEqual(TEXT("IMU edit survives batch teardown"),Imu->GetConfig().RateHz,400.);
            int32 Code=0;const auto Sensors=Control->Dispatch(TEXT("GET"),TEXT("/v1/sensors"),nullptr,Code);
            bool Found=false;for(const auto& V:Sensors->GetArrayField(TEXT("sensors")))if(V->AsObject()->GetStringField(TEXT("sensor_id"))==Lidar->GetSensorId())Found=V->AsObject()->HasField(TEXT("imu"));
            Test->TestTrue(TEXT("combined API reports attached IMU"),Found);
            Camera->StartSensor();
            auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("sensor_id"),Camera->GetSensorId());
            Control->Dispatch(TEXT("POST"),TEXT("/v1/capture"),Body,Code);
            Test->TestEqual(TEXT("API asynchronous capture admitted"),Code,202);
            auto Config=Imu->GetConfig();Config.Seed=22016;FString Error;
            Test->TestTrue(TEXT("IMU can be configured while API capture is active"),Imu->Configure(Config,Error));
            Phase=2;return false;
        }
        if(Phase==2)
        {
            const auto Status=Control->GetStatus();if(!Status->GetBoolField(TEXT("terminal")))return false;
            Test->TestTrue(TEXT("API capture succeeds despite independent IMU edit"),Status->GetBoolField(TEXT("success")));
            Test->TestTrue(TEXT("API capture produces an artifact"),!Status->GetObjectField(TEXT("detail"))->GetArrayField(TEXT("artifacts")).IsEmpty());
            Test->TestEqual(TEXT("IMU config survives API capture completion"),Imu->GetConfig().Seed,22016);
            Camera->StopSensor();
            Panel->DatasetDraft=FVirtualDatasetSettings();Panel->DatasetDraft.Name=TEXT("UXR16_combined");Panel->DatasetDraft.Scenes=1;
            Panel->DatasetSlab=Slab;Panel->DatasetLidar=Lidar;Panel->DatasetCamera=Camera;
            const auto Controls=Panel->BuildOwnedDatasetControls();
            Uxr15ToggleTests::Toggle(Controls,TEXT("데이터셋 생성 사용"),Test);
            Uxr15ToggleTests::Toggle(Controls,TEXT("카메라 정답 포함"),Test);
            Uxr15ToggleTests::Toggle(Controls,TEXT("IMU 값 포함"),Test);
            Uxr15ToggleTests::Click(Controls,TEXT("생성 후 내보내기 형식: 없음"),Test);
            Test->TestEqual(TEXT("real export button selects COCO"),Panel->DatasetDraft.ExportFormat,FString(TEXT("coco")));
            Uxr15ToggleTests::Click(Controls,TEXT("생성 시작"),Test);
            Test->TestTrue(TEXT("UI combined generation admitted"),Dataset->IsActive());Phase=3;return false;
        }
        if(Dataset->IsActive())return false;
        Test->TestTrue(TEXT("truth + IMU + external export succeeds together"),Dataset->HasSucceeded());
        Test->TestEqual(TEXT("one committed scene"),Dataset->CompletedScenes(),1);
        Test->TestTrue(TEXT("scene pose restored"),Slab->GetActorTransform().Equals(Before));
        FString Text;TSharedPtr<FJsonObject> Label;
        FFileHelper::LoadFileToString(Text,*(Dataset->Directory()/TEXT("scene_00000/label.json")));
        if(Test->TestTrue(TEXT("actual combined label parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Label)))
        {
            Test->TestEqual(TEXT("minor label preserved"),int32(Label->GetNumberField(TEXT("schema_minor"))),1);
            Test->TestTrue(TEXT("OBB survives optional IMU label"),!Label->GetArrayField(TEXT("boxes_3d")).IsEmpty());
            Test->TestEqual(TEXT("one selected IMU snapshot"),Label->GetArrayField(TEXT("imu")).Num(),1);
            Test->TestTrue(TEXT("IMU latest sample present"),Label->GetArrayField(TEXT("imu"))[0]->AsObject()->HasField(TEXT("latest")));
        }
        Test->TestTrue(TEXT("truth plane exists"),FPaths::FileExists(Dataset->Directory()/TEXT("scene_00000/instance.png")));
        Test->TestTrue(TEXT("COCO converter reloaded output"),FPaths::FileExists(Dataset->ExportDirectory()/TEXT("coco/export-report.json")));
        Test->TestEqual(TEXT("dataset does not reconfigure independent IMU"),Imu->GetConfig().RateHz,400.);
        auto Evidence=MakeShared<FJsonObject>();Evidence->SetStringField(TEXT("dataset"),Dataset->Directory());Evidence->SetStringField(TEXT("export"),Dataset->ExportDirectory());
        FJsonSerializer::Serialize(Evidence,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Root()/TEXT("artifacts.json")));
        return Finish();
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr16IntegrationTest,"MA0T10.UXR16.CombinedRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr16IntegrationTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_DATASET_PYTHON")).IsEmpty()){AddInfo(TEXT("SKIP explicit MA0T10_DATASET_PYTHON required for combined export"));return true;}
    if(GUsingNullRHI){AddError(TEXT("UXR16 combined test requires actual RHI"));return false;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr16Integration(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
