#include "DeviceUI/StableHeaderReservation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchHeaderStableTest, "HATCH26.HeaderStableReservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHatchHeaderStableTest::RunTest(const FString&)
{
	FStableHeaderReservation R;
	R.Reset(120, 0);
	int32 LayoutChanges = 0;
	double Previous = R.Height;
	for (int32 I = 1; I <= 200; ++I)
	{
		const double H = R.Update(I % 2 ? 80 : 120, I * .2);
		LayoutChanges += !FMath::IsNearlyEqual(H, Previous);
		Previous = H;
	}
	TestEqual(TEXT("preview/measured alternation cannot resize view or recreate TSR history"), LayoutChanges, 0);
	TestEqual(TEXT("short text initially retains reservation"), R.Update(80, 41), 120.);
	TestEqual(TEXT("short text retains reservation before five seconds"), R.Update(80, 45.9), 120.);
	TestEqual(TEXT("stable short text releases reservation after five seconds"), R.Update(80, 46.1), 80.);
	TestEqual(TEXT("long text grows immediately without clipping"), R.Update(130, 46.2), 130.);
	R.Reset(150, 47);
	TestEqual(TEXT("explicit font/window resize recomputes immediately"), R.Height, 150.);
	return true;
}
#endif
