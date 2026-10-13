#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/GameInstance.h"
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabScenarioDeletionTest,"MA0T10.SlabScenario.ArchiveDeletionLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabScenarioDeletionTest::RunTest(const FString&)
{
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* GI=NewObject<UGameInstance>(); auto* Catalog=NewObject<USlabScenarioReplaySubsystem>(GI);
	Catalog->bInitialized=true; Catalog->PlaybackWorld=World;
	auto Parse=[this](const FString& Id){ FSlabScenarioDataPtr Data; FString Error; TestTrue(TEXT("fixture parsed"),FSlabScenarioCodec::Parse(FSlabScenarioCodec::MakeSyntheticJson(Id),Data,Error)); return Data; };
	const auto First=Parse(FGuid::NewGuid().ToString()),Other=Parse(FGuid::NewGuid().ToString());
	if(!First||!Other) return false;
	Catalog->RegisterValidatedScenario(First); Catalog->RegisterValidatedScenario(Other);
	FString Reason;
	TestTrue(TEXT("inactive item can be removed"),Catalog->CanDeleteScenario(First->ScenarioUUID.ToUpper(),Reason));
	for(const auto State:{ESlabScenarioReplayState::Starting,ESlabScenarioReplayState::Playing,ESlabScenarioReplayState::Draining})
	{
		Catalog->Status.State=State; Catalog->Status.ScenarioUUID=First->ScenarioUUID;
		TestFalse(TEXT("replay lifecycle protects its own item"),Catalog->DeleteScenario(First->ScenarioUUID,Reason));
		TestFalse(TEXT("blocked deletion explains reason"),Reason.IsEmpty());
		TestTrue(TEXT("unrelated item remains removable during replay"),Catalog->CanDeleteScenario(Other->ScenarioUUID,Reason));
	}
	Catalog->Status.State=ESlabScenarioReplayState::Idle;
	Catalog->SetLiveScenarioPlaybackActive(true,First->ScenarioUUID);
	TestFalse(TEXT("active live item protected"),Catalog->CanDeleteScenario(First->ScenarioUUID,Reason));
	TestTrue(TEXT("other live archive item is not globally locked"),Catalog->CanDeleteScenario(Other->ScenarioUUID,Reason));
	Catalog->SetLivePlaybackActive(true);
	TestFalse(TEXT("legacy live player without known identity protects conservatively"),Catalog->CanDeleteScenario(Other->ScenarioUUID,Reason));
	Catalog->SetLivePlaybackActive(false);
	auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	const FString Run=Session->BeginScenarioSensorSession(FString(),{},First->ScenarioUUID,FVirtualSlabSensorOutputSelection::ObservationOnly());
	TestFalse(TEXT("sensor ready state protects archive"),Catalog->CanDeleteScenario(First->ScenarioUUID,Reason));
	Session->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),0,0);
	Session->SetSlabSensorSessionPaused(Run,true);
	TestFalse(TEXT("paused sensor session protects archive"),Catalog->CanDeleteScenario(First->ScenarioUUID,Reason));
	Session->EndSlabSensorSession(Run,false);
	Catalog->SetLiveScenarioPlaybackActive(false,FString()); Catalog->Status.State=ESlabScenarioReplayState::Failed;
	TestFalse(TEXT("live flag released and failed replay cannot bypass sensor drain pin"),Catalog->DeleteScenario(First->ScenarioUUID,Reason));
	for(int32 I=0;I<11;++I) Catalog->RegisterValidatedScenario(Parse(FGuid::NewGuid().ToString()));
	TestEqual(TEXT("catalog remains bounded while draining"),Catalog->GetScenarios().Num(),10);
	TestTrue(TEXT("automatic eviction also protects the draining item"),Catalog->GetValidatedScenario(First->ScenarioUUID).IsValid());
	Session->Tick(0);
	TestTrue(TEXT("completed sensor session releases deletion protection"),Catalog->CanDeleteScenario(First->ScenarioUUID,Reason));
	const FString Original=First->OriginalJson;
	TestTrue(TEXT("completed archive item removed"),Catalog->DeleteScenario(First->ScenarioUUID,Reason));
	TestFalse(TEXT("catalog lookup no longer finds deleted item"),Catalog->GetValidatedScenario(First->ScenarioUUID).IsValid());
	TestEqual(TEXT("actor/chart immutable snapshot remains valid and unchanged"),First->OriginalJson,Original);
	TestFalse(TEXT("missing item deletion is explicit failure"),Catalog->DeleteScenario(First->ScenarioUUID,Reason));
	TestTrue(TEXT("same UUID received later is a new archive entry"),Catalog->RegisterValidatedScenario(First));

	// Model the catalog's worker boundary without launching an async task: the serial belongs to admission, not completion.
	const uint64 InFlight=++Catalog->NextRegistrationSerial;
	Catalog->bParsing=true; Catalog->ActiveRegistrationSerial=InFlight;
	const uint64 Queued=++Catalog->NextRegistrationSerial;
	Catalog->PendingJson.Add({Original,true,Queued});
	TestTrue(TEXT("deletion records cutoff for admitted pending jobs"),Catalog->DeleteScenario(First->ScenarioUUID,Reason));
	TestFalse(TEXT("pre-delete in-flight result cannot resurrect item"),Catalog->RegisterValidatedScenarioAtSerial(First,InFlight));
	TestFalse(TEXT("pre-delete queued result cannot resurrect item"),Catalog->RegisterValidatedScenarioAtSerial(First,Queued));
	TestTrue(TEXT("post-delete newly received item allowed while older parse is pending"),Catalog->RegisterValidatedScenario(First));
	TestFalse(TEXT("late old result cannot overwrite new entry"),Catalog->RegisterValidatedScenarioAtSerial(First,InFlight));
	TestTrue(TEXT("new entry keeps immutable original"),Catalog->GetValidatedScenario(First->ScenarioUUID).Get()==First.Get());
	Catalog->PendingJson.Reset(); Catalog->bParsing=false; Catalog->ActiveRegistrationSerial=0; Catalog->PruneDeletedRegistrationCutoffs();
	TestEqual(TEXT("finished requests release deletion tombstones"),Catalog->DeletedRegistrationCutoffs.Num(),0);
	Catalog->Deinitialize(); TestFalse(TEXT("closed catalog cannot delete"),Catalog->DeleteScenario(First->ScenarioUUID,Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabScenarioDeletionUiTest,"MA0T10.SlabScenario.ArchiveDeletionSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabScenarioDeletionUiTest::RunTest(const FString&)
{
	const TArray<FString> Before{TEXT("a"),TEXT("b"),TEXT("c")};
	TestEqual(TEXT("deleting other item keeps selected UUID"),USlabScenarioReplayPanelWidget::ResolveStableSelection(Before,{TEXT("b"),TEXT("c")},TEXT("c")),FString(TEXT("c")));
	TestEqual(TEXT("deleting selected item chooses newest remaining row"),USlabScenarioReplayPanelWidget::ResolveStableSelection(Before,{TEXT("a"),TEXT("c")},TEXT("b")),FString(TEXT("a")));
	TestEqual(TEXT("deleting oldest selected item also chooses newest row"),USlabScenarioReplayPanelWidget::ResolveStableSelection(Before,{TEXT("a"),TEXT("b")},TEXT("c")),FString(TEXT("a")));
	TestTrue(TEXT("empty list clears selection"),USlabScenarioReplayPanelWidget::ResolveStableSelection(Before,{},TEXT("a")).IsEmpty());
	TestEqual(TEXT("new latest item does not steal existing selection"),USlabScenarioReplayPanelWidget::ResolveStableSelection(Before,{TEXT("new"),TEXT("a"),TEXT("b"),TEXT("c")},TEXT("b")),FString(TEXT("b")));
	for(const FName Function:{FName(TEXT("SelectScenario")),FName(TEXT("ReplaySelected")),FName(TEXT("SetReplayOutputs")),FName(TEXT("CopyScenarioUUID")),FName(TEXT("RequestScenarioDeletion")),FName(TEXT("ConfirmScenarioDeletion")),FName(TEXT("CancelScenarioDeletion"))})
		TestNotNull(TEXT("existing and additive Blueprint APIs available"),USlabScenarioReplayPanelWidget::StaticClass()->FindFunctionByName(Function));
	return true;
}
#endif
