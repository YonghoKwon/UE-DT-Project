#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"

#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/Sensor/RealSensorSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"

namespace
{
struct FLidarPointCloudOnlyState
{
    bool bPointCloudPreviewEnabled = false;
    bool bPointCloudPreviewHitOnly = true;
    bool bDrawDebugRays = false;
    bool bDrawPointCloudPreviewDebugPoints = false;
    int32 PointCloudPreviewStride = 1;
    int32 MaxPointCloudPreviewInstances = 0;
    int32 PreviewPointStride = 1;
    int32 MaxPreviewPoints = 0;
    bool bHadVisualizationComponent = false;
    bool bShowWorldPointCloud = false;
};

TMap<TWeakObjectPtr<UVirtualLidarScanComponent>, FLidarPointCloudOnlyState> GLidarPointCloudOnlyStates;

void SaveLidarViewState(UVirtualLidarScanComponent* LidarComp)
{
    if (!LidarComp || GLidarPointCloudOnlyStates.Contains(LidarComp))
    {
        return;
    }

    FLidarPointCloudOnlyState State;
    State.bPointCloudPreviewEnabled = LidarComp->IsPointCloudPreviewEnabled();
    State.bPointCloudPreviewHitOnly = LidarComp->bPointCloudPreviewHitOnly;
    State.bDrawDebugRays = LidarComp->bDrawDebugRays;
    State.bDrawPointCloudPreviewDebugPoints = LidarComp->bDrawPointCloudPreviewDebugPoints;
    State.PointCloudPreviewStride = LidarComp->PointCloudPreviewStride;
    State.MaxPointCloudPreviewInstances = LidarComp->MaxPointCloudPreviewInstances;
    State.PreviewPointStride = LidarComp->PreviewPointStride;
    State.MaxPreviewPoints = LidarComp->MaxPreviewPoints;
    if (const AVirtualLidarSensorActor* LidarActor = Cast<AVirtualLidarSensorActor>(LidarComp->GetOwner()))
    {
        if (const UVirtualLidarVisualizationComponent* Visualization = LidarActor->VisualizationComponent)
        {
            State.bHadVisualizationComponent = true;
            State.bShowWorldPointCloud = Visualization->GetVisualizationSettings().bShowWorldPointCloud;
        }
    }
    GLidarPointCloudOnlyStates.Add(LidarComp, State);
}

void ApplyPointCloudOnlyToLidar(UVirtualLidarScanComponent* LidarComp, bool bSelected)
{
    if (!LidarComp)
    {
        return;
    }

    SaveLidarViewState(LidarComp);
    LidarComp->bDrawDebugRays = false;
    LidarComp->bDrawPointCloudPreviewDebugPoints = false;
    const int32 PreviewStride = FMath::Max(2, LidarComp->PreviewPointStride);
    const int32 PreviewMaxPoints = LidarComp->MaxPreviewPoints > 0
        ? FMath::Min(LidarComp->MaxPreviewPoints, 3000)
        : 3000;
    LidarComp->SetPreviewPolicy(PreviewStride, PreviewMaxPoints, true);
    LidarComp->SetPointCloudPreviewEnabled(bSelected);
    if (AVirtualLidarSensorActor* LidarActor = Cast<AVirtualLidarSensorActor>(LidarComp->GetOwner()))
    {
        if (UVirtualLidarVisualizationComponent* Visualization = LidarActor->VisualizationComponent)
        {
            // The V2 visualization owns the final renderer choice. Keeping its
            // setting in sync prevents the next completed scan from disabling
            // the point-cloud-only preview again.
            Visualization->SetWorldPointCloudEnabled(bSelected);
        }
    }
}

void RestoreLidarViewState(UVirtualLidarScanComponent* LidarComp)
{
    if (!LidarComp)
    {
        return;
    }

    if (FLidarPointCloudOnlyState* State = GLidarPointCloudOnlyStates.Find(LidarComp))
    {
        LidarComp->bDrawDebugRays = State->bDrawDebugRays;
        LidarComp->bDrawPointCloudPreviewDebugPoints = State->bDrawPointCloudPreviewDebugPoints;
        LidarComp->PointCloudPreviewStride = State->PointCloudPreviewStride;
        LidarComp->MaxPointCloudPreviewInstances = State->MaxPointCloudPreviewInstances;
        LidarComp->SetPreviewPolicy(State->PreviewPointStride, State->MaxPreviewPoints, State->bPointCloudPreviewHitOnly);
        if (State->bHadVisualizationComponent)
        {
            if (AVirtualLidarSensorActor* LidarActor = Cast<AVirtualLidarSensorActor>(LidarComp->GetOwner()))
            {
                if (UVirtualLidarVisualizationComponent* Visualization = LidarActor->VisualizationComponent)
                {
                    Visualization->SetWorldPointCloudEnabled(State->bShowWorldPointCloud);
                }
            }
        }
        LidarComp->SetPointCloudPreviewEnabled(State->bPointCloudPreviewEnabled);
        GLidarPointCloudOnlyStates.Remove(LidarComp);
    }
    else
    {
        LidarComp->SetPointCloudPreviewEnabled(false);
    }
}
}

AVirtualSensorCoordinator::AVirtualSensorCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
    SharedTransportComponent = CreateDefaultSubobject<UVirtualSensorTransportComponent>(TEXT("SharedSensorTransport"));
    SharedRecorderComponent = CreateDefaultSubobject<UVirtualSensorRecorderComponent>(TEXT("SharedSensorRecorder"));
}

void AVirtualSensorCoordinator::BeginPlay()
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
        GetWorld()->GetTimerManager().SetTimer(SynchronizedTimerHandle, this, &AVirtualSensorCoordinator::RunSynchronizedCapture, SynchronizedInterval, true, 0.0f);
    }
}

void AVirtualSensorCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RestorePointCloudOnlyVisibility();
    for (UVirtualLidarScanComponent* LidarComp : Lidars)
    {
        RestoreLidarViewState(LidarComp);
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SynchronizedTimerHandle);
    }
    Super::EndPlay(EndPlayReason);
}

void AVirtualSensorCoordinator::DiscoverSensorsInLevel()
{
    Cameras.Reset();
    Lidars.Reset();
    RealSensorSources.Reset();
    SensorActors.Reset();
    UWorld* World = GetWorld();
    if (!World) return;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;
        if (AVirtualSensorActorBase* SensorActor = Cast<AVirtualSensorActorBase>(Actor))
        {
            RegisterSensorActor(SensorActor);
        }
        TArray<UVirtualCameraCaptureComponent*> FoundCameras;
        Actor->GetComponents<UVirtualCameraCaptureComponent>(FoundCameras);
        for (UVirtualCameraCaptureComponent* CameraComp : FoundCameras)
        {
            RegisterCamera(CameraComp);
        }
        TArray<UVirtualLidarScanComponent*> FoundLidars;
        Actor->GetComponents<UVirtualLidarScanComponent>(FoundLidars);
        for (UVirtualLidarScanComponent* LidarComp : FoundLidars)
        {
            RegisterLidar(LidarComp);
        }
        TArray<URealSensorSourceComponent*> FoundRealSensorSources;
        Actor->GetComponents<URealSensorSourceComponent>(FoundRealSensorSources);
        for (URealSensorSourceComponent* RealSensorSourceComponent : FoundRealSensorSources)
        {
            RegisterRealSensorSource(RealSensorSourceComponent);
        }
    }
}

void AVirtualSensorCoordinator::RegisterCamera(UVirtualCameraCaptureComponent* CameraComp)
{
    if (CameraComp && !Cameras.Contains(CameraComp))
    {
        Cameras.Add(CameraComp);
        if (!Cast<AVirtualSensorActorBase>(CameraComp->GetOwner()))
        {
            AssignSharedServicesIfPossible(CameraComp);
        }
        ApplyWidgetBinding();
    }
}

void AVirtualSensorCoordinator::RegisterLidar(UVirtualLidarScanComponent* LidarComp)
{
    if (LidarComp && !Lidars.Contains(LidarComp))
    {
        Lidars.Add(LidarComp);
        if (!Cast<AVirtualSensorActorBase>(LidarComp->GetOwner()))
        {
            AssignSharedServicesIfPossible(LidarComp);
        }
        if (bPointCloudOnlyModeEnabled)
        {
            ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
        }
        ApplyWidgetBinding();
    }
}

void AVirtualSensorCoordinator::RegisterRealSensorSource(URealSensorSourceComponent* RealSensorSourceComponent)
{
    if (RealSensorSourceComponent && !RealSensorSources.Contains(RealSensorSourceComponent))
    {
        RealSensorSources.Add(RealSensorSourceComponent);
        ApplyWidgetBinding();
    }
}

void AVirtualSensorCoordinator::BindMonitorWidget(UVirtualSensorMonitorPanelWidget* MonitorWidget)
{
    BoundMonitorWidget = MonitorWidget;
    ApplyWidgetBinding();
}

void AVirtualSensorCoordinator::SelectCameraByIndex(int32 Index)
{
    if (Cameras.IsValidIndex(Index))
    {
        SelectedCameraIndex = Index;
        ApplyWidgetBinding();
    }
}

void AVirtualSensorCoordinator::SelectLidarByIndex(int32 Index)
{
    if (Lidars.IsValidIndex(Index))
    {
        SelectedLidarIndex = Index;
        if (bPointCloudOnlyModeEnabled)
        {
            for (UVirtualLidarScanComponent* LidarComp : Lidars)
            {
                ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
            }
        }
        ApplyWidgetBinding();
    }
}

void AVirtualSensorCoordinator::SelectRealSensorSourceByIndex(int32 Index)
{
    if (RealSensorSources.IsValidIndex(Index))
    {
        SelectedRealSensorSourceIndex = Index;
        ApplyWidgetBinding();
    }
}

void AVirtualSensorCoordinator::SelectNextCamera()
{
    if (Cameras.Num() <= 0)
    {
        return;
    }
    SelectedCameraIndex = (SelectedCameraIndex + 1) % Cameras.Num();
    ApplyWidgetBinding();
}

void AVirtualSensorCoordinator::SelectNextLidar()
{
    if (Lidars.Num() <= 0)
    {
        return;
    }
    SelectedLidarIndex = (SelectedLidarIndex + 1) % Lidars.Num();
    if (bPointCloudOnlyModeEnabled)
    {
        for (UVirtualLidarScanComponent* LidarComp : Lidars)
        {
            ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
        }
    }
    ApplyWidgetBinding();
}

void AVirtualSensorCoordinator::SetViewMode(EVirtualSensorViewMode NewMode)
{
    if (NewMode == EVirtualSensorViewMode::PointCloudOnly)
    {
        SetPointCloudOnlyMode(true);
        return;
    }

    if (bPointCloudOnlyModeEnabled)
    {
        SetPointCloudOnlyMode(false);
    }

    CurrentViewMode = NewMode;
    if (BoundMonitorWidget)
    {
        if (CurrentViewMode == EVirtualSensorViewMode::Camera) BoundMonitorWidget->ShowCameraView();
        if (CurrentViewMode == EVirtualSensorViewMode::Lidar) BoundMonitorWidget->ShowLidarView();
    }
    OnViewModeChanged.Broadcast(CurrentViewMode);
}

void AVirtualSensorCoordinator::SetPointCloudOnlyMode(bool bEnabled)
{
    if (bPointCloudOnlyModeEnabled == bEnabled)
    {
        return;
    }

    if (bEnabled)
    {
        PreviousViewModeBeforePointCloudOnly = CurrentViewMode == EVirtualSensorViewMode::PointCloudOnly
            ? EVirtualSensorViewMode::Lidar
            : CurrentViewMode;
        bPointCloudOnlyModeEnabled = true;
        CurrentViewMode = EVirtualSensorViewMode::PointCloudOnly;

        if (bPointCloudOnlyHideWorld)
        {
            ApplyPointCloudOnlyVisibility();
        }

        for (UVirtualLidarScanComponent* LidarComp : Lidars)
        {
            ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
        }

        if (bPointCloudOnlyAutoSelectLidarView && BoundMonitorWidget)
        {
            BoundMonitorWidget->ShowLidarView();
        }
    }
    else
    {
        bPointCloudOnlyModeEnabled = false;
        RestorePointCloudOnlyVisibility();
        for (UVirtualLidarScanComponent* LidarComp : Lidars)
        {
            RestoreLidarViewState(LidarComp);
        }
        CurrentViewMode = PreviousViewModeBeforePointCloudOnly;
        if (BoundMonitorWidget)
        {
            if (CurrentViewMode == EVirtualSensorViewMode::Camera) BoundMonitorWidget->ShowCameraView();
            if (CurrentViewMode == EVirtualSensorViewMode::Lidar) BoundMonitorWidget->ShowLidarView();
        }
    }

    OnViewModeChanged.Broadcast(CurrentViewMode);
}

void AVirtualSensorCoordinator::ToggleSensorView()
{
    SetViewMode(CurrentViewMode == EVirtualSensorViewMode::Camera ? EVirtualSensorViewMode::Lidar : EVirtualSensorViewMode::Camera);
}

void AVirtualSensorCoordinator::TogglePointCloudOnlyView()
{
    SetPointCloudOnlyMode(!bPointCloudOnlyModeEnabled);
}

void AVirtualSensorCoordinator::StartAllSensors()
{
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (SensorActor) SensorActor->StartSensor();
    }
    for (UVirtualCameraCaptureComponent* CameraComp : Cameras)
    {
        if (CameraComp && !Cast<AVirtualSensorActorBase>(CameraComp->GetOwner())) CameraComp->StartCapture();
    }
    for (UVirtualLidarScanComponent* LidarComp : Lidars)
    {
        if (LidarComp && !Cast<AVirtualSensorActorBase>(LidarComp->GetOwner())) LidarComp->StartScan();
    }
}

void AVirtualSensorCoordinator::StopAllSensors()
{
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (SensorActor) SensorActor->StopSensor();
    }
    for (UVirtualCameraCaptureComponent* CameraComp : Cameras)
    {
        if (CameraComp && !Cast<AVirtualSensorActorBase>(CameraComp->GetOwner())) CameraComp->StopCapture();
    }
    for (UVirtualLidarScanComponent* LidarComp : Lidars)
    {
        if (LidarComp && !Cast<AVirtualSensorActorBase>(LidarComp->GetOwner())) LidarComp->StopScan();
    }
}

int32 AVirtualSensorCoordinator::StartAllRealSensorSources()
{
    int32 StartedCount = 0;
    for (URealSensorSourceComponent* RealSensorSourceComponent : RealSensorSources)
    {
        if (RealSensorSourceComponent && RealSensorSourceComponent->StartSource())
        {
            ++StartedCount;
        }
    }
    return StartedCount;
}

void AVirtualSensorCoordinator::StopAllRealSensorSources()
{
    for (URealSensorSourceComponent* RealSensorSourceComponent : RealSensorSources)
    {
        if (RealSensorSourceComponent)
        {
            RealSensorSourceComponent->StopSource();
        }
    }
}

bool AVirtualSensorCoordinator::PushSelectedRealSensorSourceOnce(bool bSendTransport)
{
    URealSensorSourceComponent* RealSensorSourceComponent = GetSelectedRealSensorSource();
    return RealSensorSourceComponent && RealSensorSourceComponent->PushFrameOnce(bSendTransport);
}

void AVirtualSensorCoordinator::CaptureAllOnce()
{
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (SensorActor) SensorActor->CaptureSensorOnce();
    }
    for (UVirtualCameraCaptureComponent* CameraComp : Cameras)
    {
        if (CameraComp && !Cast<AVirtualSensorActorBase>(CameraComp->GetOwner())) CameraComp->CaptureAndSendImage();
    }
    for (UVirtualLidarScanComponent* LidarComp : Lidars)
    {
        if (LidarComp && !Cast<AVirtualSensorActorBase>(LidarComp->GetOwner())) LidarComp->ScanAndSend();
    }
}

void AVirtualSensorCoordinator::CaptureSelectedOnce()
{
    if (UVirtualCameraCaptureComponent* CameraComp = GetSelectedCamera())
    {
        CameraComp->CaptureAndSendImage();
    }
    if (UVirtualLidarScanComponent* LidarComp = GetSelectedLidar())
    {
        LidarComp->ScanAndSend();
    }
}

void AVirtualSensorCoordinator::RefreshSelectedSensorOnce(bool bLidar)
{
    if (bLidar)
    {
        if (UVirtualLidarScanComponent* LidarComp = GetSelectedLidar())
        {
            LidarComp->ScanAndSend();
        }
        return;
    }

    if (UVirtualCameraCaptureComponent* CameraComp = GetSelectedCamera())
    {
        CameraComp->CaptureAndSendImage();
    }
}

void AVirtualSensorCoordinator::RegisterSensorActor(AVirtualSensorActorBase* SensorActor)
{
    if (!SensorActor || SensorActors.Contains(SensorActor)) return;
    SensorActors.Add(SensorActor);
    SensorActor->SetSharedOutputServices(SharedTransportComponent, SharedRecorderComponent);
}

void AVirtualSensorCoordinator::SetSelectedLidarPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly)
{
    if (UVirtualLidarScanComponent* LidarComp = GetSelectedLidar())
    {
        LidarComp->SetPreviewPolicy(InStride, InMaxPoints, bInHitOnly);
    }
}

void AVirtualSensorCoordinator::AdjustSelectedLidarPreviewBudget(int32 StrideDelta, int32 MaxPointsDelta)
{
    if (UVirtualLidarScanComponent* LidarComp = GetSelectedLidar())
    {
        const int32 NextStride = FMath::Clamp(LidarComp->PreviewPointStride + StrideDelta, 1, 100);
        const int32 NextMaxPoints = FMath::Clamp(LidarComp->MaxPreviewPoints + MaxPointsDelta, 0, 1000000);
        LidarComp->SetPreviewPolicy(NextStride, NextMaxPoints, LidarComp->bPointCloudPreviewHitOnly);
    }
}

UVirtualCameraCaptureComponent* AVirtualSensorCoordinator::GetSelectedCamera() const
{
    return Cameras.IsValidIndex(SelectedCameraIndex) ? Cameras[SelectedCameraIndex] : nullptr;
}

UVirtualLidarScanComponent* AVirtualSensorCoordinator::GetSelectedLidar() const
{
    return Lidars.IsValidIndex(SelectedLidarIndex) ? Lidars[SelectedLidarIndex] : nullptr;
}

URealSensorSourceComponent* AVirtualSensorCoordinator::GetSelectedRealSensorSource() const
{
    if (UVirtualLidarScanComponent* SelectedLidar = GetSelectedLidar())
    {
        for (URealSensorSourceComponent* RealSensorSourceComponent : RealSensorSources)
        {
            if (RealSensorSourceComponent && RealSensorSourceComponent->TargetLidar == SelectedLidar)
            {
                return RealSensorSourceComponent;
            }
        }
    }
    return RealSensorSources.IsValidIndex(SelectedRealSensorSourceIndex) ? RealSensorSources[SelectedRealSensorSourceIndex] : nullptr;
}

AVirtualSensorActorBase* AVirtualSensorCoordinator::GetSelectedSensorActor() const
{
    const EVirtualSensorKind TargetKind = CurrentViewMode == EVirtualSensorViewMode::Lidar ||
        CurrentViewMode == EVirtualSensorViewMode::PointCloudOnly
        ? EVirtualSensorKind::Lidar
        : EVirtualSensorKind::Camera;
    return GetSelectedSensorActorByKind(TargetKind);
}

AVirtualSensorActorBase* AVirtualSensorCoordinator::GetSelectedSensorActorByKind(EVirtualSensorKind SensorKind) const
{
    const int32 TargetIndex = SensorKind == EVirtualSensorKind::Camera ? SelectedCameraIndex : SelectedLidarIndex;
    int32 MatchingIndex = 0;
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (!SensorActor || SensorActor->GetSensorKind() != SensorKind) continue;
        if (MatchingIndex++ == TargetIndex) return SensorActor;
    }
    return nullptr;
}

TArray<AVirtualSensorActorBase*> AVirtualSensorCoordinator::GetSensorActors() const
{
    TArray<AVirtualSensorActorBase*> Result;
    Result.Reserve(SensorActors.Num());
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (SensorActor) Result.Add(SensorActor);
    }
    return Result;
}

TArray<FVirtualSensorSummary> AVirtualSensorCoordinator::GetCameraSummaries() const
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

TArray<FVirtualSensorSummary> AVirtualSensorCoordinator::GetLidarSummaries() const
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

TArray<FVirtualSensorSummary> AVirtualSensorCoordinator::GetRealSensorSourceSummaries() const
{
    TArray<FVirtualSensorSummary> Result;
    for (int32 Index = 0; Index < RealSensorSources.Num(); ++Index)
    {
        FVirtualSensorSummary Summary;
        Summary.Index = Index;
        Summary.SensorType = TEXT("real_sensor_source");
        Summary.bValid = RealSensorSources[Index] != nullptr;
        Summary.SensorId = RealSensorSources[Index] ? RealSensorSources[Index]->SourceId : TEXT("");
        Result.Add(Summary);
    }
    return Result;
}

FVirtualSensorHealthSummary AVirtualSensorCoordinator::GetHealthSummary() const
{
    FVirtualSensorHealthSummary Health;
    Health.CameraCount = Cameras.Num();
    Health.LidarCount = Lidars.Num();
    Health.RealSensorSourceCount = RealSensorSources.Num();

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

    for (const UVirtualCameraCaptureComponent* CameraComp : Cameras)
    {
        if (!CameraComp || CountIfStale(CameraComp->GetRuntimeStatus()))
        {
            ++Health.StaleSensorCount;
        }
    }

    for (const UVirtualLidarScanComponent* LidarComp : Lidars)
    {
        if (!LidarComp || CountIfStale(LidarComp->GetRuntimeStatus()))
        {
            ++Health.StaleSensorCount;
        }
    }

    for (const URealSensorSourceComponent* RealSensorSourceComponent : RealSensorSources)
    {
        if (!RealSensorSourceComponent)
        {
            ++Health.ErrorRealSensorSourceCount;
            continue;
        }

        if (RealSensorSourceComponent->GetConnectionState() == ERealSensorSourceConnectionState::Running)
        {
            ++Health.RunningRealSensorSourceCount;
        }
        else if (RealSensorSourceComponent->GetConnectionState() == ERealSensorSourceConnectionState::Error)
        {
            ++Health.ErrorRealSensorSourceCount;
        }

        if (RealSensorSourceComponent->RequiresExternalDeploymentEvidence())
        {
            ++Health.ExternalEvidenceRequiredRealSensorSourceCount;
        }
    }

    Health.bHealthy = Health.StaleSensorCount == 0 && Health.ErrorRealSensorSourceCount == 0;
    Health.Summary = FString::Printf(TEXT("Cameras=%d Lidars=%d RealSources=%d RunningRealSources=%d ErrorRealSources=%d ExternalEvidenceRequired=%d Stale=%d Healthy=%s PointCloudOnly=%s"),
        Health.CameraCount,
        Health.LidarCount,
        Health.RealSensorSourceCount,
        Health.RunningRealSensorSourceCount,
        Health.ErrorRealSensorSourceCount,
        Health.ExternalEvidenceRequiredRealSensorSourceCount,
        Health.StaleSensorCount,
        Health.bHealthy ? TEXT("true") : TEXT("false"),
        bPointCloudOnlyModeEnabled ? TEXT("true") : TEXT("false"));
    return Health;
}

void AVirtualSensorCoordinator::ApplyWidgetBinding()
{
    if (!BoundMonitorWidget) return;
    BoundMonitorWidget->BindVirtualCamera(GetSelectedCamera());
    BoundMonitorWidget->BindVirtualLidar(GetSelectedLidar());
    BoundMonitorWidget->BindRealSensorSource(GetSelectedRealSensorSource());
    if (CurrentViewMode == EVirtualSensorViewMode::PointCloudOnly)
    {
        BoundMonitorWidget->ShowLidarView();
    }
    else
    {
        SetViewMode(CurrentViewMode);
    }
}

void AVirtualSensorCoordinator::RunSynchronizedCapture()
{
    CaptureAllOnce();
}

void AVirtualSensorCoordinator::AssignSharedServicesIfPossible(UActorComponent* SensorComp)
{
    if (!SensorComp)
    {
        return;
    }

    if (UVirtualCameraCaptureComponent* CameraComp = Cast<UVirtualCameraCaptureComponent>(SensorComp))
    {
        if (SharedTransportComponent)
        {
            CameraComp->SetTransportComponent(SharedTransportComponent);
        }
        if (SharedRecorderComponent)
        {
            CameraComp->SetRecorderComponent(SharedRecorderComponent);
        }
    }
    else if (UVirtualLidarScanComponent* LidarComp = Cast<UVirtualLidarScanComponent>(SensorComp))
    {
        if (SharedTransportComponent)
        {
            LidarComp->SetTransportComponent(SharedTransportComponent);
        }
        if (SharedRecorderComponent)
        {
            LidarComp->SetRecorderComponent(SharedRecorderComponent);
        }
    }
}

void AVirtualSensorCoordinator::ApplyPointCloudOnlyVisibility()
{
    RestorePointCloudOnlyVisibility();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || ShouldKeepActorVisibleInPointCloudOnly(Actor))
        {
            continue;
        }

        TArray<UPrimitiveComponent*> PrimitiveComponents;
        Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
        for (UPrimitiveComponent* PrimitiveComp : PrimitiveComponents)
        {
            if (!PrimitiveComp)
            {
                continue;
            }

            FVirtualSensorHiddenComponentState State;
            State.Component = PrimitiveComp;
            State.bWasHiddenInGame = PrimitiveComp->bHiddenInGame;
            State.bWasVisible = PrimitiveComp->IsVisible();
            HiddenComponentStates.Add(State);

            PrimitiveComp->SetHiddenInGame(true);
            PrimitiveComp->SetVisibility(false, true);
        }
    }
}

void AVirtualSensorCoordinator::RestorePointCloudOnlyVisibility()
{
    for (const FVirtualSensorHiddenComponentState& State : HiddenComponentStates)
    {
        if (State.Component)
        {
            State.Component->SetHiddenInGame(State.bWasHiddenInGame);
            State.Component->SetVisibility(State.bWasVisible, true);
        }
    }
    HiddenComponentStates.Reset();
}

bool AVirtualSensorCoordinator::ShouldKeepActorVisibleInPointCloudOnly(const AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    if (Actor == this)
    {
        return true;
    }

    if (const UVirtualLidarScanComponent* SelectedLidar = GetSelectedLidar())
    {
        if (Actor == SelectedLidar->GetOwner())
        {
            return true;
        }
    }

    if (const UVirtualCameraCaptureComponent* SelectedCamera = GetSelectedCamera())
    {
        if (Actor == SelectedCamera->GetOwner())
        {
            return true;
        }
    }

    for (const FName& Tag : PointCloudOnlyKeepActorTags)
    {
        if (!Tag.IsNone() && Actor->ActorHasTag(Tag))
        {
            return true;
        }
    }

    return false;
}
