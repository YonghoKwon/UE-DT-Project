#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VirtualSensorMonitorWidget.generated.h"

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

    void BuildWidgetTreeIfNeeded();
    void RefreshImageBrush();
    void RefreshTitle();

private:
    UPROPERTY(Transient)
    TObjectPtr<UVirtualCameraComp> CameraComp;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualLidarSensorComp> LidarComp;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> ViewImage;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ToggleButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ToggleButtonText;

    bool bShowingLidar = false;
};
