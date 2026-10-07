#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorSettingsReliabilityTest,"MA0T10.SensorControl.SettingsStateAndFiniteInputs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorSettingsReliabilityTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>();
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();
    auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();
    Manager->DiscoverSensorsInLevel();
    auto* Panel=NewObject<UVirtualSensorSettingsPanelWidget>(World);
    Panel->BindSensorManager(Manager);
    FString Error;
    for(auto* Actor:{static_cast<AVirtualSensorActorBase*>(Camera),static_cast<AVirtualSensorActorBase*>(Lidar)})
    {
        Actor->StopSensor();
        FVirtualSensorEditableState State;Actor->ReadEditableState(State);
        State.SensorId+=TEXT("-EDIT");State.CameraCaptureInterval=.2f;State.LidarScanInterval=.2f;
        State.SimulationQuality=EVirtualSensorSimulationQuality::Custom;
        const int64 Before=Actor->GetSensorRuntimeStatus().FrameId;
        TestTrue(TEXT("stopped edit accepted"),Actor->ApplyEditableState(State,Error));
        TestFalse(TEXT("stopped state retained"),Actor->IsSensorRunning());
        TestEqual(TEXT("no synchronous acquisition on edit"),Actor->GetSensorRuntimeStatus().FrameId,Before);
        Actor->StartSensor();
        TestTrue(TEXT("running edit accepted"),Actor->ApplyEditableState(State,Error));
        TestTrue(TEXT("running state retained"),Actor->IsSensorRunning());
        TestEqual(TEXT("running edit does not capture synchronously"),Actor->GetSensorRuntimeStatus().FrameId,Before);
        const auto Pose=Actor->GetActorTransform();
        for(float Bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        {
            auto Invalid=State;Invalid.CameraCaptureInterval=Bad;Invalid.LidarScanInterval=Bad;
            Invalid.ActorTransform.SetLocation(FVector(999,999,999));
            TestFalse(TEXT("invalid scalar rejected by Actor"),Actor->ApplyEditableState(Invalid,Error));
            TestFalse(TEXT("invalid scalar rejected by UI"),UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(Invalid,{},Error));
            TestTrue(TEXT("invalid input retains pose"),Actor->GetActorTransform().Equals(Pose));
            TestTrue(TEXT("invalid input retains running state"),Actor->IsSensorRunning());
        }
        Actor->StopSensor();
        Panel->SelectTargetKind(Actor==Lidar?EVirtualSensorTargetKind::Lidar:EVirtualSensorTargetKind::Camera);
        TestTrue(TEXT("native general settings accepted"),Panel->ApplyPendingState());
        TestFalse(TEXT("native settings do not restart stopped sensor"),Actor->IsSensorRunning());
        TestEqual(TEXT("native settings do not add sync frame"),Actor->GetSensorRuntimeStatus().FrameId,Before);
    }
    Camera->Destroy();Lidar->Destroy();Manager->Destroy();return true;
}
#endif
