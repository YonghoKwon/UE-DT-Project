#include "VirtualSensorCoordinator.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "Device/Sensor/VirtualSensorRecorderComp.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"

AVirtualSensorCoordinator::AVirtualSensorCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    SharedTransportComp = CreateDefaultSubobject<UVirtualSensorTransportComp>(TEXT("SharedSensorTransport"));
    SharedRecorderComp = CreateDefaultSubobject<UVirtualSensorRecorderComp>(TEXT("SharedSensorRecorder"));
    StreamPublisherComp = CreateDefaultSubobject<UVirtualSensorStreamPublisherComp>(TEXT("SensorStreamPublisher"));
    StreamPublisherComp->SetTransportComponent(SharedTransportComp);
}

void AVirtualSensorCoordinator::BeginPlay() { Super::BeginPlay(); }
void AVirtualSensorCoordinator::DiscoverSensorsInLevel()
{
    Super::DiscoverSensorsInLevel();
}

void AVirtualSensorCoordinator::RegisterCamera(UVirtualCameraCaptureComponent* CameraComp)
{
    Super::RegisterCamera(CameraComp);
}

void AVirtualSensorCoordinator::RegisterLidar(UVirtualLidarScanComp* LidarComp)
{
    Super::RegisterLidar(LidarComp);
}

void AVirtualSensorCoordinator::RegisterRealSensorSource(URealSensorSourceComp* RealSensorSourceComp)
{
    Super::RegisterRealSensorSource(RealSensorSourceComp);
}

void AVirtualSensorCoordinator::SelectCameraByIndex(int32 Index)
{
    Super::SelectCameraByIndex(Index);
}

void AVirtualSensorCoordinator::SelectLidarByIndex(int32 Index)
{
    Super::SelectLidarByIndex(Index);
}

void AVirtualSensorCoordinator::SelectRealSensorSourceByIndex(int32 Index)
{
    Super::SelectRealSensorSourceByIndex(Index);
}

void AVirtualSensorCoordinator::SelectNextCamera()
{
    Super::SelectNextCamera();
}

void AVirtualSensorCoordinator::SelectNextLidar()
{
    Super::SelectNextLidar();
}

void AVirtualSensorCoordinator::SetViewMode(EVirtualSensorViewMode NewMode)
{
    Super::SetViewMode(NewMode);
}

void AVirtualSensorCoordinator::SetPointCloudOnlyMode(bool bEnabled)
{
    Super::SetPointCloudOnlyMode(bEnabled);
}

void AVirtualSensorCoordinator::ToggleSensorView()
{
    Super::ToggleSensorView();
}

void AVirtualSensorCoordinator::TogglePointCloudOnlyView()
{
    Super::TogglePointCloudOnlyView();
}

void AVirtualSensorCoordinator::StartAllSensors()
{
    Super::StartAllSensors();
}

void AVirtualSensorCoordinator::StopAllSensors()
{
    Super::StopAllSensors();
}

int32 AVirtualSensorCoordinator::StartAllRealSensorSources()
{
    return Super::StartAllRealSensorSources();
}

void AVirtualSensorCoordinator::StopAllRealSensorSources()
{
    Super::StopAllRealSensorSources();
}

bool AVirtualSensorCoordinator::PushSelectedRealSensorSourceOnce(bool bSendTransport)
{
    return Super::PushSelectedRealSensorSourceOnce(bSendTransport);
}

void AVirtualSensorCoordinator::CaptureAllOnce()
{
    Super::CaptureAllOnce();
}

void AVirtualSensorCoordinator::CaptureSelectedOnce()
{
    Super::CaptureSelectedOnce();
}

void AVirtualSensorCoordinator::RefreshSelectedSensorOnce(bool bLidar)
{
    Super::RefreshSelectedSensorOnce(bLidar);
}

void AVirtualSensorCoordinator::RegisterSensorActor(AVirtualSensorActorBase* SensorActor)
{
    Super::RegisterSensorActor(SensorActor);
}

void AVirtualSensorCoordinator::UnregisterSensorActor(AVirtualSensorActorBase* Sensor)
{
    Super::UnregisterSensorActor(Sensor);
}

bool AVirtualSensorCoordinator::SelectSensorActor(AVirtualSensorActorBase* Sensor)
{
    return Super::SelectSensorActor(Sensor);
}

void AVirtualSensorCoordinator::SetSelectedLidarPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly)
{
    Super::SetSelectedLidarPreviewPolicy(InStride, InMaxPoints, bInHitOnly);
}

void AVirtualSensorCoordinator::AdjustSelectedLidarPreviewBudget(int32 StrideDelta, int32 MaxPointsDelta)
{
    Super::AdjustSelectedLidarPreviewBudget(StrideDelta, MaxPointsDelta);
}

UVirtualCameraCaptureComponent* AVirtualSensorCoordinator::GetSelectedCamera() const
{
    return Super::GetSelectedCamera();
}

UVirtualCameraCaptureComponent* AVirtualSensorCoordinator::GetCameraByIndex(int32 Index) const
{
    return Super::GetCameraByIndex(Index);
}

int32 AVirtualSensorCoordinator::FindCameraIndexBySensorId(const FString& SensorId) const
{
    return Super::FindCameraIndexBySensorId(SensorId);
}

UVirtualLidarScanComp* AVirtualSensorCoordinator::GetSelectedLidar() const
{
    return Super::GetSelectedLidar();
}

URealSensorSourceComp* AVirtualSensorCoordinator::GetSelectedRealSensorSource() const
{
    return Super::GetSelectedRealSensorSource();
}

AVirtualSensorActorBase* AVirtualSensorCoordinator::GetSelectedSensorActor() const
{
    return Super::GetSelectedSensorActor();
}

AVirtualSensorActorBase* AVirtualSensorCoordinator::GetSelectedSensorActorByKind(EVirtualSensorKind SensorKind) const
{
    return Super::GetSelectedSensorActorByKind(SensorKind);
}

TArray<AVirtualSensorActorBase*> AVirtualSensorCoordinator::GetSensorActors() const
{
    return Super::GetSensorActors();
}

TArray<FVirtualSensorSummary> AVirtualSensorCoordinator::GetCameraSummaries() const
{
    return Super::GetCameraSummaries();
}

TArray<FVirtualSensorSummary> AVirtualSensorCoordinator::GetLidarSummaries() const
{
    return Super::GetLidarSummaries();
}

TArray<FVirtualSensorSummary> AVirtualSensorCoordinator::GetRealSensorSourceSummaries() const
{
    return Super::GetRealSensorSourceSummaries();
}

FVirtualSensorHealthSummary AVirtualSensorCoordinator::GetHealthSummary() const
{
    return Super::GetHealthSummary();
}

int32 AVirtualSensorCoordinator::SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend Backend)
{
    return Super::SetLidarAcquisitionBackend(Backend);
}

bool AVirtualSensorCoordinator::SetPcdBandwidthOptions(const FString& Id,const FVirtualPcdSendOptions& Options,bool All,FString& Error)
{
    return Super::SetPcdBandwidthOptions(Id, Options, All, Error);
}

FDeviceSensorCoordinatorConfig AVirtualSensorCoordinator::GetDeviceCoordinatorConfig() const
{
    return {bDiscoverOnBeginPlay,bStartSensorsOnBeginPlay,bUseSynchronizedCapture,
        SynchronizedInterval,StaleSensorSeconds,bPointCloudOnlyHideWorld,
        bPointCloudOnlyAutoSelectLidarView,PointCloudOnlyKeepActorTags,
        SharedTransportComp.Get(),SharedRecorderComp.Get(),StreamPublisherComp.Get()};
}
