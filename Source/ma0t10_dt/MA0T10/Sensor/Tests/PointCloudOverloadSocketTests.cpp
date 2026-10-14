#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/PlatformProcess.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointCloudOverloadSocketTest, "MA0T10.SensorStream.PointCloudAutoResume.SocketRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPointCloudOverloadSocketTest::RunTest(const FString&)
{
    if (FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD20HZ_CASE")) != TEXT("overload-resume"))
    { AddInfo(TEXT("[OPT-IN SKIP] MA0T10_PCD20HZ_CASE=overload-resume and local STOMP fault harness required")); return true; }
    const double Duration = FMath::Clamp(FCString::Atod(*FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PCD20HZ_SECONDS"))), 35.0, 120.0);
    auto* World = FAutomationEditorCommonUtils::CreateNewMap();
    auto* Coordinator = World->SpawnActor<AVirtualSensorCoordinator>();
    auto* Lidar = World->SpawnActor<AVirtualLidarSensorActor>();
    Coordinator->RegisterSensorActor(Lidar);
    auto* P = Coordinator->StreamPublisherComponent.Get();
    auto* T = Coordinator->SharedTransportComponent.Get();
    auto* Raw = World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
    FVirtualSensorTransportProfile Profile;
    Profile.BrokerUrl = TEXT("tcp://127.0.0.1:61618");
    T->TransportMode = EVirtualSensorTransportMode::StompWebSocket; T->ConfigureTransportProfile(Profile);
    P->SetTransportComponent(T);
    FVirtualSensorReceiveSelection Selection; Selection.bEnabled = false; Raw->ConfigureReceiver(Selection);
    TestTrue(TEXT("actual shared worker starts"), Raw->StartHighThroughputTransport(Raw->MakeProfile(Profile), FString()));
    FVirtualSensorStreamConfig C; C.StreamKind = EVirtualSensorStreamKind::PointCloud;
    C.SensorId = Lidar->GetSensorId(); C.bEnabled = true;
    C.TransportBackend = EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;
    P->ConfigureStream(C);
    TArray<FVirtualLidarPoint> Points; Points.SetNum(576 * 56);
    for (int32 I = 0; I < Points.Num(); ++I)
    {
        auto& Point = Points[I]; Point.bHit = true; Point.Validity = EVirtualLidarPointValidity::Valid;
        Point.SensorLocalPositionMeters = FVector(1, I % 576 * .001, I / 576 * .001);
        Point.Row = I / 576; Point.Col = I % 576;
    }
    const auto Snapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));
    bool SawSuspended = false, SawResumed = false, SawNewReceipt = false, Connected = false;
    int32 Disconnects = 0; int64 FrameId = 0, Boundary = 0;
    const auto Observation = Raw->OnTransportObservation.AddLambda([&](const FVirtualSensorTransportObservation& O)
    {
        if (SawResumed && O.Phase == EVirtualSensorTransportObservationPhase::Receipt && O.SensorId == C.SensorId && O.FrameId > Boundary)
            SawNewReceipt = true;
    });
    const double Started = FPlatformTime::Seconds(); double Next = Started, Sample = Started;
    while (FPlatformTime::Seconds() - Started < Duration)
    {
        const double Now = FPlatformTime::Seconds();
        FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
        Raw->Tick(.001f); P->PumpPublisherOnce(Now);
        const bool Current = Raw->IsTransportConnected();
        if (Connected && !Current) ++Disconnects;
        Connected = Current;
        if (Current && Now >= Next)
        {
            Next = Now + .05;
            FVirtualSensorFrameEnvelope F; F.SensorId = C.SensorId; F.SensorKind = EVirtualSensorKind::Lidar;
            F.FrameId = ++FrameId; F.TimestampUtc = FDateTime::UtcNow(); F.PointSnapshot = Snapshot;
            P->SubmitFrame(F);
        }
        for (const auto& S : P->GetStreamStatuses()) if (S.SensorId == C.SensorId)
        {
            if (S.bOverloadSuspended) { SawSuspended = true; Boundary = S.LastInputFrameId; }
            if (SawSuspended && S.ResumeCount > 0 && S.bEnabled) SawResumed = true;
            if (Now >= Sample)
            {
                Sample = Now + 1;
                UE_LOG(LogTemp, Display, TEXT("[PCD-RESUME-SAMPLE] elapsed=%.2f enabled=%d suspended=%d resumes=%lld input=%lld submitted=%lld receipts=%lld discarded=%lld rawFrames=%d rawBytes=%lld newReceipt=%d"),
                    Now - Started, S.bEnabled, S.bOverloadSuspended, S.ResumeCount, S.InputFrameCount, S.SubmittedFrameCount,
                    S.ReceiptReceivedCount, S.OverloadDiscardedFrameCount, S.RawOutstandingFrameCount, S.RawOutstandingBytes, SawNewReceipt);
            }
        }
        FPlatformProcess::SleepNoStats(.001f);
    }
    TestTrue(TEXT("long read pause really suspended publisher at its queue limit"), SawSuspended);
    TestTrue(TEXT("drain automatically resumed publisher"), SawResumed);
    TestTrue(TEXT("a new frame acquired after suspension reached actual Broker receipt"), SawNewReceipt);
    TestEqual(TEXT("backpressure recovery does not replace live connection"), Disconnects, 0);
    Raw->OnTransportObservation.Remove(Observation);
    P->StopAllStreams(FString());
    const double Deadline = FPlatformTime::Seconds() + 5;
    while (FPlatformTime::Seconds() < Deadline && P->GetResourceSnapshot().Counts.FindRef(TEXT("serializing")) > 0)
    { FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread); FPlatformProcess::SleepNoStats(.001f); }
    Raw->StopHighThroughputTransport(); Raw->Tick(.001f);
    TestEqual(TEXT("shutdown releases outstanding bytes"), Raw->GetRunRetentionSnapshot().OutstandingBytes, int64(0));
    Coordinator->Destroy(); Lidar->Destroy();
    return true;
}
#endif
