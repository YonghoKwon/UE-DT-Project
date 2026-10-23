#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingPresentation.h"
#include "ma0t10_dt/MA0T10/UI/VirtualPcdOptionsPresentation.h"
#include "ma0t10_dt/MA0T10/UI/VirtualPcdBandwidthPresentation.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2CoilUnits,"MA0T10.UXRP2.CoilUnitsAndComparison",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2CoilUnits::RunTest(const FString&)
{
    const auto Summary=CoilLoadingPresentation::MetricSummary(3021.126,12.3456,7.1234,1.8766,-10.89,TEXT("42.5%"),7.093,7.098,41.234,false);
    for(const TCHAR* Value:{TEXT("3021.13 t"),TEXT("12.346 t"),TEXT("무게중심 높이(KG) 7.123 m"),TEXT("복원성 높이(GM) 1.877 m"),TEXT("횡경사 -10.890°"),TEXT("균등분포하중(UDL) 42.5%"),TEXT("흘수 현재 7.093 m / 다음 포함 7.098 m"),TEXT("흘수 1cm당 톤수(TPC) 41.23 t/cm · 자료 필요"),TEXT("코일 구멍축 선수미")})
        TestTrue(TEXT("Each unchanged numeric value retains its own unit and operator label"),Summary.Contains(Value));
    const auto Confirmed=CoilLoadingPresentation::MetricSummary(0,0,7,2,0,TEXT("자료 필요"),6.2,6.2,40,true);
    TestTrue(TEXT("Unknown load area remains unknown even with confirmed TPC"),Confirmed.Contains(TEXT("(UDL) 자료 필요")));
    TestFalse(TEXT("Confirmed TPC does not gain an unknown marker"),Confirmed.Contains(TEXT("t/cm · 자료 필요")));
    const auto Compared=CoilLoadingPresentation::ComparisonSummary(TEXT("정착"),.0123,1.24,17.34,8,true,.0256,.0012);
    TestTrue(TEXT("Comparison precision and measurement rate are retained"),Compared.Contains(TEXT("이동 0.012 m"))&&Compared.Contains(TEXT("회전 1.2°"))&&Compared.Contains(TEXT("17.3 Hz / 20 Hz"))&&Compared.Contains(TEXT("0.026 m / 표면 잔차 0.001 m")));
    const auto Missing=CoilLoadingPresentation::ComparisonSummary(TEXT("대기"),0,0,0,0,false,.9,.8);
    TestTrue(TEXT("Insufficient points/directions stay unavailable"),Missing.Contains(TEXT("비교 불가 (점/방향 부족)")));
    TestFalse(TEXT("Unavailable comparison never exposes a stale deviation"),Missing.Contains(TEXT("0.900 m")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2PcdFields,"MA0T10.UXRP2.PcdProfilesAndRawFieldNames",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2PcdFields::RunTest(const FString&)
{
    const TCHAR* Raw[]={TEXT("x"),TEXT("y"),TEXT("z"),TEXT("intensity"),TEXT("ring"),TEXT("horizontal_index"),TEXT("return_index"),TEXT("return_count"),TEXT("time_offset_ns"),TEXT("validity"),TEXT("confidence")};
    const TCHAR* Korean[]={TEXT("X 좌표"),TEXT("Y 좌표"),TEXT("Z 좌표"),TEXT("반사 강도"),TEXT("채널 번호"),TEXT("수평 샘플 번호"),TEXT("반사 순번"),TEXT("반사 수"),TEXT("프레임 내 시간차(ns)"),TEXT("유효 여부"),TEXT("신뢰도")};
    for(int32 I=0;I<UE_ARRAY_COUNT(Raw);++I)
    {
        TestEqual(TEXT("Canonical wire name stays in the codec"),FString(FVirtualPcdCodec::FieldName(I)),FString(Raw[I]));
        TestEqual(TEXT("Korean field label is an L3 adapter"),FString(VirtualPcdOptionsPresentation::FieldLabel(I)),FString(Korean[I]));
        TestTrue(TEXT("The full raw name remains accessible"),VirtualPcdOptionsPresentation::FieldTooltip(I).Contains(Raw[I]));
    }
    TestEqual(TEXT("Invalid index is bounded"),FString(VirtualPcdOptionsPresentation::FieldLabel(11)),FString());
    TestTrue(TEXT("Coordinate tooltip preserves local units and axes"),VirtualPcdOptionsPresentation::FieldTooltip(0).Contains(TEXT("센서 로컬 m · X 전방 / Y 좌측 / Z 위쪽")));
    const int32 ExpectedMasks[]={2047,7|(1<<3)|(1<<4)|(1<<8),7,7|(1<<5)|(1<<10)};
    const int32 ExpectedBytes[]={33,24,12,18};
    for(int32 I=0;I<4;++I)
    {
        FVirtualPcdSendOptions Options;Options.Profile=EVirtualPcdFieldProfile(I);Options.CustomFieldMask=(1<<5)|(1<<10);
        const auto Before=Options;
        const auto Label=VirtualPcdOptionsPresentation::ProfileLabel(Options.Profile);
        const auto Summary=VirtualPcdOptionsPresentation::FieldSummary(Options);
        const auto Names=VirtualPcdOptionsPresentation::IncludedFields(Options,true);
        TestTrue(TEXT("Display formatting does not mutate stored options"),Options==Before);
        TestEqual(TEXT("Field mask is unchanged"),Options.FieldMask(),ExpectedMasks[I]);
        TestEqual(TEXT("Wire size is unchanged"),FVirtualPcdCodec::BytesPerPoint(Options),ExpectedBytes[I]);
        TestFalse(TEXT("Profile and summary are populated"),Label.IsEmpty()||Summary.IsEmpty());
        TestTrue(TEXT("All profiles retain mandatory XYZ in canonical order"),Names.StartsWith(TEXT("x · y · z")));
        for(int32 Field=3;Field<11;++Field)TestEqual(TEXT("Raw field access agrees with the effective mask"),Names.Contains(Raw[Field]),bool(ExpectedMasks[I]&(1<<Field)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2PcdOptions,"MA0T10.UXRP2.PcdCompressionAndDivisorText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2PcdOptions::RunTest(const FString&)
{
    const FString Compatibility=VirtualPcdOptionsPresentation::CompressionCompatibility();
    for(const TCHAR* Detail:{TEXT("기본 On"),TEXT("binary_compressed(LZF)"),TEXT("PCL(loadPCDFile)"),TEXT("본 프로젝트 수신기"),TEXT("ROS2 브리지"),TEXT("미지원이면 체크 해제")})TestTrue(TEXT("Compatibility original is preserved in the tooltip"),Compatibility.Contains(Detail));
    const TCHAR* Labels[]={TEXT("매 프레임(1/1)"),TEXT("2프레임마다(1/2)"),TEXT("4프레임마다(1/4)")};
    int32 I=0;
    for(int32 Divisor:{1,2,4})
    {
        TestEqual(TEXT("Menu and current value use the same frame cadence label"),VirtualPcdOptionsPresentation::DivisorLabel(Divisor),FString(Labels[I++]));
        TestFalse(TEXT("The choice does not claim a fixed measured rate"),VirtualPcdOptionsPresentation::DivisorLabel(Divisor).Contains(TEXT("Hz")));
    }
    const FString Hint=VirtualPcdOptionsPresentation::DivisorTooltip();
    TestTrue(TEXT("20 Hz numbers are explicitly examples, independent of measurement and actual rates"),Hint.Contains(TEXT("예: 20 Hz"))&&Hint.Contains(TEXT("별개"))&&Hint.Contains(TEXT("실제 전송률을 뜻하지 않습니다")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2Bandwidth,"MA0T10.UXRP2.PcdBandwidthSourceAndUnits",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2Bandwidth::RunTest(const FString&)
{
    FVirtualPcdBandwidthPresentation View;View.PointBytes=24;View.SensorCount=3;View.SelectedMaximum=30*1048576.;View.TotalMaximum=90*1048576.;
    auto Text=View.Text().ToString();
    TestTrue(TEXT("Over-budget stopped estimate has an unambiguous first line"),Text.StartsWith(TEXT("전송 정지 · 압축 전 최대 추정\n")));
    TestTrue(TEXT("Selected, total and configured budget have separate aligned rows"),Text.Contains(TEXT("\n선택 30.00 MiB/s (251.7 Mbps)\n합계 3대 90.00 MiB/s (755.0 Mbps)\n설정 예산 80 MiB/s · 112.5%")));
    TestTrue(TEXT("Estimate detail preserves assumptions and does not claim a live failure"),View.Details().ToString().Contains(TEXT("압축 효과 미반영"))&&View.Details().ToString().Contains(TEXT("실제 전송 장애를 뜻하지 않습니다")));
    View.ActiveCount=3;View.Selected.Frames=20;View.Selected.BodyBytes=10*1048576;
    for(int32 I=0;I<3;++I){auto Sample=View.Selected;Sample.OriginalPointBytes=20*1048576;Sample.EncodedPointBytes=10*1048576;View.Total.Add(Sample);}
    Text=View.Text().ToString();
    TestTrue(TEXT("Actual first line and original byte/Hz/weighted reduction values"),Text.StartsWith(TEXT("전송 중 · 최근 1초 실측\n"))&&Text.Contains(TEXT("20.0 Hz"))&&Text.Contains(TEXT("30.00 MiB/s (251.7 Mbps)"))&&Text.Contains(TEXT("MiB/s · 37.5%"))&&Text.Contains(TEXT("압축 절감 50.0%")));
    TestFalse(TEXT("Live actual never falls back to the stopped estimate"),Text.Contains(TEXT("112.5%"))||Text.Contains(TEXT("최대 추정")));
    TestTrue(TEXT("Detailed byte origin is retained"),View.Details().ToString().Contains(TEXT("헤더 포함"))&&View.Details().ToString().Contains(TEXT("전송 완료 기준")));
    View.Selected={};View.Total={};
    TestTrue(TEXT("A measured idle interval still shows zero actual"),View.Text().ToString().Contains(TEXT("MiB/s · 0.0%")));
    for(int32 Mode:{0,1})
    {
        View.UnmeasuredCount=Mode==0?1:0;View.Total.bComplete=Mode!=1;
        Text=View.Text().ToString();
        TestTrue(TEXT("Unsupported format and incomplete aggregation are both explicit"),Text.Contains(TEXT("집계 불완전"))&&Text.Contains(TEXT("확인 불가")));
        TestFalse(TEXT("Unavailable aggregation is never zero/success"),Text.Contains(TEXT("0.0%"))||Text.Contains(TEXT("성공")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP2ScopeAndWarning,"MA0T10.UXRP2.FontScopeAndWarningText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP2ScopeAndWarning::RunTest(const FString&)
{
    for(float Scale:{1.f,1.5f})
    {
        const FString Number=FString::Printf(TEXT("%d%%"),FMath::RoundToInt(Scale*100));
        TestEqual(TEXT("Coil label displays its current scope and value"),CoilLoadingPresentation::FontLabel(Scale),TEXT("코일 글자 ")+Number);
        TestEqual(TEXT("Tools label displays its independent scope and value"),SensorToolUiText::ToolsFontPercent(Scale).ToString(),TEXT("센서 도구 글자 ")+Number);
    }
    TestTrue(TEXT("The shortcut explains separate monitor settings"),SensorToolUiText::ToolsFontHint().ToString().Contains(TEXT("코일·해치 모니터")));
    const struct FCase{const TCHAR* Original;const TCHAR* Short;} Cases[]={
        {TEXT("전송할 장비를 배치나 상단 목록에서 선택하세요"),TEXT("장비 선택 필요")},
        {TEXT("장비 없음 · 배치에서 장비를 추가하세요"),TEXT("장비 추가 필요")},
        {TEXT("전송 방식이 맞지 않습니다 · 서버 연결 설정에서 실시간 전송을 선택하세요"),TEXT("연결 방식 확인 필요")},
        {TEXT("PCD 필요 90.00 MiB/s > 예산 80.00 MiB/s · 실제 송신 합계 20.0 Hz (최근 약 1초)"),TEXT("PCD 필요량·예산 확인")},
        {TEXT("서버 연결 끊김 · 서버 연결 설정에서 확인하세요"),TEXT("서버 연결 끊김")},
        {TEXT("향후 진단 사유 전체"),TEXT("향후 진단 사유 전체")}
    };
    for(const auto& C:Cases)
    {
        const FString Original=C.Original;
        TestEqual(TEXT("Short warning cause"),SensorToolUiText::StatusWarningSummary(Original),FString(C.Short));
        TestEqual(TEXT("The original tooltip reason is never mutated"),Original,FString(C.Original));
    }
    TestTrue(TEXT("Empty warning stays empty"),SensorToolUiText::StatusWarningSummary(TEXT("")).IsEmpty());
    TestEqual(TEXT("Action is explicit and separate from the ellipsized title"),SensorToolUiText::CheckAction().ToString(),FString(TEXT("확인 ›")));
    return true;
}
#endif
