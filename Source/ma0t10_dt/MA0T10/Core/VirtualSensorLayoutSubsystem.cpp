#include "VirtualSensorLayoutSubsystem.h"
#include "VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "VirtualSensorStreamPublisherComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"

AVirtualSensorCoordinator* UVirtualSensorLayoutSubsystem::Coordinator() const
{
    AVirtualSensorCoordinator* Found=nullptr;
    for(TActorIterator<AVirtualSensorCoordinator> It(GetWorld());It;++It){if(Found)return nullptr;Found=*It;}
    return Found;
}
void UVirtualSensorLayoutSubsystem::Changed(){++Revision;if(!bEnding)OnLayoutChanged.Broadcast();}
void UVirtualSensorLayoutSubsystem::Deinitialize(){bEnding=true;Owned.Reset();Super::Deinitialize();}
void UVirtualSensorLayoutSubsystem::Tick(float)
{bool Removed=false;for(auto It=Owned.CreateIterator();It;++It)if(!It.Key().IsValid()||It.Key()->IsActorBeingDestroyed()){It.RemoveCurrent();Removed=true;}if(Removed)Changed();}
TStatId UVirtualSensorLayoutSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorLayoutSubsystem,STATGROUP_Tickables);}
bool UVirtualSensorLayoutSubsystem::OwnsSensor(AVirtualSensorActorBase* S) const {return S&&Owned.Contains(S);}
TArray<FVirtualSensorLayoutItem> UVirtualSensorLayoutSubsystem::GetLayoutSensors() const
{
    TArray<FVirtualSensorLayoutItem> Result;if(auto* C=Coordinator())for(auto* S:C->GetSensorActors())if(IsValid(S)&&!S->IsActorBeingDestroyed())
    {
        auto& I=Result.AddDefaulted_GetRef();I.Actor=S;I.SensorId=S->GetSensorId();I.Kind=S->GetSensorKind();I.bPreviewEnabled=S->IsSensorRunning();
        if(const auto* G=Owned.Find(S)){I.bToolCreated=true;I.LayoutGuid=*G;}S->CanEditSensorPlacement(I.LockReason);
    }
    return Result;
}
bool UVirtualSensorLayoutSubsystem::CanCreateSensor(EVirtualSensorKind Kind,FString& Reason) const
{
    Reason.Reset();auto* C=Coordinator();
    if(bEnding||!GetWorld()||!C){Reason=TEXT("센서 Coordinator가 정확히 하나 필요합니다.");return false;}
    if(Kind!=EVirtualSensorKind::Camera&&Kind!=EVirtualSensorKind::Lidar){Reason=TEXT("지원하지 않는 장비 종류입니다.");return false;}
    if(C->GetSensorActors().Num()>=32){Reason=TEXT("현재 배치 도구의 장비 상한은 32대입니다. 성능 지원 범위와는 별개입니다.");return false;}
    if(C->StreamPublisherComponent)for(const auto& S:C->StreamPublisherComponent->GetStreamStatuses())
        if(S.SensorId.IsEmpty()&&(S.bEnabled||S.InputQueueDepth||S.PreparedQueueDepth||S.ReceiptQueueDepth)&&
            ((S.StreamKind==EVirtualSensorStreamKind::CameraImage)==(Kind==EVirtualSensorKind::Camera)))
        {Reason=TEXT("전체 센서 송신을 중지하고 정리가 끝난 뒤 추가하세요. 새 장비의 자동 송신을 방지합니다.");return false;}
    return true;
}
bool UVirtualSensorLayoutSubsystem::ViewPose(FTransform& Pose,FString& Reason) const
{
    auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
    if(!PC||!PC->PlayerCameraManager){Reason=TEXT("현재 관찰 시점이 준비되지 않았습니다.");return false;}
    Pose=FTransform(PC->PlayerCameraManager->GetCameraRotation(),PC->PlayerCameraManager->GetCameraLocation());return true;
}
AVirtualSensorActorBase* UVirtualSensorLayoutSubsystem::SpawnStopped(EVirtualSensorKind Kind,const FTransform& Pose,FString& Reason)
{
    if(Pose.ContainsNaN()||!Pose.GetRotation().IsNormalized()){Reason=TEXT("유효하지 않은 배치 자세입니다.");return nullptr;}
    AVirtualSensorActorBase* S=nullptr;
    if(Kind==EVirtualSensorKind::Camera)
    {
        auto* A=GetWorld()->SpawnActorDeferred<AVirtualCameraSensorActor>(AVirtualCameraSensorActor::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(!A)return nullptr;auto* C=A->CaptureComponent.Get();C->bAutoStartCapture=false;C->bAutoRegisterToManager=false;C->bApplyDeviceProfileOnBeginPlay=false;
        C->ApplyDeviceProfile(EVirtualCameraDeviceProfile::IntelRealSenseD455);C->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);C->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;S=A;
    }
    else
    {
        auto* A=GetWorld()->SpawnActorDeferred<AVirtualLidarSensorActor>(AVirtualLidarSensorActor::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(!A)return nullptr;auto* C=A->ScanComponent.Get();C->bAutoStartScan=false;C->bAutoRegisterToManager=false;C->bApplyDeviceProfileOnBeginPlay=false;
        C->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);C->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);S=A;
    }
    S->FinishSpawning(Pose);S->StopSensor();return S;
}
AVirtualSensorActorBase* UVirtualSensorLayoutSubsystem::CreateSensorAtPose(EVirtualSensorKind Kind,const FTransform& Pose,FString& Reason,bool Preview)
{
    if(!CanCreateSensor(Kind,Reason))return nullptr;
    auto* S=SpawnStopped(Kind,Pose,Reason);if(!S){if(Reason.IsEmpty())Reason=TEXT("장비 생성에 실패했습니다.");return nullptr;}
    FVirtualSensorEditableState State;S->ReadEditableState(State);
    const FString Guid=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    State.SensorId=(Kind==EVirtualSensorKind::Camera?TEXT("CAMERA-"):TEXT("LIDAR-"))+Guid;
    State.ActorTransform=Pose;State.bExportCsvOnScan=false;State.bExportJsonLinesOnScan=false;State.bExportPcdOnScan=false;
    if(!S->ApplyEditableState(State,Reason)){S->Destroy();return nullptr;}
    Owned.Add(S,Guid);Coordinator()->RegisterSensorActor(S);Coordinator()->SelectSensorActor(S);if(Preview)S->StartSensor();Changed();return S;
}
AVirtualSensorActorBase* UVirtualSensorLayoutSubsystem::CreateSensorAtView(EVirtualSensorKind Kind,FString& Reason)
{FTransform Pose;if(!ViewPose(Pose,Reason))return nullptr;return CreateSensorAtPose(Kind,Pose,Reason);}
AVirtualSensorActorBase* UVirtualSensorLayoutSubsystem::DuplicateSensor(AVirtualSensorActorBase* Source,FString& Reason)
{
    if(!IsValid(Source)||!Coordinator()||!Coordinator()->GetSensorActors().Contains(Source)){Reason=TEXT("등록된 센서를 선택하세요.");return nullptr;}
    FTransform Pose;if(!ViewPose(Pose,Reason)||!CanCreateSensor(Source->GetSensorKind(),Reason))return nullptr;
    FVirtualSensorEditableState State;bool Preview=false;if(!Source->ReadCollectionState(State,Preview))return nullptr;
    auto* Copy=CreateSensorAtPose(Source->GetSensorKind(),Pose,Reason,false);if(!Copy)return nullptr;
    State.OriginalSensorId=Copy->GetSensorId();State.SensorId=Copy->GetSensorId();State.ActorTransform=Pose;
    State.bExportCsvOnScan=State.bExportJsonLinesOnScan=State.bExportPcdOnScan=false;State.CameraCaptureMode=EVirtualCameraCaptureMode::PreviewOnly;
    if(!Copy->ApplyEditableState(State,Reason)){FString Ignored;DeleteSensor(Copy,Ignored);return nullptr;}
    if(auto* L=Cast<AVirtualLidarSensorActor>(Copy))if(auto* From=Cast<AVirtualLidarSensorActor>(Source))L->ScanComponent->AcquisitionBackend=From->ScanComponent->AcquisitionBackend;
    Copy->StartSensor();Changed();return Copy;
}
bool UVirtualSensorLayoutSubsystem::CanDeleteSensor(AVirtualSensorActorBase* S,FString& Reason) const
{
    if(!IsValid(S)||!OwnsSensor(S)){Reason=TEXT("도구로 추가한 장비만 삭제할 수 있습니다. 레벨 배치 장비는 보존합니다.");return false;}
    return S->CanEditSensorPlacement(Reason);
}
bool UVirtualSensorLayoutSubsystem::DeleteSensor(AVirtualSensorActorBase* S,FString& Reason)
{
    if(!CanDeleteSensor(S,Reason))return false;
    S->OnDataCollectionStarting.Broadcast();S->EndInteractiveManipulation();
    GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->ForgetSensorIdentity(S);
    S->StopSensor();if(auto* C=Coordinator())C->UnregisterSensorActor(S);Owned.Remove(S);S->Destroy();Changed();return true;
}
FString UVirtualSensorLayoutSubsystem::CanonicalActorPath(const AActor* A)
{
    if(!A)return FString();FString Path=A->GetPathName();
    // Strip only known PIE package/world prefixes, preserving actor subpath.
    const FString Prefix=FString::Printf(TEXT("UEDPIE_%d_"),A->GetOutermost()->GetPIEInstanceID());
    if(A->GetOutermost()->GetPIEInstanceID()!=INDEX_NONE)Path.ReplaceInline(*Prefix,TEXT(""));return Path;
}
