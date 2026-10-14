#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"

// Drive the production transition with a deterministic transport readiness input.
// Actual socket reservations and connection recovery are covered by SocketRecovery.
struct FPointCloudOverloadTestAccess
{
    using FRuntime = UVirtualSensorStreamPublisherComponent::FStreamRuntime;
    using FMessage = UVirtualSensorStreamPublisherComponent::FPreparedMessage;
    static FRuntime& Runtime(UVirtualSensorStreamPublisherComponent* P)
    { return P->FindOrAddRuntime(EVirtualSensorStreamKind::PointCloud, TEXT("PCD-RESUME")); }
    static FString Key(UVirtualSensorStreamPublisherComponent* P)
    { return P->MakeStreamKey(EVirtualSensorStreamKind::PointCloud, TEXT("PCD-RESUME")); }
    static void Suspend(UVirtualSensorStreamPublisherComponent* P)
    { P->StopForPointCloudOverload(Key(P), Runtime(P), TEXT("test queue limit")); }
    static bool Recover(UVirtualSensorStreamPublisherComponent* P, double Now, bool Ready)
    { return P->UpdatePointCloudOverloadRecovery(Key(P), Runtime(P), Now, Ready); }
    static void Progress(UVirtualSensorStreamPublisherComponent* P, double Now, bool Connected)
    { P->UpdatePointCloudDeliveryProgress(Runtime(P), Now, Connected); }
    static bool HasRecoveryWork(UVirtualSensorStreamPublisherComponent* P) { return P->bHasPointCloudRecoveryWork; }
    static void CompleteOld(UVirtualSensorStreamPublisherComponent* P, int32 Revision)
    {
        UVirtualSensorStreamPublisherComponent::FPreparedMessage M;
        M.SensorId = TEXT("PCD-RESUME"); M.FrameId = 10; M.SlabContext.RunId = TEXT("discard-run");
        M.Json = TEXT("old-result");
        P->CompletePointCloudSerialization(Key(P), MoveTemp(M), FString(), Revision);
    }
    static void MergeOldTelemetry(UVirtualSensorStreamPublisherComponent* P)
    {
        FVirtualSensorStreamTelemetry T; T.StreamKind = EVirtualSensorStreamKind::PointCloud;
        T.SensorId = TEXT("PCD-RESUME"); T.Message = TEXT("historical receipt");
        P->MergeHighThroughputTelemetry({T});
    }
};

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPointCloudOverloadResumeTest, "MA0T10.SensorStream.PointCloudAutoResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FPointCloudOverloadResumeTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const TCHAR* Name : {TEXT("Cooldown"), TEXT("BackoffAndReset"), TEXT("UserStop"),
        TEXT("Reconnect"), TEXT("Disabled"), TEXT("GenerationAndAccounting"), TEXT("RoutingAndStatus")})
    { Names.Add(Name); Commands.Add(Name); }
}
bool FPointCloudOverloadResumeTest::RunTest(const FString& Case)
{
    using A = FPointCloudOverloadTestAccess;
    auto* P = NewObject<UVirtualSensorStreamPublisherComponent>();
    FVirtualSensorStreamConfig C; C.StreamKind = EVirtualSensorStreamKind::PointCloud;
    C.SensorId = TEXT("PCD-RESUME"); C.bEnabled = true;
    C.TransportBackend = EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;
    P->ConfigureStream(C);
    auto& R = A::Runtime(P);
    TestFalse(TEXT("normal streams do not schedule recovery polling"), A::HasRecoveryWork(P));
    if (Case == TEXT("Disabled")) P->bAutoResumePointCloudAfterOverload = false;
    if (Case == TEXT("GenerationAndAccounting"))
    {
        FVirtualSensorFrameEnvelope F; F.SensorId = C.SensorId; F.FrameId = 11;
        F.SlabContext.RunId = TEXT("discard-run");
        R.PendingFrameQueue.Add(F); R.LastInputSlabContext = F.SlabContext; R.LastInputSensorId = F.SensorId;
        R.Status.LastInputFrameId = 12; R.SerializationRunId = F.SlabContext.RunId;
        R.SerializationSensorId = F.SensorId; R.SerializationFrameId = 10; R.bSerializationInFlight = true;
    }
    const int32 OldRevision = R.ConfigRevision;
    A::Suspend(P);
    const double Due = R.OverloadResumeAtSeconds;
    if (Case == TEXT("Cooldown"))
    {
        TestTrue(TEXT("auto resume defaults on"), R.Status.bOverloadSuspended);
        TestTrue(TEXT("only overload schedules recovery polling"), A::HasRecoveryWork(P));
        TestEqual(TEXT("first cooldown is two seconds"), R.Status.OverloadResumeDelaySeconds, 2.0f);
        TestTrue(TEXT("stop remains available while suspended"), P->IsStreamActiveOrAwaitingResume(C.StreamKind, C.SensorId));
        TestFalse(TEXT("before cooldown remains stopped"), A::Recover(P, Due - .01, true));
        TestTrue(TEXT("cooldown plus drained connected path resumes"), A::Recover(P, Due, true));
        TestTrue(TEXT("resume enables transmission"), R.Config.bEnabled);
        TestFalse(TEXT("current overload clears"), R.Status.bOverloaded);
        TestEqual(TEXT("resume counted once"), R.Status.ResumeCount, int64(1));
        TestFalse(TEXT("second tick cannot resume again"), A::Recover(P, Due + 1, true));
    }
    else if (Case == TEXT("BackoffAndReset"))
    {
        for (double Delay : {2.0, 4.0, 8.0, 16.0, 30.0, 30.0})
        {
            TestEqual(TEXT("exponential backoff and cap"), R.OverloadBackoffSeconds, Delay);
            TestTrue(TEXT("each eligible cycle resumes"), A::Recover(P, R.OverloadResumeAtSeconds, true));
            A::Suspend(P);
        }
        A::Recover(P, R.OverloadResumeAtSeconds, true);
        const double Start = 1000.0;
        A::Progress(P, Start, true); A::Progress(P, Start + 40, true);
        TestEqual(TEXT("idle connected time cannot reset backoff"), R.OverloadBackoffSeconds, 30.0);
        for (int32 I = 0; I <= 30; ++I) { ++R.Status.ReceiptReceivedCount; A::Progress(P, Start + 50 + I, true); }
        TestEqual(TEXT("30 seconds of advancing receipts resets backoff"), R.OverloadBackoffSeconds, 0.0);
        A::Suspend(P); TestEqual(TEXT("reset returns to initial delay"), R.OverloadBackoffSeconds, 2.0);
        P->PointCloudOverloadMaxResumeDelaySeconds = 3;
        A::Recover(P, R.OverloadResumeAtSeconds, true); A::Suspend(P);
        TestEqual(TEXT("public maximum also caps exponential delay"), R.OverloadBackoffSeconds, 3.0);
    }
    else if (Case == TEXT("UserStop"))
    {
        P->StopStream(C.StreamKind, C.SensorId);
        TestFalse(TEXT("user stop cancels reservation"), A::Recover(P, Due + 60, true));
        TestFalse(TEXT("user stop stays disabled"), R.Config.bEnabled);
        P->PumpPublisherOnce(Due + 60);
        TestFalse(TEXT("last user stop retires idle recovery polling"), A::HasRecoveryWork(P));
        P->StartStream(C.StreamKind, C.SensorId); A::Suspend(P); P->StopAllStreams(FString());
        TestFalse(TEXT("global stop also cancels"), A::Recover(P, R.OverloadResumeAtSeconds + 60, true));
        P->StartStream(C.StreamKind, C.SensorId); A::Suspend(P); C.bEnabled = false; P->ConfigureStream(C);
        TestFalse(TEXT("configuration disable cancels"), A::Recover(P, Due + 60, true));
        P->StartStream(C.StreamKind, C.SensorId); A::Suspend(P); C.bEnabled = true; P->ConfigureStream(C);
        TestFalse(TEXT("explicit enabled reconfiguration also removes reservation"), R.Status.bOverloadSuspended);
        TestFalse(TEXT("reconfigured stream does not retain overload blocker"), R.Status.bOverloaded);
        FVirtualSensorFrameEnvelope Pending; Pending.SensorId = C.SensorId; Pending.FrameId = 21;
        Pending.SlabContext.RunId = TEXT("configuration-disable"); R.PendingFrameQueue.Add(Pending);
        A::FMessage Prepared; Prepared.SensorId = C.SensorId; Prepared.FrameId = 22;
        Prepared.SlabContext.RunId = Pending.SlabContext.RunId; Prepared.Json = TEXT("prepared");
        R.PreparedMessageQueue.Add(Prepared);
        C.bEnabled = false; P->ConfigureStream(C);
        TestTrue(TEXT("UI configuration disable releases pending and prepared queues"), R.PendingFrameQueue.IsEmpty() && R.PreparedMessageQueue.IsEmpty());
        TestEqual(TEXT("configuration disable records retained slab output failures"), P->GetFailedSlabRunCount(Pending.SlabContext.RunId), int64(2));
    }
    else if (Case == TEXT("Reconnect"))
    {
        TestFalse(TEXT("expired cooldown cannot override disconnect or undrained path"), A::Recover(P, Due + 60, false));
        TestTrue(TEXT("recovery remains scheduled"), R.Status.bOverloadSuspended);
        TestTrue(TEXT("connection recovery plus drain resumes without new cooldown"), A::Recover(P, Due + 61, true));
    }
    else if (Case == TEXT("Disabled"))
    {
        TestFalse(TEXT("option off preserves permanent stop"), R.Status.bOverloadSuspended);
        TestFalse(TEXT("option off never resumes"), A::Recover(P, Due + 60, true));
        TestFalse(TEXT("legacy disabled config preserved"), R.Config.bEnabled);
        P->bAutoResumePointCloudAfterOverload = true;
        TestFalse(TEXT("enabling option does not resurrect legacy stopped stream"), A::Recover(P, Due + 61, true));
        P->StartStream(C.StreamKind, C.SensorId); A::Suspend(P); P->bAutoResumePointCloudAfterOverload = false;
        A::Recover(P, R.OverloadResumeAtSeconds, true);
        TestFalse(TEXT("turning option off during cooldown cancels scheduled resume"), R.Status.bOverloadSuspended);
    }
    else if (Case == TEXT("GenerationAndAccounting"))
    {
        TestEqual(TEXT("inflight, queued and rejected frames fail slab run explicitly"), P->GetFailedSlabRunCount(TEXT("discard-run")), int64(3));
        TestEqual(TEXT("overload discard count is explicit"), R.Status.OverloadDiscardedFrameCount, int64(3));
        TestFalse(TEXT("inflight old serialization prevents recovery"), A::Recover(P, Due + 1, true));
        A::CompleteOld(P, OldRevision);
        TestTrue(TEXT("after old worker finishes recovery permitted"), A::Recover(P, Due + 2, true));
        A::CompleteOld(P, OldRevision);
        TestTrue(TEXT("old generation never repopulates queue after resume"), R.PreparedMessageQueue.IsEmpty());
        TestEqual(TEXT("old result rejected by revision"), R.Status.StaleResultDiscardCount, int64(1));
    }
    else if (Case == TEXT("RoutingAndStatus"))
    {
        FVirtualSensorStreamConfig Wildcard = C; Wildcard.SensorId.Reset(); P->ConfigureStream(Wildcard);
        FVirtualSensorStreamConfig Out;
        TestFalse(TEXT("wildcard cannot bypass exact overload suspension"), P->ResolveActiveStreamForSensor(C.StreamKind, C.SensorId, Out));
        A::MergeOldTelemetry(P);
        TestTrue(TEXT("historical worker text cannot overwrite current suspension"), A::Runtime(P).Status.Message.Contains(TEXT("overload suspended")));
        TestEqual(TEXT("overload vocabulary distinct from connection settings"), SensorToolUiText::PcdOverloadSuspended().ToString(), FString(TEXT("PCD 과부하 · 잠시 후 자동 재개")));
        TestFalse(TEXT("resume vocabulary has no connection configuration warning"), SensorToolUiText::PcdOverloadSuspended().ToString().Contains(TEXT("연결 설정 필요")));
    }
    return true;
}
#endif
