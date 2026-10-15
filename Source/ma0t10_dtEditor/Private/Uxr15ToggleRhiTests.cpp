#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Uxr15ToggleTestHelpers.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "RHIGlobals.h"

class FUxr15ToggleRhiCommand final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;TWeakObjectPtr<UWorld> World;double Started=FPlatformTime::Seconds(),Next=0;int32 Phase=0,Mode=0,DatasetSnapshots=0;
    AVirtualCameraSensorActor* Camera=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;ASlabActor* Slab=nullptr;ADirectionalLight* Light=nullptr;
    UVirtualSensorSettingsPanelWidget* Settings=nullptr;UVirtualSensorMonitorPanelWidget* Monitor=nullptr;UVirtualSensorCaptureExportPanelWidget* Data=nullptr;UVirtualDatasetSubsystem* Service=nullptr;
    TSharedPtr<SWidget> SettingsSlate,TruthControls,DatasetControls;
    FString ExpectedRandom;TArray<FString> Outputs;bool WithTruth=true;
    void ConfigureDataset(bool Enabled)
    {
        using namespace Uxr15ToggleTests;
        WithTruth=Enabled;
        if(Enabled)Toggle(DatasetControls.ToSharedRef(),TEXT("데이터셋 생성 사용"),Test);
        Toggle(DatasetControls.ToSharedRef(),TEXT("카메라 정답 포함"),Test);Toggle(DatasetControls.ToSharedRef(),TEXT("LiDAR 사실성 포함"),Test);
        for(const TCHAR* Name:{TEXT("Slab 수평 회전"),TEXT("Slab 앞뒤 기울기"),TEXT("Slab 좌우 기울기"),TEXT("Slab X 오프셋"),TEXT("Slab Y 오프셋"),TEXT("Slab Z 오프셋"),TEXT("Slab 크기 배율"),TEXT("표면 0 기본"),TEXT("표면 거칠기"),TEXT("산화도"),TEXT("조명 강도"),TEXT("조명 상하 각도"),TEXT("조명 수평 회전"),TEXT("센서 설치 위치 오차"),TEXT("센서 설치 각도 오차")})Toggle(DatasetControls.ToSharedRef(),Name,Test);
        ExpectedRandom=FVirtualDatasetSample::Make(Data->DatasetDraft,0).ToJson();
        Click(DatasetControls.ToSharedRef(),TEXT("생성 시작"),Test);
        Test->TestTrue(TEXT("actual UI Start callback creates an active dataset session"),Service->IsActive());
    }
    bool CheckDataset()
    {
        if(Service->IsActive())return false;
        Test->TestEqual(TEXT("one scene committed by UI Start"),Service->CompletedScenes(),1);
        const FString Directory=Service->Directory(),Scene=Directory/TEXT("scene_00000");Outputs.Add(Directory);
        for(const TCHAR* File:{TEXT("rgb.png"),TEXT("lidar.pcd"),TEXT("label.json"),TEXT("lidar_labels.bin")})Test->TestTrue(TEXT("actual dataset output exists"),FPaths::FileExists(Scene/File));
        for(const TCHAR* File:{TEXT("depth.exr"),TEXT("seg.png"),TEXT("instance.png")})Test->TestEqual(TEXT("UI truth toggle controls saved planes"),FPaths::FileExists(Scene/File),WithTruth);
        FString Text;TSharedPtr<FJsonObject> Manifest;FFileHelper::LoadFileToString(Text,*(Directory/TEXT("dataset.json")));
        if(Test->TestTrue(TEXT("dataset manifest parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest)))
        {
            Test->TestEqual(TEXT("UI Camera truth reaches manifest"),Manifest->GetBoolField(TEXT("camera_truth")),WithTruth);Test->TestEqual(TEXT("UI LiDAR fidelity reaches manifest"),Manifest->GetBoolField(TEXT("lidar_fidelity")),WithTruth);
            for(int32 I=0;I<15;++I)Test->TestEqual(TEXT("every UI randomization switch reaches stored conditions"),Manifest->GetObjectField(LexToString(I))->GetBoolField(TEXT("enabled")),WithTruth);
        }
        TArray<uint8> Bytes;FFileHelper::LoadFileToArray(Bytes,*(Scene/TEXT("lidar.pcd")));int32 Body=INDEX_NONE;
        for(int32 I=0;I+12<=Bytes.Num();++I)if(FMemory::Memcmp(Bytes.GetData()+I,"DATA binary\n",12)==0){Body=I+12;break;}
        bool Nonzero=false;if(Body>=0)for(int32 I=Body;I+33<=Bytes.Num();I+=33)for(int32 J=20;J<28;++J)Nonzero|=Bytes[I+J]!=0;
        Test->TestTrue(TEXT("binary PCD body exists"),Body>=0);Test->TestEqual(TEXT("UI fidelity reaches binary acquisition time offsets"),Nonzero,WithTruth);
        Test->TestFalse(TEXT("dataset restores original Camera GT after completion"),Camera->CaptureComponent->bGroundTruthEnabled);
        Test->TestFalse(TEXT("dataset restores original LiDAR fidelity after completion"),Lidar->ScanComponent->bUseRollingPointTimestamps||Lidar->ScanComponent->bEnableMotionDistortion||Lidar->ScanComponent->bEnableMaterialResponse);
        return true;
    }
    bool Finish()
    {
        if(Service){Service->OnSceneSnapshotForTests=nullptr;Service->Stop();}if(Monitor)Monitor->bOwnedAdvancedMenuOpen=false;if(Camera)Camera->StopSensor();if(Lidar)Lidar->StopSensor();
        FString Lines=FString::Join(Outputs,TEXT("\n"));FFileHelper::SaveStringToFile(Lines,*(FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-15/toggle-output-paths.txt")));return true;
    }
public:
    explicit FUxr15ToggleRhiCommand(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        using namespace Uxr15ToggleTests;
        const double Now=FPlatformTime::Seconds();if(Now-Started>140){Test->AddError(TEXT("UI-to-output toggle timeout"));return Finish();}
        if(!World.IsValid())
        {
            UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)Candidate=C.World();if(!FinalAcceptanceViewportReady(Candidate))return false;World=Candidate;
            auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();if(!C)return Finish();C->StopAllSensors();
            Slab=World->SpawnActor<ASlabActor>();Slab->Tags.Add(TEXT("SensorTestManaged"));Slab->SetActorLocation(FVector(1500,0,200));Slab->ApplyDatasetGeometry(FVector(800,200,50));Slab->SetDiagnosticHelpersVisible(false);
            const FVector Origin(1500,-1000,1000);const FTransform Pose((Slab->GetActorLocation()-Origin).Rotation(),Origin);
            Camera=World->SpawnActor<AVirtualCameraSensorActor>();Camera->Tags.Add(TEXT("SensorTestManaged"));Camera->StopSensor();Camera->SetActorTransform(Pose);Camera->CaptureComponent->SensorId=TEXT("UXR15-CAMERA");Camera->CaptureComponent->SimulationQuality=EVirtualSensorSimulationQuality::Custom;Camera->CaptureComponent->CaptureResolution=FIntPoint(320,180);Camera->CaptureComponent->CaptureInterval=.1f;Camera->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;C->RegisterSensorActor(Camera);
            Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Lidar->Tags.Add(TEXT("SensorTestManaged"));Lidar->StopSensor();Lidar->SetActorTransform(Pose);auto* Scan=Lidar->ScanComponent.Get();Scan->SensorId=TEXT("UXR15-LIDAR");Scan->SimulationQuality=EVirtualSensorSimulationQuality::Custom;Scan->HorizontalSamples=64;Scan->VerticalChannels=16;Scan->HorizontalFov=80;Scan->MinVerticalAngle=-12;Scan->MaxVerticalAngle=12;Scan->ScanInterval=.05;Scan->OutputMode=EVirtualLidarOutputMode::None;Scan->SetCompatibilityOutputPolicy(EVirtualLidarCompatibilityOutputPolicy::WhenRequested);C->RegisterSensorActor(Lidar);
            Light=World->SpawnActor<ADirectionalLight>();Light->Tags.Add(TEXT("SensorTestManaged"));Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));Monitor=Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));Data=Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data));Service=World->GetSubsystem<UVirtualDatasetSubsystem>();
            W->SetPanelOpen(ESensorToolPanelRole::Settings,true);W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);C->SelectSensorActor(Camera);W->SynchronizeOwnedSelection();Settings->SelectOwnedSettingsTab(0);SettingsSlate=Settings->TakeWidget();Monitor->BindVirtualCamera(Camera->CaptureComponent);Monitor->ShowCameraView();Monitor->bOwnedAdvancedMenuOpen=true;TruthControls=Monitor->BuildCameraTruthControls();Camera->StartSensor();Next=Now+2;return false;
        }
        if(Now<Next)return false;
        auto* Truth=Camera->CaptureComponent->GetGroundTruth();
        if(Phase==0)
        {
            Test->TestFalse(TEXT("default-off UI has no truth output"),Camera->CaptureComponent->bGroundTruthEnabled);
            Toggle(SettingsSlate.ToSharedRef(),TEXT("정답 데이터"),Test);Test->TestTrue(TEXT("UI enables actual running Camera and preserves measurement"),Camera->CaptureComponent->bGroundTruthEnabled&&Camera->IsSensorRunning());
            Click(TruthControls.ToSharedRef(),TEXT("깊이"),Test);Phase=1;return false;
        }
        if(Phase==1)
        {
            if(!Truth||!Truth->LastFrame||!Truth->LastFrame->Classes.Contains(2))return false;
            Test->TestTrue(TEXT("UI enable produces actual depth/semantic/instance frame"),Truth->LastFrame->DepthMeters.ContainsByPredicate([](float V){return V>0;})&&Truth->LastFrame->Instances.ContainsByPredicate([](uint32 V){return V>0;}));
            Monitor->RefreshImageBrush();Test->TestTrue(TEXT("UI preview uses actual cached depth texture"),Monitor->NativeViewBrush.GetResourceObject()==Truth->Preview(1));
            Click(TruthControls.ToSharedRef(),TEXT("정답 데이터 1회 저장"),Test);Phase=2;return false;
        }
        if(Phase==2)
        {
            if(!Truth||Truth->LastSavedDirectory.IsEmpty())return false;
            for(const TCHAR* Name:{TEXT("depth.exr"),TEXT("segmentation.png"),TEXT("instance.png"),TEXT("camera_info.json")})Test->TestTrue(TEXT("UI save button writes actual truth output"),FPaths::FileExists(Truth->LastSavedDirectory/Name));
            Outputs.Add(Truth->LastSavedDirectory);Phase=3;return false;
        }
        if(Phase==3)
        {
            const TCHAR* Names[]={TEXT("RGB"),TEXT("깊이"),TEXT("의미 분류"),TEXT("객체 구분")};Click(TruthControls.ToSharedRef(),Names[Mode],Test);Monitor->RefreshImageBrush();
            Test->TestTrue(TEXT("each real UI mode selects its actual output texture"),Monitor->NativeViewBrush.GetResourceObject()==(Mode?static_cast<UObject*>(Truth->Preview(uint8(Mode))):static_cast<UObject*>(Camera->CaptureComponent->GetCameraRenderTarget())));
            if(++Mode<4){Next=Now+.3;return false;}
            Toggle(SettingsSlate.ToSharedRef(),TEXT("정답 데이터"),Test);Monitor->RefreshCameraTruth();Monitor->RefreshImageBrush();Test->TestFalse(TEXT("UI off cancels real frame and pending truth"),Camera->CaptureComponent->bGroundTruthEnabled||Truth->LastFrame.IsValid()||Truth->IsBusy());Test->TestTrue(TEXT("off is visible in actual preview state"),Monitor->CameraTruthStatus.ToString().Contains(TEXT("꺼짐")));Test->TestTrue(TEXT("disabled truth mode falls back to actual RGB instead of a white image"),Monitor->NativeViewBrush.GetResourceObject()==Camera->CaptureComponent->GetCameraRenderTarget());Camera->StopSensor();
            Data->DatasetDraft.Name=TEXT("UXR15_toggle");Data->DatasetDraft.Scenes=1;Data->DatasetDraft.SettleFrames=1;Data->DatasetSlab=Slab;Data->DatasetCamera=Camera;Data->DatasetLidar=Lidar;Data->DatasetLight=Light;DatasetControls=Data->BuildOwnedDatasetControls();
            Service->OnSceneSnapshotForTests=[this](const FJsonObject& Label)
            {
                ++DatasetSnapshots;Test->TestEqual(TEXT("UI dataset Camera truth reaches actual capture component"),Camera->CaptureComponent->bGroundTruthEnabled,WithTruth);Test->TestEqual(TEXT("UI dataset fidelity reaches all scan flags"),Lidar->ScanComponent->bUseRollingPointTimestamps&&Lidar->ScanComponent->bEnableMotionDistortion&&Lidar->ScanComponent->bEnableMaterialResponse,WithTruth);
                FString Random;FJsonSerializer::Serialize(Label.GetObjectField(TEXT("random_values")).ToSharedRef(),TJsonWriterFactory<>::Create(&Random));TSharedPtr<FJsonObject> A,B;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Random),A);FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ExpectedRandom),B);Test->TestTrue(TEXT("all UI range switches affect actual sampled output"),A&&B&&FJsonValue::CompareEqual(FJsonValueObject(A),FJsonValueObject(B)));
            };
            ConfigureDataset(true);Phase=4;return false;
        }
        if(Phase==4){if(!CheckDataset())return false;ConfigureDataset(false);Phase=5;return false;}
        if(Phase==5){if(!CheckDataset())return false;Test->TestEqual(TEXT("one runtime snapshot per UI-generated scene"),DatasetSnapshots,2);Toggle(DatasetControls.ToSharedRef(),TEXT("데이터셋 생성 사용"),Test);Click(DatasetControls.ToSharedRef(),TEXT("생성 시작"),Test);Test->TestFalse(TEXT("UI master off rejects another actual session"),Service->IsActive());Test->TestEqual(TEXT("UI master off cannot commit another scene"),Service->CompletedScenes(),1);return Finish();}
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr15ToggleRhiTest,"MA0T10.UXR15.UiToggleToPreviewAndDatasetRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr15ToggleRhiTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddWarning(TEXT("SKIP requires actual RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr15ToggleRhiCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
