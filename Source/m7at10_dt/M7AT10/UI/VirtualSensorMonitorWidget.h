#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VirtualSensorMonitorWidget.generated.h"

class AVirtualSensorManager;
class UButton;
class UImage;
class UTextBlock;
class UVirtualCameraComp;
class UVirtualLidarSensorComp;

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

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    void ToggleLocalSensorCapture();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    bool IsLocalSensorCaptureActive() const { return bLocalSensorCaptureActive; }

protected:
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
    void HandleLogPointCloudButtonClicked();

    UFUNCTION()
    void HandleExportPointCloudButtonClicked();

    UFUNCTION()
    void HandleLocalSensorCaptureButtonClicked();

    void RefreshImageBrush();
    void RefreshTitle();
    void RefreshStatusText();
    void RefreshLocalCaptureButtonText();
    void CaptureLocalSensorFrame();
    bool SaveCameraSnapshotToDisk(const FString& FramePrefix);
    bool SaveLidarPointCloudToDisk(const FString& FramePrefix);
    bool RefreshLidarPreviewWithoutTransport();
    FString EnsureLocalCaptureSessionDirectory();

private:
    UPROPERTY(Transient)
    TObjectPtr<UVirtualCameraComp> CameraComp;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualLidarSensorComp> LidarComp;

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorManager> SensorManager;

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
    TObjectPtr<UButton> LogPointCloudButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ExportPointCloudButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LocalSensorCaptureButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ToggleButtonText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> LocalSensorCaptureButtonText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture", meta = (ClampMin = "0.05"))
    float LocalCaptureIntervalSeconds = 1.0f;

    UPROPERTY(EditAnywhere, Category = "DigitalTwin|SensorMonitor|LocalCapture")
    FString LocalCaptureFolderName = TEXT("LocalTimedCapture");

    bool bShowingLidar = false;
    bool bLocalSensorCaptureActive = false;
    int32 LocalCaptureFrameIndex = 0;
    FString LocalCaptureSessionDirectory;
    FTimerHandle LocalSensorCaptureTimerHandle;
};
