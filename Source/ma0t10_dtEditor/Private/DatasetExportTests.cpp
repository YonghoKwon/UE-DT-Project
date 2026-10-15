#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationProtocol.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "RHIGlobals.h"
#include "Slate/SceneViewport.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExport01AdmissionTest,"MA0T10.Dataset.ExportAdmission",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExport01AdmissionTest::RunTest(const FString&)
{
    FVirtualDatasetSettings S;FString Error;TestEqual(TEXT("default no export"),S.ExportFormat,FString(TEXT("none")));
    TestFalse(TEXT("default no tilted approximation"),S.ExportUprightEnvelope);S.Enabled=true;
    TestTrue(TEXT("default requires no Python/GT"),S.Validate(Error));S.ExportFormat=TEXT("coco");
    TestFalse(TEXT("export requires camera truth"),S.Validate(Error));S.CameraTruth=true;
    for(const FString Format:{TEXT("coco"),TEXT("kitti"),TEXT("coco,kitti"),TEXT("none")}){S.ExportFormat=Format;TestTrue(TEXT("explicit format admitted"),S.Validate(Error));}
    S.ExportFormat=TEXT("shell");TestFalse(TEXT("arbitrary format rejected"),S.Validate(Error));
    using namespace MA0T10Automation;TSharedPtr<FJsonObject> C;
    const FString Command=TEXT("{\"op\":\"dataset.start\",\"name\":\"export\",\"scenes\":20,\"slab\":\"a\",\"lidar_id\":\"b\",\"camera_id\":\"c\",\"camera_truth\":true,\"export_format\":\"coco,kitti\",\"export_upright_envelope\":true}");
    TestTrue(TEXT("job dataset step supports explicit export"),ParseObject(Command,C)&&ValidateCommand(*C,Error));
    C->SetStringField(TEXT("export_format"),TEXT("bad"));TestFalse(TEXT("job rejects bad format"),ValidateCommand(*C,Error));return true;
}

class FExport01RhiCommand final:public IAutomationLatentCommand
{
public:
    explicit FExport01RhiCommand(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(!World.IsValid())
        {
            UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
            if(W!=Observed.Get()){Observed=W;ObservedAt=Now;return false;}if(!W||Now-ObservedAt<3)return false;
            World=W;Started=Now;Manual=FParse::Param(FCommandLine::Get(),TEXT("Export01Manual"));
            for(TActorIterator<ASlabActor> It(W);It;++It)It->StopSimulation();for(TActorIterator<AVirtualSensorActorBase> It(W);It;++It)It->StopSensor();
            Slab=W->SpawnActor<ASlabActor>();Slab->Tags.Add(TEXT("SensorTestManaged"));Slab->SetActorLocation(FVector(1500,0,200));Slab->ApplyDatasetGeometry(FVector(800,200,50));Slab->SetDiagnosticHelpersVisible(false);
            Lidar=W->SpawnActor<AVirtualLidarSensorActor>();Lidar->Tags.Add(TEXT("SensorTestManaged"));Lidar->StopSensor();Lidar->SetActorLocation(FVector(1500,-1000,1000));Lidar->SetActorRotation((Slab->GetActorLocation()-Lidar->GetActorLocation()).Rotation());
            auto* Scan=Lidar->ScanComponent.Get();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->bEnableRangeNoise=false;Scan->OutputMode=EVirtualLidarOutputMode::None;Scan->SensorId=TEXT("EXPORT01-LIDAR");
            Camera=W->SpawnActor<AVirtualCameraSensorActor>();Camera->Tags.Add(TEXT("SensorTestManaged"));Camera->StopSensor();Camera->SetActorTransform(Lidar->GetActorTransform());Camera->CaptureComponent->ApplyDeviceProfile(EVirtualCameraDeviceProfile::IntelRealSenseD455);Camera->CaptureComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Camera->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;Camera->CaptureComponent->SensorId=TEXT("EXPORT01-CAMERA");
            Service=W->GetSubsystem<UVirtualDatasetSubsystem>();Before=Slab->GetActorTransform();
            return false;
        }
        if(Now-Started>360){Test->AddError(TEXT("EXPORT-01 timeout"));return Finish();}
        if(!Began)
        {
            if(Now-Started<10)return false;
            Service->OnSceneSnapshotForTests=[this](const FJsonObject& Label)
            {
                ++Snapshots;const auto Box=Label.GetArrayField(TEXT("boxes_3d"))[0]->AsObject();
                Test->TestEqual(TEXT("minor schema additive"),int32(Label.GetNumberField(TEXT("schema_minor"))),1);
                Test->TestTrue(TEXT("valid Slab PCA in export fixture"),Label.GetBoolField(TEXT("analysis_valid")));
                const auto Center=Box->GetArrayField(TEXT("center_m"));const FVector Actual=Slab->SlabMesh->GetComponentTransform().TransformPosition(Slab->SlabMesh->GetStaticMesh()->GetBounds().Origin)*.01;
                Test->TestTrue(TEXT("OBB center actual rendered mesh"),FVector(Center[0]->AsNumber(),Center[1]->AsNumber(),Center[2]->AsNumber()).Equals(Actual,1.e-6));
            };
            Settings.Enabled=Settings.CameraTruth=true;Settings.Name=TEXT("EXPORT01");Settings.Scenes=20;Settings.Seed=22016;Settings.ExportFormat=TEXT("coco,kitti");Settings.ExportUprightEnvelope=true;Settings.Yaw.Enabled=Settings.Size.Enabled=true;
            if(Manual)
            {
                auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Panel=Cast<UVirtualSensorCaptureExportPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Data));
                if(!Panel)return false;
                Panel->DatasetDraft=FVirtualDatasetSettings();Panel->DatasetDraft.Name=TEXT("EXPORT01_manual");Panel->DatasetDraft.Scenes=1;Panel->DatasetSlab=Slab;Panel->DatasetLidar=Lidar;Panel->DatasetCamera=Camera;
                Workspace->SetOwnedPanelFontScale(1);Workspace->SetPanelOpen(ESensorToolPanelRole::Data,true);Panel->OpenOwnedTab(ESensorOwnedDataTab::FileSave);if(auto Area=Panel->DatasetArea.Pin())Area->SetExpanded(true);
                if(GEngine->GameViewport&&GEngine->GameViewport->GetGameViewport())GEngine->GameViewport->GetGameViewport()->ResizeFrame(1920,1080,EWindowMode::Windowed);
                FFileHelper::SaveStringToFile(TEXT("ready"),*(Report()/TEXT("manual/ready.flag")));
            }
            else {FString Error;Test->TestTrue(TEXT("export start accepted"),Service->Start(Settings,Slab, Lidar,Camera,nullptr,Error));if(!Error.IsEmpty()){Test->AddError(Error);return Finish();}}
            Began=true;return false;
        }
        if(Manual)
        {
            if(FPaths::FileExists(Report()/TEXT("manual/finish.flag")))return Finish();
            return false;
        }
        if(Service->IsActive())return false;
        Test->TestTrue(TEXT("export subprocess and dataset succeeded"),Service->HasSucceeded());
        Test->TestEqual(TEXT("twenty actual GPU snapshots"),Snapshots,20);
        Test->TestTrue(TEXT("scene pose restored"),Slab->GetActorTransform().Equals(Before));
        Test->TestTrue(TEXT("COCO reloaded report"),FPaths::FileExists(Service->ExportDirectory()/TEXT("coco/export-report.json")));
        Test->TestTrue(TEXT("KITTI reloaded report"),FPaths::FileExists(Service->ExportDirectory()/TEXT("kitti/export-report.json")));
        return Finish();
    }
private:
    FString Report() const{return FPaths::ProjectSavedDir()/TEXT("Reports/EXPORT-01");}
    bool Finish()
    {
        if(Service){Service->OnSceneSnapshotForTests=nullptr;Service->Stop();auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("dataset"),Service->Directory());O->SetStringField(TEXT("export"),Service->ExportDirectory());O->SetNumberField(TEXT("snapshots"),Snapshots);O->SetBoolField(TEXT("succeeded"),Service->HasSucceeded());FString T;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&T));FFileHelper::SaveStringToFile(T,*(Report()/(Manual?TEXT("manual/session.json"):TEXT("generation.json"))));}
        if(Manual)FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){FPlatformMisc::RequestExit(false);return false;}),2.f);
        return true;
    }
    FAutomationTestBase* Test;TWeakObjectPtr<UWorld> World,Observed;double ObservedAt=0,Started=0;bool Began=false,Manual=false;int32 Snapshots=0;
    ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;AVirtualCameraSensorActor* Camera=nullptr;UVirtualDatasetSubsystem* Service=nullptr;FVirtualDatasetSettings Settings;FTransform Before;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExport01RhiTest,"MA0T10.Dataset.ExportRealRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExport01RhiTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("Export01"))&&!FParse::Param(FCommandLine::Get(),TEXT("Export01Manual"))){AddInfo(TEXT("[OPT-IN SKIP] -Export01 or -Export01Manual required"));return true;}
    if(GUsingNullRHI){AddError(TEXT("EXPORT-01 requires real RHI"));return false;}
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Reports/EXPORT-01/manual")),true);
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FExport01RhiCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
