#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

bool UVirtualSensorSchedulerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UVirtualSensorSchedulerSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorSchedulerSubsystem, STATGROUP_Tickables);
}

int32 UVirtualSensorSchedulerSubsystem::ResolveTargetFps(int32 CameraCount, int32 LidarCount)
{
    return CameraCount <= 2 && LidarCount <= 2 ? 60 : 30;
}

float UVirtualSensorSchedulerSubsystem::ResolveLidarBudgetMs(int32 TargetFps)
{
    // The 60-FPS tier keeps the anti-starvation floor introduced for 2+2.
    // Four FullSpec LiDARs need a wider ceiling to sustain 2 Hz each; the
    // rolling frame guard can still contract this toward 2.5 ms.
    return TargetFps <= 30 ? 7.0f : 5.0f;
}

bool UVirtualSensorSchedulerSubsystem::IsBestEffortConfiguration(int32 CameraCount, int32 LidarCount)
{
    return CameraCount > 4 || LidarCount > 4;
}

float UVirtualSensorSchedulerSubsystem::ResolveNominalCameraRatePerSensor(int32 TargetFps, int32 CameraCount)
{
    return CameraCount > 0 ? 12.0f / static_cast<float>(CameraCount) : 0.0f;
}

float UVirtualSensorSchedulerSubsystem::ResolveAdaptiveCameraAdmissionHz(float CurrentHz, float ObservedFrameMs, int32 TargetFps, float MinimumAdmissionHz)
{
    const float TargetFrameMs = 1000.0f / FMath::Max(1, TargetFps);
    const float CeilingHz = 12.0f;
    const float DefaultFloorHz = TargetFps >= 60 ? 4.0f : 2.0f;
    const float FloorHz = MinimumAdmissionHz >= 0.0f
        ? FMath::Clamp(MinimumAdmissionHz, 0.0f, CeilingHz)
        : DefaultFloorHz;
    if (ObservedFrameMs > TargetFrameMs * 1.02f)
    {
        CurrentHz -= 0.25f;
    }
    else if (ObservedFrameMs < TargetFrameMs * 0.90f)
    {
        CurrentHz += 0.05f;
    }
    return FMath::Clamp(CurrentHz, FloorHz, CeilingHz);
}

void UVirtualSensorSchedulerSubsystem::RegisterCamera(UVirtualCameraCaptureComponent* Camera)
{
    if (Camera && !Cameras.Contains(Camera)) Cameras.Add(Camera);
}

void UVirtualSensorSchedulerSubsystem::RegisterTask(UActorComponent* TaskComponent)
{
    if (UVirtualCameraCaptureComponent* Camera = Cast<UVirtualCameraCaptureComponent>(TaskComponent))
    {
        RegisterCamera(Camera);
    }
    else if (UVirtualLidarScanComponent* Lidar = Cast<UVirtualLidarScanComponent>(TaskComponent))
    {
        RegisterLidar(Lidar);
    }
}

void UVirtualSensorSchedulerSubsystem::UnregisterTask(UActorComponent* TaskComponent)
{
    if (UVirtualCameraCaptureComponent* Camera = Cast<UVirtualCameraCaptureComponent>(TaskComponent))
    {
        UnregisterCamera(Camera);
    }
    else if (UVirtualLidarScanComponent* Lidar = Cast<UVirtualLidarScanComponent>(TaskComponent))
    {
        UnregisterLidar(Lidar);
    }
}

void UVirtualSensorSchedulerSubsystem::UnregisterCamera(UVirtualCameraCaptureComponent* Camera)
{
    if (PreferredCamera.Get() == Camera) PreferredCamera.Reset();
    Cameras.RemoveAll([Camera](const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Item) { return !Item.IsValid() || Item.Get() == Camera; });
    NextCameraIndex = Cameras.Num() > 0 ? NextCameraIndex % Cameras.Num() : 0;
}

void UVirtualSensorSchedulerSubsystem::RegisterLidar(UVirtualLidarScanComponent* Lidar)
{
    if (Lidar && !Lidars.Contains(Lidar)) Lidars.Add(Lidar);
}

void UVirtualSensorSchedulerSubsystem::UnregisterLidar(UVirtualLidarScanComponent* Lidar)
{
    if (PreferredLidar.Get() == Lidar) PreferredLidar.Reset();
    Lidars.RemoveAll([Lidar](const TWeakObjectPtr<UVirtualLidarScanComponent>& Item) { return !Item.IsValid() || Item.Get() == Lidar; });
    NextLidarIndex = Lidars.Num() > 0 ? NextLidarIndex % Lidars.Num() : 0;
}

void UVirtualSensorSchedulerSubsystem::SetPreferredCamera(UVirtualCameraCaptureComponent* Camera)
{
    PreferredCamera = Camera;
}

void UVirtualSensorSchedulerSubsystem::SetPreferredLidar(UVirtualLidarScanComponent* Lidar)
{
    PreferredLidar = Lidar;
}

bool UVirtualSensorSchedulerSubsystem::ShouldRefreshLidarPreview(const UVirtualLidarScanComponent* Lidar) const
{
    return !PreferredLidar.IsValid() || PreferredLidar.Get() == Lidar;
}

void UVirtualSensorSchedulerSubsystem::CompactRegistrations()
{
    Cameras.RemoveAll([](const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Item) { return !Item.IsValid(); });
    Lidars.RemoveAll([](const TWeakObjectPtr<UVirtualLidarScanComponent>& Item) { return !Item.IsValid(); });
    NextCameraIndex = Cameras.Num() > 0 ? NextCameraIndex % Cameras.Num() : 0;
    NextLidarIndex = Lidars.Num() > 0 ? NextLidarIndex % Lidars.Num() : 0;
}

void UVirtualSensorSchedulerSubsystem::Tick(float DeltaTime)
{
    ConfigureCommandLineBenchmarkIfRequested();
    CompactRegistrations();
    if (DeltaTime > SMALL_NUMBER && DeltaTime < 1.0f)
    {
        RecentFrameTimesMs.Add(DeltaTime * 1000.0f);
        constexpr int32 MaxFrameSamples = 600;
        if (RecentFrameTimesMs.Num() > MaxFrameSamples)
        {
            RecentFrameTimesMs.RemoveAt(0, RecentFrameTimesMs.Num() - MaxFrameSamples, false);
        }
    }
    const double StartSeconds = FPlatformTime::Seconds();
    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (!bCommandLineBenchmarkStatisticsReset && CommandLineBenchmarkStatisticsResetTime >= 0.0 &&
        NowSeconds >= CommandLineBenchmarkStatisticsResetTime)
    {
        RecentFrameTimesMs.Reset();
        Telemetry.AverageFps = 0.0f;
        Telemetry.OnePercentLowFps = 0.0f;
        Telemetry.P95FrameTimeMs = 0.0f;
        bCommandLineBenchmarkStatisticsReset = true;
        UE_LOG(LogTemp, Display, TEXT("[VirtualSensorPerf] warmup complete; rolling frame statistics reset"));
    }
    const int32 TargetFps = ResolveTargetFps(Cameras.Num(), Lidars.Num());
    const float TargetFrameMs = 1000.0f / FMath::Max(1, TargetFps);
    const float InstantFrameMs = DeltaTime > SMALL_NUMBER ? DeltaTime * 1000.0f : TargetFrameMs;
    const float TailFrameMs = Telemetry.P95FrameTimeMs > 0.0f ? Telemetry.P95FrameTimeMs : InstantFrameMs;
    const float OnePercentFrameMs = Telemetry.OnePercentLowFps > SMALL_NUMBER ? 1000.0f / Telemetry.OnePercentLowFps : InstantFrameMs;
    const float ObservedFrameMs = FMath::Max3(InstantFrameMs, TailFrameMs, OnePercentFrameMs);
    // FullSpec SceneCapture and CPU ray tracing contend for both render and
    // memory bandwidth. Keep the camera-only floor responsive, but allow the
    // mixed Camera+LiDAR tier to shed more stale camera frames before it
    // sacrifices the game-frame target.
    // Keep a small scheduling margin above the evidence threshold. An exact
    // 10 Hz aggregate floor can measure as 4.98-4.99 Hz per camera because of
    // frame-boundary jitter even when no acquisition was skipped.
    const float CameraAdmissionFloorHz = Cameras.Num() > 0
        ? FMath::Min(12.0f, Cameras.Num() <= 2 ? 10.5f : 2.625f * static_cast<float>(FMath::Min(4, Cameras.Num())))
        : 0.0f;
    EffectiveAggregateCameraCaptureHz = ResolveAdaptiveCameraAdmissionHz(
        EffectiveAggregateCameraCaptureHz,
        ObservedFrameMs,
        TargetFps,
        CameraAdmissionFloorHz);
    EffectiveAggregateCameraCaptureHz = FMath::Min(12.0f, EffectiveAggregateCameraCaptureHz);

    for (const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Camera : Cameras)
    {
        if (Camera.IsValid()) Camera->TickScheduledCapture(NowSeconds, false);
    }
    // Admit at most one new SceneCapture per game frame. At the supported
    // 60-FPS tier this gives two FullSpec cameras a fair nominal 30 Hz each,
    // while per-sensor readback/encode backpressure still drops stale work.
    if (Cameras.Num() > 0 && (LastCameraCaptureAdmissionTime < 0.0 ||
        NowSeconds - LastCameraCaptureAdmissionTime >= 1.0 / FMath::Max(1.0f, EffectiveAggregateCameraCaptureHz)))
    {
        for (int32 Attempt = 0; Attempt < Cameras.Num(); ++Attempt)
        {
            const int32 Index = (NextCameraIndex + Attempt) % Cameras.Num();
            if (Cameras[Index].IsValid() && Cameras[Index]->TickScheduledCapture(NowSeconds, true))
            {
                LastCameraCaptureAdmissionTime = NowSeconds;
                NextCameraIndex = (Index + 1) % Cameras.Num();
                break;
            }
        }
    }

    for (const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar : Lidars)
    {
        if (Lidar.IsValid()) Lidar->PrepareScheduledScan(NowSeconds);
    }

    const float BudgetCeilingMs = ResolveLidarBudgetMs(TargetFps);
    if (ObservedFrameMs > TargetFrameMs * 1.05f)
    {
        EffectiveLidarBudgetMs = FMath::Max(2.5f, EffectiveLidarBudgetMs - 0.25f);
    }
    else if (ObservedFrameMs < TargetFrameMs * 0.98f)
    {
        EffectiveLidarBudgetMs = FMath::Min(BudgetCeilingMs, EffectiveLidarBudgetMs + 0.1f);
    }
    EffectiveLidarBudgetMs = FMath::Min(EffectiveLidarBudgetMs, BudgetCeilingMs);
    const double BudgetSeconds = EffectiveLidarBudgetMs / 1000.0;
    int32 ConsecutiveIdle = 0;
    while (Lidars.Num() > 0 && FPlatformTime::Seconds() - StartSeconds < BudgetSeconds && ConsecutiveIdle < Lidars.Num())
    {
        const int32 Index = NextLidarIndex % Lidars.Num();
        NextLidarIndex = (Index + 1) % Lidars.Num();
        UVirtualLidarScanComponent* Lidar = Lidars[Index].Get();
        const double ChunkStart = FPlatformTime::Seconds();
        const int32 Processed = Lidar ? Lidar->ProcessScheduledScanChunk(AdaptiveLidarChunkSize) : 0;
        const double ChunkMs = (FPlatformTime::Seconds() - ChunkStart) * 1000.0;

        if (Processed <= 0)
        {
            ++ConsecutiveIdle;
            continue;
        }

        ConsecutiveIdle = 0;
        if (ChunkMs > 0.75 && AdaptiveLidarChunkSize > 128) AdaptiveLidarChunkSize = FMath::Max(128, AdaptiveLidarChunkSize / 2);
        else if (ChunkMs < 0.25 && AdaptiveLidarChunkSize < 1024) AdaptiveLidarChunkSize = FMath::Min(1024, AdaptiveLidarChunkSize * 2);
    }

    RefreshTelemetry(static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0));

    TelemetryLogAccumulator += FMath::Max(0.0f, DeltaTime);
    if (TelemetryLogAccumulator >= 1.0f)
    {
        TelemetryLogAccumulator = FMath::Fmod(TelemetryLogAccumulator, 1.0f);
        RefreshFrameStatistics();
        UE_LOG(LogTemp, Display,
            TEXT("[VirtualSensorPerf] targetFps=%d camera=%d lidar=%d averageFps=%.2f onePercentLowFps=%.2f p95FrameMs=%.2f schedulerMs=%.2f pendingAcquisition=%d pendingDerived=%d droppedAcquisition=%d droppedDerived=%d bestEffort=%d budgetSkipped=%d failedAcquisition=%d queueOverflow=%d minCameraHz=%.2f minLidarHz=%.2f"),
            Telemetry.TargetFps,
            Telemetry.ActiveCameraCount,
            Telemetry.ActiveLidarCount,
            Telemetry.AverageFps,
            Telemetry.OnePercentLowFps,
            Telemetry.P95FrameTimeMs,
            Telemetry.LastSchedulerWorkMs,
            Telemetry.PendingAcquisitionCount,
            Telemetry.PendingDerivedWorkCount,
            Telemetry.DroppedAcquisitionFrameCount,
            Telemetry.DroppedDerivedFrameCount,
            Telemetry.bBestEffort ? 1 : 0,
            Telemetry.BudgetSkippedAcquisitionFrameCount,
            Telemetry.FailedAcquisitionFrameCount,
            Telemetry.QueueOverflowCount,
            Telemetry.MinimumCameraCompletionHz,
            Telemetry.MinimumLidarCompletionHz);

        for (const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Camera : Cameras)
        {
            if (!Camera.IsValid()) continue;
            const FVirtualSensorRuntimeStatus& Status = Camera->GetRuntimeStatus();
            UE_LOG(LogTemp, Display,
                TEXT("[VirtualSensorPerfSensor] kind=Camera sensorId=%s width=%d height=%d rateHz=%.2f acquisitionMs=%.2f postMs=%.2f pendingAcquisition=%d pendingDerived=%d droppedAcquisition=%d droppedDerived=%d budgetSkipped=%d failedAcquisition=%d queueOverflow=%d"),
                *Camera->SensorId,
                Camera->CaptureResolution.X,
                Camera->CaptureResolution.Y,
                Status.MeasuredCompletionRateHz,
                Status.LastAcquisitionDurationMs,
                Status.LastPostProcessDurationMs,
                Status.bAcquisitionInFlight ? 1 : 0,
                Status.bDerivedWorkInFlight ? 1 : 0,
                Status.DroppedAcquisitionFrameCount,
                Status.DroppedDerivedFrameCount,
                Status.BudgetSkippedAcquisitionFrameCount,
                Status.FailedAcquisitionFrameCount,
                Status.QueueOverflowCount);
        }
        for (const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar : Lidars)
        {
            if (!Lidar.IsValid()) continue;
            const FVirtualSensorRuntimeStatus& Status = Lidar->GetRuntimeStatus();
            UE_LOG(LogTemp, Display,
                TEXT("[VirtualSensorPerfSensor] kind=Lidar sensorId=%s horizontal=%d vertical=%d rays=%d rateHz=%.2f acquisitionMs=%.2f postMs=%.2f pendingAcquisition=%d pendingDerived=%d droppedAcquisition=%d droppedDerived=%d budgetSkipped=%d failedAcquisition=%d queueOverflow=%d"),
                *Lidar->SensorId,
                Lidar->HorizontalSamples,
                Lidar->VerticalChannels,
                Lidar->HorizontalSamples * Lidar->VerticalChannels,
                Status.MeasuredCompletionRateHz,
                Status.LastAcquisitionDurationMs,
                Status.LastPostProcessDurationMs,
                Status.bAcquisitionInFlight ? 1 : 0,
                Status.bDerivedWorkInFlight ? 1 : 0,
                Status.DroppedAcquisitionFrameCount,
                Status.DroppedDerivedFrameCount,
                Status.BudgetSkippedAcquisitionFrameCount,
                Status.FailedAcquisitionFrameCount,
                Status.QueueOverflowCount);
        }
    }
}

void UVirtualSensorSchedulerSubsystem::ConfigureCommandLineBenchmarkIfRequested()
{
    if (bCommandLineBenchmarkConfigured) return;

    int32 RequestedCameras = 0;
    int32 RequestedLidars = 0;
    const bool bHasCameraRequest = FParse::Value(FCommandLine::Get(), TEXT("VirtualSensorPerfCameras="), RequestedCameras);
    const bool bHasLidarRequest = FParse::Value(FCommandLine::Get(), TEXT("VirtualSensorPerfLidars="), RequestedLidars);
    if (!bHasCameraRequest && !bHasLidarRequest)
    {
        bCommandLineBenchmarkConfigured = true;
        return;
    }

    bCommandLineBenchmarkConfigured = true;
    RequestedCameras = FMath::Clamp(RequestedCameras, 0, 16);
    RequestedLidars = FMath::Clamp(RequestedLidars, 0, 16);
    FString RequestedLidarRenderer = TEXT("Niagara");
    FParse::Value(FCommandLine::Get(), TEXT("VirtualSensorPerfLidarRenderer="), RequestedLidarRenderer);

    const TArray<TWeakObjectPtr<UVirtualCameraCaptureComponent>> ExistingCameras = Cameras;
    const TArray<TWeakObjectPtr<UVirtualLidarScanComponent>> ExistingLidars = Lidars;
    for (int32 Index = 0; Index < ExistingCameras.Num(); ++Index)
    {
        const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Camera = ExistingCameras[Index];
        if (!Camera.IsValid()) continue;
        if (Index >= RequestedCameras)
        {
            Camera->StopCapture();
            continue;
        }
        Camera->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
        Camera->CaptureMode = EVirtualCameraCaptureMode::Payload;
        Camera->StartCapture();
    }
    for (int32 Index = 0; Index < ExistingLidars.Num(); ++Index)
    {
        const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar = ExistingLidars[Index];
        if (!Lidar.IsValid()) continue;
        if (Index >= RequestedLidars)
        {
            Lidar->StopScan();
            continue;
        }
        Lidar->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
        Lidar->bUseMultiHit = false;
        Lidar->bExportCsvOnScan = false;
        Lidar->bExportJsonLinesOnScan = false;
        Lidar->bExportPcdOnScan = false;
        Lidar->StartScan();
    }

    UWorld* World = GetWorld();
    if (!World) return;

    float RequestedWarmupSeconds = 10.0f;
    FParse::Value(FCommandLine::Get(), TEXT("VirtualSensorPerfWarmupSeconds="), RequestedWarmupSeconds);
    CommandLineBenchmarkStatisticsResetTime = World->GetTimeSeconds() + FMath::Max(0.0f, RequestedWarmupSeconds);
    bCommandLineBenchmarkStatisticsReset = RequestedWarmupSeconds <= 0.0f;

    CompactRegistrations();

    for (int32 Index = Cameras.Num(); Index < RequestedCameras; ++Index)
    {
        AActor* Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(0.0, Index * 50.0, 170.0)));
        if (!Owner) continue;
        UVirtualCameraCaptureComponent* Camera = NewObject<UVirtualCameraCaptureComponent>(Owner, *FString::Printf(TEXT("BenchmarkCamera_%02d"), Index + 1));
        Camera->SensorId = FString::Printf(TEXT("BENCH-CAM-%02d"), Index + 1);
        Camera->bAutoStartCapture = false;
        Camera->bAutoRegisterToManager = false;
        Camera->bApplyDeviceProfileOnBeginPlay = false;
        Camera->ApplyDeviceProfile(EVirtualCameraDeviceProfile::IntelRealSenseD455);
        Camera->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
        Camera->CaptureMode = EVirtualCameraCaptureMode::Payload;
        Owner->SetRootComponent(Camera);
        Camera->RegisterComponent();
        Camera->StartCapture();
        CommandLineBenchmarkActors.Add(Owner);
    }

    for (int32 Index = Lidars.Num(); Index < RequestedLidars; ++Index)
    {
        AVirtualLidarSensorActor* Owner = World->SpawnActor<AVirtualLidarSensorActor>(AVirtualLidarSensorActor::StaticClass(), FTransform(FVector(0.0, Index * 50.0, 150.0)));
        if (!Owner || !Owner->ScanComponent) continue;
        UVirtualLidarScanComponent* Lidar = Owner->ScanComponent;
        Lidar->StopScan();
        Lidar->SensorId = FString::Printf(TEXT("BENCH-LIDAR-%02d"), Index + 1);
        Lidar->bAutoRegisterToManager = false;
        Lidar->bApplyDeviceProfileOnBeginPlay = false;
        Lidar->ApplyDeviceProfile(EVirtualLidarDeviceProfile::LivoxMid360S);
        Lidar->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
        Lidar->bUseMultiHit = false;
        Lidar->bExportCsvOnScan = false;
        Lidar->bExportJsonLinesOnScan = false;
        Lidar->bExportPcdOnScan = false;
        Lidar->StartScan();
        CommandLineBenchmarkActors.Add(Owner);
    }

    CompactRegistrations();
    UVirtualLidarScanComponent* PreviewLidar = nullptr;
    for (const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar : Lidars)
    {
        if (Lidar.IsValid() && Lidar->IsScanRunning())
        {
            PreviewLidar = Lidar.Get();
            break;
        }
    }
    SetPreferredLidar(PreviewLidar);
    for (const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar : Lidars)
    {
        if (!Lidar.IsValid()) continue;
        AVirtualLidarSensorActor* LidarOwner = Cast<AVirtualLidarSensorActor>(Lidar->GetOwner());
        UVirtualLidarVisualizationComponent* Visualization = LidarOwner ? LidarOwner->VisualizationComponent : nullptr;
        const bool bSelected = Lidar.Get() == PreviewLidar;
        const bool bCpu = RequestedLidarRenderer.Equals(TEXT("Cpu"), ESearchCase::IgnoreCase);
        const bool bOff = RequestedLidarRenderer.Equals(TEXT("Off"), ESearchCase::IgnoreCase);
        if (Visualization)
        {
            Visualization->SetForceCpuFallbackForBenchmark(bCpu);
            Visualization->SetWorldPointCloudEnabled(bSelected && !bOff);
        }
        if (bCpu && bSelected)
        {
            Lidar->MaxPointCloudPreviewInstances = 5000;
            Lidar->MaxPreviewPoints = 5000;
        }
        if (!bSelected || bOff) Lidar->SetPointCloudPreviewEnabled(false);
    }

    UE_LOG(LogTemp, Display, TEXT("[VirtualSensorPerf] command-line FullSpec benchmark requested camera=%d lidar=%d renderer=%s"), RequestedCameras, RequestedLidars, *RequestedLidarRenderer);
}

void UVirtualSensorSchedulerSubsystem::RefreshFrameStatistics()
{
    if (RecentFrameTimesMs.IsEmpty())
    {
        Telemetry.AverageFps = 0.0f;
        Telemetry.OnePercentLowFps = 0.0f;
        Telemetry.P95FrameTimeMs = 0.0f;
        return;
    }

    double TotalMs = 0.0;
    TArray<float> Sorted = RecentFrameTimesMs;
    for (const float FrameMs : Sorted) TotalMs += FrameMs;
    Sorted.Sort();

    const float AverageFrameMs = static_cast<float>(TotalMs / Sorted.Num());
    const int32 P95Index = FMath::Clamp(FMath::CeilToInt(Sorted.Num() * 0.95f) - 1, 0, Sorted.Num() - 1);
    const int32 OnePercentLowIndex = FMath::Clamp(FMath::CeilToInt(Sorted.Num() * 0.99f) - 1, 0, Sorted.Num() - 1);
    Telemetry.AverageFps = AverageFrameMs > SMALL_NUMBER ? 1000.0f / AverageFrameMs : 0.0f;
    Telemetry.P95FrameTimeMs = Sorted[P95Index];
    Telemetry.OnePercentLowFps = Sorted[OnePercentLowIndex] > SMALL_NUMBER ? 1000.0f / Sorted[OnePercentLowIndex] : 0.0f;
}

void UVirtualSensorSchedulerSubsystem::RefreshTelemetry(float WorkMs)
{
    Telemetry.ActiveCameraCount = Cameras.Num();
    Telemetry.ActiveLidarCount = Lidars.Num();
    Telemetry.TargetFps = ResolveTargetFps(Cameras.Num(), Lidars.Num());
    Telemetry.LidarGameThreadBudgetMs = EffectiveLidarBudgetMs;
    Telemetry.LidarGameThreadBudgetCeilingMs = ResolveLidarBudgetMs(Telemetry.TargetFps);
    Telemetry.LastSchedulerWorkMs = WorkMs;
    Telemetry.bBestEffort = IsBestEffortConfiguration(Cameras.Num(), Lidars.Num());
    Telemetry.PendingAcquisitionCount = 0;
    Telemetry.PendingDerivedWorkCount = 0;
    Telemetry.DroppedAcquisitionFrameCount = 0;
    Telemetry.DroppedDerivedFrameCount = 0;
    Telemetry.BudgetSkippedAcquisitionFrameCount = 0;
    Telemetry.FailedAcquisitionFrameCount = 0;
    Telemetry.QueueOverflowCount = 0;
    float CameraMinHz = TNumericLimits<float>::Max();
    float CameraMaxHz = 0.0f;
    float LidarMinHz = TNumericLimits<float>::Max();
    float LidarMaxHz = 0.0f;

    auto Accumulate = [this](const FVirtualSensorRuntimeStatus& Status)
    {
        Telemetry.PendingAcquisitionCount += Status.bAcquisitionInFlight ? 1 : 0;
        Telemetry.PendingDerivedWorkCount += Status.bDerivedWorkInFlight ? 1 : 0;
        Telemetry.DroppedAcquisitionFrameCount += Status.DroppedAcquisitionFrameCount;
        Telemetry.DroppedDerivedFrameCount += Status.DroppedDerivedFrameCount;
        Telemetry.BudgetSkippedAcquisitionFrameCount += Status.BudgetSkippedAcquisitionFrameCount;
        Telemetry.FailedAcquisitionFrameCount += Status.FailedAcquisitionFrameCount;
        Telemetry.QueueOverflowCount += Status.QueueOverflowCount;
    };
    for (const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Camera : Cameras)
    {
        if (!Camera.IsValid()) continue;
        const FVirtualSensorRuntimeStatus& Status = Camera->GetRuntimeStatus();
        Accumulate(Status);
        if (Status.MeasuredCompletionRateHz > SMALL_NUMBER)
        {
            CameraMinHz = FMath::Min(CameraMinHz, Status.MeasuredCompletionRateHz);
            CameraMaxHz = FMath::Max(CameraMaxHz, Status.MeasuredCompletionRateHz);
        }
    }
    for (const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar : Lidars)
    {
        if (!Lidar.IsValid()) continue;
        const FVirtualSensorRuntimeStatus& Status = Lidar->GetRuntimeStatus();
        Accumulate(Status);
        if (Status.MeasuredCompletionRateHz > SMALL_NUMBER)
        {
            LidarMinHz = FMath::Min(LidarMinHz, Status.MeasuredCompletionRateHz);
            LidarMaxHz = FMath::Max(LidarMaxHz, Status.MeasuredCompletionRateHz);
        }
    }
    Telemetry.CameraCompletionFairnessRatio = CameraMinHz < TNumericLimits<float>::Max() && CameraMinHz > SMALL_NUMBER ? CameraMaxHz / CameraMinHz : 1.0f;
    Telemetry.LidarCompletionFairnessRatio = LidarMinHz < TNumericLimits<float>::Max() && LidarMinHz > SMALL_NUMBER ? LidarMaxHz / LidarMinHz : 1.0f;
    Telemetry.MinimumCameraCompletionHz = CameraMinHz < TNumericLimits<float>::Max() ? CameraMinHz : 0.0f;
    Telemetry.MinimumLidarCompletionHz = LidarMinHz < TNumericLimits<float>::Max() ? LidarMinHz : 0.0f;

    Telemetry.StatusMessage = Telemetry.bBestEffort
        ? TEXT("지원 기준(카메라/LiDAR 각각 4대)을 초과해 30 FPS 최선 실행 중")
        : FString::Printf(TEXT("자동 %d FPS 단계"), Telemetry.TargetFps);
}

FString UVirtualSensorSchedulerSubsystem::GetTelemetrySummaryText() const
{
    return FString::Printf(
        TEXT("%s | 카메라=%d LiDAR=%d | 평균=%.1f FPS 1%% low=%.1f FPS p95=%.1fms | 스케줄러=%.2fms/적응 예산 %.1fms(최대 %.1fms) | 완료 하한 Camera/LiDAR=%.1f/%.1fHz | 측정 대기=%d 후처리 대기=%d | 예산 생략=%d 실패=%d 큐 초과=%d 파생 생략=%d"),
        *Telemetry.StatusMessage,
        Telemetry.ActiveCameraCount,
        Telemetry.ActiveLidarCount,
        Telemetry.AverageFps,
        Telemetry.OnePercentLowFps,
        Telemetry.P95FrameTimeMs,
        Telemetry.LastSchedulerWorkMs,
        Telemetry.LidarGameThreadBudgetMs,
        Telemetry.LidarGameThreadBudgetCeilingMs,
        Telemetry.MinimumCameraCompletionHz,
        Telemetry.MinimumLidarCompletionHz,
        Telemetry.PendingAcquisitionCount,
        Telemetry.PendingDerivedWorkCount,
        Telemetry.BudgetSkippedAcquisitionFrameCount,
        Telemetry.FailedAcquisitionFrameCount,
        Telemetry.QueueOverflowCount,
        Telemetry.DroppedDerivedFrameCount);
}
