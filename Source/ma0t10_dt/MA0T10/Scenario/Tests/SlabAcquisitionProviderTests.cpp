#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabAcquisitionProviderTest, "MA0T10.Architecture.SlabProviderAdapter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlabAcquisitionProviderTest::RunTest(const FString&)
{
    auto* World = FAutomationEditorCommonUtils::CreateNewMap();
    auto* Session = World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
    auto* Provider = FVirtualSensorAcquisitionContextProviders::Get(World);
    if (!TestTrue(TEXT("world subsystem registered"), Provider == Session)) return false;
    auto* Manager = World->SpawnActor<AVirtualSensorCoordinator>();
    auto* Sensor = World->SpawnActor<AVirtualLidarSensorActor>();
    Manager->RegisterSensorActor(Sensor);
    Manager->SharedTransportComponent->TransportMode = EVirtualSensorTransportMode::StompWebSocket;
    const FString Run = Session->BeginSlabSensorSession(TEXT("12345678-1234-4321-9876-123456789abc"), {Sensor->GetSensorId()}, true);
    if (!TestFalse(TEXT("session started"), Run.IsEmpty())) return false;
    Session->NotifySlabFrameApplied(Run, TEXT("00042"), 9007199254740995LL, 1.25);
    const auto Acquired = Provider->CaptureAcquisitionContext(Sensor->GetSensorId(), 101);
    TestTrue(TEXT("advance host row"), Session->NotifySlabFrameApplied(Run, TEXT("new-material"), 9007199254740996LL, 2.5));
    TestEqual(TEXT("acquisition material frozen"), Acquired.MtlNo, FString(TEXT("00042")));
    TestEqual(TEXT("source and sensor frame IDs distinct"), Acquired.SlabFrameNo, 9007199254740995LL);
    TestEqual(TEXT("acquisition elapsed frozen"), Acquired.ElapsedSec, 1.25);
    TestTrue(TEXT("busy status mapped"), Provider->IsAcquisitionSessionBusy());
    TestEqual(TEXT("legacy frame admission retained"), Provider->AllowsAcquisitionFrame(Sensor->GetSensorId(), Acquired), Session->AllowsFrame(Sensor->GetSensorId(), FVirtualSlabFrameContext(Acquired)));
    TestTrue(TEXT("selected PCD admitted"), Provider->AllowsOutputFrame(Sensor->GetSensorId(), EVirtualSensorAcquisitionOutput::PointCloud, Acquired));
    TestFalse(TEXT("unselected telemetry denied"), Provider->AllowsOutputFrame(Sensor->GetSensorId(), EVirtualSensorAcquisitionOutput::LidarTelemetry, Acquired));
    TestFalse(TEXT("unselected camera denied"), Provider->AllowsOutputDemand(Sensor->GetSensorId(), EVirtualSensorAcquisitionOutput::CameraImage));
    Provider->CompleteAcquisition(Sensor->GetSensorId(), 101);
    TestEqual(TEXT("acquisition ledger drained"), Session->GetSlabSensorSessionStatus().PendingAcquisitions, 0);
    Session->EndSlabSensorSession(Run, false);
    Session->Tick(0);
    TestFalse(TEXT("retired snapshot rejected"), Provider->AllowsAcquisitionFrame(Sensor->GetSensorId(), Acquired));
    Sensor->Destroy(); Manager->Destroy();
    return true;
}
#endif
