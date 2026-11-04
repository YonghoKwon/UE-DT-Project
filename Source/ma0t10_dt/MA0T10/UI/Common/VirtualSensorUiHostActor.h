#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorControlTypes.h"
#include "VirtualSensorUiHostActor.generated.h"

class AVirtualSensorCoordinator;
class UDxWidgetVirtualSensorMonitorPanel;
class UDxWidgetVirtualSensorSettingsPanel;
class UDxWidgetVirtualSensorCaptureExportPanel;
class UDxWidgetVirtualSensorPlacementPanel;
class UVirtualSensorPanelHostComp;

UCLASS(BlueprintType)
class MA0T10_DT_API AVirtualSensorUiHostActor : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Appearance")
	void SetSensorToolFontScale(float Scale);
	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Appearance")
	void ResetSensorToolAppearance();
	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Appearance")
	float GetSensorToolFontScale() const;

private:
	float SensorToolFontScale = 1.0f;

public:
	AVirtualSensorUiHostActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
	UDxWidgetVirtualSensorMonitorPanel* CreateAndBindMonitorWidget();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
	void DestroyMonitorWidget();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
	void CreateAndBindToolWidgets();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPlacement")
	bool EnsurePlacementPanel(FString& OutReason);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPlacement")
	bool ShowPlacementPanel(FString& OutReason);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
	void QueueSensorStateForMapApply(const FVirtualSensorEditableState& SensorState);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor")
	void ResetAllPanelUiPreferences();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorMonitor|Debug")
	void SetScreenDebugLogVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor|Debug")
	bool IsScreenDebugLogVisible() const { return bScreenDebugLogVisible; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
	UDxWidgetVirtualSensorMonitorPanel* GetMonitorWidget() const { return MonitorWidget; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
	UDxWidgetVirtualSensorSettingsPanel* GetSettingsWidget() const { return SettingsWidget; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
	UDxWidgetVirtualSensorCaptureExportPanel* GetCaptureExportWidget() const { return CaptureExportWidget; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPlacement")
	UDxWidgetVirtualSensorPlacementPanel* GetPlacementWidget() const { return PlacementWidget; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
	TSubclassOf<UDxWidgetVirtualSensorMonitorPanel> GetEffectiveMonitorWidgetClass() const;

	const FVirtualSensorMapApplySnapshot& GetPendingMapApplySnapshot() const { return PendingMapApplySnapshot; }
	bool HasPendingMapApplySnapshot() const { return PendingMapApplySnapshot.SensorStates.Num() > 0; }
	bool CanQueueMapApply() const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
	TSubclassOf<UDxWidgetVirtualSensorMonitorPanel> MonitorWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
	TSubclassOf<UDxWidgetVirtualSensorSettingsPanel> SettingsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
	TSubclassOf<UDxWidgetVirtualSensorCaptureExportPanel> CaptureExportWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorPlacement")
	TSubclassOf<UDxWidgetVirtualSensorPlacementPanel> PlacementWidgetClass;

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
	TObjectPtr<UVirtualSensorPanelHostComp> PanelHostComp;

private:
	AVirtualSensorCoordinator* ResolveSensorManager();
	void ConfigurePlayerInput();
	void SchedulePlacementRetry();
	void RetryPlacementInitialization();
	FTimerHandle PlacementRetryTimer;
	int32 PlacementRetryAttempts = 0;
	bool bPlacementOpenRequested = false;
	bool bHostEnding = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDxWidgetVirtualSensorMonitorPanel> MonitorWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDxWidgetVirtualSensorSettingsPanel> SettingsWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDxWidgetVirtualSensorCaptureExportPanel> CaptureExportWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDxWidgetVirtualSensorPlacementPanel> PlacementWidget;

	UPROPERTY(Transient)
	FVirtualSensorMapApplySnapshot PendingMapApplySnapshot;

	bool bScreenDebugLogVisible = true;
	bool bSavedScreenDebugLogState = false;
	bool bHasScreenDebugLogOverride = false;
};
