#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraPayloadCodec.h"
#include "Misc/Base64.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPeriodicCadenceTest,"MA0T10.SensorFiles.PeriodicCadence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPeriodicCadenceTest::RunTest(const FString&)
{
	using P=UVirtualSensorPeriodicFileSaveSubsystem;
	for(double Interval:{.05,1.0})
	{
		const double Origin=1234.5;int64 Next=0;
		for(int64 I=0;I<12000;++I)
		{
			const double Now=Origin+I*Interval;
			if(P::AdvanceDeadline(Origin,Interval,Now,Next)!=1||Next!=I+1){AddError(TEXT("Monotonic periodic deadline drift or duplicate"));return false;}
		}
		TestTrue(TEXT("next deadline derived from origin not accumulated time"),FMath::IsNearlyEqual(Origin+Next*Interval,Origin+12000*Interval,1.e-8));
	}
	int64 Next=0;
	TestEqual(TEXT("hitch counts past slots without starting catchup writes"),P::AdvanceDeadline(100,.05,101.02,Next),int64(21));
	TestEqual(TEXT("hitch resumes at future grid deadline"),Next,int64(21));
	TestEqual(TEXT("same tick cannot acquire again"),P::AdvanceDeadline(100,.05,101.02,Next),int64(0));
	TestEqual(TEXT("invalid interval ignored"),P::AdvanceDeadline(100,0,200,Next),int64(0));
	TestTrue(TEXT("frame zero is a valid unique frame"),P::IsNewFrame(0,-1));
	TestFalse(TEXT("missing frame rejected"),P::IsNewFrame(-1,-1));
	TestFalse(TEXT("duplicate frame rejected"),P::IsNewFrame(10,10));
	TestFalse(TEXT("stale frame rejected"),P::IsNewFrame(9,10));
	TestTrue(TEXT("newer frame accepted"),P::IsNewFrame(11,10));
	FVirtualSensorCaptureSelection S;FString Error;
	S.IntervalSeconds=.05f;TestTrue(TEXT("minimum interval accepted"),P::ValidateSelection(EVirtualSensorKind::Lidar,S,Error));
	S.IntervalSeconds=3600;TestTrue(TEXT("maximum interval accepted"),P::ValidateSelection(EVirtualSensorKind::Lidar,S,Error));
	S.IntervalSeconds=.01f;TestFalse(TEXT("below minimum rejected not silently retimed"),P::ValidateSelection(EVirtualSensorKind::Lidar,S,Error));
	S.IntervalSeconds=1;S.bPointCloud=false;S.bLidarPayload=false;TestFalse(TEXT("selected kind needs an enabled output"),P::ValidateSelection(EVirtualSensorKind::Lidar,S,Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPeriodicSessionIsolationTest,"MA0T10.SensorFiles.PeriodicSessionIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPeriodicSessionIsolationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* P=World->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();
	auto* F=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>();
	auto* Sensor=World->SpawnActor<AVirtualLidarSensorActor>();
	FVirtualSensorCaptureSelection Selection;Selection.IntervalSeconds=1;
	const bool WasRunning=Sensor->IsSensorRunning();
	const int32 RequestsBefore=F->GetRecentRequests().Num();
	const FString Id=P->StartPeriodicSave(Sensor,Selection);
	TestTrue(TEXT("session starts independently from any widget"),P->GetPeriodicStatus(Id).bActive);
	TestTrue(TEXT("legacy timed root preserved"),P->GetPeriodicStatus(Id).Folder.Contains(TEXT("SensorCaptures/LocalTimedCapture")));
	const FString Duplicate=P->StartPeriodicSave(Sensor,Selection);
	TestFalse(TEXT("same actor cannot have duplicate periodic writers"),P->GetPeriodicStatus(Duplicate).bActive);
	TestTrue(TEXT("duplicate start preserves original session"),P->GetPeriodicStatus(Id).bActive);
	Selection.IntervalSeconds=10;Selection.bPointCloud=false;
	TestEqual(TEXT("active interval frozen at start"),P->GetPeriodicStatus(Id).IntervalSeconds,1.f);
	TestTrue(TEXT("active outputs frozen at start"),P->GetPeriodicStatus(Id).Selection.bPointCloud);
	P->Tick(.016f);
	TestFalse(TEXT("missing completed frame does not create save request"),P->GetPeriodicStatus(Id).bPending);
	TestEqual(TEXT("no implicit acquisition request from empty current frame"),F->GetRecentRequests().Num(),RequestsBefore);
	TestEqual(TEXT("periodic UI does not start sensor acquisition"),Sensor->IsSensorRunning(),WasRunning);
	TestTrue(TEXT("empty latest frame produces bounded waiting marker"),P->GetPeriodicStatus(Id).bWaitingForLatestFrame);
	TestTrue(TEXT("stop accepted"),P->StopPeriodicSave(Id));
	TestFalse(TEXT("stop disables new admissions"),P->GetPeriodicStatus(Id).bActive);
	P->Tick(.016f);TestEqual(TEXT("stopped session cannot submit more work"),F->GetRecentRequests().Num(),RequestsBefore);
	Selection.bPointCloud=true;Selection.IntervalSeconds=1;
	const FString Moving=P->StartPeriodicSave(Sensor,Selection);
	Sensor->SetActorLocation(FVector(50,0,0));P->Tick(.016f);
	TestTrue(TEXT("natural movement does not cancel periodic acquisition snapshots"),P->GetPeriodicStatus(Moving).bActive);
	P->StopPeriodicSave(Moving);
	const FString Changed=P->StartPeriodicSave(Sensor,Selection);
	Sensor->ScanComponent->MaxDistance+=10;P->Tick(.016f);
	TestFalse(TEXT("revision change ends fixed-target session"),P->GetPeriodicStatus(Changed).bActive);
	TestEqual(TEXT("revision change counted once"),P->GetPeriodicStatus(Changed).FailedCount,int64(1));
	for(int32 I=0;I<40;++I)P->StartPeriodicSave(nullptr,Selection);
	TestTrue(TEXT("inactive status history bounded"),P->GetRecentPeriodicSessions().Num()<=P->MaxRetainedSessions);
	Sensor->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPeriodicUnavailableOutputTest,"MA0T10.SensorFiles.PeriodicUnavailableCameraOutput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPeriodicUnavailableOutputTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* P=World->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();auto* Files=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>();
	auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();
	FVirtualSensorCaptureSelection Selection;Selection.bCameraImage=true;Selection.bCameraPayload=false;
	const int32 Before=Files->GetRecentRequests().Num();
	TestFalse(TEXT("invalid external input is rejected by camera parser"),Camera->CaptureComponent->InjectExternalJsonPayload(TEXT("{}"),false));
	const FString Empty=P->StartPeriodicSave(Camera,Selection);P->Tick(.016f);
	const auto NoFrame=P->GetPeriodicStatus(Empty);
	TestEqual(TEXT("missing acquisition is distinct from unsupported output"),NoFrame.WaitReason,EVirtualSensorPeriodicSaveWaitReason::NoCompletedFrame);
	TestTrue(TEXT("source parser failure remains in source diagnostic"),NoFrame.LastMessage.Contains(TEXT("rejected")));
	TestEqual(TEXT("missing input does not masquerade as a file parsing failure"),NoFrame.FailedCount,int64(0));
	P->StopPeriodicSave(Empty);

	FVirtualCameraPayloadSnapshot Descriptor;Descriptor.SensorId=Camera->GetSensorId();Descriptor.Manufacturer=TEXT("Fixture");Descriptor.Model=TEXT("JSONOnly");Descriptor.SimulationQuality=TEXT("Debug");Descriptor.FrameId=301;Descriptor.TimestampUtc=FDateTime(2026,9,21,1,2,3);Descriptor.Width=2;Descriptor.Height=2;Descriptor.HorizontalFov=87;Descriptor.VerticalFov=58;
	const TArray<uint8> Signature={0xff,0xd8,0xff,0xd9};
	const FString Original=FVirtualCameraPayloadCodec::EncodeJpegBase64(Descriptor,FBase64::Encode(Signature),Signature.Num());
	TestTrue(TEXT("valid JSON-only input accepted"),Camera->CaptureComponent->InjectExternalJsonPayload(Original,false));
	TestEqual(TEXT("source frame exists even without local JPEG snapshot"),Files->GetAvailableFrameId(Camera),int64(301));
	const FString Id=P->StartPeriodicSave(Camera,Selection);P->Tick(.016f);
	const auto Waiting=P->GetPeriodicStatus(Id);
	TestTrue(TEXT("unsupported requested JPEG waits instead of stopping session"),Waiting.bActive&&Waiting.bWaitingForLatestFrame);
	TestEqual(TEXT("wait reason identifies selected output rather than missing frame"),Waiting.WaitReason,EVirtualSensorPeriodicSaveWaitReason::SelectedOutputUnavailable);
	TestEqual(TEXT("one skipped deadline not a failed save"),Waiting.SkippedCount,int64(1));
	TestEqual(TEXT("missing JPEG is not a codec failure"),Waiting.FailedCount,int64(0));
	TestFalse(TEXT("no fake readback or file request created"),Waiting.bPending);
	TestTrue(TEXT("no unrelated substitute image request"),Waiting.LastRequestId.IsEmpty());
	TestTrue(TEXT("user is told the selected JPEG is unavailable"),Waiting.LastMessage.Contains(TEXT("JPEG")));
	TestEqual(TEXT("file service receives no unsupported periodic requests"),Files->GetRecentRequests().Num(),Before);
	Selection.bCameraImage=false;Selection.bCameraPayload=true;
	TestTrue(TEXT("same valid snapshot remains available to explicit JSON-only selection"),Files->CanSaveCurrent(Camera,Selection));
	TestTrue(TEXT("running output policy did not mutate"),P->GetPeriodicStatus(Id).Selection.bCameraImage);
	P->StopPeriodicSave(Id);Camera->Destroy();return true;
}
#endif
