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

protected:
    virtual void NativeConstruct() override;
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

    void RefreshImageBrush();
    void RefreshTitle();
    void RefreshStatusText();

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
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ToggleButtonText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StatusText;

    bool bShowingLidar = false;
};
