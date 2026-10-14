#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"

// Socket stress only: immutable synthetic 576x56x33-byte bodies. Actual sensor
// acquisition and golden-wire acceptance are separate from this fault test.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPcd20HzSocketTest,
    "MA0T10.SensorStream.Pcd20Hz.SocketFaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSensorPcd20HzSocketTest::RunTest(const FString&)
{
    const FString Mode=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD20HZ_CASE"));
    if(Mode.IsEmpty()) { AddInfo(TEXT("[OPT-IN SKIP] MA0T10_PCD20HZ_CASE unset; local STOMP fault harness required")); return true; }
    const double Duration=FMath::Clamp(FCString::Atod(*FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD20HZ_SECONDS"))),8.0,600.0);
    const double Interval=FMath::Clamp(FCString::Atod(*FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD20HZ_INTERVAL"))),.05,.2);
    const int32 Count=FMath::Clamp(FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD20HZ_LIDARS"))),1,2);
    auto* Raw=NewObject<UVirtualSensorHighThroughputTransportSubsystem>(); Raw->AddToRoot();
    FVirtualSensorHighThroughputProfile Profile;
    Profile.BrokerUrl=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_RAW_URL"));
    if(Profile.BrokerUrl.IsEmpty()) Profile.BrokerUrl=TEXT("tcp://127.0.0.1:61618");
    Profile.HeartbeatIntervalMs=100;
    FVirtualSensorReceiveSelection Selection; Selection.bEnabled=false; Raw->ConfigureReceiver(Selection);
    TestTrue(TEXT("worker starts"),Raw->StartHighThroughputTransport(Profile,FString()));
    TArray64<uint8> Bytes; Bytes.SetNumZeroed(576*56*33);
    const auto Body=MakeShared<const TArray64<uint8>,ESPMode::ThreadSafe>(MoveTemp(Bytes));
    const double Started=FPlatformTime::Seconds(); double Next=Started,Sample=Started;
    bool Connected=false,SawDelay=false; int32 Connections=0,Disconnects=0; int64 Attempted=0,Rejected=0,FrameId=0;
    while(FPlatformTime::Seconds()-Started<Duration)
    {
        const double Now=FPlatformTime::Seconds(); Raw->Tick(.001f);
        SawDelay|=Raw->IsSendDelayed();
        const bool Current=Raw->IsTransportConnected();
        if(Current!=Connected)
        {
            Connected=Current; if(Current)++Connections;else ++Disconnects;
            UE_LOG(LogTemp,Display,TEXT("[PCD20HZConnection] elapsed=%.3f connected=%d reason=%s"),Now-Started,Current,*Raw->GetConnectionMessage());
        }
        if(Mode!=TEXT("heartbeat") && Mode!=TEXT("silent") && Now>=Next)
        {
            Next=Now+Interval; ++FrameId;
            for(int32 I=0;I<Count;++I)
            {
                FVirtualSensorBinaryFrame Frame; Frame.StreamKind=EVirtualSensorStreamKind::PointCloud;
                Frame.SensorId=FString::Printf(TEXT("PCD20HZ-%d"),I); Frame.FrameId=FrameId;Frame.TimestampUtc=FDateTime::UtcNow();
                Frame.Schema=TEXT("virtual-pointcloud.pcd.v1"); Frame.ContentType=TEXT("application/vnd.pcd");
                Frame.Destination=Profile.PointCloudTopic;Frame.Body64=Body;
                Frame.RequestId=Frame.SensorId+TEXT("-")+LexToString(FrameId);
                FString Error; ++Attempted; if(!Raw->EnqueueBinaryFrame(Frame,Error))++Rejected;
            }
        }
        if(Now>=Sample)
        {
            Sample=Now+1;int64 Submitted=0,Receipts=0,SubmittedBytes=0,Overload=0,Failures=0,Outstanding=0;
            for(const auto& T:Raw->GetStreamTelemetry()){Submitted+=T.SubmittedCount;Receipts+=T.ReceiptCount;SubmittedBytes+=T.SubmittedBytes;Overload+=T.OverloadCount;Failures+=T.DeliveryFailureCount;Outstanding+=T.OutstandingBytes;}
            UE_LOG(LogTemp,Display,TEXT("[PCD20HZSample] elapsed=%.3f connected=%d connections=%d disconnects=%d attempted=%lld rejected=%lld submitted=%lld receipt=%lld bytes=%lld overload=%lld failed=%lld outstanding=%lld"),Now-Started,Connected,Connections,Disconnects,Attempted,Rejected,Submitted,Receipts,SubmittedBytes,Overload,Failures,Outstanding);
        }
        FPlatformProcess::SleepNoStats(.001f);
    }
    TestTrue(TEXT("at least one STOMP connection"),Connections>0);
    if(Mode==TEXT("healthy")||Mode==TEXT("slow")||Mode==TEXT("heartbeat"))TestEqual(TEXT("healthy/backpressured transport does not disconnect"),Disconnects,0);
    if(Mode==TEXT("healthy"))
    {
        int64 Receipts=0;for(const auto& T:Raw->GetStreamTelemetry())Receipts+=T.ReceiptCount;
        TestEqual(TEXT("healthy socket load skips no frame"),Rejected,int64(0));
        TestTrue(TEXT("healthy socket load actually delivered at least 90 percent of offered frames"),Receipts>=Attempted*.9);
    }
    if(Mode==TEXT("slow")){TestTrue(TEXT("backpressure skips new frames at existing caps"),Rejected>0);TestTrue(TEXT("write delay is reported without disconnect"),SawDelay);}
    if(Mode==TEXT("error")||Mode==TEXT("reset")||Mode==TEXT("silent")) {TestTrue(TEXT("fault reported"),Disconnects>0);TestTrue(TEXT("transport reconnects"),Connections>1);}
    Raw->StopHighThroughputTransport();Raw->Tick(.001f);
    TestEqual(TEXT("stop releases all outstanding reservations"),Raw->GetRunRetentionSnapshot().OutstandingBytes,int64(0));
    Raw->RemoveFromRoot(); return true;
}
#endif
