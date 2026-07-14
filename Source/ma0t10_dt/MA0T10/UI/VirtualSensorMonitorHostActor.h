#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorMonitorHostActor.generated.h"

class AVirtualSensorManager;
class UVirtualSensorMonitorWidget;

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

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    UVirtualSensorMonitorWidget* GetMonitorWidget() const { return MonitorWidget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorMonitorWidget> GetEffectiveMonitorWidgetClass() const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    TSubclassOf<UVirtualSensorMonitorWidget> MonitorWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorMonitor")
    bool bUseNativeMonitorWidgetFallback = true;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorMonitor|Status")
    FString LastStatusMessage;

private:
    AVirtualSensorManager* ResolveSensorManager();

private:
    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorMonitorWidget> MonitorWidget;
};