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
class URealSensorSourceComp;
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

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorMonitorDisplayData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    bool bShowingLidar = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString TitleText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString SelectedSensorText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString FrameText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString MeasurementText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString ServerPayloadText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString PreviewText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString SlabText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString LazExportText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString TransportText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString WarningText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString ViewModeText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString AcceptanceGateText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString RealSensorText;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Display")
    FString FullStatusText;
};

UCLASS()
class MA0T10_DT_API UVirtualSensorMonitorWidget : public UUserWidget
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
    void BindRealSensorSource(URealSensorSourceComp* InRealSensorSourceComp);

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

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|RealSensor")
    int32 StartRealSensorSources();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|RealSensor")
    void StopRealSensorSources();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|RealSensor")
    bool PushSelectedRealSensorSourceOnce(bool bSendTransport = true);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Export")
    bool ExportSelectedLidarServerPayload(const FString& FileNamePrefix = TEXT("manual_server_payload"));

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Export")
    bool ExportSelectedSensorServerPayload(const FString& FileNamePrefix = TEXT("manual_server_payload"));

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    void ToggleLocalSensorCapture();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool IsLocalSensorCaptureActive() const { return bLocalSensorCaptureActive; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    bool IsShowingLidar() const { return bShowingLidar; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    bool HasBoundCamera() const { return CameraComp != nullptr; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    bool HasBoundLidar() const { return LidarComp != nullptr; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetMonitorTitleText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetMonitorStatusText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FVirtualSensorMonitorDisplayData GetMonitorDisplayData() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetSelectedSensorIdText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetFrameSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetMeasurementSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetServerPayloadSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetPreviewPolicySummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetSlabAnalysisSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetLazExportSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetTransportStatusSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetTransportWarningText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetViewModeSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetAcceptanceGateSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    FString GetRealSensorDeploymentSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    const FString& GetLastManualExportPath() const { return LastManualExportPath; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    const FString& GetLastManualExportMessage() const { return LastManualExportMessage; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Status")
    const FString& GetLastRealSensorControlMessage() const { return LastRealSensorControlMessage; }

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
    void HandleExportServerPayloadButtonClicked();

    UFUNCTION()
    void HandlePreviewMoreButtonClicked();

    UFUNCTION()
    void HandlePreviewLessButtonClicked();

    UFUNCTION()
    void HandlePreviewHitOnlyButtonClicked();

    UFUNCTION()
    void HandleStartRealSensorSourcesButtonClicked();

    UFUNCTION()
    void HandleStopRealSensorSourcesButtonClicked();

    UFUNCTION()
    void HandlePushRealSensorSourceButtonClicked();

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
    void RefreshLocalCaptureCameraPendingState();
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
    TObjectPtr<URealSensorSourceComp> RealSensorSourceComp;

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
    TObjectPtr<UButton> ExportServerPayloadButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PreviewMoreButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PreviewLessButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PreviewHitOnlyButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> StartRealSensorSourcesButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> StopRealSensorSourcesButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> PushRealSensorSourceButton;

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

    // Writes a LAS-compatible source by default; can produce LAZ only when the selected LiDAR has an external compressor configured.
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
    int32 LocalCaptureCameraAsyncWriteCount = 0;
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
    FString LastManualExportMessage;
    FString LastManualExportPath;
    FString LastRealSensorControlMessage;
    FTimerHandle LocalSensorCaptureTimerHandle;
    TArray<FVirtualSensorPendingCameraReadback> PendingCameraReadbacks;
    TSharedPtr<STextBlock> NativeTitleTextBlock;
    TSharedPtr<STextBlock> NativeStatusTextBlock;
};
