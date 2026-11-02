#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorDeviceProfileTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "DeviceSensorCoordinatorTypes.h"
#include "DeviceSensorCoordinator.generated.h"

class UActorComponent;
class APlayerController;
class AVirtualSensorActorBase;
class UPrimitiveComponent;
class URealSensorSourceComponent;
class UVirtualCameraCaptureComponent;
class UVirtualLidarScanComponent;
class UVirtualSensorTransportComponent;
class UVirtualSensorRecorderComponent;
class UVirtualSensorStreamPublisherComponent;
class IVirtualSensorMonitorView;

struct FDeviceLidarPointCloudOnlyState
{
    bool bPointCloudPreviewEnabled = false;
    bool bDrawDebugRays = false;
    bool bDrawPointCloudPreviewDebugPoints = false;
    int32 PointCloudPreviewStride = 1;
    int32 MaxPointCloudPreviewInstances = 0;
    int32 PreviewPointStride = 1;
    int32 MaxPreviewPoints = 0;
    bool bHadVisualizationComponent = false;
    bool bShowWorldPointCloud = false;
};

/** Read-only host policy snapshot. No containers are copied and no UI types cross this boundary. */
struct FDeviceSensorCoordinatorConfig
{
    bool bDiscoverOnBeginPlay=true, bStartSensorsOnBeginPlay=true, bUseSynchronizedCapture=false;
    float SynchronizedInterval=0.5f, StaleSensorSeconds=3.0f;
    bool bPointCloudOnlyHideWorld=true, bPointCloudOnlyAutoSelectLidarView=true;
    TConstArrayView<FName> PointCloudOnlyKeepActorTags;
    UVirtualSensorTransportComponent* SharedTransportComponent=nullptr;
    UVirtualSensorRecorderComponent* SharedRecorderComponent=nullptr;
    UVirtualSensorStreamPublisherComponent* StreamPublisherComponent=nullptr;
};
UCLASS(Abstract)
class MA0T10_DT_API ADeviceSensorCoordinator : public AActor
{
    GENERATED_BODY()

public:
    bool SetPcdBandwidthOptions(const FString& SensorId,const FVirtualPcdSendOptions& Options,bool bApplyToAll,FString& Error);
    ADeviceSensorCoordinator();
    virtual FDeviceSensorCoordinatorConfig GetDeviceCoordinatorConfig() const { return {}; }
    /** Native presentation boundary. The owner is weak; binding does not keep a widget alive. */
    void BindDeviceMonitorView(UObject* ViewOwner);
    UObject* GetDeviceMonitorOwner() const { return MonitorOwner.Get(); }
protected:
    virtual void NotifyDeviceViewModeChanged(EVirtualSensorViewMode) {}
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    void DiscoverSensorsInLevel();
    void RegisterSensorActor(AVirtualSensorActorBase* SensorActor);
    void UnregisterSensorActor(AVirtualSensorActorBase* SensorActor);
    bool SelectSensorActor(AVirtualSensorActorBase* SensorActor);
    FSimpleMulticastDelegate OnSensorInventoryChanged;
    void RegisterCamera(UVirtualCameraCaptureComponent* CameraComp);
    void RegisterLidar(UVirtualLidarScanComponent* LidarComp);
    void RegisterRealSensorSource(URealSensorSourceComponent* RealSensorSourceComponent);
    void SelectCameraByIndex(int32 Index);
    void SelectLidarByIndex(int32 Index);
    void SelectRealSensorSourceByIndex(int32 Index);
    void SelectNextCamera();
    void SelectNextLidar();
    void SetViewMode(EVirtualSensorViewMode NewMode);
    void SetPointCloudOnlyMode(bool bEnabled);
    void ToggleSensorView();
    void TogglePointCloudOnlyView();
    void StartAllSensors();
    void StopAllSensors();
    /** Apply through each native Actor contract; locked sensors are preserved. */
    int32 SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend Backend);
    int32 StartAllRealSensorSources();
    void StopAllRealSensorSources();
    bool PushSelectedRealSensorSourceOnce(bool bSendTransport = true);
    void CaptureAllOnce();
    void CaptureSelectedOnce();
    void RefreshSelectedSensorOnce(bool bLidar);
    void SetSelectedLidarPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly);
    void AdjustSelectedLidarPreviewBudget(int32 StrideDelta, int32 MaxPointsDelta);
    UVirtualCameraCaptureComponent* GetSelectedCamera() const;
    int32 GetCameraCount() const { return Cameras.Num(); }
    UVirtualCameraCaptureComponent* GetCameraByIndex(int32 Index) const;
    int32 FindCameraIndexBySensorId(const FString& SensorId) const;
    UVirtualLidarScanComponent* GetSelectedLidar() const;
    URealSensorSourceComponent* GetSelectedRealSensorSource() const;
    AVirtualSensorActorBase* GetSelectedSensorActor() const;

    /** Returns the selected actor for the requested sensor kind, independently of the monitor view mode. */
    AVirtualSensorActorBase* GetSelectedSensorActorByKind(EVirtualSensorKind SensorKind) const;
    TArray<AVirtualSensorActorBase*> GetSensorActors() const;
    TArray<FVirtualSensorSummary> GetCameraSummaries() const;
    TArray<FVirtualSensorSummary> GetLidarSummaries() const;

    const TArray<TObjectPtr<UVirtualLidarScanComponent>>& GetLidarComponents() const { return Lidars; }
    TArray<FVirtualSensorSummary> GetRealSensorSourceSummaries() const;
    FVirtualSensorHealthSummary GetHealthSummary() const;
    EVirtualSensorViewMode GetViewMode() const { return CurrentViewMode; }
    bool IsPointCloudOnlyModeEnabled() const { return bPointCloudOnlyModeEnabled; }


private:
    UFUNCTION() void HandleSensorEndPlay(AActor* Actor,EEndPlayReason::Type Reason);
    void HandleSensorActivated(AVirtualSensorActorBase* Actor) { SelectSensorActor(Actor); }
    void ApplyWidgetBinding();
    void RunSynchronizedCapture();
    void AssignSharedServicesIfPossible(UActorComponent* SensorComp);
    void ApplyPointCloudOnlyVisibility();
    void RestorePointCloudOnlyVisibility();
    bool ShouldKeepActorVisibleInPointCloudOnly(const AActor* Actor) const;

private:
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    friend class FDeviceCoordinatorViewStateIsolation;
#endif
    void SaveLidarViewState(UVirtualLidarScanComponent* LidarComp);
    void ApplyPointCloudOnlyToLidar(UVirtualLidarScanComponent* LidarComp,bool bSelected);
    void RestoreLidarViewState(UVirtualLidarScanComponent* LidarComp);
    TMap<TWeakObjectPtr<UVirtualLidarScanComponent>,FDeviceLidarPointCloudOnlyState> LidarPointCloudOnlyStates;
    IVirtualSensorMonitorView* GetMonitorView() const;
    TWeakObjectPtr<UObject> MonitorOwner;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UVirtualCameraCaptureComponent>> Cameras;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AVirtualSensorActorBase>> SensorActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UVirtualLidarScanComponent>> Lidars;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URealSensorSourceComponent>> RealSensorSources;


    UPROPERTY(Transient)
    TArray<FVirtualSensorHiddenComponentState> HiddenComponentStates;

    // View-local ownership only. Never hide source primitives from sensor SceneCaptures.
    TWeakObjectPtr<APlayerController> PointCloudOnlyViewController;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> OwnedViewHiddenComponents;

    int32 SelectedCameraIndex = 0;
    int32 SelectedLidarIndex = 0;
    int32 SelectedRealSensorSourceIndex = 0;
    EVirtualSensorViewMode CurrentViewMode = EVirtualSensorViewMode::Camera;
    EVirtualSensorViewMode PreviousViewModeBeforePointCloudOnly = EVirtualSensorViewMode::Camera;
    bool bPointCloudOnlyModeEnabled = false;
    FTimerHandle SynchronizedTimerHandle;
};
