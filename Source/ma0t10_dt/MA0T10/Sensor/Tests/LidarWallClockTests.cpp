#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarCadence.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarCadenceCohort.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include <algorithm>

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarWallClockCohortChangesTest,"MA0T10.LidarWallClock.CohortChanges",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarWallClockCohortChangesTest::RunTest(const FString&)
{
    using M=FVirtualLidarCadenceCohort::FMember;
    for(const auto& Order : {std::vector<int>{0,1,2},std::vector<int>{2,0,1},std::vector<int>{1,2,0}})
    {
        FVirtualLidarCadenceCohort Cohort;
        FVirtualLidarCadence Clocks[4]; std::vector<M> Members;
        auto Apply=[&](double Now)
        {
            std::sort(Members.begin(),Members.end(),[](const M& A,const M& B){return A.Key<B.Key;});
            for(const auto& A:Cohort.Update(Members))
            {
                auto& C=Clocks[A.Key];
                const double Interval=std::find_if(Members.begin(),Members.end(),[&](const M& N){return N.Key==A.Key;})->Interval;
                if(A.bInitial)C.StartAligned(Now,Interval,A.Phase);
                else C.RephaseNextSlot(Now,Interval,A.Phase);
                TestTrue(TEXT("changed group begins at its next slot"),C.GetNextDue()>=Now-1.e-8);
                TestFalse(TEXT("cohort has no catch-up admission at rephase time"),!A.bInitial&&C.Poll(Now,Interval,99,false).bAdmit);
            }
            std::vector<double> Phases;
            for(const auto& N:Members)if(N.Interval==.05)
                Phases.push_back(std::fmod(Clocks[N.Key].GetNextDue()/.05,1.0));
            std::sort(Phases.begin(),Phases.end());
            for(size_t I=0;I<Phases.size();++I)
            {
                double Gap=Phases[(I+1)%Phases.size()]-Phases[I];if(I+1==Phases.size())Gap+=1;
                TestTrue(TEXT("all equal-rate circular gaps >=1/count-epsilon"),Gap>=1.0/Phases.size()-1.e-6);
            }
        };
        double Now=1000.023;
        for(int Id:Order){Clocks[Id].Start(Now,.05);Members.push_back({uint64(Id),1,.05});Apply(Now);Now+=.017;}
        // An unrelated10Hz group neither changes20Hz phases nor resets their deadlines.
        const double Due=Clocks[0].GetNextDue();
        Clocks[3].Start(Now,.1);Members.push_back({3,1,.1});Apply(Now);
        TestEqual(TEXT("mixed rate leaves existing20Hz deadline intact"),Clocks[0].GetNextDue(),Due);
        Members.erase(std::remove_if(Members.begin(),Members.end(),[](const M& N){return N.Key==1;}),Members.end());
        Clocks[1].Stop();Apply(Now+=.017);
        Clocks[1].Start(Now+=.017,.05);Members.push_back({1,2,.05});Apply(Now);
        // Stop/start between scheduler ticks still changes the generation.
        for(auto& N:Members)if(N.Key==1)++N.Generation;
        Apply(Now+=.017);
        // Move a member to a different-rate group; both remaining/new groups change.
        for(auto& N:Members)if(N.Key==2)N.Interval=.1;
        auto Changes=Cohort.Update(Members);
        TestEqual(TEXT("period change reassigns both affected groups"),int32(Changes.size()),4);
        TestTrue(TEXT("every period-edit assignment uses future slots"),std::all_of(Changes.begin(),Changes.end(),[](const auto& A){return !A.bInitial;}));
        for(const auto& A:Changes)
        {
            const double Interval=std::find_if(Members.begin(),Members.end(),[&](const M& N){return N.Key==A.Key;})->Interval;
            Clocks[A.Key].RephaseNextSlot(Now,Interval,A.Phase);
        }
        for(double Interval:{.05,.1})
        {
            std::vector<double> Phases;
            for(const auto& N:Members)if(N.Interval==Interval)Phases.push_back(std::fmod(Clocks[N.Key].GetNextDue()/Interval,1.));
            std::sort(Phases.begin(),Phases.end());
            for(size_t I=0;I<Phases.size();++I)
            {
                double Gap=Phases[(I+1)%Phases.size()]-Phases[I];if(I+1==Phases.size())Gap+=1;
                TestTrue(TEXT("both mixed-rate groups retain uniform circular spacing"),Gap>=1./Phases.size()-1.e-6);
            }
        }
    }
    // Exact simultaneous-start compatibility: same inputs => same deadlines/admissions.
    FVirtualLidarCadenceCohort Initial;std::vector<M> Members={{0,1,.05},{1,1,.05},{2,1,.05}};
    FVirtualLidarCadence Before[3],After[3];
    for(int I=0;I<3;++I){Before[I].StartAligned(1000.023+I*.001,.05,double(I)/3);After[I].Start(1000.023+I*.001,.05);}
    for(const auto& A:Initial.Update(Members))
    {TestTrue(TEXT("initial simultaneous cohort preserves old alignment"),A.bInitial);After[A.Key].StartAligned(1000.023+A.Key*.001,.05,A.Phase);}
    for(int Frame=0;Frame<59*90;++Frame)for(int I=0;I<3;++I)
    {
        TestEqual(TEXT("simultaneous-start due time unchanged"),After[I].GetNextDue(),Before[I].GetNextDue());
        TestEqual(TEXT("simultaneous-start90s admission sequence unchanged"),After[I].Poll(1000.023+Frame/59.,.05,Frame,false).bAdmit,Before[I].Poll(1000.023+Frame/59.,.05,Frame,false).bAdmit);
    }
    TestTrue(TEXT("unchanged cohort creates no assignments"),Initial.Update(Members).empty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarWallClockCounterSemanticsTest,"MA0T10.LidarWallClock.CounterSemantics",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarWallClockCounterSemanticsTest::RunTest(const FString&)
{
    FVirtualLidarCadence C;C.Start(10,.05);C.Poll(10,.05,1,false);
    const auto First=C.Poll(10.06,.05,2,true);
    TestTrue(TEXT("first busy due poll is one delayed-acquisition event"),First.bDelayedAcquisitionEvent);
    TestEqual(TEXT("a delayed acquisition need not expire a slot yet"),int64(First.SkippedSlots),int64(0));
    const auto Long=C.Poll(10.26,.05,3,true);
    TestFalse(TEXT("same acquisition is not counted again"),Long.bDelayedAcquisitionEvent);
    TestEqual(TEXT("four expired slots have a separate count"),int64(Long.SkippedSlots),int64(4));
    C.RephaseNextSlot(10.27,.05,.5);
    TestFalse(TEXT("rephase preserves active acquisition event latch"),C.Poll(10.5,.05,4,true).bDelayedAcquisitionEvent);
    C.Poll(10.5,.05,4,false);
    TestFalse(TEXT("post-poll never bursts"),C.Poll(10.5,.05,4,false).bAdmit);
    TestTrue(TEXT("next active acquisition can record its own delay"),C.Poll(10.6,.05,5,true).bDelayedAcquisitionEvent);
    return true;
}
#endif
