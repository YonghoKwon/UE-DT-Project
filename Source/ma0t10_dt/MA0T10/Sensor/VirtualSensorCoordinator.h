#pragma once

#include "CoreMinimal.h"
#include "DeviceSensorCoordinator.h"
#include "VirtualSensorDeviceProfileTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorCoordinator.generated.h"

class UActorComponent;
class APlayerController;
class AVirtualSensorActorBase;
class UPrimitiveComponent;
class URealSensorSourceComponent;
class UVirtualCameraCaptureComponent;
class UVirtualLidarScanComponent;
class UVirtualSensorMonitorPanelWidget;
class UVirtualSensorTransportComponent;
class UVirtualSensorRecorderComponent;
class UVirtualSensorStreamPublisherComponent;
class IVirtualSensorMonitorView;

UCLASS()
class MA0T10_DT_API AVirtualSensorCoordinator : public ADeviceSensorCoordinator
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="DigitalTwin|Sensor|PCD")
    bool SetPcdBandwidthOptions(const FString& SensorId,const FVirtualPcdSendOptions& Options,bool bApplyToAll,FString& Error);
    AVirtualSensorCoordinator();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void DiscoverSensorsInLevel();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorCoordinator")
    void RegisterSensorActor(AVirtualSensorActorBase* SensorActor);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorCoordinator") void UnregisterSensorActor(AVirtualSensorActorBase* SensorActor);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorCoordinator") bool SelectSensorActor(AVirtualSensorActorBase* SensorActor);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void RegisterCamera(UVirtualCameraCaptureComponent* CameraComp);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void RegisterLidar(UVirtualLidarScanComponent* LidarComp);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void RegisterRealSensorSource(URealSensorSourceComponent* RealSensorSourceComponent);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void BindMonitorWidget(UVirtualSensorMonitorPanelWidget* MonitorWidget);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectCameraByIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectLidarByIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectRealSensorSourceByIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectNextCamera();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SelectNextLidar();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SetViewMode(EVirtualSensorViewMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void SetPointCloudOnlyMode(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void ToggleSensorView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void TogglePointCloudOnlyView();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void StartAllSensors();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void StopAllSensors();
    /** Apply through each native Actor contract; locked sensors are preserved. */
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorManager")
    int32 SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend Backend);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager|RealSensor")
    int32 StartAllRealSensorSources();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager|RealSensor")
    void StopAllRealSensorSources();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager|RealSensor")
    bool PushSelectedRealSensorSourceOnce(bool bSendTransport = true);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void CaptureAllOnce();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void CaptureSelectedOnce();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager")
    void RefreshSelectedSensorOnce(bool bLidar);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager|PointCloudPreview")
    void SetSelectedLidarPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorManager|PointCloudPreview")
    void AdjustSelectedLidarPreviewBudget(int32 StrideDelta, int32 MaxPointsDelta);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    UVirtualCameraCaptureComponent* GetSelectedCamera() const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
	int32 GetCameraCount() const { return Super::GetCameraCount(); }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
	UVirtualCameraCaptureComponent* GetCameraByIndex(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
	int32 FindCameraIndexBySensorId(const FString& SensorId) const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    UVirtualLidarScanComponent* GetSelectedLidar() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    URealSensorSourceComponent* GetSelectedRealSensorSource() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorCoordinator")
    AVirtualSensorActorBase* GetSelectedSensorActor() const;

    /** Returns the selected actor for the requested sensor kind, independently of the monitor view mode. */
    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorCoordinator")
    AVirtualSensorActorBase* GetSelectedSensorActorByKind(EVirtualSensorKind SensorKind) const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorCoordinator")
    TArray<AVirtualSensorActorBase*> GetSensorActors() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    TArray<FVirtualSensorSummary> GetCameraSummaries() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    TArray<FVirtualSensorSummary> GetLidarSummaries() const;

    const TArray<TObjectPtr<UVirtualLidarScanComponent>>& GetLidarComponents() const { return Super::GetLidarComponents(); }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    TArray<FVirtualSensorSummary> GetRealSensorSourceSummaries() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    FVirtualSensorHealthSummary GetHealthSummary() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    EVirtualSensorViewMode GetViewMode() const { return Super::GetViewMode(); }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorManager")
    bool IsPointCloudOnlyModeEnabled() const { return Super::IsPointCloudOnlyModeEnabled(); }

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager|PointCloudOnly")
    bool bPointCloudOnlyHideWorld = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager|PointCloudOnly")
    bool bPointCloudOnlyAutoSelectLidarView = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager|PointCloudOnly")
    TArray<FName> PointCloudOnlyKeepActorTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager")
    TObjectPtr<UVirtualSensorTransportComponent> SharedTransportComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager")
    TObjectPtr<UVirtualSensorRecorderComponent> SharedRecorderComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorManager")
    TObjectPtr<UVirtualSensorStreamPublisherComponent> StreamPublisherComponent;

protected:
    virtual FDeviceSensorCoordinatorConfig GetDeviceCoordinatorConfig() const override;
    virtual void NotifyDeviceViewModeChanged(EVirtualSensorViewMode Mode) override { OnViewModeChanged.Broadcast(Mode); }
private:
    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorMonitorPanelWidget> BoundMonitorWidget;
};
