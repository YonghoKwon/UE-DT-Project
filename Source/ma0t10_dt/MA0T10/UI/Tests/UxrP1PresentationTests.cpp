#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingPresentation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMetricPresentation.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP1PreflightText,"MA0T10.UXRP1.PreflightShortReasonsAndRawDetails",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP1PreflightText::RunTest(const FString&)
{
    const struct FCase { const TCHAR* Reason; const TCHAR* Title; } Cases[]={
        {TEXT("실시간 Topic 송신에는 STOMP 전송 모드가 필요합니다."),TEXT("방식 확인 필요")},
        {TEXT("STOMP 설정 없음"),TEXT("서버 연결 설정 필요")},
        {TEXT("PCD Topic 없음"),TEXT("PCD 출력 주소 필요")},
        {TEXT("Camera Topic 없음"),TEXT("Camera 이미지 출력 주소 필요")},
        {TEXT("LiDAR Topic 없음"),TEXT("LiDAR 정보 출력 주소 필요")},
        {TEXT("Broker URL을 설정하세요."),TEXT("서버 주소 필요")},
        {TEXT("Coordinator 없음"),TEXT("장비 연결 확인 필요")},
        {TEXT("중복 SensorId: lidar-a"),TEXT("장비 ID 중복")},
        {TEXT("선택 센서 없음"),TEXT("장비 선택 필요")},
        {TEXT("대상 SensorId 없음: lidar-a"),TEXT("장비 선택 필요")},
        {TEXT("PCD 측정 대상 LiDAR가 없습니다."),TEXT("전송할 LiDAR 필요")},
        {TEXT("이미지 송신 대상 Camera가 없습니다."),TEXT("전송할 Camera 필요")},
        {TEXT("lidar-a: 측정 주기를 확인하세요."),TEXT("측정 주기 확인 필요")},
        {TEXT("lidar-a: 수집 설정을 읽을 수 없습니다."),TEXT("센서 상세 설정 확인 필요")},
        {TEXT("lidar-a: 데이터 필수 실행에는 PCD 전송 1/1이 필요합니다(현재 1/2)."),TEXT("PCD 전송 간격 확인 필요")}
    };
    FVirtualSensorOutputPreflightResult Check;
    for(const auto& C:Cases)
    {
        const FString Original=C.Reason;
        TestEqual(TEXT("Every recognized blocker has a short operator title"),SensorToolUiText::PreflightShortReason(Original),FString(C.Title));
        TestEqual(TEXT("Formatting does not mutate the blocker"),Original,FString(C.Reason));
        Check.BlockingReasons.Add(Original);
    }
    const FString Unknown=TEXT("새로운 점검 사유 · 전체 설명은 생략하지 않습니다");
    TestEqual(TEXT("Future blockers retain their full presentation text"),SensorToolUiText::PreflightShortReason(Unknown),Unknown);
    const FString Details=UVirtualSensorOutputPreflight::FormatDetails(Check);
    for(const auto& C:Cases)TestTrue(TEXT("Expanded details retain each original blocker"),Details.Contains(C.Reason));
    TestEqual(TEXT("UI titles never clear blocking reasons"),Check.BlockingReasons.Num(),int32(UE_ARRAY_COUNT(Cases)));
    return true;
}
#endif
