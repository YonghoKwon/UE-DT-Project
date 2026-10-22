#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Misc/SecureHash.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformProcess.h"

struct FPcdFairBudgetTestAccess
{
    static auto& Runtime(UVirtualSensorStreamPublisherComponent* P,const FString& Id)
    {return P->FindOrAddRuntime(EVirtualSensorStreamKind::PointCloud,Id);}
    static void Update(UVirtualSensorStreamPublisherComponent* P,double Now){P->UpdatePointCloudFairBudgets(Now);}
    static void Queue(UVirtualSensorStreamPublisherComponent* P,const FString& Route,const FString& Id,int64 FrameId,TArray<uint8>&& Bytes,int32 Points,FDateTime Timestamp)
    {
        auto& R=Runtime(P,Route);auto& M=R.PreparedMessageQueue.AddDefaulted_GetRef();
        M.SensorId=Id;M.FrameId=FrameId;M.StreamKind=EVirtualSensorStreamKind::PointCloud;M.TimestampUtc=Timestamp;
        M.bBinaryPcd=true;M.ByteCount=Bytes.Num();M.ConfigRevision=R.ConfigRevision;
        auto& Meta=M.BinaryMetadata;Meta.SensorId=Id;Meta.FrameId=FrameId;Meta.TimestampUtc=M.TimestampUtc.ToIso8601();
        Meta.ByteCount=M.ByteCount;Meta.PointCount=Meta.SourcePointCount=Points;
        uint8 Hash[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash);Meta.ChecksumSha1=BytesToHex(Hash,FSHA1::DigestSize).ToLower();
        M.BinaryBody=MakeShared<const TArray<uint8>,ESPMode::ThreadSafe>(MoveTemp(Bytes));
    }
    static void Prime(UVirtualSensorStreamPublisherComponent* P,double Now)
    {P->PointCloudTokenBucketBytes=P->PointCloudBandwidthLimitMegabytesPerSecond*1048576.;P->LastTokenUpdateSeconds=Now;}
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
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPcdFifoBodyShrinkTest,"MA0T10.SensorStream.PcdFifoBodyShrink",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FPcdFifoBodyShrinkTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands)const
{for(const TCHAR* C:{TEXT("Exact"),TEXT("Wildcard")}){Names.Add(C);Commands.Add(C);}}
bool FPcdFifoBodyShrinkTest::RunTest(const FString& Case)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* C=W->SpawnActor<AVirtualSensorCoordinator>();C->Tags.Add(TEXT("SensorTestManaged"));
    auto* P=C->StreamPublisherComponent.Get();auto* T=C->SharedTransportComponent.Get();auto* Raw=W->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
    int32 Port=1;const bool SocketProof=FParse::Value(FCommandLine::Get(),TEXT("PcdFifoRegressionPort="),Port);
    if(SocketProof&&!TestTrue(TEXT("regression broker is unprivileged loopback"),Port>=1024&&Port<=65535)){C->Destroy();return false;}
    FVirtualSensorTransportProfile Profile;Profile.BrokerUrl=FString::Printf(TEXT("tcp://127.0.0.1:%d"),Port);Profile.UserName.Reset();
    T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;T->ConfigureTransportProfile(Profile);T->SetSessionCredentials(TEXT(""),TEXT(""));P->SetTransportComponent(T);
    FVirtualSensorReceiveSelection Receive;Raw->ConfigureReceiver(Receive);
    const FString Id=TEXT("FIFO-SHRINK-")+Case,Route=Case==TEXT("Wildcard")?FString():Id;
    FVirtualSensorStreamConfig Config;Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.SensorId=Route;Config.bEnabled=true;Config.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;Config.PcdDataMode=EVirtualPcdDataMode::Binary;P->ConfigureStream(Config);
    const FDateTime Timestamp=FDateTime::UtcNow();auto MakeBody=[&](int32 Count,int64 FrameId)
    {
        TArray<FVirtualLidarPoint> Points;Points.SetNum(Count);for(auto& Point:Points){Point.bHit=true;Point.Validity=EVirtualLidarPointValidity::Valid;Point.SensorLocalPositionMeters=FVector(1,2,3);Point.EchoCount=1;Point.Confidence=.9f;}
        FVirtualSensorFrameEnvelope F;F.SensorId=Id;F.SensorKind=EVirtualSensorKind::Lidar;F.FrameId=FrameId;F.TimestampUtc=Timestamp;F.PointSnapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>(MoveTemp(Points));
        FString Extension,Error;TArray<uint8> Bytes;int32 Encoded=0;
        TestTrue(TEXT("shrink fixture uses real33-byte PCD serialization"),P->SerializePointCloudForTesting(F,Config,Extension,Bytes,Encoded,Error));TestEqual(TEXT("fixture preserves valid points"),Encoded,Count);
        TestEqual(TEXT("fixture is PCD rather than legacy default CSV"),Extension,FString(TEXT("pcd")));TestTrue(TEXT("fixture has33-byte records plus bounded header"),Bytes.Num()>Count*33&&Bytes.Num()<Count*33+1024);return Bytes;
    };
    auto Large=MakeBody(40000,1);const double LargeBytes=Large.Num();auto Small=MakeBody(2000,2);const double SmallBytes=Small.Num();
    FPcdFairBudgetTestAccess::Queue(P,Route,Id,1,MoveTemp(Large),40000,Timestamp);FPcdFairBudgetTestAccess::Queue(P,Route,Id,2,MoveTemp(Small),2000,Timestamp);
    auto& R=FPcdFairBudgetTestAccess::Runtime(P,Route);auto& S=R.SensorSequences.FindOrAdd(Id);const double Started=FPlatformTime::Seconds();
    S.FrameBytes=SmallBytes;S.InputPeriod=.05;S.BandwidthTokens=0;S.BandwidthUpdated=Started;FPcdFairBudgetTestAccess::Prime(P,Started);
    TestTrue(TEXT("latest body shrank about1/20 while large body remains FIFO head"),SmallBytes<LargeBytes/19&&R.PreparedMessageQueue[0].ByteCount==LargeBytes);
    double DrainedAt=0;
    for(int Tick=1;Tick<=180;++Tick)
    {
        const double Now=Started+Tick/60.;S.LastInputSeconds=Now;P->PumpPublisherOnce(Now);
        if(R.PreparedMessageQueue.IsEmpty()){DrainedAt=Tick/60.;break;}
    }
    TestTrue(TEXT("old large head and following small body reach actual sender queue within3seconds"),DrainedAt>0&&DrainedAt<=3);
    int64 Enqueued=0;for(const auto& Row:Raw->GetStreamTelemetry())if(Row.SensorId==Id)Enqueued+=Row.EnqueuedCount;
    TestEqual(TEXT("both FIFO bodies accepted by Raw worker"),Enqueued,int64(2));
    if(SocketProof&&DrainedAt>0)
    {
        // Editor worlds have no GameInstance receiver dispatch. Verify socket
        // writes/receipts here; the local harness independently validates PCD.
        const double Deadline=FPlatformTime::Seconds()+5;int64 Submitted=0,Receipts=0;
        while(FPlatformTime::Seconds()<Deadline)
        {
            Raw->Tick(.001f);Submitted=Receipts=0;for(const auto& Row:Raw->GetStreamTelemetry())if(Row.SensorId==Id){Submitted+=Row.SubmittedCount;Receipts+=Row.ReceiptCount;}
            if(Submitted==2&&Receipts==2)break;FPlatformProcess::SleepNoStats(.001f);
        }
        TestEqual(TEXT("large and small bodies really written to loopback socket"),Submitted,int64(2));TestEqual(TEXT("both loopback receipts"),Receipts,int64(2));
    }
    TestEqual(TEXT("size shrink creates no false publisher overload"),R.Status.OverloadCount,int64(0));TestEqual(TEXT("no FIFO replacement"),R.Status.ReplacedPendingFrameCount,int64(0));
    for(const auto& Row:Raw->GetStreamTelemetry())TestEqual(TEXT("no false worker overload"),Row.OverloadCount,int64(0));
    AddInfo(FString::Printf(TEXT("%s large=%.0f latest=%.0f FIFO drain bucket-time=%.6fs socketProof=%d"),*Case,LargeBytes,SmallBytes,DrainedAt,SocketProof));
    P->StopAllStreams(FString());Raw->StopHighThroughputTransport();C->Destroy();return true;
}
#endif
