#include "Modules/ModuleManager.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraAct.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorAct.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorHostActor.h"

namespace
{
void NotifyMapApply(const FString& Message, bool bSuccess)
{
    UE_LOG(LogTemp, Log, TEXT("[SensorTestMapApply] %s"), *Message);
    FNotificationInfo Info(FText::FromString(Message));
    Info.ExpireDuration = 6.0f;
    Info.bUseSuccessFailIcons = true;
    const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
    if (Item.IsValid())
    {
        Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
    }
}

bool ApplyStateToEditorActor(AActor* Actor, const FVirtualSensorEditableState& State)
{
    if (!Actor) return false;
    Actor->Modify();
    Actor->SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);

    if (State.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        AVirtualCameraAct* CameraActor = Cast<AVirtualCameraAct>(Actor);
        UVirtualCameraComp* Camera = CameraActor ? CameraActor->VirtualCameraComp : nullptr;
        if (!Camera) return false;
        Camera->Modify();
        Camera->ApplyDeviceProfile(State.CameraProfile);
        Camera->ApplySimulationQuality(State.SimulationQuality);
        Camera->SensorId = State.SensorId;
        Camera->CaptureResolution = State.CameraResolution;
        Camera->CaptureInterval = State.CameraCaptureInterval;
        Camera->FOVAngle = State.CameraFov;
        Camera->JpegQuality = State.CameraJpegQuality;
        Camera->CaptureMode = State.CameraCaptureMode;
        CameraActor->RerunConstructionScripts();
        return true;
    }

    AVirtualSensorAct* LidarActor = Cast<AVirtualSensorAct>(Actor);
    UVirtualLidarSensorComp* Lidar = LidarActor ? LidarActor->LidarSensorComp : nullptr;
    if (!Lidar) return false;
    Lidar->Modify();
    Lidar->ApplyDeviceProfile(State.LidarProfile);
    Lidar->ApplySimulationQuality(State.SimulationQuality);
    Lidar->SensorId = State.SensorId;
    Lidar->ScanInterval = State.LidarScanInterval;
    Lidar->MaxDistance = State.LidarMaxDistance;
    Lidar->HorizontalSamples = State.LidarHorizontalSamples;
    Lidar->VerticalChannels = State.LidarVerticalChannels;
    Lidar->HorizontalFov = State.LidarHorizontalFov;
    Lidar->MinVerticalAngle = State.LidarMinVerticalAngle;
    Lidar->MaxVerticalAngle = State.LidarMaxVerticalAngle;
    Lidar->SetServerPayloadPolicy(State.ServerPayloadStride, State.MaxServerPayloadPoints, State.bIncludeMissPointsInServerPayload);
    Lidar->SetPreviewPolicy(State.PreviewPointStride, State.MaxPreviewPoints, State.bPreviewHitOnly);
    Lidar->bUseMultiHit = State.bUseMultiHit;
    Lidar->MaxHitsPerRay = State.MaxHitsPerRay;
    Lidar->bExportCsvOnScan = State.bExportCsvOnScan;
    Lidar->bExportJsonLinesOnScan = State.bExportJsonLinesOnScan;
    Lidar->bExportPcdOnScan = State.bExportPcdOnScan;
    LidarActor->RerunConstructionScripts();
    return true;
}

bool IsEditorActorCompatible(const AActor* Actor, const FVirtualSensorEditableState& State)
{
    if (State.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        const AVirtualCameraAct* CameraActor = Cast<AVirtualCameraAct>(Actor);
        return CameraActor && CameraActor->VirtualCameraComp;
    }
    const AVirtualSensorAct* LidarActor = Cast<AVirtualSensorAct>(Actor);
    return LidarActor && LidarActor->LidarSensorComp;
}
}

class FMa0t10DtEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        PrePieEndedHandle = FEditorDelegates::PrePIEEnded.AddRaw(this, &FMa0t10DtEditorModule::HandlePrePieEnded);
        EndPieHandle = FEditorDelegates::EndPIE.AddRaw(this, &FMa0t10DtEditorModule::HandleEndPie);
    }

    virtual void ShutdownModule() override
    {
        FEditorDelegates::PrePIEEnded.Remove(PrePieEndedHandle);
        FEditorDelegates::EndPIE.Remove(EndPieHandle);
    }

private:
    void HandlePrePieEnded(bool bIsSimulating)
    {
        PendingSnapshot = FVirtualSensorMapApplySnapshot();
        bHasPendingSnapshot = false;
        UWorld* PieWorld = GEditor ? GEditor->PlayWorld : nullptr;
        if (!PieWorld) return;

        for (TActorIterator<AVirtualSensorMonitorHostActor> It(PieWorld); It; ++It)
        {
            if (It->HasPendingMapApplySnapshot())
            {
                PendingSnapshot = It->GetPendingMapApplySnapshot();
                bHasPendingSnapshot = true;
                break;
            }
        }
    }

    void HandleEndPie(bool bIsSimulating)
    {
        if (!bHasPendingSnapshot || PendingSnapshot.SensorStates.Num() <= 0) return;
        bHasPendingSnapshot = false;

        UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != PendingSnapshot.SourceMapPackage)
        {
            NotifyMapApply(TEXT("SensorTestMap apply rejected: the editor world is not /Game/MA0T10/Maps/SensorTestMap."), false);
            return;
        }

        TArray<AActor*> MatchedActors;
        MatchedActors.Reserve(PendingSnapshot.SensorStates.Num());
        for (const FVirtualSensorEditableState& State : PendingSnapshot.SensorStates)
        {
            TArray<AActor*> Matches;
            for (TActorIterator<AActor> It(EditorWorld); It; ++It)
            {
                if (It->ActorHasTag(State.PersistentActorTag)) Matches.Add(*It);
            }
            if (State.PersistentActorTag.IsNone() || Matches.Num() != 1)
            {
                NotifyMapApply(FString::Printf(TEXT("SensorTestMap apply rejected: persistent tag '%s' matched %d actors."), *State.PersistentActorTag.ToString(), Matches.Num()), false);
                return;
            }
            if (!IsEditorActorCompatible(Matches[0], State))
            {
                NotifyMapApply(FString::Printf(TEXT("SensorTestMap apply rejected: actor tagged '%s' has the wrong sensor type."), *State.PersistentActorTag.ToString()), false);
                return;
            }
            MatchedActors.Add(Matches[0]);
        }

        {
            FScopedTransaction Transaction(FText::FromString(TEXT("Apply PIE sensor settings to SensorTestMap")));
            for (int32 Index = 0; Index < PendingSnapshot.SensorStates.Num(); ++Index)
            {
                if (!ApplyStateToEditorActor(MatchedActors[Index], PendingSnapshot.SensorStates[Index]))
                {
                    Transaction.Cancel();
                    NotifyMapApply(TEXT("SensorTestMap apply failed while copying a sensor state."), false);
                    return;
                }
            }
        }

        EditorWorld->MarkPackageDirty();
        if (!FEditorFileUtils::SaveLevel(EditorWorld->PersistentLevel))
        {
            if (GEditor)
            {
                GEditor->UndoTransaction();
            }
            NotifyMapApply(TEXT("SensorTestMap save failed; the queued sensor changes were rolled back."), false);
            return;
        }
        NotifyMapApply(FString::Printf(TEXT("SensorTestMap saved with %d queued sensor change(s)."), PendingSnapshot.SensorStates.Num()), true);
    }

    FDelegateHandle PrePieEndedHandle;
    FDelegateHandle EndPieHandle;
    FVirtualSensorMapApplySnapshot PendingSnapshot;
    bool bHasPendingSnapshot = false;
};

IMPLEMENT_MODULE(FMa0t10DtEditorModule, ma0t10_dtEditor);
