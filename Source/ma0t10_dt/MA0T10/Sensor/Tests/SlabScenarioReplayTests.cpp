#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Json.h"
#include "Engine/GameInstance.h"
#include "VirtualSlabSensorTestDriver.h"
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabReplayCatalogTest,"MA0T10.ScenarioReplay.Catalog",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabReplayCatalogTest::RunTest(const FString&)
{
	auto* GameInstance=NewObject<UGameInstance>();
	auto* Catalog=NewObject<USlabScenarioReplaySubsystem>(GameInstance);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(AVirtualSlabSensorTestDriver::MakeSyntheticBulkJson()),Root);
	FSlabScenarioSummary Summary; FString Error,Json,FirstId;
	TestFalse(TEXT("missing UUID rejected"),USlabScenarioReplaySubsystem::ValidateScenario(AVirtualSlabSensorTestDriver::MakeSyntheticBulkJson(),Summary,Error));
	for(int I=0;I<11;++I)
	{
		const FString Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		Root->GetObjectField(TEXT("_meta"))->SetStringField(TEXT("UUID"),Id);
		Json.Reset(); FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
		TestTrue(TEXT("600 row fixture valid"),USlabScenarioReplaySubsystem::ValidateScenario(Json,Summary,Error));
		TestEqual(TEXT("600 states"),Summary.RowCount,600); TestEqual(TEXT("last sample is not completion time"),Summary.LastElapsedSec,29.95);
		Catalog->StoreValidated(Summary,Json);
		if(I==0) { FirstId=Id; Catalog->Status.ScenarioUUID=Id; Catalog->Status.State=ESlabScenarioReplayState::Playing; }
	}
	TestEqual(TEXT("bounded ten records"),Catalog->GetScenarios().Num(),10);
	FString Copy; TestTrue(TEXT("active oldest entry protected"),Catalog->GetScenarioJson(FirstId,Copy));
	TestTrue(TEXT("latest raw JSON available"),Catalog->GetScenarioJson(Summary.UUID,Copy)); TestEqual(TEXT("exact original JSON"),Copy,Json);
	Copy=TEXT("modified by colleague"); Catalog->GetScenarioJson(Summary.UUID,Copy); TestEqual(TEXT("consumer copy cannot mutate stored JSON"),Copy,Json);
	const auto Before=Catalog->GetScenarios(); Catalog->StoreValidated(Summary,TEXT("ignored replacement"));
	TestEqual(TEXT("duplicate does not reorder"),Catalog->GetScenarios()[0].UUID,Before[0].UUID);
	Catalog->GetScenarioJson(Summary.UUID,Copy); TestEqual(TEXT("duplicate cannot overwrite"),Copy,Json);
	TestFalse(TEXT("live player blocked during replay"),Catalog->SetLivePlaybackActive(true));
	Catalog->Status.State=ESlabScenarioReplayState::Idle;
	Catalog->bInitialized=true; Catalog->bParsing=true;
	TestTrue(TEXT("pending one"),Catalog->RegisterScenarioJson(Json)); TestTrue(TEXT("pending two"),Catalog->RegisterScenarioJson(Json));
	TestFalse(TEXT("pending overflow explicit"),Catalog->RegisterScenarioJson(Json));
	TestFalse(TEXT("8MiB cap"),Catalog->RegisterScenarioJson(FString::ChrN(8*1024*1024+1,TEXT('a'))));
	Root->GetArrayField(TEXT("DATA_MAP"))[1]->AsObject()->SetNumberField(TEXT("frame_no"),0);
	Json.Reset(); FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
	TestFalse(TEXT("reversed/duplicate frame rejected"),USlabScenarioReplaySubsystem::ValidateScenario(Json,Summary,Error));
	Catalog->Deinitialize(); TestEqual(TEXT("shutdown releases memory catalog"),Catalog->GetScenarios().Num(),0);
	TestEqual(TEXT("shutdown clears pending"),Catalog->GetPendingRegistrationCount(),0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabReplayObservationTest,"MA0T10.ScenarioReplay.ObservationGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabReplayObservationTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>(); auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();
	Manager->RegisterSensorActor(Lidar); auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	const FString Scenario=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	const FString Run=Session->BeginReplaySensorSession(FString(),{Lidar->GetSensorId()},Scenario,false);
	TestFalse(TEXT("observation needs no broker"),Run.IsEmpty());
	TestTrue(TEXT("frame applied"),Session->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),0,0));
	const auto Context=Session->CaptureContext(Lidar->GetSensorId(),99);
	TestEqual(TEXT("scenario is separate from run"),Context.ScenarioUUID,Scenario);
	TestFalse(TEXT("all automatic output blocked"),Session->AllowsFrame(Lidar->GetSensorId(),Context));
	TestEqual(TEXT("observation does not accumulate pending output"),Session->GetSlabSensorSessionStatus().PendingAcquisitions,0);
	Session->EndSlabSensorSession(Run,false); Session->Tick(0);
	TestTrue(TEXT("observation completes"),Session->GetSlabSensorSessionStatus().State==EVirtualSlabSessionState::Completed);
	for(const auto& S:Manager->StreamPublisherComponent->GetStreamStatuses()) TestFalse(TEXT("no streams auto resume"),S.bEnabled);
	Manager->Destroy(); Lidar->Destroy(); return true;
}
#endif
