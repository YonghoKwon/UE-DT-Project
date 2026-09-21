#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/GameInstance.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabScenarioArchivePolicyTest,"MA0T10.SlabScenario.ArchiveIdentityAndIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabScenarioArchivePolicyTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FSlabScenarioCodec::MakeSyntheticJson()),Root);
	Root->GetObjectField(TEXT("_meta"))->RemoveField(TEXT("UUID"));
	FString Original; FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Original));
	FSlabScenarioDataPtr First,Duplicate; FString Error;
	TestFalse(TEXT("legacy strict registration still requires UUID"),FSlabScenarioCodec::Parse(Original,First,Error,false));
	TestTrue(TEXT("explicit fallback accepts original without UUID"),FSlabScenarioCodec::Parse(Original,First,Error,true));
	if(!First) return false;
	TestTrue(TEXT("resolved identity is flagged internal"),First->bGeneratedArchiveId);
	FGuid ParsedId; TestTrue(TEXT("internal ID stays compatible with PCD GUID contracts"),FGuid::Parse(First->ScenarioUUID,ParsedId)&&ParsedId.IsValid());
	TestEqual(TEXT("original JSON is not rewritten"),First->OriginalJson,Original);
	TestTrue(TEXT("duplicate parses"),FSlabScenarioCodec::Parse(Original,Duplicate,Error,true));
	TestEqual(TEXT("same raw payload has same internal ID"),Duplicate->ScenarioUUID,First->ScenarioUUID);
	auto* GI=NewObject<UGameInstance>(); auto* Catalog=NewObject<USlabScenarioReplaySubsystem>(GI); Catalog->bInitialized=true;
	TestTrue(TEXT("validated snapshot registered without reparse"),Catalog->RegisterValidatedScenario(First));
	TestTrue(TEXT("archive shares the immutable snapshot"),Catalog->GetValidatedScenario(First->ScenarioUUID).Get()==First.Get());
	TestFalse(TEXT("duplicate is not inserted"),Catalog->RegisterValidatedScenario(Duplicate));
	TestTrue(TEXT("live scenario pin applied"),Catalog->SetLiveScenarioPlaybackActive(true,First->ScenarioUUID));
	for(int32 I=0;I<11;++I)
	{
		FSlabScenarioDataPtr Other;
		TestTrue(TEXT("new source scenario parsed"),FSlabScenarioCodec::Parse(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString()),Other,Error));
		Catalog->RegisterValidatedScenario(Other);
	}
	TestEqual(TEXT("ten entries retained"),Catalog->GetScenarios().Num(),10);
	TestTrue(TEXT("active live entry protected from eviction"),Catalog->GetValidatedScenario(First->ScenarioUUID).IsValid());
	FString Returned; Catalog->GetScenarioJson(First->ScenarioUUID,Returned); Returned=TEXT("mutable consumer copy");
	Catalog->GetScenarioJson(First->ScenarioUUID,Returned); TestEqual(TEXT("consumer mutation cannot edit archive"),Returned,Original);
	Catalog->Deinitialize(); TestFalse(TEXT("late callback cannot register after shutdown"),Catalog->RegisterValidatedScenario(First));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabScenarioOutputPolicyTest,"MA0T10.SlabScenario.OutputSelectionAndWildcardIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabScenarioOutputPolicyTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap(); auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	const FString Scenario=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	const FString Observer=Session->BeginScenarioSensorSession(FString(),{},Scenario,FVirtualSlabSensorOutputSelection::ObservationOnly());
	TestFalse(TEXT("observation works without sensors or Coordinator"),Observer.IsEmpty());
	TestTrue(TEXT("observer row accepted"),Session->NotifySlabFrameApplied(Observer,TEXT("SQ83521 047"),0,0));
	TestEqual(TEXT("observation does not claim active transmission"),Session->GetSlabSensorSessionStatus().Message,FString(TEXT("관찰 전용 · 자동 송신 없음")));
	TestTrue(TEXT("observer pause accepted"),Session->SetSlabSensorSessionPaused(Observer,true));
	TestEqual(TEXT("paused observer stays output-free"),Session->GetSlabSensorSessionStatus().Message,FString(TEXT("일시정지 · 관찰 전용 · 자동 송신 없음")));
	TestTrue(TEXT("observer resume accepted"),Session->SetSlabSensorSessionPaused(Observer,false));
	TestEqual(TEXT("resumed observer stays output-free"),Session->GetSlabSensorSessionStatus().Message,FString(TEXT("관찰 전용 · 자동 송신 없음")));
	Session->EndSlabSensorSession(Observer,false); Session->Tick(0);
	TestEqual(TEXT("observer finishes without broker"),Session->GetSlabSensorSessionStatus().State,EVirtualSlabSessionState::Completed);
	auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>(); auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>(); auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();
	Manager->RegisterSensorActor(Lidar); Manager->RegisterSensorActor(Camera);
	auto* Publisher=Manager->StreamPublisherComponent.Get(); Publisher->SetTransportComponent(Manager->SharedTransportComponent);
	Manager->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
	Publisher->StartStream(EVirtualSensorStreamKind::LidarPayload,FString());
	Publisher->StartStream(EVirtualSensorStreamKind::CameraImage,FString());
	FVirtualSlabSensorOutputSelection Outputs;
	const FString Run=Session->BeginScenarioSensorSession(FString(),{},Scenario,Outputs);
	TestFalse(TEXT("PCD default session begins"),Run.IsEmpty());
	Session->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),0,0);
	TestEqual(TEXT("PCD-only running label matches output policy"),Session->GetSlabSensorSessionStatus().Message,FString(TEXT("Slab 연동 PCD 전용 송신 중")));
	Session->SetSlabSensorSessionPaused(Run,true);
	TestEqual(TEXT("PCD-only pause label is not active transmission"),Session->GetSlabSensorSessionStatus().Message,FString(TEXT("일시정지 · PCD 전용 신규 송신 보류")));
	Session->SetSlabSensorSessionPaused(Run,false);
	TestEqual(TEXT("PCD-only resume restores policy label"),Session->GetSlabSensorSessionStatus().Message,FString(TEXT("Slab 연동 PCD 전용 송신 중")));
	const auto LidarContext=Session->CaptureContext(Lidar->GetSensorId(),7100);
	const auto CameraContext=Session->CaptureContext(Camera->GetSensorId(),7101);
	TestEqual(TEXT("camera off does not add pending acquisition"),Session->GetSlabSensorSessionStatus().PendingAcquisitions,1);
	Session->CompleteAcquisition(Camera->GetSensorId(),7101,false);
	TestEqual(TEXT("camera-off failure cannot fail PCD session"),Session->GetSlabSensorSessionStatus().AcquisitionFailures,0);
	TestTrue(TEXT("PCD is admitted"),Session->AllowsStreamFrame(Lidar->GetSensorId(),EVirtualSensorStreamKind::PointCloud,LidarContext));
	TestFalse(TEXT("LiDAR metadata rejected independently of wildcard config"),Session->AllowsStreamFrame(Lidar->GetSensorId(),EVirtualSensorStreamKind::LidarPayload,LidarContext));
	TestFalse(TEXT("Camera rejected independently of wildcard config"),Session->AllowsStreamFrame(Camera->GetSensorId(),EVirtualSensorStreamKind::CameraImage,CameraContext));
	TestFalse(TEXT("Camera readback demand masked"),Session->AllowsStreamDemand(Camera->GetSensorId(),EVirtualSensorStreamKind::CameraImage));
	Publisher->StopStream(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());
	for(auto Kind:{EVirtualSensorKind::Lidar,EVirtualSensorKind::Camera})
	{
		FVirtualSensorFrameEnvelope F; F.SensorKind=Kind; F.SensorId=Kind==EVirtualSensorKind::Lidar?Lidar->GetSensorId():Camera->GetSensorId(); F.FrameId=7200;
		F.SlabContext=Kind==EVirtualSensorKind::Lidar?LidarContext:CameraContext;
		F.JsonPayload=MakeShared<const FString,ESPMode::ThreadSafe>(TEXT("{}")); Publisher->SubmitFrame(F);
	}
	for(const auto& S:Publisher->GetStreamStatuses()) TestEqual(TEXT("wildcards cannot enqueue disabled session kinds"),S.InputFrameCount,static_cast<int64>(0));
	TestFalse(TEXT("same frame plus interpolated time is not a new row"),Session->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),0,0.01));
	Session->CompleteAcquisition(Lidar->GetSensorId(),7100); Session->EndSlabSensorSession(Run,false); Session->Tick(0);
	TestEqual(TEXT("only selected output work participates in drain"),Session->GetSlabSensorSessionStatus().State,EVirtualSlabSessionState::Completed);
	Outputs.bCameraImage=true; Outputs.bLidarTelemetry=true;
	TestTrue(TEXT("explicit requested Camera missing rejected atomically"),Session->BeginScenarioSensorSession(FString(),{Lidar->GetSensorId()},Scenario,Outputs).IsEmpty());
	const FString AllRun=Session->BeginScenarioSensorSession(FString(),{},Scenario,Outputs);
	TestFalse(TEXT("all three selected outputs begin"),AllRun.IsEmpty());
	Session->NotifySlabFrameApplied(AllRun,TEXT("SQ83521 047"),0,0);
	const auto AllContext=Session->CaptureContext(Lidar->GetSensorId(),7300);
	TestTrue(TEXT("LiDAR metadata opt-in admitted"),Session->AllowsStreamFrame(Lidar->GetSensorId(),EVirtualSensorStreamKind::LidarPayload,AllContext));
	TestTrue(TEXT("Camera demand opt-in admitted"),Session->AllowsStreamDemand(Camera->GetSensorId(),EVirtualSensorStreamKind::CameraImage));
	TestFalse(TEXT("previous run context rejected"),Session->AllowsStreamFrame(Lidar->GetSensorId(),EVirtualSensorStreamKind::PointCloud,LidarContext));
	Session->CompleteAcquisition(Lidar->GetSensorId(),7300); Session->EndSlabSensorSession(AllRun,false); Session->Tick(0);
	Camera->StopSensor(); Lidar->StopSensor(); Manager->Destroy(); Camera->Destroy(); Lidar->Destroy(); return true;
}
#endif
