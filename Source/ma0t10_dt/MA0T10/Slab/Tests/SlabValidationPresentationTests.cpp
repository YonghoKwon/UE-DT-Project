#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Json.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabValidationDisabledTest,"MA0T10.SlabReliability.ValidationDisabledPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabValidationDisabledTest::RunTest(const FString&)
{
    FSlabRunDeliverySummary R;R.bHasStreamDetails=true;R.StreamDetails.AddDefaulted();
    const FString Off=TEXT("자체 검증 꺼짐(외부 수신기 검증 사용)");
    TestTrue(TEXT("summary clearly declares disabled observation"),USlabRunResultsSubsystem::Describe(R).Contains(Off));
    TestTrue(TEXT("details declare disabled observation"),USlabRunResultsSubsystem::DescribeStreamDetails(R).Contains(Off));
    TestFalse(TEXT("disabled zero is not presented as success count"),USlabRunResultsSubsystem::Describe(R).Contains(TEXT("검증(기록 시점) 0")));
    TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(USlabRunResultsSubsystem::SerializeSummary(R)),Json);
    TestTrue(TEXT("disabled report count is null, never a numeric zero"),Json.IsValid()&&Json->HasTypedField<EJson::Null>(TEXT("consumerValidated")));
    TestTrue(TEXT("disabled validation has no fabricated snapshot time"),Json.IsValid()&&Json->HasTypedField<EJson::Null>(TEXT("consumerSnapshotUtc")));
    FSlabRunComparison C;C.A=R;C.B=R;C.B.bConsumerValidationEnabled=true;C.B.ConsumerValidated=12;
    const FString Comparison=DescribeRunComparison(C);TestTrue(TEXT("mixed comparison shows disabled status"),Comparison.Contains(Off));
    TestFalse(TEXT("mixed comparison does not subtract unlike observation states"),Comparison.Contains(TEXT("자체 검증 A/B")));
    R.bConsumerValidationEnabled=true;R.ConsumerValidated=12;R.ConsumerInvalid=1;
    TestTrue(TEXT("explicit observation retains real counts"),USlabRunResultsSubsystem::Describe(R).Contains(TEXT("12 · 검증 오류 1")));
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(USlabRunResultsSubsystem::SerializeSummary(R)),Json);
    TestEqual(TEXT("enabled report retains original numeric count"),Json->GetNumberField(TEXT("consumerValidated")),12.);
    return true;
}
#endif
