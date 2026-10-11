#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"

namespace
{
bool Coherent(const FVirtualSensorFrameEnvelope& F)
{
    if(!F.HasJsonPayload()||!F.LidarFrameSnapshot.IsValid())return false;
    TSharedPtr<FJsonObject> J;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(*F.JsonPayload),J))return false;
    const auto& P=J->GetObjectField(TEXT("sensorTransform"))->GetArrayField(TEXT("location"));
    const auto L=F.LidarFrameSnapshot->AcquisitionTransform.GetLocation();
    return int64(J->GetNumberField(TEXT("frameId")))==F.FrameId&&J->GetStringField(TEXT("timestampUtc"))==F.TimestampUtc.ToIso8601()&&
        F.FrameId==F.LidarFrameSnapshot->FrameId&&F.SlabContext.SlabFrameNo==F.LidarFrameSnapshot->SlabContext.SlabFrameNo&&
        P.Num()==3&&FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()).Equals(L,.001);
}
struct FDerivedProof {TArray<FVirtualSensorFrameEnvelope> Frames;bool Recording=true;};
class FSlowDerivedCheck:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;TWeakObjectPtr<UVirtualLidarScanComponent> Comp;
    TSharedRef<FDerivedProof> Proof;TFunction<void(int64)> Enqueue;TFunction<int32()> Jobs;
    double At=FPlatformTime::Seconds(),Next=At;int32 Phase=0,PhaseStartCount=0;int64 Frame=0;
public:
    FSlowDerivedCheck(FAutomationTestBase* T,UVirtualLidarScanComponent* C,TSharedRef<FDerivedProof> P,TFunction<void(int64)> E,TFunction<int32()> J):Test(T),Comp(C),Proof(P),Enqueue(MoveTemp(E)),Jobs(MoveTemp(J)){}
    bool Update() override
    {
        if(!Comp.IsValid()){Test->AddError(TEXT("derived fixture sensor deleted prematurely"));return true;}
        const double Now=FPlatformTime::Seconds();
        if(Phase<2)
        {
            Comp->JsonEncodeDelayMsForTests=Phase==0?200:700;
            if(Now>=Next){Enqueue(++Frame);Next=Now+.05;}
            Test->TestTrue(TEXT("one active plus one latest pending at most"),Jobs()<=2);
            if(Now-At<1.6)return false;
            Test->TestTrue(TEXT("slow encoder still publishes coherent completed results while acquisition advances"),Proof->Frames.Num()>PhaseStartCount);
            for(const auto& F:Proof->Frames){Test->TestTrue(TEXT("JSON, UTC, pose and slab context remain captured together"),Coherent(F));auto Bad=F;++Bad.FrameId;Test->TestFalse(TEXT("old JSON/new metadata negative control rejected"),Coherent(Bad));}
            Test->AddInfo(FString::Printf(TEXT("delay=%dms delivered=%d replacements=%d"),Comp->JsonEncodeDelayMsForTests,Proof->Frames.Num()-PhaseStartCount,Comp->GetRuntimeStatus().CompatibilityPendingReplacementCount));
            if(++Phase==1){Comp->StopScan();PhaseStartCount=Proof->Frames.Num();At=Now;Next=Now;return false;}
            Enqueue(++Frame);Comp->StopScan();PhaseStartCount=Proof->Frames.Num();At=Now;return false;
        }
        if(Now-At<.9)return false;
        Test->TestEqual(TEXT("stopped generation cannot publish late result"),Proof->Frames.Num(),PhaseStartCount);
        Test->TestEqual(TEXT("stop clears owned pending work"),Jobs(),0);
        Test->TestTrue(TEXT("invalid generation distinct from pending replacement"),Comp->GetRuntimeStatus().DiscardedDerivedGenerationCount>0);
        Proof->Recording=false;Comp->GetOwner()->Destroy();return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarDerivedProgressTest,"MA0T10.LidarOutput.SlowDerivedCoherence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarDerivedProgressTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* A=W->SpawnActor<AVirtualLidarSensorActor>();auto* C=A->ScanComponent.Get();C->StopScan();
    auto P=MakeShared<FDerivedProof>();C->OnDerivedFrameCompletedNative.AddLambda([P](const FVirtualSensorFrameEnvelope& F){if(P->Recording)P->Frames.Add(F);});
    TWeakObjectPtr<UVirtualLidarScanComponent> Weak(C);
    auto Enqueue=[Weak](int64 Id)
    {
        if(!Weak.IsValid())return;auto* S=Weak.Get();auto F=MakeShared<FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe>();
        auto Points=MakeShared<TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>();auto& Point=Points->AddDefaulted_GetRef();Point.bHit=true;Point.WorldLocation=FVector(Id,0,100);Point.Distance=100;
        F->Points=Points;F->FrameId=Id;F->HorizontalSamples=1;F->VerticalChannels=1;F->MaxDistanceCm=500;F->AcquisitionTransform=FTransform(FVector(Id*10,20,30));
        const FDateTime Utc=FDateTime::UtcNow();F->AcquisitionStartUnixNanoseconds=(Utc-FDateTime(1970,1,1)).GetTicks()*100;
        F->SlabContext.RunId=TEXT("derived-proof");F->SlabContext.MtlNo=TEXT("test-mtl");F->SlabContext.SlabFrameNo=Id*5;
        S->LastFrameSnapshot=F;S->FrameId=Id;S->QueueScheduledPayloadBuild(Id,FPlatformTime::Seconds());
    };
    auto Jobs=[Weak](){return Weak.IsValid()?int32(Weak->ActiveDerivedJob.IsValid())+int32(Weak->PendingDerivedJob.IsValid()):0;};
    ADD_LATENT_AUTOMATION_COMMAND(FSlowDerivedCheck(this,C,P,MoveTemp(Enqueue),MoveTemp(Jobs)));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarDerivedRouteTest,"MA0T10.LidarOutput.NoDuplicateAcquisitionRouting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarDerivedRouteTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* A=W->SpawnActor<AVirtualLidarSensorActor>();A->StopSensor();
    auto* Publisher=NewObject<UVirtualSensorStreamPublisherComponent>(A);auto* Transport=NewObject<UVirtualSensorTransportComponent>(A);Publisher->TransportComponent=Transport;
    auto Profile=Transport->GetTransportProfile();Profile.BrokerUrl=TEXT("tcp://127.0.0.1:61616");Transport->ConfigureTransportProfile(Profile);Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
    Publisher->StartStream(EVirtualSensorStreamKind::PointCloud,A->GetSensorId());Publisher->StartStream(EVirtualSensorStreamKind::LidarPayload,A->GetSensorId());
    FVirtualSensorFrameEnvelope F;F.SensorId=A->GetSensorId();F.SensorKind=EVirtualSensorKind::Lidar;F.FrameId=123;F.JsonPayload=MakeShared<const FString,ESPMode::ThreadSafe>(TEXT("{\"frameId\":123}"));F.bLidarCompatibilityDerived=true;
    Publisher->SubmitFrame(F);for(const auto& S:Publisher->GetStreamStatuses())TestEqual(TEXT("derived completion never re-enters PCD or Raw telemetry"),S.InputFrameCount,int64(0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarCompatibilityDrainTest,"MA0T10.LidarOutput.CompatibilityDrain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarCompatibilityDrainTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* A=W->SpawnActor<AVirtualLidarSensorActor>();A->StopSensor();
    auto* M=W->SpawnActor<AVirtualSensorCoordinator>();auto* S=W->GetSubsystem<UVirtualSensorSlabContextSubsystem>();auto* R=W->GetSubsystem<USlabRunResultsSubsystem>();
    S->Coordinator=M;S->Targets.Add(A->GetSensorId(),A);S->ControlledIds.Add(A->GetSensorId());S->Status.RunId=TEXT("compat-drain");S->Status.State=EVirtualSlabSessionState::Running;S->Status.Outputs.bLidarTelemetry=true;
    FVirtualSensorStreamConfig Config;Config.StreamKind=EVirtualSensorStreamKind::LidarPayload;Config.SensorId=A->GetSensorId();Config.TransportBackend=EVirtualSensorStreamTransportBackend::EngineStompCompatibility;Config.bEnabled=true;M->StreamPublisherComponent->ConfigureStream(Config);
    FVirtualSlabFrameContext C;C.RunId=S->Status.RunId;C.Generation=S->Generation;C.bEligible=true;
    R->BeginRun(C.RunId,TEXT("scenario"),ESlabExecutionPolicy::ObservationAllowed,S->Status.Outputs);
    TestTrue(TEXT("selected compatibility job reserves drain work"),S->BeginCompatibilityDerivation(A->GetSensorId(),1,C));
    TestTrue(TEXT("normal stop enters drain"),S->EndSlabSensorSession(C.RunId,false));S->Tick(0);
    TestEqual(TEXT("session waits for unaccepted derivation"),S->Status.State,EVirtualSlabSessionState::Draining);
    S->EndCompatibilityDerivation(C.RunId,A->GetSensorId(),1);S->Tick(0);TestEqual(TEXT("completion releases bounded drain"),S->CompatibilityDerivations.Num(),0);
    S->Status.State=EVirtualSlabSessionState::Running;S->Status.Outputs.bLidarTelemetry=false;
    TestFalse(TEXT("PCD-only never waits on compatibility JSON"),S->BeginCompatibilityDerivation(A->GetSensorId(),2,C));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWildcardSequenceTest,"MA0T10.LidarOutput.WildcardSensorSequences",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWildcardSequenceTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();
    auto* A=W->SpawnActor<AVirtualLidarSensorActor>(); A->StopSensor();
    auto* P=NewObject<UVirtualSensorStreamPublisherComponent>(A);
    FVirtualSensorStreamConfig C; C.StreamKind=EVirtualSensorStreamKind::LidarPayload;
    C.TransportBackend=EVirtualSensorStreamTransportBackend::EngineStompCompatibility; C.bEnabled=true; P->ConfigureStream(C);
    FVirtualSensorFrameEnvelope F; F.SensorKind=EVirtualSensorKind::Lidar;
    F.JsonPayload=MakeShared<const FString,ESPMode::ThreadSafe>(TEXT("{}"));
    for(int64 Id=1;Id<=3;++Id) for(const FString Sensor:{FString(TEXT("sequence-A")),FString(TEXT("sequence-B"))})
    { F.SensorId=Sensor; F.FrameId=Id; P->SubmitFrame(F); }
    auto S=P->GetStreamStatuses()[0];
    TestEqual(TEXT("identical frame ids from different sensors both accepted"),S.InputFrameCount,int64(6));
    TestEqual(TEXT("interleaved sensor ids create no false gaps"),S.FrameGapCount,int64(0));
    P->SubmitFrame(F); TestEqual(TEXT("same sensor duplicate still suppressed"),P->GetStreamStatuses()[0].InputFrameCount,int64(6));
    F.SensorId=TEXT("sequence-A");F.FrameId=5;P->SubmitFrame(F);
    TestEqual(TEXT("real sensor-local gap recorded"),P->GetStreamStatuses()[0].FrameGapCount,int64(1));
    C.TransportBackend=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;P->ConfigureStream(C);
    FVirtualSensorStreamTelemetry AStats,BStats;AStats.SensorId=TEXT("sequence-A");BStats.SensorId=TEXT("sequence-B");
    AStats.SubmittedCount=11;BStats.SubmittedCount=13;AStats.ReceiptCount=11;BStats.ReceiptCount=12;
    AStats.OutstandingBytes=4;BStats.OutstandingBytes=8;AStats.GlobalOutstandingBytes=BStats.GlobalOutstandingBytes=12;
    AStats.SubmittedHz=20;BStats.SubmittedHz=19;AStats.EndToEndP95LatencyMs=25;BStats.EndToEndP95LatencyMs=40;
    P->MergeHighThroughputTelemetry({AStats,BStats});S=P->GetStreamStatuses()[0];
    TestEqual(TEXT("wildcard telemetry sums both sensor submissions"),S.SubmittedFrameCount,int64(24));
    TestEqual(TEXT("wildcard telemetry sums receipts"),S.ReceiptReceivedCount,int64(23));
    TestEqual(TEXT("stream bytes sum, global bytes do not double count"),S.RawOutstandingBytes,int64(12));
    TestEqual(TEXT("shared global byte total remains unchanged"),S.RawGlobalOutstandingBytes,int64(12));
    TestEqual(TEXT("wildcard rate is aggregate"),S.SubmittedHz,39.f);
    TestEqual(TEXT("wildcard p95 is conservative worst sensor"),S.EndToEndP95LatencyMs,40.f);
    P->MergeHighThroughputTelemetry({AStats,BStats});TestEqual(TEXT("telemetry refresh does not accumulate previous refresh"),P->GetStreamStatuses()[0].SubmittedFrameCount,int64(24));
    return true;
}
namespace
{
class FDerivedGlobalLimitCheck:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;TFunction<int32()> Active;TFunction<void()> Pump,Cleanup;
    TSharedRef<int32> Completed;double Started=FPlatformTime::Seconds();bool Stopped=false;int32 Frozen=0;
public:
    FDerivedGlobalLimitCheck(FAutomationTestBase* T,TFunction<int32()> A,TFunction<void()> P,TFunction<void()> C,TSharedRef<int32> Count)
        :Test(T),Active(MoveTemp(A)),Pump(MoveTemp(P)),Cleanup(MoveTemp(C)),Completed(Count){}
    bool Update() override
    {
        if(!Stopped)
        {
            Test->TestTrue(TEXT("global JSON workers never exceed four"),Active()<=4);Pump();
            if(FPlatformTime::Seconds()-Started<2)return false;
            Test->TestEqual(TEXT("fifth sensor progresses after global slot released"),*Completed,5);
            Cleanup();Frozen=*Completed;Stopped=true;Started=FPlatformTime::Seconds();return false;
        }
        if(FPlatformTime::Seconds()-Started<.9)return false;
        Test->TestEqual(TEXT("deleted sensors do not apply late callbacks"),*Completed,Frozen);return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarDerivedGlobalLimitTest,"MA0T10.LidarOutput.GlobalJobLimit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarDerivedGlobalLimitTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();TArray<TWeakObjectPtr<UVirtualLidarScanComponent>> Sensors;
    auto Count=MakeShared<int32>(0);
    for(int32 I=0;I<5;++I)
    {
        auto* A=W->SpawnActor<AVirtualLidarSensorActor>();auto* C=A->ScanComponent.Get();C->StopScan();C->JsonEncodeDelayMsForTests=200;
        C->SensorId=FString::Printf(TEXT("global-json-%d"),I);C->OnDerivedFrameCompletedNative.AddLambda([Count](const auto&){++*Count;});
        auto F=MakeShared<FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe>();F->FrameId=1;F->Points=MakeShared<TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>();F->HorizontalSamples=1;F->VerticalChannels=1;
        F->AcquisitionStartUnixNanoseconds=(FDateTime::UtcNow()-FDateTime(1970,1,1)).GetTicks()*100;
        C->LastFrameSnapshot=F;C->FrameId=1;C->QueueScheduledPayloadBuild(1,FPlatformTime::Seconds());Sensors.Add(C);
    }
    auto Active=[Sensors](){int32 N=0;for(auto C:Sensors)if(C.IsValid())N+=C->ActiveDerivedJob.IsValid();return N;};
    TestEqual(TEXT("fifth initial request remains pending"),Active(),4);
    auto Pump=[Sensors](){for(auto C:Sensors)if(C.IsValid())C->TryStartPendingPayloadBuild();};
    auto Cleanup=[Sensors](){for(auto C:Sensors)if(C.IsValid())
    {
        auto F=MakeShared<FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe>(*C->LastFrameSnapshot);F->FrameId=2;
        C->LastFrameSnapshot=F;C->FrameId=2;C->QueueScheduledPayloadBuild(2,FPlatformTime::Seconds());C->StopScan();C->GetOwner()->Destroy();
    }};
    ADD_LATENT_AUTOMATION_COMMAND(FDerivedGlobalLimitCheck(this,MoveTemp(Active),MoveTemp(Pump),MoveTemp(Cleanup),Count));return true;
}
#endif
