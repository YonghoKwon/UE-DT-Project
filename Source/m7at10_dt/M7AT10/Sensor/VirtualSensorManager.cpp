#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"

#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h"
#include "m7at10_dt/M7AT10/UI/VirtualSensorMonitorWidget.h"

AVirtualSensorManager::AVirtualSensorManager()
{
    PrimaryActorTick.bCanEverTick = false;
    SharedTransportComponent = CreateDefaultSubobject<UVirtualSensorDataTransportComp>(TEXT("SharedSensorTransport"));
}

void AVirtualSensorManager::BeginPlay()
{
    Super::BeginPlay();

    if (bDiscoverOnBeginPlay)
    {
        DiscoverSensorsInLevel();
    }

    ApplyWidgetBinding();

    if (bStartSensorsOnBeginPlay)
    {
        StartAllSensors();
    }

    if (bUseSynchronizedCapture && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(SynchronizedTimerHandle, this, &AVirtualSensorManager::RunSynchronizedCapture, SynchronizedInterval, true, 0.0f);
    }
}

void AVirtualSensorManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SynchronizedTimerHandle);
    }
    Super::EndPlay(EndPlayReason);
}

void AVirtualSensorManager::DiscoverSensorsInLevel()
{
    Cameras.Reset();
    Lidars.Reset();
    UWorld* World = GetWorld();
    if (!World) return;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;
        TArray<UVirtualCameraComp*> FoundCameras;
        Actor->GetComponents<UVirtualCameraComp>(FoundCameras);
        for (UVirtualCameraComp* CameraComp : FoundCameras)
        {
            RegisterCamera(CameraComp);
        }
        TArray<UVirtualLidarSensorComp*> FoundLidars;
        Actor->GetComponents<UVirtualLidarSensorComp>(FoundLidars);
        for (UVirtualLidarSensorComp* LidarComp : FoundLidars)
        {
            RegisterLidar(LidarComp);
        }
    }
}

void AVirtualSensorManager::RegisterCamera(UVirtualCameraComp* CameraComp)
{
    if (CameraComp && !Cameras.Contains(CameraComp))
    {
        Cameras.Add(CameraComp);
        AssignSharedTransportIfPossible(CameraComp);
        ApplyWidgetBinding();
    }
}

void AVirtualSensorManager::RegisterLidar(UVirtualLidarSensorComp* LidarComp)
{
    if (LidarComp && !Lidars.Contains(LidarComp))
    {
        Lidars.Add(LidarComp);
        AssignSharedTransportIfPossible(LidarComp);
        ApplyWidgetBinding();
    }
}

void AVirtualSensorManager::BindMonitorWidget(UVirtualSensorMonitorWidget* MonitorWidget)
{
    BoundMonitorWidget = MonitorWidget;
    ApplyWidgetBinding();
}

void AVirtualSensorManager::SelectCameraByIndex(int32 Index)
{
    if (Cameras.IsValidIndex(Index))
    {
        SelectedCameraIndex = Index;
        ApplyWidgetBinding();
    }
}

void AVirtualSensorManager::SelectLidarByIndex(int32 Index)
{
    if (Lidars.IsValidIndex(Index))
    {
        SelectedLidarIndex = Index;
        ApplyWidgetBinding();
    }
}

void AVirtualSensorManager::SelectNextCamera()
{
    if (Cameras.Num() <= 0)
    {
        return;
    }
    SelectedCameraIndex = (SelectedCameraIndex + 1) % Cameras.Num();
    ApplyWidgetBinding();
}

void AVirtualSensorManager::SelectNextLidar()
{
    if (Lidars.Num() <= 0)
    {
        return;
    }
    SelectedLidarIndex = (SelectedLidarIndex + 1) % Lidars.Num();
    ApplyWidgetBinding();
}

void AVirtualSensorManager::SetViewMode(EVirtualSensorViewMode NewMode)
{
    CurrentViewMode = NewMode;
    if (BoundMonitorWidget)
    {
        if (CurrentViewMode == EVirtualSensorViewMode::Camera) BoundMonitorWidget->ShowCameraView();
        if (CurrentViewMode == EVirtualSensorViewMode::Lidar) BoundMonitorWidget->ShowLidarView();
    }
    OnViewModeChanged.Broadcast(CurrentViewMode);
}

void AVirtualSensorManager::ToggleSensorView()
{
    SetViewMode(CurrentViewMode == EVirtualSensorViewMode::Camera ? EVirtualSensorViewMode::Lidar : EVirtualSensorViewMode::Camera);
}

void AVirtualSensorManager::StartAllSensors()
{
    for (UVirtualCameraComp* CameraComp : Cameras)
    {
        if (CameraComp) CameraComp->StartCapture();
    }
    for (UVirtualLidarSensorComp* LidarComp : Lidars)
    {
        if (LidarComp) LidarComp->StartScan();
    }
}

void AVirtualSensorManager::StopAllSensors()
{
    for (UVirtualCameraComp* CameraComp : Cameras)
    {
        if (CameraComp) CameraComp->StopCapture();
    }
    for (UVirtualLidarSensorComp* LidarComp : Lidars)
    {
        if (LidarComp) LidarComp->StopScan();
    }
}

void AVirtualSensorManager::CaptureAllOnce()
{
    for (UVirtualCameraComp* CameraComp : Cameras)
    {
        if (CameraComp) CameraComp->CaptureAndSendImage();
    }
    for (UVirtualLidarSensorComp* LidarComp : Lidars)
    {
        if (LidarComp) LidarComp->ScanAndSend();
    }
}

UVirtualCameraComp* AVirtualSensorManager::GetSelectedCamera() const
{
    return Cameras.IsValidIndex(SelectedCameraIndex) ? Cameras[SelectedCameraIndex] : nullptr;
}

UVirtualLidarSensorComp* AVirtualSensorManager::GetSelectedLidar() const
{
    return Lidars.IsValidIndex(SelectedLidarIndex) ? Lidars[SelectedLidarIndex] : nullptr;
}

TArray<FVirtualSensorSummary> AVirtualSensorManager::GetCameraSummaries() const
{
    TArray<FVirtualSensorSummary> Result;
    for (int32 Index = 0; Index < Cameras.Num(); ++Index)
    {
        FVirtualSensorSummary Summary;
        Summary.Index = Index;
        Summary.SensorType = TEXT("virtual_camera");
        Summary.bValid = Cameras[Index] != nullptr;
        Summary.SensorId = Cameras[Index] ? Cameras[Index]->SensorId : TEXT("");
        Result.Add(Summary);
    }
    return Result;
}

TArray<FVirtualSensorSummary> AVirtualSensorManager::GetLidarSummaries() const
{
    TArray<FVirtualSensorSummary> Result;
    for (int32 Index = 0; Index < Lidars.Num(); ++Index)
    {
        FVirtualSensorSummary Summary;
        Summary.Index = Index;
        Summary.SensorType = TEXT("virtual_lidar");
        Summary.bValid = Lidars[Index] != nullptr;
        Summary.SensorId = Lidars[Index] ? Lidars[Index]->SensorId : TEXT("");
        Result.Add(Summary);
    }
    return Result;
}

FVirtualSensorHealthSummary AVirtualSensorManager::GetHealthSummary() const
{
    FVirtualSensorHealthSummary Health;
    Health.CameraCount = Cameras.Num();
    Health.LidarCount = Lidars.Num();

    const FDateTime NowUtc = FDateTime::UtcNow();
    auto CountIfStale = [&NowUtc, this](const FVirtualSensorRuntimeStatus& Status) -> bool
    {
        if (Status.FrameId <= 0)
        {
            return true;
        }
        const FTimespan Age = NowUtc - Status.LastUpdateUtc;
        return Age.GetTotalSeconds() > StaleSensorSeconds;
    };

    for (const UVirtualCameraComp* CameraComp : Cameras)
    {
        if (!CameraComp || CountIfStale(CameraComp->GetRuntimeStatus()))
        {
            ++Health.StaleSensorCount;
        }
    }

    for (const UVirtualLidarSensorComp* LidarComp : Lidars)
    {
        if (!LidarComp || CountIfStale(LidarComp->GetRuntimeStatus()))
        {
            ++Health.StaleSensorCount;
        }
    }

    Health.bHealthy = Health.StaleSensorCount == 0;
    Health.Summary = FString::Printf(TEXT("Cameras=%d Lidars=%d Stale=%d Healthy=%s"),
        Health.CameraCount,
        Health.LidarCount,
        Health.StaleSensorCount,
        Health.bHealthy ? TEXT("true") : TEXT("false"));
    return Health;
}

void AVirtualSensorManager::ApplyWidgetBinding()
{
    if (!BoundMonitorWidget) return;
    BoundMonitorWidget->BindVirtualCamera(GetSelectedCamera());
    BoundMonitorWidget->BindVirtualLidar(GetSelectedLidar());
    SetViewMode(CurrentViewMode);
}

void AVirtualSensorManager::RunSynchronizedCapture()
{
    CaptureAllOnce();
}

void AVirtualSensorManager::AssignSharedTransportIfPossible(UActorComponent* SensorComp)
{
    if (!SensorComp || !SharedTransportComponent)
    {
        return;
    }

    if (UVirtualCameraComp* CameraComp = Cast<UVirtualCameraComp>(SensorComp))
    {
        CameraComp->SetTransportComponent(SharedTransportComponent);
    }
    else if (UVirtualLidarSensorComp* LidarComp = Cast<UVirtualLidarSensorComp>(SensorComp))
    {
        LidarComp->SetTransportComponent(SharedTransportComponent);
    }
}
