#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchSubsystem.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/TrainBatchScenarioTC.h"
#include "ma0t10_dt/MA0T10/UI/TrainBatchWidget.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchDefaultOffTest,"MA0T10.TrainBatch.DefaultOff",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchDefaultOffTest::RunTest(const FString&)
{
    auto* S=NewObject<UTrainBatchSubsystem>();FString Error;
    TestFalse(TEXT("default off"),S->IsEnabled());TestFalse(TEXT("no worker/timer/subscription by default"),S->HasBackgroundWorkForTests());
    TestTrue(TEXT("empty session"),S->Jobs().IsEmpty());TestFalse(TEXT("no implicit activation on input"),S->SubmitEnvelope(TEXT("{}"),Error));TestFalse(TEXT("no implicit activation on import"),S->ImportManifest(TEXT("C:/not-opened/job.json"),Error));
    TestFalse(TEXT("start cannot activate feature"),S->StartJob(TEXT("not-admitted"),Error));S->CancelJob();S->StopPreview();
    TestFalse(TEXT("rejected input creates no background work"),S->HasBackgroundWorkForTests());
    TestTrue(TEXT("panel uses DTCore"),UTrainBatchWidget::StaticClass()->IsChildOf(UDxWidget::StaticClass()));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchTcIsolationTest,"MA0T10.TrainBatch.TcIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchTcIsolationTest::RunTest(const FString&)
{
    auto* TC=NewObject<UTrainBatchScenarioTC>();TestEqual(TEXT("new TC distinct from ordinary input"),TC->TransactionCode,FString(TrainBatchContract::MessageId));
    auto P=UTrainBatchScenarioTC::ParsePayload(TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[]}"));TestFalse(TEXT("ordinary input rejected, never demo triggered"),P->Error.IsEmpty());return true;
}
#endif
