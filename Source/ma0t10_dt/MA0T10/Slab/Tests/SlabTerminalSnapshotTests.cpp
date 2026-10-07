#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "HAL/PlatformProcess.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabTerminalSnapshotTest,"MA0T10.SlabReliability.TerminalSnapshotBeforeCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabTerminalSnapshotTest::RunTest(const FString&)
{
 auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* Manager=W->SpawnActor<AVirtualSensorCoordinator>();auto* Sensor=W->SpawnActor<AVirtualLidarSensorActor>();Manager->RegisterSensorActor(Sensor);
 auto* Transport=Manager->SharedTransportComponent.Get();auto P=Transport->GetTransportProfile();P.BrokerUrl=TEXT("tcp://127.0.0.1:1");Transport->ConfigureTransportProfile(P);Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
 auto* Session=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>();auto* Raw=W->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();auto* Results=W->GetSubsystem<USlabRunResultsSubsystem>();
 const auto Run=Session->BeginSlabSensorSession(FString(),{Sensor->GetSensorId()},true);TestFalse(TEXT("fixture begins"),Run.IsEmpty());Session->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),15,.75);
 Session->CaptureContext(Sensor->GetSensorId(),41);Session->CaptureContext(Sensor->GetSensorId(),42);
 FVirtualSensorHighThroughputProfile Profile;Profile.BrokerUrl=P.BrokerUrl;Raw->StartHighThroughputTransport(Profile,FString());
 FVirtualSensorBinaryFrame Frame;Frame.SensorId=Sensor->GetSensorId();Frame.StreamKind=EVirtualSensorStreamKind::PointCloud;Frame.FrameId=41;Frame.RequestId=TEXT("terminal-41");Frame.Headers.Add(TEXT("x-run-uuid"),Run);Frame.Body32=MakeShared<const TArray<uint8>,ESPMode::ThreadSafe>(TArray<uint8>{1,2,3});FString Error;
 TestTrue(TEXT("frame admitted to disconnected worker"),Raw->EnqueueBinaryFrame(Frame,Error));
 TestEqual(TEXT("reproduction: status before a drain tick still says zero unfinished"),Session->GetSlabSensorSessionStatus().UnfinishedFrames,int64(0));
 // No drain tick: exercise the shutdown path with real admission plus acquisition work.
 Session->Deinitialize();FSlabRunDeliverySummary R;TestTrue(TEXT("shutdown retains result"),Results->GetRunResult(Run,R));
 TestEqual(TEXT("forced shutdown remains partial"),R.Outcome,ESlabRunDeliveryOutcome::Partial);TestEqual(TEXT("acquisition stages preserved"),R.TerminalSnapshot.PendingAcquisitions,int64(2));
 TestEqual(TEXT("socket queue preserved before cancel"),R.TerminalSnapshot.QueuedForSocket,int64(1));TestEqual(TEXT("accepted unconfirmed preserved"),R.TerminalSnapshot.AcceptedUnconfirmed,int64(1));
 TestEqual(TEXT("acquisition and same output not double counted"),R.Unfinished,int64(2));TestTrue(TEXT("terminal timestamp recorded"),R.TerminalSnapshot.CapturedUtc.GetTicks()>0);TestTrue(TEXT("consumer cutoff recorded"),R.ConsumerSnapshotUtc.GetTicks()>0);
 const auto Finished=R.FinishedUtc;Session->Deinitialize();FVirtualSensorTransportObservation Late;Late.RunId=Run;Late.SensorId=Frame.SensorId;Late.FrameId=41;Late.Kind=Frame.StreamKind;Late.Phase=EVirtualSensorTransportObservationPhase::Receipt;Results->ObserveTransport(Late);
 Results->GetRunResult(Run,R);TestEqual(TEXT("late receipt cannot turn forced shutdown into success"),R.Receipts,int64(0));TestEqual(TEXT("finalized once"),R.FinishedUtc,Finished);
 const double Deadline=FPlatformTime::Seconds()+2;while(Raw->GetRunRetentionSnapshot(Run).OutstandingBytes&&FPlatformTime::Seconds()<Deadline)FPlatformProcess::SleepNoStats(.01);
 TestEqual(TEXT("cancel returns held body without reconnect"),Raw->GetRunRetentionSnapshot(Run).OutstandingBytes,int64(0));Raw->StopHighThroughputTransport();Manager->Destroy();Sensor->Destroy();return true;
}
#endif
