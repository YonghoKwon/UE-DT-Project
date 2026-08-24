#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorCadence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorCadenceLongRunTest, "MA0T10.SensorPerformance.RealtimeCadenceLongRun", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorCadenceMissAndResumeTest, "MA0T10.SensorPerformance.RealtimeCadenceMissAndResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorCadenceLongRunTest::RunTest(const FString& Parameters)
{
	for (const double Interval : {1.0 / 30.0, 1.0 / 20.0})
	{
		FVirtualSensorCadenceState Cadence;
		Cadence.Start(100.0, 1000000000000LL, Interval);
		FVirtualSensorCadenceDeadline Deadline;
		const int32 ExpectedCount = FMath::RoundToInt(600.0 / Interval);
		for (int32 Index = 0; Index < ExpectedCount; ++Index)
		{
			const double Now = 100.0 + Index * Interval;
			const int64 Utc = 1000000000000LL + static_cast<int64>(Index * Interval * 1.0e9);
			TestTrue(TEXT("each exact deadline is admitted once"), Cadence.ConsumeDeadline(Now, Utc, Deadline));
			TestFalse(TEXT("the same deadline cannot be consumed twice"), Cadence.ConsumeDeadline(Now, Utc, Deadline));
		}
		TestEqual(TEXT("ten-minute cadence count has no drift"), Cadence.GetTelemetry().ScheduledAcquisitionCount, static_cast<int64>(ExpectedCount));
		TestEqual(TEXT("exact cadence has no deadline misses"), Cadence.GetTelemetry().DeadlineMissCount, static_cast<int64>(0));
		TestTrue(TEXT("next deadline remains phase locked"), FMath::IsNearlyEqual(Cadence.GetNextDeadlineMonotonicSeconds(), 100.0 + ExpectedCount * Interval, 1.0e-8));
	}
	return true;
}

bool FVirtualSensorCadenceMissAndResumeTest::RunTest(const FString& Parameters)
{
	FVirtualSensorCadenceState Cadence;
	Cadence.Start(10.0, 2000000000000LL, 0.05);
	FVirtualSensorCadenceDeadline Deadline;
	TestTrue(TEXT("first frame starts at the initial phase"), Cadence.ConsumeDeadline(10.0, 2000000000000LL, Deadline));
	TestTrue(TEXT("a hitch admits one coherent frame"), Cadence.ConsumeDeadline(10.16, 2160000000000LL, Deadline));
	TestEqual(TEXT("hitch records skipped sensor periods"), Deadline.MissedDeadlines, 2);
	TestEqual(TEXT("hitch does not synthesize duplicate frames"), Cadence.GetTelemetry().ScheduledAcquisitionCount, static_cast<int64>(2));
	Cadence.Resume(30.0, 4000000000000LL);
	TestFalse(TEXT("resume does not immediately replay paused deadlines"), Cadence.IsDue(30.0));
	TestTrue(TEXT("resume starts on the next clean period"), Cadence.ConsumeDeadline(30.05, 4050000000000LL, Deadline));
	TestEqual(TEXT("pause duration is not counted as a deadline miss"), Cadence.GetTelemetry().DeadlineMissCount, static_cast<int64>(2));
	return true;
}

#endif
