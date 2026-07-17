#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorUiHostActor.generated.h"

class AVirtualSensorCoordinator;
class UVirtualSensorMonitorPanelWidget;
class UVirtualSensorSettingsPanelWidget;
class UVirtualSensorCaptureExportPanelWidget;
class UVirtualSensorPanelHostComponent;

UCLASS(BlueprintType)
class MA0T10_DT_API AVirtualSensorUiHostActor : public AActor
{
    GENERATED_BODY()

public:
    AVirtualSensorUiHostActor();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorMonitorPanelWidget* CreateAndBindMonitorWidget();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void DestroyMonitorWidget();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void CreateAndBindToolWidgets();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void QueueSensorStateForMapApply(const FVirtualSensorEditableState& SensorState);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
    void ResetAllPanelUiPreferences();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Debug")
    void SetScreenDebugLogVisible(bool bVisible);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Debug")
    bool IsScreenDebugLogVisible() const { return bScreenDebugLogVisible; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorMonitorPanelWidget* GetMonitorWidget() const { return MonitorWidget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorSettingsPanelWidget* GetSettingsWidget() const { return SettingsWidget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorCaptureExportPanelWidget* GetCaptureExportWidget() const { return CaptureExportWidget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorMonitorPanelWidget> GetEffectiveMonitorWidgetClass() const;

    const FVirtualSensorMapApplySnapshot& GetPendingMapApplySnapshot() const { return PendingMapApplySnapshot; }
    bool HasPendingMapApplySnapshot() const { return PendingMapApplySnapshot.SensorStates.Num() > 0; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorMonitorPanelWidget> MonitorWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorSettingsPanelWidget> SettingsWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorCaptureExportPanelWidget> CaptureExportWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bUseNativeMonitorWidgetFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bUseNativeToolWidgetFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bAutoCreateToolPanels = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TObjectPtr<AVirtualSensorCoordinator> SensorManager;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bAutoCreateOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bAutoFindSensorManager = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bAllowViewportFallback = true;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor|Debug")
    bool bHideScreenDebugLogOnBeginPlay = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Status")
    FString LastStatusMessage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor")
    TObjectPtr<UVirtualSensorPanelHostComponent> PanelHostComponent;

private:
    AVirtualSensorCoordinator* ResolveSensorManager();
    void ConfigurePlayerInput();

private:
    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorMonitorPanelWidget> MonitorWidget;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorSettingsPanelWidget> SettingsWidget;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorCaptureExportPanelWidget> CaptureExportWidget;

    UPROPERTY(Transient)
    FVirtualSensorMapApplySnapshot PendingMapApplySnapshot;

    bool bScreenDebugLogVisible = true;
    bool bSavedScreenDebugLogState = false;
    bool bHasScreenDebugLogOverride = false;
};
