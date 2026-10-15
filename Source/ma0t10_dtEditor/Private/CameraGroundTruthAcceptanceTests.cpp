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
