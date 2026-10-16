#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "EngineUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SToolTip.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorSemanticLabels.h"

// Opt-in real-RHI audit. Existing map is opened in memory; all additional Actors
// are transient session fixtures. No file output, transport or asset saves.
class FUxr17PreviewSession final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),Due=0;
    int32 Combination=0,Step=0;
    bool Initialized=false,Requested=false;
    FString Path;
    FDateTime OldTime;
    TWeakObjectPtr<AVirtualCameraSensorActor> CameraA,CameraB;
    TWeakObjectPtr<AVirtualLidarSensorActor> LidarA,LidarB;
    TUniquePtr<TGuardValue<bool>> Debug;
    bool ChecksOnly=false;
    int32 CheckPhase=0;
    double WaitBegan=0;
    TArray<double> SwitchMs;
    FString TimeoutShot;
    static void Visit(const TSharedRef<SWidget>& Item,TFunctionRef<void(const TSharedRef<SWidget>&)> Fn)
    {Fn(Item);auto* Children=Item->GetChildren();for(int32 I=0;I<Children->Num();++I)Visit(Children->GetChildAt(I),Fn);}
    bool Check(UWorld* World,UVirtualSensorToolWorkspaceSubsystem* W,AVirtualSensorCoordinator* C,UVirtualSensorMonitorPanelWidget* M,double Now)
    {
        if(CheckPhase==0)
        {
            C->StopAllSensors();W->ResetOwnedWorkspaceLayout();W->SetSidebarMode(true);W->SetOwnedPanelFontScale(1.5f);W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);
            CameraA=World->SpawnActor<AVirtualCameraSensorActor>();CameraB=World->SpawnActor<AVirtualCameraSensorActor>();
            for(auto* A:{CameraA.Get(),CameraB.Get()})
            {
                A->Tags.Add(TEXT("SensorTestManaged"));A->StopSensor();A->CaptureComponent->CaptureResolution=FIntPoint(320,180);A->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;A->CaptureComponent->CaptureInterval=.1f;C->RegisterSensorActor(A);
            }
            CameraA->CaptureComponent->SensorId=TEXT("UXR17-아주 긴 카메라 이름 · 상태와 주파수가 잘리면 안 됩니다-ABCDEFGHIJKLMNOPQRSTUVWXYZ");CameraB->CaptureComponent->SensorId=TEXT("UXR17-B");
            CameraA->CaptureComponent->FreezeAcquisitionForTests=true;CameraA->StartSensor();CameraB->StartSensor();C->SelectSensorActor(CameraA.Get());
            W->SetDockedCardRect(FVector2D(1504,740),W->GetDockedCardMinimum());M->RefreshImageBrush();M->RefreshOwnedMonitorText();
            Test->TestTrue(TEXT("fresh running camera shows preparing before first image"),M->bOwnedPreviewWaiting&&!M->bOwnedPreviewTimedOut);
            Test->TestEqual(TEXT("preparing copy"),M->OwnedPreparingText.ToString(),FString(TEXT("영상 준비 중…")));
            Test->TestEqual(TEXT("name keeps full value for tooltip"),M->OwnedSensorName.ToString(),CameraA->GetSensorId());
            Test->TestTrue(TEXT("status/Hz exclude arbitrarily long name"),M->OwnedRateText.ToString().Contains(TEXT("Hz"))&&!M->OwnedRateText.ToString().Contains(TEXT("UXR17")));
            WaitBegan=Now;Due=Now+5.2;CheckPhase=1;return false;
        }
        if(CheckPhase==1)
        {
            if(Now<Due)return false;M->RefreshImageBrush();M->RefreshOwnedMonitorText();
            Test->TestTrue(TEXT("no image after five seconds shows timeout guidance"),M->bOwnedPreviewWaiting&&M->bOwnedPreviewTimedOut);
            Test->TestTrue(TEXT("timeout includes next action"),M->OwnedTimeoutText.ToString().Contains(TEXT("센서 상태를 확인하세요")));
            int32 Headers=0;Visit(M->TakeWidget(),[&](const TSharedRef<SWidget>& Item)
            {
                if(Item->GetType()!=TEXT("STextBlock"))return;auto Text=StaticCastSharedRef<STextBlock>(Item);
                if(Text->GetText().ToString()==M->OwnedRateText.ToString())
                {
                    const auto G=Text->GetCachedGeometry();Test->TestTrue(TEXT("priority status/Hz allotted width fits at 150 percent minimum card"),G.GetLocalSize().X+.5f>=Text->GetDesiredSize().X);
                    const auto Panel=M->TakeWidget()->GetCachedGeometry();const auto Right=Panel.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));Test->TestTrue(TEXT("priority status/Hz stays within card"),Right.X<=Panel.GetLocalSize().X+.5f);++Headers;
                }
            });Test->TestTrue(TEXT("separate rate header is present"),Headers>0);
            TimeoutShot=FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-17/states")/FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S-timeout.png"));
            FScreenshotRequest::RequestScreenshot(TimeoutShot,true,false);CheckPhase=11;return false;
        }
        if(CheckPhase==11)
        {
            if(!FPaths::FileExists(TimeoutShot))return false;
            CameraA->CaptureComponent->FreezeAcquisitionForTests=false;Due=Now;CheckPhase=2;return false;
        }
        if(CheckPhase==2)
        {
            if(CameraA->CaptureComponent->GetAvailableLocalFrameId()<0)return false;
            M->RefreshImageBrush();Test->TestFalse(TEXT("first available image removes wait immediately"),M->bOwnedPreviewWaiting);
            Test->TestFalse(TEXT("first image removes timeout"),M->bOwnedPreviewTimedOut);
            Test->AddInfo(FString::Printf(TEXT("UXR17 first image after frozen acquisition %.3f s; wait removed in same callback"),Now-WaitBegan));
            CheckPhase=20;return false;
        }
        if(CheckPhase==20)
        {
            // One switch per rendered frame, with both cameras already acquiring.
            auto* A=SwitchMs.Num()%2?CameraA.Get():CameraB.Get();const double Begin=FPlatformTime::Seconds();C->SelectSensorActor(A);M->RefreshImageBrush();SwitchMs.Add((FPlatformTime::Seconds()-Begin)*1000);
            Test->TestFalse(TEXT("running camera switch needs no blank/wait frame"),M->bOwnedPreviewWaiting);
            Test->TestTrue(TEXT("switch uses selected camera's own cached target"),M->NativeViewBrush.GetResourceObject()==A->CaptureComponent->GetCameraRenderTarget());
            if(SwitchMs.Num()<20)return false;
            SwitchMs.Sort();Test->AddInfo(FString::Printf(TEXT("UXR17 running switch n=20 separate rendered frames callback p50=%.4f p95=%.4f max=%.4f ms; blank/wait frames=0; cached target bound in same callback"),(SwitchMs[9]+SwitchMs[10])*.5,SwitchMs[18],SwitchMs[19]));
            C->SelectSensorActor(CameraA.Get());CameraA->CaptureComponent->bGroundTruthEnabled=true;M->CameraTruthDisplayMode=1;M->RefreshCameraTruth();M->RefreshImageBrush();
            Test->TestTrue(TEXT("first truth texture shows preparing instead of a blank brush"),M->bOwnedPreviewWaiting);
            Due=Now+.4;CheckPhase=3;return false;
        }
        if(CheckPhase==3)
        {
            const auto* Truth=CameraA->CaptureComponent->GetGroundTruth();if(Now<Due||!Truth||!Truth->Preview(1))return false;
            for(uint8 Mode=1;Mode<=3;++Mode)
            {
                M->CameraTruthDisplayMode=Mode;M->RefreshCameraTruth();M->RefreshImageBrush();M->RefreshOwnedMonitorText();
                Test->TestEqual(TEXT("card legend uses actual truth mode"),M->OwnedCameraMode,Mode);Test->TestFalse(TEXT("valid truth texture removes waiting"),M->bOwnedPreviewWaiting);
            }
            bool Min=false,Max=false,Instance=false;Visit(M->TakeWidget(),[&](const TSharedRef<SWidget>& Item){if(Item->GetType()!=TEXT("STextBlock"))return;const FString Text=StaticCastSharedRef<STextBlock>(Item)->GetText().ToString();Min|=Text==TEXT("0 m");Max|=Text==TEXT("50 m");Instance|=Text.Contains(TEXT("개체별 색"));});
            Test->TestTrue(TEXT("depth display min/max and instance explanation exist in card"),Min&&Max&&Instance);
            TArray<FLinearColor> Colors;Visit(M->TakeWidget(),[&](const TSharedRef<SWidget>& Item){if(Item->GetType()==TEXT("SBorder"))Colors.Add(StaticCastSharedRef<SBorder>(Item)->GetBorderBackgroundColor().GetSpecifiedColor());});
            for(uint8 Id=0;Id<7;++Id)Test->TestTrue(TEXT("card contains canonical class palette swatch"),Colors.Contains(FLinearColor::FromSRGBColor(VirtualSensorSemanticLabels::Color(Id))));
            Test->TestTrue(TEXT("depth bar blue endpoint"),Colors.Contains(FLinearColor::Blue));Test->TestTrue(TEXT("depth bar red endpoint"),Colors.Contains(FLinearColor::Red));
            M->CameraTruthDisplayMode=0;M->RefreshCameraTruth();M->RefreshImageBrush();M->RefreshOwnedMonitorText();Test->TestEqual(TEXT("RGB badge restored"),M->OwnedModeText.ToString(),FString(TEXT("일반")));
            W->SetPanelOpen(ESensorToolPanelRole::Monitor,false);const auto Old=M->OwnedMonitorStatus;M->RefreshOwnedMonitorText();Test->TestTrue(TEXT("hidden card does not assemble status"),Old.EqualTo(M->OwnedMonitorStatus));
            CameraA->StopSensor();CameraB->StopSensor();C->UnregisterSensorActor(CameraA.Get());CameraA->Destroy();C->UnregisterSensorActor(CameraB.Get());CameraB->Destroy();return true;
        }
        return false;
    }
    const TCHAR* Names[12]={TEXT("01_default"),TEXT("02_camera_a"),TEXT("03_camera_b_immediate"),TEXT("04_camera_b_after_1_5s"),TEXT("05_lidar_normal"),TEXT("06_lidar_pointcloud_only"),TEXT("07_lidar_b"),TEXT("08_camera_truth_depth"),TEXT("09_camera_truth_semantic"),TEXT("10_camera_truth_instance"),TEXT("11_camera_stopped"),TEXT("12_lidar_stopped")};
public:
    explicit FUxr17PreviewSession(FAutomationTestBase* In,bool InChecksOnly=false):Test(In),ChecksOnly(InChecksOnly){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
        const bool Manual=FParse::Param(FCommandLine::Get(),TEXT("Uxr17Manual"));
        if(Now-Started>(Manual?1200:240)){Test->AddError(TEXT("UXR17 preview timeout"));return true;}
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W?W->GetCoordinator():nullptr;
        auto* M=W?Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor)):nullptr;
        if(!C||!M)return false;
        if(ChecksOnly)return Check(World,W,C,M,Now);
        if(!Initialized)
        {
            if(!Debug)Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            C->StopAllSensors();W->ResetOwnedWorkspaceLayout();W->SetSidebarMode(true);
            W->SetOwnedPanelFontScale(Combination%2?1.5f:1.f);
            for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);
            for(auto* A:C->GetSensorActors())
            {
                if(!CameraA.IsValid())CameraA=Cast<AVirtualCameraSensorActor>(A);
                if(!LidarA.IsValid())LidarA=Cast<AVirtualLidarSensorActor>(A);
            }
            if(!CameraA.IsValid()||!LidarA.IsValid()){Test->AddError(TEXT("map sensors absent"));return true;}
            // Same UXR16 manual scene as PreviewSwitch; no existing geometry moves.
            auto* Slab=World->SpawnActor<ASlabActor>();Slab->Tags.Add(TEXT("SensorTestManaged"));Slab->SetActorLocation(FVector(1500,2500,200));Slab->ApplyDatasetGeometry(FVector(800,200,50));Slab->SetDiagnosticHelpersVisible(false);
            const FVector Origin=Slab->GetActorLocation()+FVector(-1000,-1000,800);const FTransform Pose((Slab->GetActorLocation()-Origin).Rotation(),Origin);
            CameraB=World->SpawnActor<AVirtualCameraSensorActor>();CameraB->Tags.Add(TEXT("SensorTestManaged"));CameraB->StopSensor();CameraB->SetActorTransform(Pose);
            auto* Capture=CameraB->CaptureComponent.Get();Capture->SensorId=TEXT("UXR16-MANUAL-CAMERA");Capture->CaptureResolution=FIntPoint(640,360);Capture->CaptureInterval=.1f;Capture->SimulationQuality=EVirtualSensorSimulationQuality::Custom;Capture->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;C->RegisterSensorActor(CameraB.Get());
            LidarB=World->SpawnActor<AVirtualLidarSensorActor>();LidarB->Tags.Add(TEXT("SensorTestManaged"));LidarB->StopSensor();LidarB->SetActorTransform(Pose);LidarB->ScanComponent->SensorId=TEXT("UXR16-MANUAL-LIDAR");LidarB->ScanComponent->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);LidarB->ScanComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);LidarB->ScanComponent->OutputMode=EVirtualLidarOutputMode::None;C->RegisterSensorActor(LidarB.Get());
            LidarA->StartSensor();C->SelectSensorActor(LidarA.Get());M->SetMonitorPresentation(EVirtualSensorMonitorPresentation::TwoDimensional);
            W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);
            const auto Size=Combination<2?FVector2D(400,288):W->GetDockedCardMinimum();
            W->SetDockedCardRect(FVector2D(1904-Size.X,1028-Size.Y),Size);
            Initialized=true;Due=Now+1.5;return false;
        }
        if(Now<Due)return false;
        if(!Requested)
        {
            if(Step==1){C->SelectSensorActor(CameraA.Get());CameraA->StartSensor();Due=Now+1.5;Requested=true;return false;}
            if(Step==2){CameraB->StartSensor();C->SelectNextCamera();}
            if(Step==4){M->SetMonitorPresentation(EVirtualSensorMonitorPresentation::TwoDimensional);C->SelectSensorActor(LidarA.Get());Due=Now+1.5;Requested=true;return false;}
            if(Step==5){M->SetMonitorPresentation(EVirtualSensorMonitorPresentation::PointCloudOnly);Due=Now+1.5;Requested=true;return false;}
            if(Step==6){M->SetMonitorPresentation(EVirtualSensorMonitorPresentation::TwoDimensional);LidarB->StartSensor();C->SelectSensorActor(LidarB.Get());Due=Now+1.5;Requested=true;return false;}
            if(Step==7)
            {
                // Match CAM-GT-01's view of the existing validation Slab in memory.
                auto* A=CameraA.Get();A->StopSensor();
                for(TActorIterator<ASlabActor> It(World);It;++It)if(*It!=nullptr&&It->GetActorLocation()!=FVector(1500,2500,200))
                {const FVector Target=It->GetActorLocation(),Origin=Target+FVector(-1000,-1000,800);A->SetActorTransform(FTransform((Target-Origin).Rotation(),Origin));break;}
                auto* Capture=A->CaptureComponent.Get();Capture->CaptureResolution=FIntPoint(640,360);Capture->CaptureInterval=.1f;Capture->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;Capture->bGroundTruthEnabled=true;
                C->SelectSensorActor(A);A->StartSensor();M->CameraTruthDisplayMode=1;M->RefreshCameraTruth();Due=Now+3;Requested=true;return false;
            }
            if(Step==8||Step==9){M->CameraTruthDisplayMode=uint8(Step-6);M->RefreshCameraTruth();M->RefreshImageBrush();Due=Now+.4;Requested=true;return false;}
            if(Step==10){M->CameraTruthDisplayMode=0;CameraA->StopSensor();C->SelectSensorActor(CameraA.Get());Due=Now+.4;Requested=true;return false;}
            if(Step==11){LidarA->StopSensor();C->SelectSensorActor(LidarA.Get());Due=Now+.4;Requested=true;return false;}
            Requested=true;
        }
        if(Path.IsEmpty())
        {
            M->RefreshImageBrush();M->RefreshStatusText();
            if(Step==3)
            {
                Test->TestTrue(TEXT("long name card keeps state/Hz independently"),M->OwnedRateText.ToString().Contains(TEXT("Hz"))&&!M->OwnedRateText.ToString().Contains(TEXT("UXR16")));
                const auto Tip=M->NativeStatusTextBlock->GetToolTip();Test->TestTrue(TEXT("name tooltip exists"),Tip.IsValid());
                if(Tip)Test->TestEqual(TEXT("name tooltip retains full sensor name"),StaticCastSharedRef<SToolTip>(Tip->AsWidget())->GetTextTooltip().ToString(),M->OwnedMonitorStatus.ToString());
            }
            if(Step>=7&&Step<=9)Test->TestEqual(TEXT("rendered card truth mode badge matches selected mode"),M->OwnedCameraMode,uint8(Step-6));
            const auto* Truth=CameraA->CaptureComponent->GetGroundTruth();
            if(Step>=7&&Step<=9&&(!Truth||!Truth->Preview(uint8(Step-6))))return false;
            Path=Root/FString::Printf(TEXT("%s-%d-%s.png"),Combination<2?TEXT("regular"):TEXT("narrow"),Combination%2?150:100,Names[Step]);
            auto Record=MakeShared<FJsonObject>();Record->SetStringField(TEXT("capture"),Path);Record->SetNumberField(TEXT("client_width"),World->GetGameViewport()->Viewport->GetSizeXY().X);Record->SetNumberField(TEXT("client_height"),World->GetGameViewport()->Viewport->GetSizeXY().Y);Record->SetNumberField(TEXT("font"),W->GetOwnedPanelFontScale());
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Record->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);
            Record->SetStringField(TEXT("panel_size"),M->GetEffectivePanelSize().ToString());Record->SetStringField(TEXT("panel_position"),M->GetCurrentPanelPosition().ToString());Record->SetStringField(TEXT("sensor"),C->GetSelectedSensorActor()->GetSensorId());Record->SetNumberField(TEXT("seconds"),Now-Started);
            FString Json;FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*FPaths::ChangeExtension(Path,TEXT("json")));
            OldTime=IFileManager::Get().GetTimeStamp(*Path);FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=OldTime)return false;
        if(Manual)return FPaths::FileExists(Root/TEXT("finish.flag"));
        Path.Empty();Requested=false;Due=Now+(Step==2?1.5:.4);
        if(++Step<12)return false;
        C->StopAllSensors();CameraA->CaptureComponent->bGroundTruthEnabled=false;M->CameraTruthDisplayMode=0;
        C->UnregisterSensorActor(CameraB.Get());CameraB->Destroy();C->UnregisterSensorActor(LidarB.Get());LidarB->Destroy();
        if(++Combination==4)return true;
        Initialized=false;Step=0;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr17PreviewAudit,"UXR17.PreviewSwitchRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr17PreviewAudit::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP UXR17 capture requires real RHI"));return true;}
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT")).IsEmpty()){AddInfo(TEXT("SKIP explicit UXR17 capture root required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr17PreviewSession(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr17PreviewStates,"MA0T10.UXR17.PreviewStatesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr17PreviewStates::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP UXR17 preview states requires real RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr17PreviewSession(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
