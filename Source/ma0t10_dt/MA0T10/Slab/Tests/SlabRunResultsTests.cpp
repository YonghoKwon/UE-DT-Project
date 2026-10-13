#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabRunLedgerTest,"MA0T10.SlabReliability.RunLedgerAndOwnedReports",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabRunLedgerTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Results=World->GetSubsystem<USlabRunResultsSubsystem>();
    const FString Run=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),Scenario=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    Results->BeginRun(Run,Scenario,ESlabExecutionPolicy::RequireData,FVirtualSlabSensorOutputSelection());
    FVirtualSensorTransportObservation E;E.RunId=Run;E.SensorId=TEXT("LEDGER-LIDAR");E.Kind=EVirtualSensorStreamKind::PointCloud;E.FrameId=100;E.RequestId=TEXT("first");
    Results->ObserveTransport(E);Results->ObserveTransport(E);E.Phase=EVirtualSensorTransportObservationPhase::Submitted;Results->ObserveTransport(E);
    E.RequestId=TEXT("retry-new-id");Results->ObserveTransport(E);
    TestEqual(TEXT("one unconfirmed logical frame"),Results->GetUnconfirmedCount(Run),int64(1));
    E.Phase=EVirtualSensorTransportObservationPhase::Receipt;Results->ObserveTransport(E);Results->ObserveTransport(E);
    E.Phase=EVirtualSensorTransportObservationPhase::Consumed;Results->ObserveTransport(E);
    FSlabRunDeliverySummary R;Results->GetRunResult(Run,R);TestEqual(TEXT("retry does not add accepted frame"),R.Accepted,int64(1));TestEqual(TEXT("retry does not add submission"),R.Submitted,int64(1));TestEqual(TEXT("duplicate receipt ignored"),R.Receipts,int64(1));
    Results->UpdateProgress(Run,TEXT("SQ83521 047"),599,30);
    FVirtualSlabSessionStatus S;S.RunId=Run;S.State=EVirtualSlabSessionState::Completed;Results->Finalize(S);
    Results->GetRunResult(Run,R);TestEqual(TEXT("receipt defines completion"),R.Outcome,ESlabRunDeliveryOutcome::BrokerAccepted);TestEqual(TEXT("last slab frame preserved"),R.LastSlabFrame,int64(599));
    const FString Next=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);Results->BeginRun(Next,Scenario,ESlabExecutionPolicy::RequireData,FVirtualSlabSensorOutputSelection());
    E.Phase=EVirtualSensorTransportObservationPhase::Failed;Results->ObserveTransport(E);
    FSlabRunDeliverySummary New;Results->GetRunResult(Next,New);TestEqual(TEXT("old callback cannot contaminate new run"),New.DeliveryFailures,int64(0));
    E.RunId=Next;E.FrameId=101;E.Phase=EVirtualSensorTransportObservationPhase::Accepted;Results->ObserveTransport(E);
    E.Phase=EVirtualSensorTransportObservationPhase::Failed;E.Message=TEXT("fault fixture");Results->ObserveTransport(E);
    S.RunId=Next;S.State=EVirtualSlabSessionState::Incomplete;S.RequiredDataError=TEXT("fault fixture");Results->Finalize(S);Results->GetRunResult(Next,New);TestEqual(TEXT("delivery failure is partial"),New.Outcome,ESlabRunDeliveryOutcome::Partial);
    for(int32 I=0;I<25;++I)Results->BeginRun(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),Scenario,ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());
    TestEqual(TEXT("memory history bounded"),Results->GetRecentRunResults().Num(),20);
    const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/ScenarioDataReliability/retention")/FGuid::NewGuid().ToString());
    IFileManager::Get().MakeDirectory(*Dir,true);const FString Foreign=Dir/TEXT("slab-run-foreign.json");const FString Text=TEXT("{\"schema\":\"someone-else\",\"data\":\"preserve\"}");FFileHelper::SaveStringToFile(Text,*Foreign);
    for(int32 I=0;I<101;++I){const auto Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);const auto Written=Results->WriteReport(Id,TEXT("{\"schema\":\"ma0t10.slab-run.v1\"}"),Dir,100);TestTrue(TEXT("owned report saved"),Written.Success);}
    TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(Dir/TEXT("slab-run-*.json")),true,false);TestEqual(TEXT("100 owned plus foreign file retained"),Files.Num(),101);
    FString After;FFileHelper::LoadFileToString(After,*Foreign);TestEqual(TEXT("foreign report bytes preserved"),After,Text);
    const auto Invalid=Results->WriteReport(TEXT("../invalid"),TEXT("{}"),Dir,100);TestFalse(TEXT("unsafe ID cannot create report"),Invalid.Success);
    const FString Blocked=Dir/TEXT("blocked-root");FFileHelper::SaveStringToFile(TEXT("owned fixture"),*Blocked);
    const auto Unwritable=Results->WriteReport(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),TEXT("{}"),Blocked,100);
    TestFalse(TEXT("actual file write failure reported"),Unwritable.Success);
    return true;
}
#endif
