#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingPresentation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMetricPresentation.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP1AvailableArea,"MA0T10.UXRP1.CoilToolsAvailableArea",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP1AvailableArea::RunTest(const FString&)
{
    for(const FVector2D Viewport:{FVector2D(1920,1080),FVector2D(1920,1080)/(.6666667),FVector2D(1280,720)})
    {
        const auto Original=CoilLoadingPresentation::AvailableArea(Viewport,FVector2D::ZeroVector);
        TestEqual(TEXT("Hidden tools restore the complete viewport"),Original.GetSize(),Viewport);
        for(const double ToolsScale:{1.,1.5})for(const double OccupiedWidth:{76.,360.,520.})
        {
            const FVector2D Inset(FMath::Min(OccupiedWidth*ToolsScale,Viewport.X*.48)+6,56*ToolsScale);
            const auto Area=CoilLoadingPresentation::AvailableArea(Viewport,Inset);
            TestTrue(TEXT("Coil controls start beyond the visible rail/sidebar"),Area.Min.X>=Inset.X);
            TestTrue(TEXT("Footer stays above the status strip"),Area.Max.Y<=Viewport.Y-Inset.Y);
            TestTrue(TEXT("Scene space stays positive"),Area.GetSize().GetMin()>0);
        }
        const auto Rehosted=CoilLoadingPresentation::AvailableArea(Viewport,FVector2D(460,90));
        TestEqual(TEXT("Arranged rehosting offset is retained"),Rehosted.Min.X,460.);
    }
    const auto Clamped=CoilLoadingPresentation::AvailableArea(FVector2D(1280,720),FVector2D(-1,900));
    TestEqual(TEXT("Negative inset does not move controls offscreen"),Clamped.Min.X,0.);
    TestEqual(TEXT("Oversized bottom reservation stays bounded"),Clamped.GetSize().Y,1.);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP1CoilUnknown,"MA0T10.UXRP1.CoilUnknownReasonText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP1CoilUnknown::RunTest(const FString&)
{
    TestEqual(TEXT("No unknown reasons is explicit"),CoilLoadingPresentation::IssueSummary(0,0,0),FString(TEXT("위험 0 · 주의 0 · 미확인 0개 사유")));
    TestEqual(TEXT("Zero known risk never hides unknown reasons"),CoilLoadingPresentation::IssueSummary(0,0,8),FString(TEXT("위험 0 · 주의 0 · 미확인 8개 사유")));
    TestEqual(TEXT("Independent known and unknown counts"),CoilLoadingPresentation::IssueSummary(2,3,10),FString(TEXT("위험 2 · 주의 3 · 미확인 10개 사유")));
    TestEqual(TEXT("Collapsed action counts reasons"),CoilLoadingPresentation::AdditionalChecks(8,false),FString(TEXT("추가 확인 8개 사유 · 펼치기")));
    TestEqual(TEXT("Expanded action retains the count"),CoilLoadingPresentation::AdditionalChecks(8,true),FString(TEXT("추가 확인 8개 사유 · 접기")));
    TestTrue(TEXT("Count cannot mean coils or documents"),FString(CoilLoadingPresentation::UnknownExplanation()).Contains(TEXT("코일 수·문서 수가 아닙니다")));
    TestTrue(TEXT("Specific input location accompanies the original reason"),FString(CoilLoadingPresentation::InputLocation(TEXT("PatchUnknown"))).Contains(TEXT("승인 유효 면적")));
    TestTrue(TEXT("Unknown future reasons still have a review location"),FString(CoilLoadingPresentation::InputLocation(TEXT("FutureReason"))).Contains(TEXT("승인 자료 대조")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP1HatchSources,"MA0T10.UXRP1.HatchMetricSourceText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP1HatchSources::RunTest(const FString&)
{
    for(int32 I=0;I<6;++I)
    {
        const FString Caption=ShipHatchMetricPresentation::Caption(I),Source=ShipHatchMetricPresentation::Source(I);
        TestTrue(TEXT("Both full-hold metrics are marked simulated; all four analysis metrics are measured"),Caption.Contains(I<2?TEXT("\n모의값"):TEXT("\n측정값")));
        TestTrue(TEXT("Full-hold simulation is not presented as sensor measurement"),I>=2||Source.Contains(TEXT("센서 측정으로 산출한 값이 아닙니다")));
        TestTrue(TEXT("Measured cards explain retained values while waiting"),I<2||Source.Contains(TEXT("마지막 값")));
    }
    return true;
}
#endif
