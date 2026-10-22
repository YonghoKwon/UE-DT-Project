#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"

struct FPcdFairBudgetTestAccess
{
    static auto& Runtime(UVirtualSensorStreamPublisherComponent* P,const FString& Id)
    {return P->FindOrAddRuntime(EVirtualSensorStreamKind::PointCloud,Id);}
    static void Update(UVirtualSensorStreamPublisherComponent* P,double Now){P->UpdatePointCloudFairBudgets(Now);}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdFairBudgetTest,"MA0T10.SensorStream.PcdFairBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdFairBudgetTest::RunTest(const FString&)
{
    auto* P=NewObject<UVirtualSensorStreamPublisherComponent>();
    TestEqual(TEXT("new publisher defaults to approved80MiB/s"),P->PointCloudBandwidthLimitMegabytesPerSecond,80.f);
    P->PointCloudBandwidthLimitMegabytesPerSecond=64.f; // Explicit constrained-budget regression.
    auto Configure=[&](const FString& Id,double Bytes)
    {
        FVirtualSensorStreamConfig C;C.StreamKind=EVirtualSensorStreamKind::PointCloud;C.SensorId=Id;C.bEnabled=true;P->ConfigureStream(C);
        auto& S=FPcdFairBudgetTestAccess::Runtime(P,Id).SensorSequences.FindOrAdd(Id);
        S.InputPeriod=.05;S.LastInputSeconds=100;S.FrameBytes=Bytes;S.BandwidthTokens=Bytes;
    };
    const double Bytes=65536*33+1024;
    Configure(TEXT("A"),Bytes);Configure(TEXT("B"),Bytes);Configure(TEXT("C"),Bytes);
    FPcdFairBudgetTestAccess::Update(P,100);
    const double Share=64*1048576.0/3;
    for(const FString Id:{TEXT("A"),TEXT("B"),TEXT("C")})
    {
        const auto& R=FPcdFairBudgetTestAccess::Runtime(P,Id);
        TestTrue(TEXT("equal dense streams receive equal byte shares"),FMath::Abs(R.SensorSequences.FindChecked(Id).FairBytesPerSecond-Share)<1);
        TestTrue(TEXT("measured header plus33-byte point requirement exceeds budget"),R.Status.PcdRequiredMiBPerSecond>123.75);
    }
    // Adversarial fixed A/B/C service order cannot consume a neighbour's share.
    int Counts[3]={};double Used=0;
    for(int Tick=1;Tick<=1800;++Tick)
    {
        const double Now=100+Tick/60.;
        for(const FString Id:{TEXT("A"),TEXT("B"),TEXT("C")})FPcdFairBudgetTestAccess::Runtime(P,Id).SensorSequences.FindChecked(Id).LastInputSeconds=Now;
        FPcdFairBudgetTestAccess::Update(P,Now);
        int I=0;for(const FString Id:{TEXT("A"),TEXT("B"),TEXT("C")})
        {
            auto& S=FPcdFairBudgetTestAccess::Runtime(P,Id).SensorSequences.FindChecked(Id);
            if(S.BandwidthTokens>=Bytes){S.BandwidthTokens-=Bytes;++Counts[I];Used+=Bytes;}++I;
        }
    }
    TestEqual(TEXT("service order does not bias admitted byte count"),Counts[0],Counts[2]);
    TestTrue(TEXT("full OS1 bodies can sustain10Hz each within64MiB/s"),Counts[0]/30.>=10);
    TestTrue(TEXT("aggregate stays within refill plus initial frame burst"),Used<=30*64*1048576.0+3*Bytes);
    auto& Bigger=FPcdFairBudgetTestAccess::Runtime(P,TEXT("B")).SensorSequences.FindChecked(TEXT("B"));Bigger.FrameBytes=2*Bytes;
    FPcdFairBudgetTestAccess::Update(P,130.3);
    const auto& Normal=FPcdFairBudgetTestAccess::Runtime(P,TEXT("C")).SensorSequences.FindChecked(TEXT("C"));
    TestTrue(TEXT("different encoded sizes receive the same Hz ceiling"),FMath::Abs(Bigger.FairBytesPerSecond/Bigger.FrameBytes-Normal.FairBytesPerSecond/Normal.FrameBytes)<1e-6);
    Bigger.FrameBytes=Bytes;
    // A slower producer gets its full demand; unused capacity goes to peers.
    auto& Small=FPcdFairBudgetTestAccess::Runtime(P,TEXT("A")).SensorSequences.FindChecked(TEXT("A"));Small.FrameBytes=1048576*.1;
    Small.InputPeriod=.5;
    FPcdFairBudgetTestAccess::Update(P,130.6);
    TestTrue(TEXT("slow producer retains2Hz"),FMath::Abs(Small.FairBytesPerSecond-.2*1048576)<1);
    TestTrue(TEXT("large producer reclaims unused capacity"),FPcdFairBudgetTestAccess::Runtime(P,TEXT("B")).SensorSequences.FindChecked(TEXT("B")).FairBytesPerSecond>Share);
    // Expired and stopped producers release their reservation.
    FPcdFairBudgetTestAccess::Runtime(P,TEXT("B")).Config.bEnabled=false;
    FPcdFairBudgetTestAccess::Runtime(P,TEXT("C")).SensorSequences.FindChecked(TEXT("C")).LastInputSeconds=1;
    FPcdFairBudgetTestAccess::Update(P,130.9);
    TestTrue(TEXT("disabled producer has no share"),FPcdFairBudgetTestAccess::Runtime(P,TEXT("B")).Status.PcdFairShareMiBPerSecond==0);
    TestTrue(TEXT("inactive producer has no share"),FPcdFairBudgetTestAccess::Runtime(P,TEXT("C")).Status.PcdFairShareMiBPerSecond==0);
    TestEqual(TEXT("explicit64MiB/s override survives stream configuration and scheduling"),P->PointCloudBandwidthLimitMegabytesPerSecond,64.f);
    auto& R=FPcdFairBudgetTestAccess::Runtime(P,TEXT("A"));R.bSerializationInFlight=true;
    const double RememberedBytes=Small.FrameBytes,RememberedTokens=Small.BandwidthTokens;
    FVirtualSensorFrameEnvelope Frame;Frame.SensorId=TEXT("A");Frame.SensorKind=EVirtualSensorKind::Lidar;Frame.FrameId=1;Frame.RequestedAcquisitionIntervalSeconds=.05;
    P->SubmitFrame(Frame);
    TestEqual(TEXT("new segment retains measured body cost"),Small.FrameBytes,RememberedBytes);
    TestEqual(TEXT("new segment retains sender tokens for accepted FIFO"),Small.BandwidthTokens,RememberedTokens);
    auto& Old=FPcdFairBudgetTestAccess::Runtime(P,TEXT(""));Old.Config.bEnabled=true;
    auto& OldSensor=Old.SensorSequences.FindOrAdd(TEXT("A"));OldSensor.FrameBytes=Bytes;OldSensor.InputPeriod=.05;OldSensor.LastInputSeconds=1;
    Old.PreparedMessageQueue.AddDefaulted_GetRef().SensorId=TEXT("A");
    FPcdFairBudgetTestAccess::Update(P,131.3);
    TestTrue(TEXT("accepted wildcard FIFO drains after exact-route shadowing and input expiry"),OldSensor.FairBytesPerSecond>0);
    auto* Recent=NewObject<UVirtualSensorStreamPublisherComponent>();auto& RecentRuntime=FPcdFairBudgetTestAccess::Runtime(Recent,TEXT("recent"));
    RecentRuntime.Status.SubmittedFrameCount=100;FPcdFairBudgetTestAccess::Update(Recent,100);
    RecentRuntime.Status.SubmittedFrameCount=120;FPcdFairBudgetTestAccess::Update(Recent,101);
    TestEqual(TEXT("recent Hz uses new socket counts, not lifetime average"),RecentRuntime.Status.RecentSubmittedHz,20.f);
    FPcdFairBudgetTestAccess::Update(Recent,102);
    TestEqual(TEXT("stopped output becomes zero in the next sample"),RecentRuntime.Status.RecentSubmittedHz,0.f);
    RecentRuntime.Status.SubmittedFrameCount=1;FPcdFairBudgetTestAccess::Update(Recent,103);
    TestEqual(TEXT("transport counter reset cannot produce negative Hz"),RecentRuntime.Status.RecentSubmittedHz,0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdDefaultBudgetTest,"MA0T10.SensorStream.PcdDefaultBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdDefaultBudgetTest::RunTest(const FString&)
{
    auto* P=NewObject<UVirtualSensorStreamPublisherComponent>();
    TestEqual(TEXT("publisher default80MiB/s"),P->PointCloudBandwidthLimitMegabytesPerSecond,80.f);
    TestEqual(TEXT("status default80MiB/s"),FVirtualSensorStreamStatus().PcdBudgetMiBPerSecond,80.f);
    TestEqual(TEXT("preflight default80MiB/s"),FVirtualSensorOutputPreflightResult().PcdBudgetMegabytesPerSecond,80.);
    // Measured default Rev7 demand, not the worst-case all-rays-return fixture above.
    const double FrameBytes=75.9*1048576./60.;
    for(const FString Id:{TEXT("A"),TEXT("B"),TEXT("C")})
    {
        FVirtualSensorStreamConfig C;C.StreamKind=EVirtualSensorStreamKind::PointCloud;C.SensorId=Id;C.bEnabled=true;P->ConfigureStream(C);
        auto& S=FPcdFairBudgetTestAccess::Runtime(P,Id).SensorSequences.FindOrAdd(Id);
        S.InputPeriod=.05;S.LastInputSeconds=100;S.FrameBytes=FrameBytes;
    }
    FPcdFairBudgetTestAccess::Update(P,100);
    for(const FString Id:{TEXT("A"),TEXT("B"),TEXT("C")})
    {
        const auto& R=FPcdFairBudgetTestAccess::Runtime(P,Id);const auto& S=R.SensorSequences.FindChecked(Id);
        TestTrue(TEXT("measured default Rev7 demand retains20Hz per stream"),FMath::IsNearlyEqual(S.FairBytesPerSecond/S.FrameBytes,20.,1.e-6));
        TestTrue(TEXT("measured demand does not produce budget warning"),R.Status.PcdRequiredMiBPerSecond<R.Status.PcdBudgetMiBPerSecond);
    }
    // Preserve a serialized component override without a PostLoad/default migration.
    auto* Configured=NewObject<UVirtualSensorStreamPublisherComponent>();Configured->PointCloudBandwidthLimitMegabytesPerSecond=64.f;
    TArray<uint8> Bytes;FObjectWriter Writer(Configured,Bytes);
    auto* Restored=NewObject<UVirtualSensorStreamPublisherComponent>();FObjectReader Reader(Restored,Bytes);
    TestEqual(TEXT("serialized user64MiB/s survives the new80 default"),Restored->PointCloudBandwidthLimitMegabytesPerSecond,64.f);
    FVirtualSensorStreamConfig C;C.StreamKind=EVirtualSensorStreamKind::PointCloud;C.SensorId=TEXT("custom");C.bEnabled=true;Restored->ConfigureStream(C);
    FPcdFairBudgetTestAccess::Update(Restored,100);
    TestEqual(TEXT("restored override survives runtime configuration"),Restored->PointCloudBandwidthLimitMegabytesPerSecond,64.f);
    return true;
}
#endif
