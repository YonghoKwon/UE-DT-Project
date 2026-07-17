#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/StatusVisualizerCompBase.h"
#include "VirtualLidarSensorTypes.h"
#include "VirtualLidarVisualizationComponent.generated.h"

class UTexture2D;
class UNiagaraComponent;
class UNiagaraSystem;
class UVirtualLidarScanComponent;

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualLidarVisualizationComponent : public UStatusVisualizerCompBase
{
    GENERATED_BODY()

public:
    UVirtualLidarVisualizationComponent();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void BindScanComponent(UVirtualLidarScanComponent* InScanComponent);
    void RefreshLatestFrame();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void SetVisualizationSettings(const FVirtualLidarVisualizationSettings& InSettings);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void SetProjectionMode(ELidarMonitorProjectionMode InProjectionMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void SetColorMode(ELidarColorMode InColorMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void SetWorldPointCloudEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void SetPointCloudRenderPolicy(ELidarPointCloudRenderPolicy InPolicy);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void PanProjectionView(ELidarMonitorProjectionMode ProjectionMode, FVector2D PixelDelta, FVector2D ViewportSize);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void RotateProjectionView(ELidarMonitorProjectionMode ProjectionMode, float DeltaDegrees);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void ZoomProjectionView(ELidarMonitorProjectionMode ProjectionMode, float ZoomFactor);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Visualization")
    void ResetProjectionView(ELidarMonitorProjectionMode ProjectionMode);

    void SetForceCpuFallbackForBenchmark(bool bForce) { bForceCpuFallbackForBenchmark = bForce; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    const FVirtualLidarVisualizationSettings& GetVisualizationSettings() const { return Settings; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    UTexture2D* GetPreviewTexture() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    UTexture2D* GetSecondaryPreviewTexture() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    int32 GetVisiblePointCount() const { return VisiblePointCount; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    const FVirtualLidarRendererTelemetry& GetRendererTelemetry() const { return RendererTelemetry; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    FString GetLegendText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    FString GetActiveRendererName() const { return ActiveRendererName; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    FString GetRendererFallbackReason() const { return RendererFallbackReason; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
    int32 GetTextureResourceCreateCount() const { return TextureResourceCreateCount; }

    static ELidarColorMode MapLegacyViewMode(EVirtualLidarViewMode LegacyMode);
    static EVirtualLidarViewMode MapColorModeToLegacy(ELidarColorMode ColorMode);
    static FColor ResolveDisplayColor(
        const UVirtualLidarScanComponent* ScanComponent,
        ELidarColorMode ColorMode,
        const FVirtualLidarPoint& Point,
        float NormalizedDistance,
        float NormalizedHeight);
    static FIntPoint ProjectTopDown(const FVector& SensorLocalPoint, float MaxDistanceCm, int32 Resolution);
    static FIntPoint ProjectElevation(const FVector& SensorLocalPoint, float MaxDistanceCm, float MinHeightCm, float MaxHeightCm, int32 Width, int32 Height);
    static ELidarPointCloudRendererState ResolveRendererState(
        ELidarPointCloudRenderPolicy Policy,
        bool bWorldPointCloudEnabled,
        int32 HitPointCount,
        bool bNiagaraAvailable,
        bool bNiagaraReady);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization")
    FVirtualLidarVisualizationSettings Settings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization")
    ELidarPointCloudRenderPolicy PointCloudRenderPolicy = ELidarPointCloudRenderPolicy::AutoPreferNiagara;

private:
    void RebuildProjectionTextures();
    void StartProjectionBuild();
    void BuildRangeImage(TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight);
    void BuildTopDownImage(TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight);
    void BuildElevationImage(TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight);
    UTexture2D* UploadTexture(TObjectPtr<UTexture2D>& Texture, const TArray<FColor>& Pixels, int32 Width, int32 Height);
    void RefreshWorldPointCloud();
    bool TryRefreshNiagaraPointCloud();
    void RefreshCpuPointCloud(const FString& Reason, bool bIsError = false);
    void UpdateRendererTelemetry(ELidarPointCloudRendererState State, int32 UploadedPoints, int32 VisiblePoints, const FString& Renderer, const FString& Message);

    UPROPERTY(Transient)
    TObjectPtr<UVirtualLidarScanComponent> ScanComponent;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> RangeTexture;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> TopDownTexture;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> ElevationTexture;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> NiagaraPointCloudComponent;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|VirtualLidar|Visualization")
    TSoftObjectPtr<UNiagaraSystem> NiagaraPointCloudSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/MA0T10/Sensor/VFX/NS_VirtualLidarPointCloud.NS_VirtualLidarPointCloud")));

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|VirtualLidar|Visualization", meta = (ClampMin = "1", ClampMax = "21600"))
    int32 MaxNiagaraPreviewPoints = 21600;

    int32 VisiblePointCount = 0;
    int32 TextureResourceCreateCount = 0;
    float LastMinDistanceCm = 0.0f;
    float LastMaxDistanceCm = 0.0f;
    float LastMinHeightCm = 0.0f;
    float LastMaxHeightCm = 0.0f;
    FString ActiveRendererName = TEXT("CPU ISM fallback");
    FString RendererFallbackReason;
    FVirtualLidarRendererTelemetry RendererTelemetry;
    TArray<FVector> NiagaraPositions;
    TArray<FLinearColor> NiagaraColors;
    uint64 ProjectionGeneration = 0;
    bool bProjectionBuildInFlight = false;
    bool bForceCpuFallbackForBenchmark = false;
    double NextNiagaraRetryTimeSeconds = 0.0;
    int64 LastVisualizedFrameId = INDEX_NONE;
};
