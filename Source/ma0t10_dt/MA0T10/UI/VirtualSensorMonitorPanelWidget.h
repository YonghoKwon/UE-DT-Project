#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "Styling/SlateBrush.h"
#include "VirtualSensorMonitorPanelWidget.generated.h"

class AVirtualSensorCoordinator;
class FRHIGPUTextureReadback;
class SImage;
class STextBlock;
class SWidget;
template <typename OptionType> class SComboBox;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class URealSensorSourceComponent;
class UVirtualCameraCaptureComponent;
class UVirtualLidarScanComponent;
class UVirtualLidarVisualizationComponent;

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
class MA0T10_DT_API UVirtualSensorMonitorPanelWidget : public UVirtualSensorPanelWidgetBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void BindVirtualCamera(UVirtualCameraCaptureComponent* InCameraComp);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void BindVirtualLidar(UVirtualLidarScanComponent* InLidarComp);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void BindSensorManager(AVirtualSensorCoordinator* InSensorManager);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void BindRealSensorSource(URealSensorSourceComponent* InRealSensorSourceComponent);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void ShowCameraView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void ShowLidarView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void ToggleView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void CycleLidarViewMode();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void SetLidarViewMode(EVirtualLidarViewMode InViewMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void SetLidarProjectionMode(ELidarMonitorProjectionMode InProjectionMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void SetLidarColorMode(ELidarColorMode InColorMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void SetLidarWorldPointCloudEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void SetLidarWorldTopDownAutoFit(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LidarView")
    void SetLidarPointSize(float InPointSize);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LidarView")
    ELidarMonitorProjectionMode GetLidarProjectionMode() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LidarView")
    ELidarColorMode GetLidarColorMode() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LidarView")
    FString GetLidarViewModeDescription() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LidarView")
    FString GetLidarViewLegendText() const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|UI")
    void ResetMonitorUiPreferencesToDefault();

    static FColor ResolveLidarPointDisplayColor(const UVirtualLidarScanComponent* InLidarComp, EVirtualLidarViewMode ViewMode, const FVirtualLidarPoint& Point, float NormalizedDistance);

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

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    const FString& GetLocalCaptureSessionDirectory() const { return LocalCaptureSessionDirectory; }

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    void ConfigureLocalCapture(const FVirtualSensorCaptureSelection& Selection);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    FVirtualSensorCaptureSelection GetLocalCaptureSelection() const { return LocalCaptureSelection; }

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    void CaptureConfiguredOutputsOnce();

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

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Camera")
	void SetDualCameraModeEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Camera")
	bool IsDualCameraModeEnabled() const { return bDualCameraModeEnabled; }

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Camera")
	bool SelectPrimaryCameraBySensorId(const FString& SensorId);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Camera")
	bool SelectSecondaryCameraBySensorId(const FString& SensorId);

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
    FString GetPointCloudRendererStatusText() const;

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
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

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
    void RestoreMonitorUiPreferences();
    void SaveMonitorUiPreferences() const;
    void SetLidarOverlayOptions(bool bAdaptiveDepth, bool bGrid, bool bDepthEdges);
    FString BuildCompactStatusText() const;
    FString BuildSemanticLegendText() const;
    FLinearColor GetLidarLegendSwatchColor(int32 SwatchIndex) const;
    FString BuildTitleText() const;
    FString BuildStatusText() const;
    FText BuildPointCloudOnlyButtonText() const;
    bool ShouldUseNativeFallbackWidget() const;
    FString GetLidarViewModeDisplayText() const;
    UObject* GetLidarBrushResource();
    UObject* GetSecondaryLidarBrushResource();
    UVirtualLidarVisualizationComponent* GetLidarVisualizationComponent() const;
    bool ResolveInteractiveProjection(const FVector2D& ScreenPosition, ELidarMonitorProjectionMode& OutProjection, FVector2D& OutViewportSize) const;
    UTexture2D* RebuildEnhancedLidarViewTexture();
    void InvalidateEnhancedLidarView();
	void RefreshCameraSelectionOptions();
	void ResolveDualCameraSelection();
    void CaptureLocalSensorFrame();
	void CaptureConfiguredFrame();
    bool SaveCameraSnapshotToDisk(const FString& FramePrefix);
    bool QueueCameraGpuReadbackToDisk(const FString& FramePrefix);
    bool SaveCameraSnapshotToDiskSynchronous(const FString& FramePrefix);
    void ProcessPendingCameraReadbacks();
    void StartAsyncCameraJpegWrite(TArray<FColor>&& RawPixels, int32 Width, int32 Height, const FString& Path);
    void RefreshLocalCaptureCameraPendingState();
    bool SaveLidarPointCloudToDisk(const FString& FramePrefix);
    bool RefreshLidarPreviewWithoutTransport();
    FString EnsureLocalCaptureSessionDirectory();
    UVirtualLidarScanComponent* GetTargetLidarForPreview() const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UVirtualCameraCaptureComponent> CameraComp;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualLidarScanComponent> LidarComp;

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorCoordinator> SensorManager;

    UPROPERTY(Transient)
    TObjectPtr<URealSensorSourceComponent> RealSensorSourceComponent;

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

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool bLocalCaptureSaveCameraPayload = false;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool bLocalCaptureSaveLidarPayload = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture|PointCloudFormat")
    bool bLocalCaptureSaveLidarCsv = true;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture|PointCloudFormat")
    bool bLocalCaptureSaveLidarJsonLines = false;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture|PointCloudFormat")
    bool bLocalCaptureSaveLidarPcd = false;

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
	bool bDualCameraModeEnabled = false;
	bool bMonitorPreferencesRestored = false;
    bool bMonitorDetailsExpanded = false;
    bool bLocalSensorCaptureActive = false;
	FVirtualSensorCaptureSelection LocalCaptureSelection;
	UPROPERTY(Transient) TObjectPtr<UVirtualCameraCaptureComponent> SecondaryCameraComp;
	FString PreferredPrimaryCameraId;
	FString PreferredSecondaryCameraId;
	int64 LastPrimaryCameraDisplayFrameId = INDEX_NONE;
	int64 LastSecondaryCameraDisplayFrameId = INDEX_NONE;
	bool bConfiguredOneShotPending = false;
	bool bConfiguredOneShotLidar = false;
	int64 ConfiguredOneShotStartFrameId = INDEX_NONE;
	double ConfiguredOneShotStartedSeconds = 0.0;
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
    TArray<TSharedPtr<EVirtualLidarViewMode>> NativeLidarViewModeOptions;
    TArray<TSharedPtr<ELidarMonitorProjectionMode>> NativeLidarProjectionOptions;
    TArray<TSharedPtr<ELidarColorMode>> NativeLidarColorOptions;
    TSharedPtr<STextBlock> NativeTitleTextBlock;
    TSharedPtr<STextBlock> NativeStatusTextBlock;
    TSharedPtr<STextBlock> NativeDetailedStatusTextBlock;
    TSharedPtr<STextBlock> NativeWarningTextBlock;
    TSharedPtr<SImage> NativeViewImage;
    TSharedPtr<SImage> NativeSecondaryViewImage;
	TSharedPtr<SImage> NativeSecondaryCameraImage;
	FSlateBrush NativeSecondaryCameraBrush;
	TArray<TSharedPtr<FString>> NativeCameraOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> NativePrimaryCameraCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> NativeSecondaryCameraCombo;
    FSlateBrush NativeViewBrush;
    FSlateBrush NativeSecondaryViewBrush;
    double StatusRefreshAccumulator = 0.0;
    bool bPanningLidarProjection = false;
    bool bRotatingLidarProjection = false;
    ELidarMonitorProjectionMode ActiveProjectionInteraction = ELidarMonitorProjectionMode::RangeImage;
    FVector2D ActiveProjectionViewportSize = FVector2D(1.0f, 1.0f);
};
