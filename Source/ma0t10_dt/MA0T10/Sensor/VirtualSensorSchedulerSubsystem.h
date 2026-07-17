#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VirtualSensorSchedulerSubsystem.generated.h"

class UVirtualCameraCaptureComponent;
class UVirtualLidarScanComponent;
class AActor;
class UActorComponent;

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorPerformanceTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    int32 TargetFps = 60;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    int32 ActiveCameraCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    int32 ActiveLidarCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    float LidarGameThreadBudgetMs = 4.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    float LidarGameThreadBudgetCeilingMs = 4.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    float LastSchedulerWorkMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    float AverageFps = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    float OnePercentLowFps = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    float P95FrameTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    int32 PendingAcquisitionCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    int32 PendingDerivedWorkCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    int32 DroppedAcquisitionFrameCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    int32 DroppedDerivedFrameCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    bool bBestEffort = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorPerformance")
    FString StatusMessage;
};

UCLASS()
class MA0T10_DT_API UVirtualSensorSchedulerSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    void RegisterCamera(UVirtualCameraCaptureComponent* Camera);
    void UnregisterCamera(UVirtualCameraCaptureComponent* Camera);
    void RegisterLidar(UVirtualLidarScanComponent* Lidar);
    void UnregisterLidar(UVirtualLidarScanComponent* Lidar);

    void RegisterTask(UActorComponent* TaskComponent);
    void UnregisterTask(UActorComponent* TaskComponent);
    void SetPreferredCamera(UVirtualCameraCaptureComponent* Camera);
    void SetPreferredLidar(UVirtualLidarScanComponent* Lidar);
    bool ShouldRefreshLidarPreview(const UVirtualLidarScanComponent* Lidar) const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPerformance")
    const FVirtualSensorPerformanceTelemetry& GetTelemetry() const { return Telemetry; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPerformance")
    FString GetTelemetrySummaryText() const;

    static int32 ResolveTargetFps(int32 CameraCount, int32 LidarCount);
    static float ResolveLidarBudgetMs(int32 TargetFps);
    static bool IsBestEffortConfiguration(int32 CameraCount, int32 LidarCount);
    static float ResolveNominalCameraRatePerSensor(int32 TargetFps, int32 CameraCount);
    static float ResolveAdaptiveCameraAdmissionHz(float CurrentHz, float ObservedFrameMs, int32 TargetFps, float MinimumAdmissionHz = -1.0f);

private:
    void CompactRegistrations();
    void RefreshTelemetry(float WorkMs);
    void RefreshFrameStatistics();
    void ConfigureCommandLineBenchmarkIfRequested();

    TArray<TWeakObjectPtr<UVirtualCameraCaptureComponent>> Cameras;
    TArray<TWeakObjectPtr<UVirtualLidarScanComponent>> Lidars;
    TWeakObjectPtr<UVirtualCameraCaptureComponent> PreferredCamera;
    TWeakObjectPtr<UVirtualLidarScanComponent> PreferredLidar;
    bool bPreferCameraOnNextAdmission = true;
    int32 NextCameraIndex = 0;
    int32 NextLidarIndex = 0;
    int32 AdaptiveLidarChunkSize = 256;
    float EffectiveLidarBudgetMs = 4.0f;
    float EffectiveAggregateCameraCaptureHz = 15.0f;
    double LastCameraCaptureAdmissionTime = -1.0;
    float TelemetryLogAccumulator = 0.0f;
    TArray<float> RecentFrameTimesMs;
    bool bCommandLineBenchmarkConfigured = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> CommandLineBenchmarkActors;

    UPROPERTY(Transient)
    FVirtualSensorPerformanceTelemetry Telemetry;
};
