#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorMonitorHostActor.generated.h"

class AVirtualSensorManager;
class UVirtualSensorMonitorWidget;
class UVirtualSensorSettingsWidget;
class UVirtualSensorCaptureExportWidget;

UCLASS(BlueprintType)
class MA0T10_DT_API AVirtualSensorMonitorHostActor : public AActor
{
    GENERATED_BODY()

public:
    AVirtualSensorMonitorHostActor();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorMonitorWidget* CreateAndBindMonitorWidget();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void DestroyMonitorWidget();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void CreateAndBindToolWidgets();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void QueueSensorStateForMapApply(const FVirtualSensorEditableState& SensorState);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void ResetAllPanelUiPreferences();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorMonitorWidget* GetMonitorWidget() const { return MonitorWidget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorSettingsWidget* GetSettingsWidget() const { return SettingsWidget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorCaptureExportWidget* GetCaptureExportWidget() const { return CaptureExportWidget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorMonitorWidget> GetEffectiveMonitorWidgetClass() const;

    const FVirtualSensorMapApplySnapshot& GetPendingMapApplySnapshot() const { return PendingMapApplySnapshot; }
    bool HasPendingMapApplySnapshot() const { return PendingMapApplySnapshot.SensorStates.Num() > 0; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorMonitorWidget> MonitorWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorSettingsWidget> SettingsWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorCaptureExportWidget> CaptureExportWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bUseNativeMonitorWidgetFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bUseNativeToolWidgetFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bAutoCreateToolPanels = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TObjectPtr<AVirtualSensorManager> SensorManager;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bAutoCreateOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bAutoFindSensorManager = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bAddToViewport = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bShowLidarViewOnStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bEnablePointCloudOnlyOnStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    int32 ViewportZOrder = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    int32 SettingsViewportZOrder = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    int32 CaptureExportViewportZOrder = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor|Input")
    bool bConfigurePlayerInputOnCreate = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor|Input", meta = (EditCondition = "bConfigurePlayerInputOnCreate"))
    bool bShowMouseCursorOnCreate = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Status")
    FString LastStatusMessage;

private:
    AVirtualSensorManager* ResolveSensorManager();
    void ConfigurePlayerInput();

private:
    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorMonitorWidget> MonitorWidget;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorSettingsWidget> SettingsWidget;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorCaptureExportWidget> CaptureExportWidget;

    UPROPERTY(Transient)
    FVirtualSensorMapApplySnapshot PendingMapApplySnapshot;
};
