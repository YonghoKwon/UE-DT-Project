#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarCadence.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarWallClockRatesTest, "MA0T10.LidarWallClock.Rates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarWallClockRatesTest::RunTest(const FString&)
{
    for (const int32 Fps : {59, 58, 45, 120})
    {
        FVirtualLidarCadence Clock;
        Clock.Start(1000.0, .05);
        int32 Acquisitions = 0;
        for (int32 Frame = 0; Frame < Fps * 90; ++Frame)
        {
            const double Now = 1000.0 + double(Frame) / Fps;
            Acquisitions += Clock.Poll(Now, .05, Frame, false).bAdmit;
            TestFalse(TEXT("post-poll cannot admit twice in one engine frame"), Clock.Poll(Now + .0001, .05, Frame, false).bAdmit);
        }
        TestEqual(FString::Printf(TEXT("%dFPS sustains 20Hz for 90 virtual seconds"), Fps), Acquisitions, 1800);
        TestEqual(TEXT("no missing slots when frame rate exceeds requested Hz"), int64(Clock.GetSkippedSlots()), int64(0));
    }
    // Long virtual run with varying phase jitter; expect at most one boundary slot difference.
    FVirtualLidarCadence Jitter;
    Jitter.Start(5000.0, .05, .37);
    int32 Count = 0;
    for (int32 Frame = 0; Frame < 58000; ++Frame)
        Count += Jitter.Poll(5000.0 + (Frame + (Frame % 7) * .02) / 58.0, .05, Frame, false).bAdmit;
    TestTrue(TEXT("1000 seconds of jitter has no cumulative phase drift"), FMath::Abs(Count - 20000) <= 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarWallClockBackpressureTest, "MA0T10.LidarWallClock.Backpressure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarWallClockBackpressureTest::RunTest(const FString&)
{
    FVirtualLidarCadence Clock;
    Clock.Start(10, .05);
    TestTrue(TEXT("first slot admitted"), Clock.Poll(10, .05, 1, false).bAdmit);
    auto Decision = Clock.Poll(10.26, .05, 2, true);
    TestFalse(TEXT("busy CPU/GPU acquisition never overlaps"), Decision.bAdmit);
    TestEqual(TEXT("four expired slots counted, newest remains pending"), int64(Decision.SkippedSlots), int64(4));
    TestEqual(TEXT("second pre-poll does not count misses again"), int64(Clock.Poll(10.26, .05, 2, true).SkippedSlots), int64(0));
    TestTrue(TEXT("completion post-poll can start newest slot in same frame"), Clock.Poll(10.26, .05, 2, false).bAdmit);
    TestFalse(TEXT("no catchup burst"), Clock.Poll(10.26, .05, 2, false).bAdmit);
    Clock.RequestImmediate(10.27);
    TestFalse(TEXT("immediate request respects per-frame latch"), Clock.Poll(10.27, .05, 2, false).bAdmit);
    TestTrue(TEXT("immediate request resumes next frame"), Clock.Poll(10.28, .05, 3, false).bAdmit);
    Clock.Suspend(10.29);
    TestFalse(TEXT("pause preserves remaining deadline without counting pause debt"), Clock.Poll(110.29, .05, 4, false).bAdmit);
    TestTrue(TEXT("deadline resumes after pause"), Clock.Poll(110.32, .05, 5, false).bAdmit);
    TestEqual(TEXT("only actual missed slots counted"), int64(Clock.GetSkippedSlots()), int64(4));
    TestTrue(TEXT("interval edit starts new grid"), Clock.Poll(110.33, .1, 6, false).bAdmit);
    Clock.HoldAdmissions();
    TestFalse(TEXT("dataset single shot holds admission while polling completion"), Clock.Poll(120, .1, 7, false).bAdmit);
    TestTrue(TEXT("failed one-shot backend can request a bounded retry"), Clock.RetryHeldAdmission(120));
    TestTrue(TEXT("one-shot retry resumes admission"), Clock.Poll(120, .1, 7, false).bAdmit);
    Clock.Stop();
    TestFalse(TEXT("stopped sensor never admits"), Clock.Poll(200, .1, 8, false).bAdmit);
    Clock.Start(200, .05);
    TestTrue(TEXT("restart drops old debt"), Clock.Poll(200, .05, 8, false).bAdmit);
    Clock.Stop(); Clock.Start(200, .05);
    TestFalse(TEXT("stop/start cannot bypass frame latch"), Clock.Poll(200, .05, 8, false).bAdmit);
    FVirtualLidarCadence Slow;
    Slow.Start(0, .05);
    int32 Starts = 0;
    for (int32 Frame = 0; Frame <= 10; ++Frame) Starts += Slow.Poll(Frame / 10.0, .05, Frame, false).bAdmit;
    TestEqual(TEXT("10FPS never exceeds one start per frame"), Starts, 11);
    TestEqual(TEXT("10FPS missing 20Hz slots remain visible"), int64(Slow.GetSkippedSlots()), int64(10));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarWallClockPhaseSpreadTest,"MA0T10.LidarWallClock.PhaseSpread",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarWallClockPhaseSpreadTest::RunTest(const FString&)
{
    for(const int32 Fps:{59,58,45,120})
    {
        FVirtualLidarCadence Clocks[3];int32 Counts[3]={},DoubleFrames=0,Peak=0;
        constexpr double Started=1000.023;
        for(int I=0;I<3;++I)
        {
            Clocks[I].StartAligned(Started+I*.001,.05,double(I)/3);
            TestTrue(TEXT("aligned first deadline never predates sensor start"),Clocks[I].GetNextDue()>=Started+I*.001-1.e-7);
        }
        for(int Frame=0;Frame<Fps*90;++Frame)
        {
            int Admissions=0;const double Now=Started+double(Frame)/Fps;
            for(int I=0;I<3;++I)
            {
                if(Clocks[I].Poll(Now,.05,Frame,false).bAdmit){++Counts[I];++Admissions;}
                TestFalse(TEXT("phase spreading never bypasses frame latch"),Clocks[I].Poll(Now,.05,Frame,false).bAdmit);
            }
            Peak=FMath::Max(Peak,Admissions);DoubleFrames+=Admissions>1;
        }
        for(int I=0;I<3;++I)
        {
            TestTrue(TEXT("every phased sensor sustains20Hz with one boundary-slot tolerance"),FMath::Abs(Counts[I]-1800)<=1);
            TestEqual(TEXT("phase spreading drops no requested slots"),int64(Clocks[I].GetSkippedSlots()),int64(0));
        }
        TestTrue(TEXT("regular45..120FPS never admits three simultaneous20Hz sensors"),Peak<=2);
        if(Fps==58||Fps==59)TestTrue(TEXT("near59FPS multi-admission frames stay below5percent"),DoubleFrames<double(Fps*90)*.05);
    }
    FVirtualLidarCadence Delayed;Delayed.StartAligned(1.023,.05,0);
    const auto D=Delayed.Poll(1.26,.05,99,false);
    TestTrue(TEXT("delayed first poll admits only one due scan"),D.bAdmit);
    TestEqual(TEXT("phase initialization preserves actual delayed-start missed slots"),int64(D.SkippedSlots),int64(4));
    return true;
}
#endif
