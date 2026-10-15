#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"

struct FPcdRateControlTestAccess
{
    static auto& Runtime(UVirtualSensorStreamPublisherComponent* P){return P->FindOrAddRuntime(EVirtualSensorStreamKind::PointCloud,TEXT("rate-test"));}
    static void Update(UVirtualSensorStreamPublisherComponent* P,double Now,const FVirtualSensorHighThroughputProfile& Profile={}){P->UpdatePointCloudRateControl(Runtime(P),Now,Profile);}
    static void Log(UVirtualSensorStreamPublisherComponent* P,double Now){P->LogPointCloudTransition(TEXT("rate-test"),Runtime(P),TEXT("test transition"),Now);}
};
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPcdRateControlTest,"MA0T10.SensorStream.PcdRateControl",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FPcdRateControlTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{for(const TCHAR* Name:{TEXT("FrameAndBytePressure"),TEXT("EvenAdmissionAndSlabFailure"),TEXT("SlowerInterval"),TEXT("Recovery"),TEXT("Disabled"),TEXT("ConsoleThrottle")}){Names.Add(Name);Commands.Add(Name);}}
bool FPcdRateControlTest::RunTest(const FString& Case)
{
    auto* P=NewObject<UVirtualSensorStreamPublisherComponent>();auto* T=NewObject<UVirtualSensorTransportComponent>();
    T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;FVirtualSensorTransportProfile Profile;Profile.BrokerUrl=TEXT("tcp://127.0.0.1:1");T->ConfigureTransportProfile(Profile);P->SetTransportComponent(T);
    FVirtualSensorStreamConfig C;C.StreamKind=EVirtualSensorStreamKind::PointCloud;C.SensorId=TEXT("rate-test");C.bEnabled=true;P->ConfigureStream(C);
    auto& R=FPcdRateControlTestAccess::Runtime(P);
    TestTrue(TEXT("admission adaptation defaults on"),P->bAutoAdjustPointCloudSendRate);
    if(Case==TEXT("ConsoleThrottle"))
    {
        for(int32 I=0;I<10;++I)FPcdRateControlTestAccess::Log(P,100+I*.01);
        TestEqual(TEXT("only one same-stream transition per second reaches Output Log"),R.Status.SuppressedConsoleLogCount,int64(9));
        FPcdRateControlTestAccess::Log(P,101.01);TestEqual(TEXT("next log summarizes omissions"),R.PendingPcdConsoleLogs,int64(0));
    }
    else if(Case==TEXT("FrameAndBytePressure"))
    {
        R.Status.RawOutstandingFrameCount=5;FPcdRateControlTestAccess::Update(P,100);
        TestTrue(TEXT("25 percent frame cap acts before 64MiB fills"),R.AdmissionRatio<1);
        R.AdmissionRatio=1;R.PreviousPressure=0;R.Status.RawOutstandingFrameCount=0;R.Status.RawOutstandingBytes=1200;
        FVirtualSensorHighThroughputProfile Limits;Limits.MaxOutstandingBytesPerStream=4000;FPcdRateControlTestAccess::Update(P,101,Limits);
        TestTrue(TEXT("actual configured byte budget also constrains admission"),R.AdmissionRatio<1);
        R.AdmissionRatio=1;R.PreviousPressure=0;R.Status.RawOutstandingBytes=0;R.Status.RawGlobalOutstandingBytes=1500;Limits.MaxOutstandingBytes=4000;
        FPcdRateControlTestAccess::Update(P,102,Limits);TestTrue(TEXT("other streams global byte pressure included"),R.AdmissionRatio<1);
    }
    else if(Case==TEXT("Recovery"))
    {
        R.Status.RawOutstandingFrameCount=8;
        for(int32 I=0;I<20;++I)FPcdRateControlTestAccess::Update(P,100+I*.25);
        TestTrue(TEXT("congested ratio is bounded and reduced"),R.AdmissionRatio>=.02&&R.AdmissionRatio<.5);
        R.Status.RawOutstandingFrameCount=0;
        for(int32 I=0;I<60;++I)FPcdRateControlTestAccess::Update(P,110+I*.25);
        TestEqual(TEXT("drain returns original input admission"),R.AdmissionRatio,1.0);
        TestFalse(TEXT("normal state clears rate caption"),R.Status.bRateControlActive);
    }
    else
    {
        R.bSerializationInFlight=true;R.AdmissionRatio=.5;
        if(Case==TEXT("Disabled"))P->bAutoAdjustPointCloudSendRate=false;
        TArray<FVirtualLidarPoint> Points;Points.SetNum(1);Points[0].bHit=true;
        const auto Snapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>(MoveTemp(Points));
        auto Submit=[&](int64 Id,double Period){FVirtualSensorFrameEnvelope F;F.SensorId=C.SensorId;F.SensorKind=EVirtualSensorKind::Lidar;F.FrameId=Id;F.PointSnapshot=Snapshot;F.SlabContext.RunId=TEXT("explicit-omission-run");F.RequestedAcquisitionIntervalSeconds=Period;P->SubmitFrame(F);};
        for(int32 I=1;I<=10;++I)Submit(I,.05);
        if(Case==TEXT("Disabled"))
        {TestEqual(TEXT("off restores all frames"),R.PendingFrameQueue.Num(),10);TestEqual(TEXT("off records no admission omission"),R.Status.RateControlSkippedFrameCount,int64(0));}
        else
        {
            TestEqual(TEXT("half-rate retains five, without replacing accepted data"),R.PendingFrameQueue.Num(),5);
            TestEqual(TEXT("omissions counted separately"),R.Status.RateControlSkippedFrameCount,int64(5));
            TestEqual(TEXT("NoLoss Slab omissions are failures, never successful delivery"),P->GetFailedSlabRunCount(TEXT("explicit-omission-run")),int64(5));
            for(int32 I=0;I<R.PendingFrameQueue.Num();++I)TestEqual(TEXT("uniform every second frame admission"),R.PendingFrameQueue[I].FrameId,int64((I+1)*2));
            TestEqual(TEXT("accepted FIFO not silently replaced"),R.Status.ReplacedPendingFrameCount,int64(0));
            TestTrue(TEXT("omission does not suspend stream"),R.Config.bEnabled&&!R.Status.bOverloadSuspended);
            if(Case==TEXT("SlowerInterval"))
            {
                for(int32 I=11;I<=16;++I)Submit(I,.1);
                TestEqual(TEXT("0.1 immediately uses spare capacity with no lingering deadline"),R.Status.RateControlSkippedFrameCount,int64(5));
                TestEqual(TEXT("slower setting returns admission ratio"),R.AdmissionRatio,1.0);
                Submit(17,.05);R.AdmissionRatio=.85;
                for(int32 I=18;I<=23;++I)Submit(I,.06);
                TestEqual(TEXT("0.06 also restores full admission immediately after congested 0.05"),R.Status.RateControlSkippedFrameCount,int64(5));
                TestEqual(TEXT("trusted small interval increase is not treated as callback jitter"),R.AdmissionRatio,1.0);
            }
        }
    }
    TestTrue(TEXT("rate caption states throughput matching"),SensorToolUiText::PcdRateControlled(17.5f).ToString().Contains(TEXT("서버 처리량에 맞춤")));
    return true;
}
#endif
