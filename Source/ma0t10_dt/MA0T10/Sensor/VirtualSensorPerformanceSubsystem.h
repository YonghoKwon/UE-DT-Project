#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VirtualSensorPerformanceSubsystem.generated.h"

class UVirtualCameraComp;
class UVirtualLidarSensorComp;
class AActor;

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
class MA0T10_DT_API UVirtualSensorPerformanceSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    void RegisterCamera(UVirtualCameraComp* Camera);
    void UnregisterCamera(UVirtualCameraComp* Camera);
    void RegisterLidar(UVirtualLidarSensorComp* Lidar);
    void UnregisterLidar(UVirtualLidarSensorComp* Lidar);
    void SetPreferredCamera(UVirtualCameraComp* Camera);
    void SetPreferredLidar(UVirtualLidarSensorComp* Lidar);
    bool ShouldRefreshLidarPreview(const UVirtualLidarSensorComp* Lidar) const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPerformance")
    const FVirtualSensorPerformanceTelemetry& GetTelemetry() const { return Telemetry; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPerformance")
    FString GetTelemetrySummaryText() const;

    static int32 ResolveTargetFps(int32 CameraCount, int32 LidarCount);
    static float ResolveLidarBudgetMs(int32 TargetFps);
    static bool IsBestEffortConfiguration(int32 CameraCount, int32 LidarCount);

private:
    void CompactRegistrations();
    void RefreshTelemetry(float WorkMs);
    void RefreshFrameStatistics();
    void ConfigureCommandLineBenchmarkIfRequested();

    TArray<TWeakObjectPtr<UVirtualCameraComp>> Cameras;
    TArray<TWeakObjectPtr<UVirtualLidarSensorComp>> Lidars;
    TWeakObjectPtr<UVirtualCameraComp> PreferredCamera;
    TWeakObjectPtr<UVirtualLidarSensorComp> PreferredLidar;
    bool bPreferCameraOnNextAdmission = true;
    int32 NextCameraIndex = 0;
    int32 NextLidarIndex = 0;
    int32 AdaptiveLidarChunkSize = 256;
    float EffectiveLidarBudgetMs = 4.0f;
    double LastCameraCaptureAdmissionTime = -1.0;
    float TelemetryLogAccumulator = 0.0f;
    TArray<float> RecentFrameTimesMs;
    bool bCommandLineBenchmarkConfigured = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> CommandLineBenchmarkActors;

    UPROPERTY(Transient)
    FVirtualSensorPerformanceTelemetry Telemetry;
};
