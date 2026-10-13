#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "Json.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabPerSensorDeliveryTest,"MA0T10.SlabReliability.PerSensorLedger",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabPerSensorDeliveryTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* R=W->GetSubsystem<USlabRunResultsSubsystem>();
    auto* A=W->SpawnActor<AVirtualLidarSensorActor>();auto* B=W->SpawnActor<AVirtualLidarSensorActor>();A->ScanComponent->SensorId=TEXT("A");B->ScanComponent->SensorId=TEXT("B");
    TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>> Sensors;Sensors.Add(TEXT("A"),A);Sensors.Add(TEXT("B"),B);
    R->ReportDirectoryOverrideForTests=FPaths::ProjectSavedDir()/TEXT("Reports/SensorOutputReliability/ledger");R->BeginRun(TEXT("run"),TEXT("scenario"),ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection());R->SetSensors(TEXT("run"),Sensors,nullptr);
    auto* Camera=W->SpawnActor<AVirtualCameraSensorActor>();Camera->CaptureComponent->SensorId=TEXT("C");Sensors.Add(TEXT("C"),Camera);R->SetSensors(TEXT("run"),Sensors,nullptr);
    const_cast<FVirtualSensorRuntimeStatus&>(A->ScanComponent->GetRuntimeStatus()).MeasuredAcquisitionRateHz=20;
    const_cast<FVirtualSensorRuntimeStatus&>(B->ScanComponent->GetRuntimeStatus()).MeasuredAcquisitionRateHz=20;
    R->SetAppliedOutputs(TEXT("run"),FVirtualSlabSensorOutputSelection());R->SetMeasuredRates(TEXT("run"),Sensors);
    FSlabRunDeliverySummary Rates;R->GetRunResult(TEXT("run"),Rates);
    TestTrue(TEXT("unselected stopped camera does not create a rate failure"),Rates.Warning.IsEmpty());
    FVirtualSlabSensorOutputSelection All;All.bCameraImage=true;All.bLidarTelemetry=true;R->SetAppliedOutputs(TEXT("run"),All);R->SetMeasuredRates(TEXT("run"),Sensors);R->GetRunResult(TEXT("run"),Rates);
    TestFalse(TEXT("selected stopped camera still reports low acquisition rate"),Rates.Warning.IsEmpty());
    R->RecordAcquisition(TEXT("run"),TEXT("A"),1,true);R->RecordAcquisition(TEXT("run"),TEXT("A"),1,true);R->RecordAcquisition(TEXT("run"),TEXT("B"),1,false);
    auto Event=[&](const TCHAR* Id,EVirtualSensorStreamKind Kind,int64 Frame,EVirtualSensorTransportObservationPhase Phase,double At)
    {FVirtualSensorTransportObservation E;E.RunId=TEXT("run");E.SensorId=Id;E.Kind=Kind;E.FrameId=Frame;E.RequestId=FString(Id)+LexToString(Frame);E.Phase=Phase;E.MonotonicSeconds=At;R->ObserveTransport(E);};
    for(int64 F:{1,2})
    {Event(TEXT("A"),EVirtualSensorStreamKind::PointCloud,F,EVirtualSensorTransportObservationPhase::Accepted,F);Event(TEXT("A"),EVirtualSensorStreamKind::PointCloud,F,EVirtualSensorTransportObservationPhase::Submitted,F);Event(TEXT("A"),EVirtualSensorStreamKind::PointCloud,F,EVirtualSensorTransportObservationPhase::Receipt,F+.1);Event(TEXT("A"),EVirtualSensorStreamKind::PointCloud,F,EVirtualSensorTransportObservationPhase::Receipt,F+.2);Event(TEXT("A"),EVirtualSensorStreamKind::PointCloud,F,EVirtualSensorTransportObservationPhase::Consumed,F+.15);}
    Event(TEXT("B"),EVirtualSensorStreamKind::PointCloud,1,EVirtualSensorTransportObservationPhase::Accepted,1);Event(TEXT("B"),EVirtualSensorStreamKind::PointCloud,1,EVirtualSensorTransportObservationPhase::Submitted,1);
    Event(TEXT("A"),EVirtualSensorStreamKind::LidarPayload,1,EVirtualSensorTransportObservationPhase::Accepted,1);Event(TEXT("A"),EVirtualSensorStreamKind::LidarPayload,1,EVirtualSensorTransportObservationPhase::Failed,2);
    Event(TEXT(""),EVirtualSensorStreamKind::PointCloud,77,EVirtualSensorTransportObservationPhase::Failed,2);Event(TEXT(""),EVirtualSensorStreamKind::PointCloud,77,EVirtualSensorTransportObservationPhase::Failed,2);
    FSlabRunDeliverySummary S;R->GetRunResult(TEXT("run"),S);
    TestEqual(TEXT("acquisition duplicate counted once"),S.Acquired,int64(1));TestEqual(TEXT("per-sensor acquisition failure counted"),S.AcquisitionFailures,int64(1));
    TestEqual(TEXT("only selected sensor has completed acquisition"),S.Sensors.FindByPredicate([](const auto& X){return X.SensorId==TEXT("A");})->Acquired,int64(1));
    TestEqual(TEXT("stream details separated by sensor and output"),S.StreamDetails.Num(),3);
    int64 Accepted=0,Receipt=0,Unconfirmed=0,Failure=0;for(const auto& D:S.StreamDetails){Accepted+=D.Accepted;Receipt+=D.Receipts;Unconfirmed+=D.AcceptedUnconfirmed;Failure+=D.Failures;}
    TestTrue(TEXT("attributed totals match aggregate including unknown errors"),Accepted==S.Accepted&&Receipt==S.Receipts&&Unconfirmed==S.Unfinished&&Failure+S.UnattributedDeliveryFailures==S.DeliveryFailures);
    TestEqual(TEXT("unknown request duplicate counted once"),S.UnattributedDeliveryFailures,int64(1));
    const auto* D=S.StreamDetails.FindByPredicate([](const auto& X){return X.SensorId==TEXT("A")&&X.Kind==EVirtualSensorStreamKind::PointCloud;});
    TestTrue(TEXT("event-based rate uses deduplicated timestamps"),D&&D->bHasReceiptRate&&FMath::Abs(D->ReceiptHz-1)<.00001);
    TestTrue(TEXT("B awaiting receipt stays separate"),S.StreamDetails.ContainsByPredicate([](const auto& X){return X.SensorId==TEXT("B")&&X.AcceptedUnconfirmed==1;}));
    FVirtualSlabSessionStatus End;End.RunId=TEXT("run");End.State=EVirtualSlabSessionState::Incomplete;End.UnfinishedFrames=1;End.StreamFailures=5;End.AcquisitionFailures=4;R->Finalize(End);
    R->GetRunResult(TEXT("run"),S);const auto Frozen=S;Event(TEXT("B"),EVirtualSensorStreamKind::PointCloud,1,EVirtualSensorTransportObservationPhase::Receipt,3);R->GetRunResult(TEXT("run"),S);
    TestTrue(TEXT("late receipt never changes finalized aggregate/details"),S.Receipts==Frozen.Receipts&&S.StreamDetails[0].Receipts==Frozen.StreamDetails[0].Receipts);
    TestEqual(TEXT("unattributed final session failures stay visible"),S.UnattributedDeliveryFailures,int64(4));
    TestEqual(TEXT("unattributed terminal acquisition failures stay visible"),S.UnattributedAcquisitionFailures,int64(3));
    TestTrue(TEXT("previous records retain unknown detail state"),USlabRunResultsSubsystem::DescribeStreamDetails(FSlabRunDeliverySummary()).Contains(TEXT("확인 불가")));
    const auto Json=R->SerializeSummary(S);TestTrue(TEXT("v1 schema retained with optional details"),Json.Contains(TEXT("ma0t10.slab-run.v1"))&&Json.Contains(TEXT("streamDetails")));
    return true;
}
#endif
