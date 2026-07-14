#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.h"

#include "ma0t10_dt/MA0T10/Camera/VirtualCameraComp.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/Sensor/RealSensorSourceComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDataTransportComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComp.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.h"

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
};

TMap<TWeakObjectPtr<UVirtualLidarSensorComp>, FLidarPointCloudOnlyState> GLidarPointCloudOnlyStates;

void SaveLidarViewState(UVirtualLidarSensorComp* LidarComp)
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
    GLidarPointCloudOnlyStates.Add(LidarComp, State);
}

void ApplyPointCloudOnlyToLidar(UVirtualLidarSensorComp* LidarComp, bool bSelected)
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
}

void RestoreLidarViewState(UVirtualLidarSensorComp* LidarComp)
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
        LidarComp->SetPointCloudPreviewEnabled(State->bPointCloudPreviewEnabled);
        GLidarPointCloudOnlyStates.Remove(LidarComp);
    }
    else
    {
        LidarComp->SetPointCloudPreviewEnabled(false);
    }
}
}

AVirtualSensorManager::AVirtualSensorManager()
{
    PrimaryActorTick.bCanEverTick = false;
    SharedTransportComponent = CreateDefaultSubobject<UVirtualSensorDataTransportComp>(TEXT("SharedSensorTransport"));
    SharedRecorderComponent = CreateDefaultSubobject<UVirtualSensorRecorderComp>(TEXT("SharedSensorRecorder"));
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
    RestorePointCloudOnlyVisibility();
    for (UVirtualLidarSensorComp* LidarComp : Lidars)
    {
        RestoreLidarViewState(LidarComp);
    }

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
    RealSensorSources.Reset();
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
        TArray<URealSensorSourceComp*> FoundRealSensorSources;
        Actor->GetComponents<URealSensorSourceComp>(FoundRealSensorSources);
        for (URealSensorSourceComp* RealSensorSourceComp : FoundRealSensorSources)
        {
            RegisterRealSensorSource(RealSensorSourceComp);
        }
    }
}

void AVirtualSensorManager::RegisterCamera(UVirtualCameraComp* CameraComp)
{
    if (CameraComp && !Cameras.Contains(CameraComp))
    {
        Cameras.Add(CameraComp);
        AssignSharedServicesIfPossible(CameraComp);
        ApplyWidgetBinding();
    }
}

void AVirtualSensorManager::RegisterLidar(UVirtualLidarSensorComp* LidarComp)
{
    if (LidarComp && !Lidars.Contains(LidarComp))
    {
        Lidars.Add(LidarComp);
        AssignSharedServicesIfPossible(LidarComp);
        if (bPointCloudOnlyModeEnabled)
        {
            ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
        }
        ApplyWidgetBinding();
    }
}

void AVirtualSensorManager::RegisterRealSensorSource(URealSensorSourceComp* RealSensorSourceComp)
{
    if (RealSensorSourceComp && !RealSensorSources.Contains(RealSensorSourceComp))
    {
        RealSensorSources.Add(RealSensorSourceComp);
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
        if (bPointCloudOnlyModeEnabled)
        {
            for (UVirtualLidarSensorComp* LidarComp : Lidars)
            {
                ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
            }
        }
        ApplyWidgetBinding();
    }
}

void AVirtualSensorManager::SelectRealSensorSourceByIndex(int32 Index)
{
    if (RealSensorSources.IsValidIndex(Index))
    {
        SelectedRealSensorSourceIndex = Index;
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
    if (bPointCloudOnlyModeEnabled)
    {
        for (UVirtualLidarSensorComp* LidarComp : Lidars)
        {
            ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
        }
    }
    ApplyWidgetBinding();
}

void AVirtualSensorManager::SetViewMode(EVirtualSensorViewMode NewMode)
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

void AVirtualSensorManager::SetPointCloudOnlyMode(bool bEnabled)
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

        for (UVirtualLidarSensorComp* LidarComp : Lidars)
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
        for (UVirtualLidarSensorComp* LidarComp : Lidars)
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

void AVirtualSensorManager::ToggleSensorView()
{
    SetViewMode(CurrentViewMode == EVirtualSensorViewMode::Camera ? EVirtualSensorViewMode::Lidar : EVirtualSensorViewMode::Camera);
}

void AVirtualSensorManager::TogglePointCloudOnlyView()
{
    SetPointCloudOnlyMode(!bPointCloudOnlyModeEnabled);
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

int32 AVirtualSensorManager::StartAllRealSensorSources()
{
    int32 StartedCount = 0;
    for (URealSensorSourceComp* RealSensorSourceComp : RealSensorSources)
    {
        if (RealSensorSourceComp && RealSensorSourceComp->StartSource())
        {
            ++StartedCount;
        }
    }
    return StartedCount;
}

void AVirtualSensorManager::StopAllRealSensorSources()
{
    for (URealSensorSourceComp* RealSensorSourceComp : RealSensorSources)
    {
        if (RealSensorSourceComp)
        {
            RealSensorSourceComp->StopSource();
        }
    }
}

bool AVirtualSensorManager::PushSelectedRealSensorSourceOnce(bool bSendTransport)
{
    URealSensorSourceComp* RealSensorSourceComp = GetSelectedRealSensorSource();
    return RealSensorSourceComp && RealSensorSourceComp->PushFrameOnce(bSendTransport);
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

void AVirtualSensorManager::CaptureSelectedOnce()
{
    if (UVirtualCameraComp* CameraComp = GetSelectedCamera())
    {
        CameraComp->CaptureAndSendImage();
    }
    if (UVirtualLidarSensorComp* LidarComp = GetSelectedLidar())
    {
        LidarComp->ScanAndSend();
    }
}

void AVirtualSensorManager::RefreshSelectedSensorOnce(bool bLidar)
{
    if (bLidar)
    {
        if (UVirtualLidarSensorComp* LidarComp = GetSelectedLidar())
        {
            LidarComp->ScanAndSend();
        }
        return;
    }

    if (UVirtualCameraComp* CameraComp = GetSelectedCamera())
    {
        CameraComp->CaptureAndSendImage();
    }
}

void AVirtualSensorManager::SetSelectedLidarPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly)
{
    if (UVirtualLidarSensorComp* LidarComp = GetSelectedLidar())
    {
        LidarComp->SetPreviewPolicy(InStride, InMaxPoints, bInHitOnly);
    }
}

void AVirtualSensorManager::AdjustSelectedLidarPreviewBudget(int32 StrideDelta, int32 MaxPointsDelta)
{
    if (UVirtualLidarSensorComp* LidarComp = GetSelectedLidar())
    {
        const int32 NextStride = FMath::Clamp(LidarComp->PreviewPointStride + StrideDelta, 1, 100);
        const int32 NextMaxPoints = FMath::Clamp(LidarComp->MaxPreviewPoints + MaxPointsDelta, 0, 1000000);
        LidarComp->SetPreviewPolicy(NextStride, NextMaxPoints, LidarComp->bPointCloudPreviewHitOnly);
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

URealSensorSourceComp* AVirtualSensorManager::GetSelectedRealSensorSource() const
{
    if (UVirtualLidarSensorComp* SelectedLidar = GetSelectedLidar())
    {
        for (URealSensorSourceComp* RealSensorSourceComp : RealSensorSources)
        {
            if (RealSensorSourceComp && RealSensorSourceComp->TargetLidar == SelectedLidar)
            {
                return RealSensorSourceComp;
            }
        }
    }
    return RealSensorSources.IsValidIndex(SelectedRealSensorSourceIndex) ? RealSensorSources[SelectedRealSensorSourceIndex] : nullptr;
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

TArray<FVirtualSensorSummary> AVirtualSensorManager::GetRealSensorSourceSummaries() const
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

FVirtualSensorHealthSummary AVirtualSensorManager::GetHealthSummary() const
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

    for (const URealSensorSourceComp* RealSensorSourceComp : RealSensorSources)
    {
        if (!RealSensorSourceComp)
        {
            ++Health.ErrorRealSensorSourceCount;
            continue;
        }

        if (RealSensorSourceComp->GetConnectionState() == ERealSensorSourceConnectionState::Running)
        {
            ++Health.RunningRealSensorSourceCount;
        }
        else if (RealSensorSourceComp->GetConnectionState() == ERealSensorSourceConnectionState::Error)
        {
            ++Health.ErrorRealSensorSourceCount;
        }

        if (RealSensorSourceComp->RequiresExternalDeploymentEvidence())
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

void AVirtualSensorManager::ApplyWidgetBinding()
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

void AVirtualSensorManager::RunSynchronizedCapture()
{
    CaptureAllOnce();
}

void AVirtualSensorManager::AssignSharedServicesIfPossible(UActorComponent* SensorComp)
{
    if (!SensorComp)
    {
        return;
    }

    if (UVirtualCameraComp* CameraComp = Cast<UVirtualCameraComp>(SensorComp))
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
    else if (UVirtualLidarSensorComp* LidarComp = Cast<UVirtualLidarSensorComp>(SensorComp))
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

void AVirtualSensorManager::ApplyPointCloudOnlyVisibility()
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

void AVirtualSensorManager::RestorePointCloudOnlyVisibility()
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

bool AVirtualSensorManager::ShouldKeepActorVisibleInPointCloudOnly(const AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    if (Actor == this)
    {
        return true;
    }

    if (const UVirtualLidarSensorComp* SelectedLidar = GetSelectedLidar())
    {
        if (Actor == SelectedLidar->GetOwner())
        {
            return true;
        }
    }

    if (const UVirtualCameraComp* SelectedCamera = GetSelectedCamera())
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
