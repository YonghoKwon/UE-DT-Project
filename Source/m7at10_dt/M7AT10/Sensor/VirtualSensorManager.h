#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorManager.generated.h"

class UActorComponent;
class UVirtualCameraComp;
class UVirtualLidarSensorComp;
class UVirtualSensorMonitorWidget;
class UVirtualSensorDataTransportComp;

UENUM(BlueprintType)
enum class EVirtualSensorViewMode : uint8
{
    RealWorld UMETA(DisplayName = "Real World"),
    Camera UMETA(DisplayName = "Camera"),
    Lidar UMETA(DisplayName = "Lidar")
};

USTRUCT(BlueprintType)
struct M7AT10_DT_API FVirtualSensorSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|Sensor")
    FString SensorId;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|Sensor")
    FString SensorType;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|Sensor")
    int32 Index = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|Sensor")
    bool bValid = false;
};

USTRUCT(BlueprintType)
struct M7AT10_DT_API FVirtualSensorHealthSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    FString Summary;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 CameraCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 LidarCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    int32 StaleSensorCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorHealth")
    bool bHealthy = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorViewModeChanged, EVirtualSensorViewMode, NewMode);

UCLASS()
class M7AT10_DT_API AVirtualSensorManager : public AActor
{
    GENERATED_BODY()

public:
    AVirtualSensorManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void DiscoverSensorsInLevel();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void RegisterCamera(UVirtualCameraComp* CameraComp);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void RegisterLidar(UVirtualLidarSensorComp* LidarComp);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void BindMonitorWidget(UVirtualSensorMonitorWidget* MonitorWidget);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectCameraByIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectLidarByIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectNextCamera();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectNextLidar();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SetViewMode(EVirtualSensorViewMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void ToggleSensorView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void StartAllSensors();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void StopAllSensors();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void CaptureAllOnce();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    UVirtualCameraComp* GetSelectedCamera() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    UVirtualLidarSensorComp* GetSelectedLidar() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    TArray<FVirtualSensorSummary> GetCameraSummaries() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    TArray<FVirtualSensorSummary> GetLidarSummaries() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    FVirtualSensorHealthSummary GetHealthSummary() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    EVirtualSensorViewMode GetViewMode() const { return CurrentViewMode; }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|SensorManager")
    FOnVirtualSensorViewModeChanged OnViewModeChanged;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager")
    bool bDiscoverOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager")
    bool bStartSensorsOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager")
    bool bUseSynchronizedCapture = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager", meta = (ClampMin = "0.033"))
    float SynchronizedInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager|Health", meta = (ClampMin = "0.1"))
    float StaleSensorSeconds = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager")
    TObjectPtr<UVirtualSensorDataTransportComp> SharedTransportComponent;

private:
    void ApplyWidgetBinding();
    void RunSynchronizedCapture();
    void AssignSharedTransportIfPossible(UActorComponent* SensorComp);

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UVirtualCameraComp>> Cameras;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UVirtualLidarSensorComp>> Lidars;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorMonitorWidget> BoundMonitorWidget;

    int32 SelectedCameraIndex = 0;
    int32 SelectedLidarIndex = 0;
    EVirtualSensorViewMode CurrentViewMode = EVirtualSensorViewMode::Camera;
    FTimerHandle SynchronizedTimerHandle;
};
