#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"

namespace
{
class FManipulationRuntimeCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
    int32 KindIndex = 0;
    int32 Step = 0;
    int64 FrameBeforeExit = 0;
    TWeakObjectPtr<AVirtualSensorActorBase> Target;
    FVirtualSensorEditableState Original;
public:
    explicit FManipulationRuntimeCheck(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 25)
        {
            Test->AddError(TEXT("Manipulation runtime test timed out waiting for a restored frame"));
            return true;
        }
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE) World = Context.World();
        if (!World) return false;
        auto* Workspace = World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        auto* Coordinator = Workspace->GetCoordinator();
        auto* Settings = Cast<UVirtualSensorSettingsPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Settings));
        if (!Coordinator || !Settings) return false;
        if (FPlatformTime::Seconds() - Started < 3.0) return false;
        if (Step != 0 && !Target.IsValid())
        {
            Test->AddError(TEXT("Test sensor disappeared during runtime verification"));
            return true;
        }
        if (Step == 0)
        {
            Workspace->SetPanelOpen(ESensorToolPanelRole::Settings, true);
            Coordinator->SetViewMode(KindIndex == 0 ? EVirtualSensorViewMode::Camera : EVirtualSensorViewMode::Lidar);
            Workspace->SynchronizeOwnedSelection();
            Settings->SetSelectedSimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
            Target = Coordinator->GetSelectedSensorActor();
            if (!Target.IsValid()) { Test->AddError(TEXT("No test sensor")); return true; }
            Original = FVirtualSensorEditableState();
            Target->ReadEditableState(Original);
            Target->StartSensor();
            Settings->SetSensorManipulationEnabled(true);
            auto* Gizmo = Settings->GetTransformGizmoActor();
            if (!Gizmo) { Test->AddError(TEXT("No manipulation gizmo")); return true; }
            Test->TestTrue(TEXT("actor and gizmo begin together"), Target->IsInteractiveManipulationActive() && Gizmo->IsManipulationEnabled());
            FTransform Moved = Target->GetActorTransform();
            Moved.AddToTranslation(FVector(10, 0, 0));
            Gizmo->OnTransformChanged.Broadcast(Moved);
            Gizmo->OnTransformCommitted.Broadcast(Moved);
            Test->TestTrue(TEXT("drag commit does not exit interaction"), Target->IsInteractiveManipulationActive());
            FrameBeforeExit = Target->GetSensorRuntimeStatus().FrameId;
            Gizmo->RequestManipulationExit(); // Same production path used by Escape; not a physical keyboard test.
            Test->TestFalse(TEXT("exit request ends actor interaction"), Target->IsInteractiveManipulationActive());
            Test->TestFalse(TEXT("exit request ends gizmo interaction"), Gizmo->IsManipulationEnabled());
            Test->TestTrue(TEXT("running sensor resumes"), Target->IsSensorRunning());
            FVirtualSensorEditableState Restored;
            Target->ReadEditableState(Restored);
            Test->TestEqual(TEXT("camera resolution restored"), Restored.CameraResolution, Original.CameraResolution);
            Test->TestEqual(TEXT("camera interval restored"), Restored.CameraCaptureInterval, Original.CameraCaptureInterval);
            Test->TestEqual(TEXT("camera output restored"), Restored.CameraCaptureMode, Original.CameraCaptureMode);
            Test->TestEqual(TEXT("LiDAR horizontal count restored"), Restored.LidarHorizontalSamples, Original.LidarHorizontalSamples);
            Test->TestEqual(TEXT("LiDAR vertical count restored"), Restored.LidarVerticalChannels, Original.LidarVerticalChannels);
            Test->TestEqual(TEXT("LiDAR interval restored"), Restored.LidarScanInterval, Original.LidarScanInterval);
            Step = 1;
            return false;
        }
        if (Step == 1)
        {
            if (Target->GetSensorRuntimeStatus().FrameId <= FrameBeforeExit) return false;
            Test->TestTrue(TEXT("restored acquisition produces a subsequent frame"), true);
            Settings->SetSensorManipulationEnabled(true);
            Settings->GetTransformGizmoActor()->SetManipulationEnabled(false);
            Step = 2;
            return false;
        }
        if (Step == 2)
        {
            if (Target->IsInteractiveManipulationActive()) return false;
            Test->TestTrue(TEXT("legacy direct gizmo stop also finalizes actor"), true);
            Settings->SetSensorManipulationEnabled(true);
            Workspace->SetPanelOpen(ESensorToolPanelRole::Settings, false);
            Test->TestFalse(TEXT("hidden settings finalize actor"), Target->IsInteractiveManipulationActive());
            Workspace->SetPanelOpen(ESensorToolPanelRole::Settings, true);
            Settings->SetSensorManipulationEnabled(true);
            Settings->GetTransformGizmoActor()->Destroy();
            Test->TestFalse(TEXT("gizmo EndPlay finalizes actor"), Target->IsInteractiveManipulationActive());
            Settings->SetSensorManipulationEnabled(false);
            Test->TestTrue(TEXT("repeated exit does not stop acquisition"), Target->IsSensorRunning());
            if (++KindIndex < 2) { Step = 0; return false; }
            return true;
        }
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorManipulationRuntimeTest, "MA0T10.SensorV2.UI.ManipulationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSensorManipulationRuntimeTest::RunTest(const FString&)
{
    if (FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_MANIPULATION_RHI")) != TEXT("1"))
    {
        AddInfo(TEXT("SKIP: actual RHI manipulation test requires MA0T10_MANIPULATION_RHI=1"));
        return true;
    }
    if (!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"), true)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FManipulationRuntimeCheck(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
