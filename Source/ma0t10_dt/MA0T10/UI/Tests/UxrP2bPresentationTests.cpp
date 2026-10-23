#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingPresentation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchLabelPresentation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMetricPresentation.h"
#include "ma0t10_dt/MA0T10/UI/SensorOwnedPresentationStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Misc/AutomationTest.h"

struct FUxrP2bCoilProbe
{
    static UVerticalBox* Prepare(UCoilLoadingMonitor& M)
    {
        M.WidgetTree=NewObject<UWidgetTree>(&M);
        auto* Root=M.WidgetTree->ConstructWidget<UVerticalBox>();M.WidgetTree->RootWidget=Root;return Root;
    }
    static UTextBlock* Row(UCoilLoadingMonitor& M,const FString& Text,FLinearColor Color)
    { return M.MakeIssueText(Text,Color); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2bCoilFont,"MA0T10.UXRP2b.CoilRegeneratedIssueFont",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2bCoilFont::RunTest(const FString&)
{
    auto* Monitor=NewObject<UCoilLoadingMonitor>();auto* Root=FUxrP2bCoilProbe::Prepare(*Monitor);
    auto* Foreign=NewObject<UTextBlock>();const auto ForeignFont=Foreign->GetFont();
    const FString Warning=TEXT("위험 · 하중 · 3개 코일 · 최악 1.234 / 1.000"),Unknown=CoilLoadingPresentation::UnknownExplanation();
    Monitor->DangerCount=3;Monitor->CautionCount=2;Monitor->DataNeededCount=8;
    for(int32 Percent:{100,150,150,100,150,100})
    {
        Monitor->SetFontPercent(Percent);
        for(int32 Revision=0;Revision<3;++Revision)
        {
            Root->ClearChildren();
            auto* Known=FUxrP2bCoilProbe::Row(*Monitor,Warning,FSensorOwnedPresentationStyle::Danger());Root->AddChildToVerticalBox(Known);
            auto* Missing=FUxrP2bCoilProbe::Row(*Monitor,Unknown,FSensorOwnedPresentationStyle::Muted());Root->AddChildToVerticalBox(Missing);
            const float Expected=Percent==150?18.f:12.f;
            TestEqual(TEXT("Each newly generated warning uses the current owned scale"),Known->GetFont().Size,Expected);
            TestEqual(TEXT("Unknown explanation and warning share the same scale after regeneration"),Missing->GetFont().Size,Expected);
            TestEqual(TEXT("Warning values and units survive text construction"),Known->GetText().ToString(),Warning);
            TestEqual(TEXT("Unknown meaning survives text construction"),Missing->GetText().ToString(),Unknown);
            TestTrue(TEXT("Danger and unknown keep their distinct colors"),Known->GetColorAndOpacity().GetSpecifiedColor()==FSensorOwnedPresentationStyle::Danger()&&Missing->GetColorAndOpacity().GetSpecifiedColor()==FSensorOwnedPresentationStyle::Muted());
        }
    }
    TestEqual(TEXT("Font changes preserve all issue counters"),Monitor->DangerCount+Monitor->CautionCount+Monitor->DataNeededCount,13);
    TestEqual(TEXT("An unregistered text is unaffected"),Foreign->GetFont().Size,ForeignFont.Size);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2bRoles,"MA0T10.UXRP2b.HatchRolesAndTfIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2bRoles::RunTest(const FString&)
{
    const TCHAR* Ids[]={TEXT("Mesh_origin"),TEXT("Crane"),TEXT("Trolley"),TEXT("Cabin"),TEXT("os_lidar_boom"),TEXT("os_lidar_cabin_01"),TEXT("os_lidar_cabin_02"),TEXT("Grab")};
    const TCHAR* Labels[]={TEXT("모델 원점"),TEXT("하역기"),TEXT("대차"),TEXT("운전실"),TEXT("붐"),TEXT("운전실 좌"),TEXT("운전실 우"),TEXT("그랩")};
    for(int32 I=0;I<8;++I)
    {
        TestEqual(TEXT("The complete original TF identifier is retained"),FString(ShipHatchLabelPresentation::FrameId(I)),FString(Ids[I]));
        TestEqual(TEXT("The primary label identifies the operator role"),ShipHatchLabelPresentation::FrameLabel(I),FString(Labels[I]));
        if(I>=4&&I<7)
        {
            const int32 Sensor=I-4;const auto Hint=ShipHatchLabelPresentation::SensorTooltip(Sensor);
            TestEqual(TEXT("Badge, model selector and site TF share the same physical role"),FString(ShipHatchLabelPresentation::SensorRole(Sensor)),FString(Labels[I]));
            TestTrue(TEXT("Tooltip preserves raw identity and fixed installation side"),Hint.Contains(Ids[I])&&Hint.Contains(Labels[I])&&Hint.Contains(TEXT("화면의 좌우 순서가 아닙니다")));
        }
    }
    for(int32 I:{-1,8})TestTrue(TEXT("Invalid frame index has no invented identity"),ShipHatchLabelPresentation::FrameLabel(I).IsEmpty()&&FString(ShipHatchLabelPresentation::FrameId(I)).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2bRates,"MA0T10.UXRP2b.HatchRoleAcquisitionRates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2bRates::RunTest(const FString&)
{
    const double Goals[]={20,10,20},Actual[]={19.94,0,17.86};
    const auto Text=ShipHatchLabelPresentation::AcquisitionRates(Goals,Actual);
    TestTrue(TEXT("Goal and actual are labelled as acquisition Hz"),Text.StartsWith(TEXT("● 획득 Hz · 목표/실제\n")));
    TestTrue(TEXT("Different goals stay paired with each physical sensor, including zero Hz"),Text.Contains(TEXT("붐 20/19.9"))&&Text.Contains(TEXT("운전실 좌 10/0.0"))&&Text.Contains(TEXT("운전실 우 20/17.9")));
    TestFalse(TEXT("Formatting does not claim transmission, receipt or acknowledgement"),Text.Contains(TEXT("전송"))||Text.Contains(TEXT("receipt"))||Text.Contains(TEXT("ACK")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2bLabels,"MA0T10.UXRP2b.HatchClusteredLabelLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2bLabels::RunTest(const FString&)
{
    using namespace ShipHatchLabelPresentation;
    for(float Scale:{1.f,1.5f})for(double Width:{480.,720.,1165.,1888.})for(double Height:{150.,300.,600.})
    {
        const FSlateRect Bounds(16,40,16+Width,40+Height);
        for(FVector2D Anchor:{FVector2D(20,44),FVector2D(16+Width*.5,40+Height*.5),FVector2D(12+Width,36+Height)})
        {
            TArray<FSlateRect,TInlineAllocator<8>> Occupied;
            for(int32 I=0;I<8;++I)
            {
                FSlateRect Rect;const FVector2D Extent((I<3?100.:65.)*Scale,24*Scale);
                if(!PlaceLabel(Bounds,Anchor,Extent,Scale,Occupied.GetData(),Occupied.Num(),Rect))continue;
                TestTrue(TEXT("Near-edge and clustered labels stay within the assigned 3D view"),Rect.Left>=Bounds.Left&&Rect.Top>=Bounds.Top&&Rect.Right<=Bounds.Right&&Rect.Bottom<=Bounds.Bottom);
                for(const auto& Other:Occupied)TestFalse(TEXT("Selected-first placement never covers an earlier role"),Intersects(Rect,Other));
                Occupied.Add(Rect);
            }
            TestTrue(TEXT("The first (highest priority) role always has space in this fixture"),Occupied.Num()>0);
        }
    }
    FSlateRect Unused;
    TestFalse(TEXT("An offscreen origin does not acquire a misleading edge label"),PlaceLabel(FSlateRect(0,0,100,100),FVector2D(-1,50),FVector2D(40,20),1,nullptr,0,Unused));
    TestFalse(TEXT("An oversized caption is omitted rather than clipped or overlapped"),PlaceLabel(FSlateRect(0,0,40,20),FVector2D(20,10),FVector2D(100,24),1.5,nullptr,0,Unused));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2bCoverage,"MA0T10.UXRP2b.HatchUnobservedExplanation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2bCoverage::RunTest(const FString&)
{
    TestEqual(TEXT("The card names the unobserved grid ratio"),FString(ShipHatchMetricPresentation::Caption(5)),FString(TEXT("미관측 격자 비율\n측정값")));
    const FString Source=ShipHatchMetricPresentation::Source(5),Hint=ShipHatchMetricPresentation::CoverageHint();
    for(const TCHAR* Meaning:{TEXT("최근 검출점"),TEXT("장비 고장 판정이 아닙니다"),TEXT("차폐·거리·시야 범위"),TEXT("실제 측정 샘플과 별도"),TEXT("마지막 값")})TestTrue(TEXT("The explanation preserves absence, diagnosis and stale-value limits"),Source.Contains(Meaning));
    TestTrue(TEXT("The adjacent hint names the existing hover action and causes"),Hint.Contains(TEXT("회색 격자에 마우스를 올려 확인"))&&Hint.Contains(TEXT("차폐·거리·시야 범위")));
    for(int32 I=0;I<6;++I)TestTrue(TEXT("P1 simulated/measured source distinction is preserved"),FString(ShipHatchMetricPresentation::Caption(I)).Contains(I<2?TEXT("\n모의값"):TEXT("\n측정값")));
    return true;
}
#endif
