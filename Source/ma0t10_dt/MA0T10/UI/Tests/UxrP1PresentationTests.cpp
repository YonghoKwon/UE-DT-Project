#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingPresentation.h"
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
#endif
