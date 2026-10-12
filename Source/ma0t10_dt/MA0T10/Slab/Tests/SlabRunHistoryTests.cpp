#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabRunHistoryTest,"MA0T10.SlabReliability.RunHistoryAndSaveRetry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabRunHistoryTest::RunTest(const FString&)
{
 auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* Results=W->GetSubsystem<USlabRunResultsSubsystem>();
 const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/SlabRepeatRunStability/history-fixture")/FGuid::NewGuid().ToString());
 IFileManager::Get().MakeDirectory(*Dir,true);const auto Blocked=Dir/TEXT("blocked");FFileHelper::SaveStringToFile(TEXT("fixture"),*Blocked);
 Results->ReportDirectoryOverrideForTests=Blocked;
 const FString Scenario=FGuid::NewGuid().ToString(),Run=FGuid::NewGuid().ToString();
 Results->BeginRun(Run,Scenario,ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());
 Results->UpdateProgress(Run,TEXT("SQ83521 047"),599,30);
 FVirtualSlabSessionStatus Status;Status.RunId=Run;Status.State=EVirtualSlabSessionState::Completed;Results->Finalize(Status);
 for(auto& Write:Results->Writes)Write.Wait();Results->Tick(0);
 FSlabRunDeliverySummary Before;Results->GetRunResult(Run,Before);TestNotEqual(TEXT("real save failure visible"),Before.SaveState,FString(TEXT("저장 완료")));
 auto* Panel=NewObject<UVirtualSensorCaptureExportPanelWidget>(W);W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->RegisterOwnedPanel(ESensorToolPanelRole::Data,Panel);
 Panel->OpenOwnedTab(ESensorOwnedDataTab::Results);auto Content=Panel->BuildRunResultsContent();Panel->RefreshRunResults();
 TestTrue(TEXT("select stored in-memory result"),Panel->SelectScenarioRunResult(Run));Panel->RefreshRunResults();
 TestTrue(TEXT("UUID details shown"),Panel->OwnedRunResultsText.Contains(Run)&&Panel->OwnedRunResultsText.Contains(Scenario));
 TestTrue(TEXT("snapshot output distinction shown"),Panel->OwnedRunResultsText.Contains(TEXT("적용 출력")));
 Panel->OpenOwnedTab(ESensorOwnedDataTab::Diagnostics);Panel->bOwnedRunDiagnosticsExpanded=true;Panel->RefreshRunResults();
 TestTrue(TEXT("terminal snapshot and receipt detail retained in diagnostics"),Panel->RunResultsText.Contains(TEXT("snapshot"))&&Panel->RunResultsText.Contains(TEXT("receipt")));
 Panel->OpenOwnedTab(ESensorOwnedDataTab::Results);Panel->RefreshRunResults();
 TestFalse(TEXT("foreign UUID selection rejected"),Panel->SelectScenarioRunResult(FGuid::NewGuid().ToString()));
 FString Reason;TestTrue(TEXT("finalized failed report eligible"),Results->CanRetryRunReportSave(Run,Reason));
 Results->ReportDirectoryOverrideForTests=Dir/TEXT("retry");
 TestTrue(TEXT("save retry accepted"),Panel->RetrySelectedScenarioRunReport(Reason));TestFalse(TEXT("duplicate retry blocked"),Panel->RetrySelectedScenarioRunReport(Reason));
 for(auto& Write:Results->Writes)Write.Wait();Results->Tick(0);
 FSlabRunDeliverySummary After;Panel->GetSelectedScenarioRunResult(After);TestEqual(TEXT("actual retry creates report"),After.SaveState,FString(TEXT("저장 완료")));TestTrue(TEXT("actual file exists"),IFileManager::Get().FileExists(*After.ReportPath));
 TestEqual(TEXT("save does not change run identity"),After.RunUUID,Before.RunUUID);TestEqual(TEXT("save does not change receipt count"),After.Receipts,Before.Receipts);TestEqual(TEXT("save does not change outcome"),After.Outcome,Before.Outcome);
 Results->BeginRun(FGuid::NewGuid().ToString(),Scenario,ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());Panel->RefreshRunResults();TestEqual(TEXT("new execution preserves selected result"),Panel->GetSelectedScenarioRunId(),Run);
 Panel->SetVisibility(ESlateVisibility::Hidden);const auto Revision=Panel->CachedRunResultsRevision;
 Results->BeginRun(FGuid::NewGuid().ToString(),Scenario,ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());Panel->RefreshNativeText();TestEqual(TEXT("hidden panel skips strings"),Panel->CachedRunResultsRevision,Revision);
 Panel->SetVisibility(ESlateVisibility::Visible);Panel->RefreshNativeText();TestEqual(TEXT("show preserves selection"),Panel->GetSelectedScenarioRunId(),Run);
 for(int I=0;I<21;++I)Results->BeginRun(FGuid::NewGuid().ToString(),Scenario,ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());Panel->RefreshRunResults();
 TestNotEqual(TEXT("evicted selection falls back to latest"),Panel->GetSelectedScenarioRunId(),Run);TestFalse(TEXT("eviction notice visible"),Panel->RunSelectionNotice.IsEmpty());TestEqual(TEXT("selector bounded"),Panel->RunResultOptions.Num(),20);
 const auto Fallback=FGuid::NewGuid().ToString();Results->BeginRun(Fallback,Scenario,ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection());
 Status.RunId=Fallback;Status.bObservationOnly=true;Status.Outputs=FVirtualSlabSensorOutputSelection::ObservationOnly();Results->Finalize(Status);Results->GetRunResult(Fallback,After);
 TestEqual(TEXT("allowed observation fallback has observation outcome"),After.Outcome,ESlabRunDeliveryOutcome::ObservationCompleted);TestTrue(TEXT("requested but unsent output remains explicit"),After.Outputs.bPointCloud&&!After.AppliedOutputs.bPointCloud&&!After.Warning.IsEmpty());
 W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->UnregisterPanel(Panel);return true;
}
#endif
