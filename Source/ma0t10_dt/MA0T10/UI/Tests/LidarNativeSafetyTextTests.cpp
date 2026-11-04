#include "DeviceUI/LidarNativeSafetyText.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarNativeSafetyTextTest, "MA0T10.SensorNativeSafety.StatusText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarNativeSafetyTextTest::RunTest(const FString&)
{
	TestTrue(TEXT("normal state adds no safety warning"), FString(LidarNativeSafetyText::Message(SensorNativeSafety::EStatus::None)).IsEmpty());
	const FString Banned = LidarNativeSafetyText::Message(SensorNativeSafety::EStatus::Banned);
	TestTrue(TEXT("ban explains unchanged two-failure policy"), Banned.Contains(TEXT("비정상 종료 2회")));
	TestTrue(TEXT("ban names both recovery choices"), Banned.Contains(TEXT("드라이버 변경")) && Banned.Contains(TEXT("안전 상태 초기화")));
	TestFalse(TEXT("ban is one diagnostic line"), Banned.Contains(TEXT("\n")));
	const FString Recovery = LidarNativeSafetyText::Message(SensorNativeSafety::EStatus::Recovery);
	TestTrue(TEXT("recovery explicitly uses supported RHI this launch"), Recovery.Contains(TEXT("이번 실행은 RHI")));
	TestFalse(TEXT("one recovery is not described as a two-failure ban"), Recovery.Contains(TEXT("2회")));
	return true;
}
#endif
