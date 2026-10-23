#include "ma0t10_dt/MA0T10/UI/SensorAcquisitionRateWarning.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorAcquisitionRateWarningTest,
    "MA0T10.SensorPerformance.RequestedRateWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSensorAcquisitionRateWarningTest::RunTest(const FString&)
{
    for (const double Fps : {59.0, 58.0, 45.0, 120.0})
    {
        FSensorAcquisitionRateWarning Warning;
        for (int32 Frame = 0; Frame < Fps * 90; ++Frame)
        {
            const double Now = Frame / Fps;
            Warning.Observe(Now, FMath::FloorToInt64(Now * 20), 20, true);
            if (Warning.IsBelowRequest())
            {
                AddError(FString::Printf(TEXT("normal 20Hz pacing at %.0fFPS triggered a warning"), Fps));
                break;
            }
        }
    }

    FSensorAcquisitionRateWarning Warning;
    for (int32 Step = 0; Step <= 30; ++Step)
        Warning.Observe(Step * 0.2, FMath::FloorToInt64(Step * 0.2 * 9.7), 20, true);
    TestTrue(TEXT("ongoing 9.7Hz progress warns for requested 20Hz"), Warning.IsBelowRequest());
    TestTrue(TEXT("observed completion rate is shown, not output division"), Warning.GetObservedHz() < 10.0);
    for (int32 Step = 31; Step <= 80; ++Step)
        Warning.Observe(Step * 0.2, 58 + FMath::FloorToInt64((Step * 0.2 - 6) * 20), 20, true);
    TestFalse(TEXT("recovery clears after a complete observation window"), Warning.IsBelowRequest());
    Warning.Observe(16.2, 260, 20, false);
    TestFalse(TEXT("pause, stop and manipulation clear warning"), Warning.IsBelowRequest());
    Warning.Observe(20, 1, 20, true);
    Warning.Observe(25, 2, 20, true);
    TestFalse(TEXT("hidden panel or long polling gap restarts observation"), Warning.IsBelowRequest());
    for (int32 Step = 0; Step <= 30; ++Step)
        Warning.Observe(25 + Step * 0.2, 2 + Step, 20, true);
    TestTrue(TEXT("capacity shortfall returns after observation resumes"), Warning.IsBelowRequest());
    Warning.Observe(31.2, 33, 5, true);
    TestFalse(TEXT("requested rate change restarts grace"), Warning.IsBelowRequest());
    Warning.Observe(31.4, 0, 5, true);
    TestFalse(TEXT("new frame sequence starts cleanly"), Warning.IsBelowRequest());
    return true;
}
#endif
