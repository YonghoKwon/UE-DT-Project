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
    return CameraCount > 0
        ? FMath::Min(
            30.0f,
            static_cast<float>(TargetFps * ResolveCameraCapturesPerFrame(TargetFps, CameraCount))
                / static_cast<float>(CameraCount))
        : 0.0f;
}

int32 UVirtualSensorSchedulerSubsystem::ResolveCameraCapturesPerFrame(int32 TargetFps, int32 CameraCount)
{
    if (CameraCount <= 0) return 0;
    if (CameraCount <= 2) return 1;
    return TargetFps <= 30 ? 2 : 1;
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
    if (Camera && !Cameras.Contains(Camera))
    {
        Cameras.Add(Camera);
        RegistrationTimes.Add(Camera, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
    }
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
    RegistrationTimes.Remove(Camera);
    if (PreferredCamera.Get() == Camera) PreferredCamera.Reset();
    Cameras.RemoveAll([Camera](const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Item) { return !Item.IsValid() || Item.Get() == Camera; });
    NextCameraIndex = Cameras.Num() > 0 ? NextCameraIndex % Cameras.Num() : 0;
}

void UVirtualSensorSchedulerSubsystem::RegisterLidar(UVirtualLidarScanComponent* Lidar)
{
    if (Lidar && !Lidars.Contains(Lidar))
    {
        Lidars.Add(Lidar);
        RegistrationTimes.Add(Lidar, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
        AdaptiveLidarChunkSizes.FindOrAdd(Lidar, 256);
    }
}

void UVirtualSensorSchedulerSubsystem::UnregisterLidar(UVirtualLidarScanComponent* Lidar)
{
    RegistrationTimes.Remove(Lidar);
    if (PreferredLidar.Get() == Lidar) PreferredLidar.Reset();
    Lidars.RemoveAll([Lidar](const TWeakObjectPtr<UVirtualLidarScanComponent>& Item) { return !Item.IsValid() || Item.Get() == Lidar; });
    AdaptiveLidarChunkSizes.Remove(Lidar);
    LastLidarPreviewRefreshTimes.Remove(Lidar);
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

bool UVirtualSensorSchedulerSubsystem::ConsumeLidarPreviewRefresh(
    UVirtualLidarScanComponent* Lidar,
    float MaximumRefreshHz)
{
    if (!Lidar || !ShouldRefreshLidarPreview(Lidar)) return false;
    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const double MinimumInterval = 1.0 / FMath::Max(1.0f, MaximumRefreshHz);
    double& LastRefreshSeconds = LastLidarPreviewRefreshTimes.FindOrAdd(Lidar, -1.0e30);
    if (NowSeconds - LastRefreshSeconds < MinimumInterval) return false;
    LastRefreshSeconds = NowSeconds;
    return true;
}

void UVirtualSensorSchedulerSubsystem::CompactRegistrations()
{
    for (auto It = RegistrationTimes.CreateIterator(); It; ++It)
        if (!It.Key().IsValid()) It.RemoveCurrent();
    Cameras.RemoveAll([](const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Item) { return !Item.IsValid(); });
    Lidars.RemoveAll([](const TWeakObjectPtr<UVirtualLidarScanComponent>& Item) { return !Item.IsValid(); });
    for (auto It = AdaptiveLidarChunkSizes.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) It.RemoveCurrent();
    }
    for (auto It = LastLidarPreviewRefreshTimes.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) It.RemoveCurrent();
    }
    NextCameraIndex = Cameras.Num() > 0 ? NextCameraIndex % Cameras.Num() : 0;
    NextLidarIndex = Lidars.Num() > 0 ? NextLidarIndex % Lidars.Num() : 0;
}

void UVirtualSensorSchedulerSubsystem::Tick(float DeltaTime)
{
    if (GetWorld() && GetWorld()->IsPaused())
    {
        CompactRegistrations();
        RefreshTelemetry(0.0f);
        return; // Diagnostics may tick while paused, acquisition must not.
    }
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
    EffectiveAggregateCameraCaptureHz =
        ResolveNominalCameraRatePerSensor(TargetFps, Cameras.Num()) * Cameras.Num();

    for (const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Camera : Cameras)
    {
        if (Camera.IsValid()) Camera->TickScheduledCapture(NowSeconds, false);
    }
    // Acquisition is independent from bounded readback/JPEG output. Two
    // SceneCaptures per frame are allowed for the 4-camera/30-FPS tier.
    int32 CameraCapturesThisFrame = ResolveCameraCapturesPerFrame(TargetFps, Cameras.Num());
    if (ObservedFrameMs > TargetFrameMs * 1.10f)
    {
        CameraCapturesThisFrame = FMath::Min(CameraCapturesThisFrame, 1);
    }
    int32 CameraAdmissions = 0;
    int32 CameraAttempts = 0;
    while (Cameras.Num() > 0
        && CameraAdmissions < CameraCapturesThisFrame
        && CameraAttempts < Cameras.Num())
    {
        const int32 Index = NextCameraIndex % Cameras.Num();
        NextCameraIndex = (Index + 1) % Cameras.Num();
        ++CameraAttempts;
        if (Cameras[Index].IsValid() && Cameras[Index]->TickScheduledCapture(NowSeconds, true))
        {
            ++CameraAdmissions;
            LastCameraCaptureAdmissionTime = NowSeconds;
        }
    }
    if (CameraAdmissions >= CameraCapturesThisFrame && CameraCapturesThisFrame > 0)
    {
        for (const TWeakObjectPtr<UVirtualCameraCaptureComponent>& Camera : Cameras)
        {
            if (Camera.IsValid() && Camera->IsScheduledCaptureDue(NowSeconds))
            {
                Camera->MarkBudgetSkippedAcquisition();
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
        int32& AdaptiveChunkSize = AdaptiveLidarChunkSizes.FindOrAdd(Lidars[Index], 256);
        const double ChunkStart = FPlatformTime::Seconds();
        const int32 Processed = Lidar ? Lidar->ProcessScheduledScanChunk(AdaptiveChunkSize) : 0;
        const double ChunkMs = (FPlatformTime::Seconds() - ChunkStart) * 1000.0;

        if (Processed <= 0)
        {
            ++ConsecutiveIdle;
            continue;
        }

        ConsecutiveIdle = 0;
        if (ChunkMs > 0.75 && AdaptiveChunkSize > 128) AdaptiveChunkSize = FMath::Max(128, AdaptiveChunkSize / 2);
        else if (ChunkMs < 0.25 && AdaptiveChunkSize < 1024) AdaptiveChunkSize = FMath::Min(1024, AdaptiveChunkSize * 2);
    }

    // A GPU acquisition can complete inside ProcessScheduledScanChunk after
    // the pre-pass has already observed it as in-flight. Re-run only the cheap
    // admission step so a due ML-X frame starts in this Tick instead of idling
    // for another complete 0.05-second period. No additional scan is queued;
    // every component still permits exactly one acquisition in flight.
    for (const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar : Lidars)
    {
        if (Lidar.IsValid()) Lidar->PrepareScheduledScan(NowSeconds);
    }

    RefreshTelemetry(static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0));

    TelemetryLogAccumulator += FMath::Max(0.0f, DeltaTime);
    if (TelemetryLogAccumulator >= 1.0f)
    {
        TelemetryLogAccumulator = FMath::Fmod(TelemetryLogAccumulator, 1.0f);
        RefreshFrameStatistics();
        const auto EffectiveHz = [this](const FString& Kind, const FString& Id)
        {
            const auto* Rate = Telemetry.SensorRates.FindByPredicate([&](const FVirtualSensorRateDiagnostic& R) { return R.SensorKind == Kind && R.SensorId == Id; });
            return Rate ? Rate->EffectiveHz : 0.0f;
        };
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
                TEXT("[VirtualSensorPerfSensor] kind=Camera sensorId=%s width=%d height=%d requestedHz=%.2f acquisitionHz=%.2f outputHz=%.2f acquisitionMs=%.2f postMs=%.2f pendingAcquisition=%d pendingDerived=%d droppedAcquisition=%d droppedDerived=%d deadlineMiss=%d budgetSkipped=%d failedAcquisition=%d queueOverflow=%d backend=%s"),
                *Camera->SensorId,
                Camera->CaptureResolution.X,
                Camera->CaptureResolution.Y,
                Status.RequestedAcquisitionRateHz,
                EffectiveHz(TEXT("Camera"), Camera->SensorId),
                Status.MeasuredOutputRateHz,
                Status.LastAcquisitionDurationMs,
                Status.LastPostProcessDurationMs,
                Status.bAcquisitionInFlight ? 1 : 0,
                Status.bDerivedWorkInFlight ? 1 : 0,
                Status.DroppedAcquisitionFrameCount,
                Status.DroppedDerivedFrameCount,
                Status.DeadlineMissCount,
                Status.BudgetSkippedAcquisitionFrameCount,
                Status.FailedAcquisitionFrameCount,
                Status.QueueOverflowCount,
                *Status.ActiveAcquisitionBackend);
        }
        for (const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar : Lidars)
        {
            if (!Lidar.IsValid()) continue;
            const FVirtualSensorRuntimeStatus& Status = Lidar->GetRuntimeStatus();
            UE_LOG(LogTemp, Display,
                TEXT("[VirtualSensorPerfSensor] kind=Lidar sensorId=%s horizontal=%d vertical=%d rays=%d requestedHz=%.2f acquisitionHz=%.2f outputHz=%.2f acquisitionMs=%.2f postMs=%.2f pendingAcquisition=%d pendingDerived=%d droppedAcquisition=%d droppedDerived=%d deadlineMiss=%d budgetSkipped=%d failedAcquisition=%d queueOverflow=%d backend=%s"),
                *Lidar->SensorId,
                Lidar->HorizontalSamples,
                Lidar->VerticalChannels,
                Lidar->HorizontalSamples * Lidar->VerticalChannels,
                Status.RequestedAcquisitionRateHz,
                EffectiveHz(TEXT("LiDAR"), Lidar->SensorId),
                Status.MeasuredOutputRateHz,
                Status.LastAcquisitionDurationMs,
                Status.LastPostProcessDurationMs,
                Status.bAcquisitionInFlight ? 1 : 0,
                Status.bDerivedWorkInFlight ? 1 : 0,
                Status.DroppedAcquisitionFrameCount,
                Status.DroppedDerivedFrameCount,
                Status.DeadlineMissCount,
                Status.BudgetSkippedAcquisitionFrameCount,
                Status.FailedAcquisitionFrameCount,
                Status.QueueOverflowCount,
                *Status.ActiveAcquisitionBackend);
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
    FString RequestedLidarProfile = TEXT("Mid360");
    FParse::Value(FCommandLine::Get(), TEXT("VirtualSensorPerfLidarProfile="), RequestedLidarProfile);
    FString RequestedLidarAcquisition = TEXT("Auto");
    FParse::Value(FCommandLine::Get(), TEXT("VirtualSensorPerfLidarAcquisition="), RequestedLidarAcquisition);

    const EVirtualLidarDeviceProfile BenchmarkProfile =
        RequestedLidarProfile.Equals(TEXT("MLX80Native"), ESearchCase::IgnoreCase)
            ? EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE
            : RequestedLidarProfile.Equals(TEXT("MLX80Integration"), ESearchCase::IgnoreCase)
                ? EVirtualLidarDeviceProfile::IYOBOT_MLX80
                : EVirtualLidarDeviceProfile::LivoxMid360S;
    const EVirtualLidarAcquisitionBackend BenchmarkAcquisition =
        RequestedLidarAcquisition.Equals(TEXT("Cpu"), ESearchCase::IgnoreCase)
            ? EVirtualLidarAcquisitionBackend::AccurateCpuTrace
            : RequestedLidarAcquisition.Equals(TEXT("Gpu"), ESearchCase::IgnoreCase)
                ? EVirtualLidarAcquisitionBackend::GpuDepthProjection
                : EVirtualLidarAcquisitionBackend::Auto;

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
        Lidar->ApplyDeviceProfile(BenchmarkProfile);
        Lidar->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
        Lidar->AcquisitionBackend = BenchmarkAcquisition;
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
        Lidar->ApplyDeviceProfile(BenchmarkProfile);
        Lidar->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
        Lidar->AcquisitionBackend = BenchmarkAcquisition;
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

    UE_LOG(
        LogTemp,
        Display,
        TEXT("[VirtualSensorPerf] command-line FullSpec benchmark requested camera=%d lidar=%d renderer=%s profile=%s acquisition=%s"),
        RequestedCameras,
        RequestedLidars,
        *RequestedLidarRenderer,
        *RequestedLidarProfile,
        *RequestedLidarAcquisition);
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

FVirtualSensorRateDiagnostic UVirtualSensorSchedulerSubsystem::EvaluateRate(double Now, double RegisteredAt,
    double LastProgress, float MeasuredHz, float Period, bool bRunning, bool bPaused, bool bInteractive, int32 FailureCount)
{
    FVirtualSensorRateDiagnostic Result;
    Result.bInteractionPreview = bInteractive;
    if (!bRunning) return Result;
    if (bPaused) { Result.State = EVirtualSensorRateState::Paused; return Result; }
    const double Grace = FMath::Max(1.0, 3.0 * FMath::Max(0.001, static_cast<double>(Period)));
    if (LastProgress >= RegisteredAt && LastProgress <= Now)
        Result.LastProgressAgeSeconds = Now - LastProgress;
    if (Now - RegisteredAt < Grace)
    {
        Result.State = EVirtualSensorRateState::WarmingUp;
        return Result;
    }
    Result.bEvaluable = true;
    if (Result.LastProgressAgeSeconds < 0.0 || Result.LastProgressAgeSeconds > Grace ||
        !FMath::IsFinite(MeasuredHz) || MeasuredHz <= SMALL_NUMBER)
    {
        Result.State = FailureCount > 0 ? EVirtualSensorRateState::Failed : EVirtualSensorRateState::Starved;
        return Result;
    }
    Result.EffectiveHz = MeasuredHz;
    Result.State = bInteractive ? EVirtualSensorRateState::InteractionPreview : EVirtualSensorRateState::Running;
    return Result;
}

void UVirtualSensorSchedulerSubsystem::AggregateRateDiagnostics(FVirtualSensorPerformanceTelemetry& Result)
{
    Result.StarvedSensorIds.Reset();
    auto Aggregate = [&](const TCHAR* Kind, float& MinHz, float& Fairness, bool& bFairnessEvaluable)
    {
        float Min = TNumericLimits<float>::Max(), Max = 0.0f;
        int32 Count = 0;
        bool bAllReady = true;
        for (const auto& Rate : Result.SensorRates)
        {
            if (Rate.SensorKind != Kind || Rate.State == EVirtualSensorRateState::Stopped) continue;
            ++Count;
            bAllReady &= Rate.bEvaluable && !Rate.bInteractionPreview;
            if (!Rate.bEvaluable) continue;
            Min = FMath::Min(Min, Rate.EffectiveHz);
            Max = FMath::Max(Max, Rate.EffectiveHz);
            if (Rate.State == EVirtualSensorRateState::Starved || Rate.State == EVirtualSensorRateState::Failed)
                Result.StarvedSensorIds.AddUnique(Rate.SensorId);
        }
        MinHz = Min < TNumericLimits<float>::Max() ? Min : 0.0f;
        bFairnessEvaluable = Count > 0 && bAllReady && MinHz > SMALL_NUMBER;
        Fairness = bFairnessEvaluable ? Max / MinHz : 0.0f; // Finite sentinel; consumers must read the validity flag.
    };
    Aggregate(TEXT("Camera"), Result.MinimumCameraCompletionHz, Result.CameraCompletionFairnessRatio, Result.bCameraFairnessEvaluable);
    Aggregate(TEXT("LiDAR"), Result.MinimumLidarCompletionHz, Result.LidarCompletionFairnessRatio, Result.bLidarFairnessEvaluable);
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
    Telemetry.SensorRates.Reset();
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const bool bPaused = GetWorld() && GetWorld()->IsPaused();
    const auto AddRate = [&](UActorComponent* Component, const FVirtualSensorRuntimeStatus& Status, const FString& Id,
        const TCHAR* Kind, float Period, bool bRunning)
    {
        const auto* Owner = Cast<AVirtualSensorActorBase>(Component->GetOwner());
        auto Rate = EvaluateRate(Now, RegistrationTimes.FindRef(Component), Status.LastAcquisitionProgressWorldSeconds,
            Status.MeasuredAcquisitionRateHz, Period, bRunning, bPaused, Owner && Owner->IsInteractiveManipulationActive(), Status.FailedAcquisitionFrameCount);
        Rate.SensorId = Id;
        Rate.SensorKind = Kind;
        Telemetry.SensorRates.Add(MoveTemp(Rate));
    };

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
        AddRate(Camera.Get(), Status, Camera->SensorId, TEXT("Camera"), Camera->CaptureInterval, Camera->IsCaptureRunning());
    }
    for (const TWeakObjectPtr<UVirtualLidarScanComponent>& Lidar : Lidars)
    {
        if (!Lidar.IsValid()) continue;
        const FVirtualSensorRuntimeStatus& Status = Lidar->GetRuntimeStatus();
        Accumulate(Status);
        AddRate(Lidar.Get(), Status, Lidar->SensorId, TEXT("LiDAR"), Lidar->ScanInterval, Lidar->IsScanRunning());
    }
    AggregateRateDiagnostics(Telemetry);

    Telemetry.StatusMessage = Telemetry.bBestEffort
        ? TEXT("지원 기준(카메라/LiDAR 각각 4대)을 초과해 30 FPS 최선 실행 중")
        : FString::Printf(TEXT("자동 %d FPS 단계"), Telemetry.TargetFps);
    if (!Telemetry.StarvedSensorIds.IsEmpty())
        Telemetry.StatusMessage += TEXT(" · 주기 유지 실패/정체: ") + FString::Join(Telemetry.StarvedSensorIds, TEXT(", "));
}

FString UVirtualSensorSchedulerSubsystem::GetTelemetrySummaryText() const
{
    FString Result = FString::Printf(
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
    for (const auto& Rate : Telemetry.SensorRates)
    {
        const TCHAR* State = TEXT("정지");
        switch (Rate.State)
        {
        case EVirtualSensorRateState::WarmingUp: State = TEXT("시작 유예"); break;
        case EVirtualSensorRateState::Running: State = TEXT("측정 중"); break;
        case EVirtualSensorRateState::Paused: State = TEXT("일시정지"); break;
        case EVirtualSensorRateState::InteractionPreview: State = TEXT("조작용 경량 미리보기"); break;
        case EVirtualSensorRateState::Starved: State = TEXT("측정 정체"); break;
        case EVirtualSensorRateState::Failed: State = TEXT("측정 정체(처리 실패 이력 있음)"); break;
        default: break;
        }
        Result += FString::Printf(TEXT("\n%s: %s · %.1fHz · 마지막 진행 %.2f초 전%s"), *Rate.SensorId, State,
            Rate.EffectiveHz, Rate.LastProgressAgeSeconds, Rate.bInteractionPreview ? TEXT(" · 조작 모드") : TEXT(""));
    }
    if (GetWorld() && GetWorld()->IsPaused()) Result += TEXT("\nPIE 일시정지 · 공정성 판정 보류");
    return Result;
}
