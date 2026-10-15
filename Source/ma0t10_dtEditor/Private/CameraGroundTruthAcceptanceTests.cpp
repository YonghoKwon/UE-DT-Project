#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "UxrScenarioUiFixture.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

class FCameraTruthPresentationSession:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Created=FPlatformTime::Seconds(),Next=0;
    bool Ready=false,Requested=false;
    int32 Mode=0;
    FString Path;
    FDateTime OldTime;
    TUniquePtr<TGuardValue<bool>> DebugGuard;
public:
    explicit FCameraTruthPresentationSession(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Created>100){Test->AddError(TEXT("camera truth UI render timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W?W->GetCoordinator():nullptr;
        auto* M=W?Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor)):nullptr;
        if(!C||!M)return false;
        if(!Ready)
        {
            DebugGuard=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(1);
            for(int32 Role=0;Role<7;++Role)W->SetPanelOpen(static_cast<ESensorToolPanelRole>(Role),false);
            W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);C->SelectCameraByIndex(0);C->SetViewMode(EVirtualSensorViewMode::Camera);C->BindMonitorWidget(M);
            auto* Camera=C->GetSelectedCamera();auto* Actor=Camera?Cast<AVirtualCameraSensorActor>(Camera->GetOwner()):nullptr;if(!Actor)return false;
            FVirtualSensorEditableState S;Actor->ReadEditableState(S);S.CameraResolution=FIntPoint(640,360);S.CameraCaptureInterval=.1f;S.CameraCaptureMode=EVirtualCameraCaptureMode::PreviewOnly;S.bCameraGroundTruth=true;
            for(TActorIterator<ASlabActor> It(World);It;++It)
            {
                Test->TestTrue(TEXT("memory-only Slab fixture starts"),StartUxrScenarioUiFixture(*It));
                const FVector Target=It->GetActorLocation();const FVector Origin=Target+FVector(-1000,-1000,800);
                S.ActorTransform=FTransform((Target-Origin).Rotation(),Origin);break;
            }
            FString Error;if(!Actor->ApplyEditableState(S,Error)){Test->AddError(Error);return true;}Actor->StartSensor();
            Actor->SetGroundTruthPreviewDemand(true);Actor->RequestGroundTruthSave();Next=Now+3;Ready=true;return false;
        }
        auto* Camera=C->GetSelectedCamera();auto* Truth=Camera?Camera->GetGroundTruth():nullptr;
        if(!Truth||!Truth->LastFrame||Now<Next)return false;
        if(!Requested)
        {
            M->CameraTruthDisplayMode=uint8(Mode);M->RefreshCameraTruth();M->RefreshImageBrush();Next=Now+.4;
            const TCHAR* Names[]={TEXT("rgb"),TEXT("depth"),TEXT("segmentation"),TEXT("instance")};
            Path=FPaths::ProjectSavedDir()/FString::Printf(TEXT("Reports/CAM-GT-01/automated/1920x1080-%s.png"),Names[Mode]);
            OldTime=IFileManager::Get().GetTimeStamp(*Path);FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=OldTime||Now<Next)return false;
        M->RefreshImageBrush();
        Test->TestTrue(TEXT("Slab semantic pixels exist in the presentation fixture"),Truth->LastFrame->Classes.Contains(2));
        Test->TestTrue(TEXT("display uses cached texture without sync capture"),M->NativeViewBrush.GetResourceObject()==(Mode?static_cast<UObject*>(Truth->Preview(uint8(Mode))):static_cast<UObject*>(Camera->GetCameraRenderTarget())));
        const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());
        Test->AddInfo(FString::Printf(TEXT("CAM-GT-01 automated render client=%dx%d font=100 dpi=%.2f mode=%d path=%s saved=%s; no direct mouse acceptance"),Size.X,Size.Y,Window?Window->GetDPIScaleFactor():0,Mode,*Path,*Truth->LastSavedDirectory));
        if(++Mode==4){C->StopAllSensors();return true;}Requested=false;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraTruthPresentation,"MA0T10.CameraGroundTruth.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCameraTruthPresentation::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CameraTruthRender"))){AddInfo(TEXT("SKIP: explicit render only (-CameraTruthRender)"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCameraTruthPresentationSession(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

class FCameraTruthManualSession:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Created=FPlatformTime::Seconds();
    bool Ready=false;
public:
    explicit FCameraTruthManualSession(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Created>1200){Test->AddWarning(TEXT("Manual session timed out; no acceptance implied"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        if(!Ready)
        {
            auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Coordinator=Workspace?Workspace->GetCoordinator():nullptr;if(!Coordinator)return false;
            for(auto* Actor:Coordinator->GetSensorActors())if(Actor)Actor->StopSensor();
            Coordinator->SelectCameraByIndex(0);Coordinator->SetViewMode(EVirtualSensorViewMode::Camera);
            auto* Camera=Coordinator->GetSelectedCamera();if(!Camera){Test->AddError(TEXT("Manual scene camera missing"));return true;}
            if(auto* Actor=Cast<AVirtualCameraSensorActor>(Camera->GetOwner()))
            {
                FVirtualSensorEditableState State;Actor->ReadEditableState(State);State.CameraResolution=FIntPoint(640,360);State.CameraCaptureInterval=.1f;State.CameraCaptureMode=EVirtualCameraCaptureMode::PreviewOnly;State.bCameraGroundTruth=false;
                FString Error;if(!Actor->ApplyEditableState(State,Error)){Test->AddError(Error);return true;}Actor->StartSensor();
            }
            Workspace->ResetOwnedWorkspaceLayout();Workspace->SetOwnedPanelFontScale(1);Workspace->SetPanelOpen(ESensorToolPanelRole::Monitor,true);Workspace->SetPanelOpen(ESensorToolPanelRole::Settings,true);
            Test->AddInfo(TEXT("CAM-GT-01 MANUAL READY: 1920x1080 requested; opt-in initially disabled; finish via Saved/Reports/CAM-GT-01/manual/finish.flag"));Ready=true;
        }
        return IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir()/TEXT("Reports/CAM-GT-01/manual/finish.flag")));
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraTruthManual,"MA0T10.CameraGroundTruth.ManualPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCameraTruthManual::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CameraTruthManual"))){AddInfo(TEXT("SKIP: explicit manual session only (-CameraTruthManual)"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCameraTruthManualSession(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
