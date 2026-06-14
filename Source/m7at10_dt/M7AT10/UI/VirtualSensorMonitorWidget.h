#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VirtualSensorMonitorWidget.generated.h"

class AVirtualSensorManager;
class FRHIGPUTextureReadback;
class STextBlock;
class SWidget;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UVirtualCameraComp;
class UVirtualLidarSensorComp;

enum class EVirtualLidarViewMode : uint8;

struct FVirtualSensorPendingCameraReadback
{
    TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback;
    FString OutputPath;
    int32 Width = 0;
    int32 Height = 0;
};

UCLASS()
class M7AT10_DT_API UVirtualSensorMonitorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void BindVirtualCamera(UVirtualCameraComp* InCameraComp);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void BindVirtualLidar(UVirtualLidarSensorComp* InLidarComp);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void BindSensorManager(AVirtualSensorManager* InSensorManager);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void ShowCameraView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void ShowLidarView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void ToggleView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void CycleLidarViewMode();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void SetLidarPreviewBudget(int32 InStride, int32 InMaxPoints);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void IncreaseLidarPreviewBudget();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void DecreaseLidarPreviewBudget();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void ToggleLidarPreviewHitOnly();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Capture")
    void CaptureSelectedSensorsOnce();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    void ToggleLocalSensorCapture();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool IsLocalSensorCaptureActive() const { return bLocalSensorCaptureActive; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetMonitorTitleText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetMonitorStatusText() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleToggleButtonClicked();

    UFUNCTION()
    void HandleNextCameraButtonClicked();

    UFUNCTION()
    void HandleNextLidarButtonClicked();

    UFUNCTION()
    void HandlePointCloudOnlyButtonClicked();

    UFUNCTION()
    void HandleLidarViewModeButtonClicked();

    UFUNCTION()
    void HandleLogPointCloudButtonClicked();

    UFUNCTION()
    void HandleExportPointCloudButtonClicked();

    UFUNCTION()
    void HandleLocalSensorCaptureButtonClicked();

    UFUNCTION()
    void HandleCaptureOnceButtonClicked();

    UFUNCTION()
    void HandlePreviewMoreButtonClicked();

    UFUNCTION()
    void HandlePreviewLessButtonClicked();

    UFUNCTION()
    void HandlePreviewHitOnlyButtonClicked();

    void RefreshImageBrush();
    void RefreshTitle();
    void RefreshStatusText();
    void RefreshNativeFallbackText();
    void RefreshLocalCaptureButtonText();
    void RefreshLidarViewModeButtonText();
    FString BuildTitleText() const;
    FString BuildStatusText() const;
    bool ShouldUseNativeFallbackWidget() const;
    FString GetLidarViewModeDisplayText() const;
    FString GetLidarViewLegendText() const;
    UObject* GetLidarBrushResource();
    UTexture2D* RebuildEnhancedLidarViewTexture();
    void InvalidateEnhancedLidarView();
    void CaptureLocalSensorFrame();
    bool SaveCameraSnapshotToDisk(const FString& FramePrefix);
    bool QueueCameraGpuReadbackToDisk(const FString& FramePrefix);
    bool SaveCameraSnapshotToDiskSynchronous(const FString& FramePrefix);
    void ProcessPendingCameraReadbacks();
    void StartAsyncCameraJpegWrite(TArray<FColor>&& RawPixels, int32 Width, int32 Height, const FString& Path);
    bool SaveLidarPointCloudToDisk(const FString& FramePrefix);
    bool RefreshLidarPreviewWithoutTransport();
    FString EnsureLocalCaptureSessionDirectory();
    UVirtualLidarSensorComp* GetTargetLidarForPreview() const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UVirtualCameraComp> CameraComp;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualLidarSensorComp> LidarComp;

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorManager> SensorManager;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> EnhancedLidarViewTexture;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> ViewImage;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ToggleButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> NextCameraButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> NextLidarButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PointCloudOnlyButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LidarViewModeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LogPointCloudButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ExportPointCloudButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LocalSensorCaptureButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> CaptureOnceButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PreviewMoreButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PreviewLessButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PreviewHitOnlyButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ToggleButtonText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> LidarViewModeButtonText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> LocalSensorCaptureButtonText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PreviewHitOnlyButtonText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView")
    bool bUseEnhancedLidarMonitorView = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView")
    bool bUseAdaptiveLidarDepthRange = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView")
    bool bOverlayLidarMonitorGrid = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView")
    bool bOverlayLidarDepthEdges = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView", meta = (ClampMin = "1", ClampMax = "512"))
    int32 LidarMonitorGridColumnStep = 32;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView", meta = (ClampMin = "1", ClampMax = "128"))
    int32 LidarMonitorGridRowStep = 4;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView", meta = (ClampMin = "0.1", ClampMax = "1000.0"))
    float LidarDepthEdgeThreshold = 10.0f;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView")
    FLinearColor LidarDepthEdgeColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView", meta = (ClampMin = "1", ClampMax = "20"))
    int32 PreviewStrideMin = 1;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView", meta = (ClampMin = "1", ClampMax = "64"))
    int32 PreviewStrideMax = 20;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView", meta = (ClampMin = "100", ClampMax = "200000"))
    int32 PreviewBudgetMinPoints = 500;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView", meta = (ClampMin = "100", ClampMax = "2000000"))
    int32 PreviewBudgetMaxPoints = 20000;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LidarView", meta = (ClampMin = "100", ClampMax = "100000"))
    int32 PreviewBudgetStepPoints = 1000;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture", meta = (ClampMin = "0.05"))
    float LocalCaptureIntervalSeconds = 1.0f;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    FString LocalCaptureFolderName = TEXT("LocalTimedCapture");

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool bLocalCaptureSaveCameraFrames = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool bLocalCaptureSaveLidarPointCloud = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture|PointCloudFormat")
    bool bLocalCaptureSaveLidarCsv = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture|PointCloudFormat")
    bool bLocalCaptureSaveLidarLas = false;

    // Writes an LAS-compatible source file with the suffix .laz_source.las until a true LAZ compressor is integrated.
    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture|PointCloudFormat")
    bool bLocalCaptureSaveLidarLaz = false;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool bLocalCaptureUseCachedSensorFrames = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool bSkipLocalCaptureWhenWritePending = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool bUseGpuAsyncCameraReadback = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxPendingCameraReadbacks = 1;

    bool bShowingLidar = false;
    bool bLocalSensorCaptureActive = false;
    bool bLocalCaptureCameraWritePending = false;
    bool bLocalCaptureLidarWritePending = false;
    int32 LocalCaptureFrameIndex = 0;
    int64 LastEnhancedLidarFrameId = INDEX_NONE;
    int32 LastEnhancedLidarWidth = 0;
    int32 LastEnhancedLidarHeight = 0;
    uint8 LastEnhancedLidarViewMode = 255;
    bool bLastEnhancedAdaptiveDepth = false;
    bool bLastEnhancedGrid = false;
    bool bLastEnhancedEdges = false;
    float LastEnhancedEdgeThreshold = -1.0f;
    FLinearColor LastEnhancedEdgeColor = FLinearColor::Transparent;
    FString LocalCaptureSessionDirectory;
    FTimerHandle LocalSensorCaptureTimerHandle;
    TArray<FVirtualSensorPendingCameraReadback> PendingCameraReadbacks;
    TSharedPtr<STextBlock> NativeTitleTextBlock;
    TSharedPtr<STextBlock> NativeStatusTextBlock;
};
