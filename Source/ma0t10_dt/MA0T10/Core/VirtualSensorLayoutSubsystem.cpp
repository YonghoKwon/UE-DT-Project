#include "VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
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
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"

namespace { FCriticalSection LayoutFileMutex; }

AVirtualSensorCoordinator* UVirtualSensorLayoutSubsystem::Coordinator() const
{
    AVirtualSensorCoordinator* Found=nullptr;
    for(TActorIterator<AVirtualSensorCoordinator> It(GetWorld());It;++It){if(Found)return nullptr;Found=*It;}
    return Found;
}
void UVirtualSensorLayoutSubsystem::Changed()
{
    InventoryIds.Reset();if(!bEnding)if(auto* C=Coordinator())for(auto* S:C->GetSensorActors())if(IsValid(S))InventoryIds.Add(S,S->GetSensorId());
    ++Revision;if(!bEnding)OnLayoutChanged.Broadcast();
}
void UVirtualSensorLayoutSubsystem::Deinitialize(){bEnding=true;if(PendingWrite)FinishWrite(PendingWrite->Future.Get());Owned.Reset();Super::Deinitialize();}
void UVirtualSensorLayoutSubsystem::Tick(float Delta)
{
    if(PendingWrite&&PendingWrite->Future.IsReady())FinishWrite(PendingWrite->Future.Get());
    bool Removed=false;for(auto It=Owned.CreateIterator();It;++It)if(!It.Key().IsValid()||It.Key()->IsActorBeingDestroyed()){It.RemoveCurrent();Removed=true;}
    if(Removed)Changed();
    InventoryPollSeconds+=Delta;if(InventoryPollSeconds<.2f)return;InventoryPollSeconds=0;
    TMap<TWeakObjectPtr<AVirtualSensorActorBase>,FString> Current;
    if(auto* C=Coordinator())for(auto* S:C->GetSensorActors())if(IsValid(S))Current.Add(S,S->GetSensorId());
    bool Different=Current.Num()!=InventoryIds.Num();for(const auto& I:Current){const auto* Old=InventoryIds.Find(I.Key);Different|=!Old||*Old!=I.Value;}
    if(Different)Changed();
}
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
    if(Kind==EVirtualSensorKind::Lidar)State.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;
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

FString UVirtualSensorLayoutSubsystem::Directory() const
{
    FString Root=FPaths::ProjectSavedDir()/VirtualSensorDefaults::LayoutDirectory;
#if WITH_DEV_AUTOMATION_TESTS
    if(!StorageRootOverrideForTests.IsEmpty())Root=StorageRootOverrideForTests;
#endif
    return FPaths::ConvertRelativePathToFull(Root);
}
FString UVirtualSensorLayoutSubsystem::LayoutPath(const FString& Name) const
{return Directory()/(FMD5::HashAnsiString(*(UVirtualSensorPlacementSubsystem::CanonicalMap(GetWorld())+TEXT("|")+Name.ToLower()))+TEXT(".layout.v1.json"));}
FString UVirtualSensorLayoutSubsystem::AddOperation(EVirtualSensorLayoutOperation State,const FString& Message,const FString& Path)
{
    FVirtualSensorLayoutOperationStatus S;S.RequestId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);S.State=State;S.Message=Message;S.Path=Path;
    Operations.Add(S.RequestId,S);OperationOrder.Add(S.RequestId);while(OperationOrder.Num()>40){Operations.Remove(OperationOrder[0]);OperationOrder.RemoveAt(0);}return S.RequestId;
}
FVirtualSensorLayoutOperationStatus UVirtualSensorLayoutSubsystem::GetOperationStatus(const FString& Id) const
{if(const auto* S=Operations.Find(Id))return *S;FVirtualSensorLayoutOperationStatus S;S.RequestId=Id;S.Message=TEXT("배치 요청을 찾을 수 없습니다.");return S;}
bool UVirtualSensorLayoutSubsystem::BuildLayoutSnapshot(FVirtualSensorLayoutDocument& D,FString& Reason) const
{
    auto* C=Coordinator();if(!C||bEnding){Reason=TEXT("배치 서비스가 준비되지 않았습니다.");return false;}
    D=FVirtualSensorLayoutDocument();D.MapPackage=UVirtualSensorPlacementSubsystem::CanonicalMap(GetWorld());D.SavedUtc=FDateTime::UtcNow();
    TSet<FString> Ids;
    for(auto* S:C->GetSensorActors())
    {
        if(!IsValid(S)||S->IsActorBeingDestroyed())continue;
        if(Ids.Contains(S->GetSensorId())){Reason=TEXT("중복 SensorId를 먼저 수정하세요.");return false;}Ids.Add(S->GetSensorId());
        auto& E=D.Sensors.AddDefaulted_GetRef();if(!S->ReadCollectionState(E.State,E.bPreviewEnabled)){Reason=TEXT("저장할 수 없는 센서 설정입니다.");return false;}
        E.State.bExportCsvOnScan=E.State.bExportJsonLinesOnScan=E.State.bExportPcdOnScan=false;
        if(const auto* G=Owned.Find(S)){E.bToolCreated=true;E.LayoutGuid=*G;}else E.ActorPath=CanonicalActorPath(S);
        if(auto* L=Cast<AVirtualLidarSensorActor>(S))E.LidarBackend=L->ScanComponent->AcquisitionBackend;
    }
    if(D.Sensors.Num()>32){Reason=TEXT("배치 파일은 최대 32대를 지원합니다.");return false;}return true;
}
FString UVirtualSensorLayoutSubsystem::RequestSaveLayout(const FString& RawName,bool Overwrite)
{
    const FString Name=RawName.TrimStartAndEnd();
    if(Name.IsEmpty()||Name.Len()>64||Name.Contains(TEXT("\n"))||Name.Contains(TEXT("\r")))return AddOperation(EVirtualSensorLayoutOperation::Failed,TEXT("배치 이름은 1~64자여야 합니다."));
    if(bEnding||PendingWrite)return AddOperation(EVirtualSensorLayoutOperation::Failed,TEXT("배치 파일 쓰기를 마친 뒤 요청하세요."));
    const FString Path=LayoutPath(Name);if(!Overwrite&&IFileManager::Get().FileExists(*Path))return AddOperation(EVirtualSensorLayoutOperation::Failed,TEXT("같은 배치가 있습니다. 명시적 덮어쓰기를 선택하세요."),Path);
    FVirtualSensorLayoutDocument D;FString Error;if(!BuildLayoutSnapshot(D,Error))return AddOperation(EVirtualSensorLayoutOperation::Failed,Error);
    D.Name=Name;FString Json;FJsonObjectConverter::UStructToJsonObjectString(D,Json);FTCHARToUTF8 Bytes(*Json);
    if(Bytes.Length()>1024*1024)return AddOperation(EVirtualSensorLayoutOperation::Failed,TEXT("배치 파일의 1MiB 한도를 초과했습니다."));
    const FString Id=AddOperation(EVirtualSensorLayoutOperation::Writing,TEXT("전체 배치 저장 중"),Path);
    PendingWrite=MakeUnique<FPendingWrite>();PendingWrite->Id=Id;PendingWrite->Future=Async(EAsyncExecution::ThreadPool,[Path,Json](){return WriteDocument(Path,Json);});return Id;
}
UVirtualSensorLayoutSubsystem::FWriteResult UVirtualSensorLayoutSubsystem::WriteDocument(const FString& Path,const FString& Json)
{
    FScopeLock Lock(&LayoutFileMutex);
    FWriteResult R;const FString Temp=Path+TEXT(".writing"),Backup=Path+TEXT(".previous");auto& Files=IFileManager::Get();
    if(!Files.MakeDirectory(*FPaths::GetPath(Path),true)||!FFileHelper::SaveStringToFile(Json,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)){R.Error=TEXT("배치 파일 쓰기에 실패했습니다.");return R;}
    const bool Had=Files.FileExists(*Path);if(Had&&Files.Copy(*Backup,*Path,true,true)!=COPY_OK){Files.Delete(*Temp);R.Error=TEXT("원본 배치를 보호하지 못해 저장을 거절했습니다.");return R;}
    R.Success=Files.Move(*Path,*Temp,true,true);if(!R.Success){if(Had)Files.Copy(*Path,*Backup,true,true);Files.Delete(*Temp);R.Error=TEXT("배치 저장 확정 실패 · 이전 파일 보존");}return R;
}
void UVirtualSensorLayoutSubsystem::FinishWrite(FWriteResult R)
{
    if(auto* S=Operations.Find(PendingWrite->Id)){S->State=R.Success?EVirtualSensorLayoutOperation::Saved:EVirtualSensorLayoutOperation::Failed;S->Message=R.Success?TEXT("전체 배치 저장 완료"):R.Error;}
    PendingWrite.Reset();Changed();
}
TArray<FString> UVirtualSensorLayoutSubsystem::GetSavedLayoutNames() const
{
    TArray<FString> Files,Names;IFileManager::Get().FindFiles(Files,*(Directory()/TEXT("*.layout.v1.json")),true,false);
    for(const auto& F:Files){FString Json;FVirtualSensorLayoutDocument D;const FString Path=Directory()/F;if(IFileManager::Get().FileSize(*Path)<=1024*1024&&FFileHelper::LoadFileToString(Json,*Path)&&FJsonObjectConverter::JsonObjectStringToUStruct(Json,&D)&&D.Version==1&&D.Schema==TEXT("ma0t10.sensor-layout.v1")&&D.MapPackage==UVirtualSensorPlacementSubsystem::CanonicalMap(GetWorld()))Names.Add(D.Name);}
    Names.Sort();return Names;
}
bool UVirtualSensorLayoutSubsystem::ValidateDocument(const FVirtualSensorLayoutDocument& D,TArray<AVirtualSensorActorBase*>& Matches,TArray<AVirtualSensorActorBase*>& Excess,FString& Reason) const
{
    Matches.Reset();Excess.Reset();auto* C=Coordinator();
    if(!C||bEnding||D.Version!=1||D.Schema!=TEXT("ma0t10.sensor-layout.v1")||D.MapPackage!=UVirtualSensorPlacementSubsystem::CanonicalMap(GetWorld())||D.Sensors.Num()>32){Reason=TEXT("배치 버전·맵·장비 수가 올바르지 않습니다.");return false;}
    TSet<FString> Ids,Guids,Paths;TSet<AVirtualSensorActorBase*> Included;
    for(const auto& E:D.Sensors)
    {
        if(E.State.SensorId.IsEmpty()||Ids.Contains(E.State.SensorId)||E.State.ActorTransform.ContainsNaN()||!E.State.ActorTransform.GetRotation().IsNormalized()||
            E.State.bExportCsvOnScan||E.State.bExportJsonLinesOnScan||E.State.bExportPcdOnScan||!StaticEnum<EVirtualSensorTargetKind>()->IsValidEnumValue(int64(E.State.TargetKind))||
            !StaticEnum<EVirtualSensorSimulationQuality>()->IsValidEnumValue(int64(E.State.SimulationQuality))||!StaticEnum<EVirtualLidarAcquisitionBackend>()->IsValidEnumValue(int64(E.LidarBackend))||
            !StaticEnum<EVirtualCameraDeviceProfile>()->IsValidEnumValue(int64(E.State.CameraProfile))||!StaticEnum<EVirtualLidarDeviceProfile>()->IsValidEnumValue(int64(E.State.LidarProfile)))
        {Reason=TEXT("배치에 중복 ID 또는 유효하지 않은 설정이 있습니다.");return false;}Ids.Add(E.State.SensorId);
        AVirtualSensorActorBase* Match=nullptr;
        for(auto* S:C->GetSensorActors())if(IsValid(S))
        {
            const bool Same=E.bToolCreated?(Owned.Find(S)&&*Owned.Find(S)==E.LayoutGuid):(!Owned.Contains(S)&&CanonicalActorPath(S)==E.ActorPath);
            if(Same){if(Match){Reason=TEXT("배치 대응 대상이 모호합니다.");return false;}Match=S;}
        }
        if(E.bToolCreated){FGuid G;if(!FGuid::Parse(E.LayoutGuid,G)||!G.IsValid()||Guids.Contains(E.LayoutGuid)){Reason=TEXT("배치 GUID가 올바르지 않습니다.");return false;}Guids.Add(E.LayoutGuid);}
        else if(E.ActorPath.IsEmpty()||Paths.Contains(E.ActorPath)||!Match){Reason=TEXT("기존 레벨 장비 경로가 일치하지 않습니다.");return false;}else Paths.Add(E.ActorPath);
        const auto Kind=E.State.TargetKind==EVirtualSensorTargetKind::Camera?EVirtualSensorKind::Camera:EVirtualSensorKind::Lidar;
        if(Match&&(Match->GetSensorKind()!=Kind||!Match->CanEditSensorPlacement(Reason)))return false;
        // Validate against a native CDO without creating actors or changing preview.
        auto* Validator=Kind==EVirtualSensorKind::Camera?static_cast<AVirtualSensorActorBase*>(GetMutableDefault<AVirtualCameraSensorActor>()):static_cast<AVirtualSensorActorBase*>(GetMutableDefault<AVirtualLidarSensorActor>());
        if(!Validator->ValidateEditableState(E.State,Reason))return false;
        if(!E.bToolCreated&&!Match->ValidateEditableState(E.State,Reason))return false;
        if(Match)Included.Add(Match);Matches.Add(Match);
    }
    for(const auto& P:Owned)if(P.Key.IsValid()&&!Included.Contains(P.Key.Get())){if(!P.Key->CanEditSensorPlacement(Reason))return false;Excess.Add(P.Key.Get());}
    for(auto* S:C->GetSensorActors())if(IsValid(S)&&!Included.Contains(S)&&!Excess.Contains(S)&&Ids.Contains(S->GetSensorId())){Reason=TEXT("보존할 기존 장비와 SensorId가 충돌합니다.");return false;}
    // New runtime actors must not be captured by any active wildcard output.
    for(int32 I=0;I<D.Sensors.Num();++I)if(!Matches[I]){const auto Kind=D.Sensors[I].State.TargetKind==EVirtualSensorTargetKind::Camera?EVirtualSensorKind::Camera:EVirtualSensorKind::Lidar;if(!CanCreateSensor(Kind,Reason))return false;}
    return true;
}
FString UVirtualSensorLayoutSubsystem::PreviewLayoutRestore(const FString& Name)
{
    CancelLayoutRestorePreview(RestoreId);
    if(PendingWrite||bEnding)return AddOperation(EVirtualSensorLayoutOperation::Failed,TEXT("배치 쓰기가 끝난 뒤 불러오세요."));
    FString Json,Reason;FVirtualSensorLayoutDocument D;const FString Path=LayoutPath(Name);
    if(IFileManager::Get().FileSize(*Path)>1024*1024||!FFileHelper::LoadFileToString(Json,*Path)||!FJsonObjectConverter::JsonObjectStringToUStruct(Json,&D))return AddOperation(EVirtualSensorLayoutOperation::Failed,TEXT("배치 파일을 읽을 수 없습니다. 원본 보존"),Path);
    TArray<AVirtualSensorActorBase*> Matches,Excess;if(!ValidateDocument(D,Matches,Excess,Reason))return AddOperation(EVirtualSensorLayoutOperation::Failed,Reason,Path);
    int32 Create=0,Update=0;for(auto* S:Matches){if(S)++Update;else ++Create;}
    RestoreDocument=MoveTemp(D);RestoreInventoryRevision=Revision;RestorePoses.Reset();RestoreRevisions.Reset();
    for(auto* S:Coordinator()->GetSensorActors())if(IsValid(S)){RestorePoses.Add(S,S->GetActorTransform());RestoreRevisions.Add(S,S->GetConfigurationRevision());}
    RestoreId=AddOperation(EVirtualSensorLayoutOperation::ReadyToApply,FString::Printf(TEXT("‘%s’ 변경 미리보기 · 복원 %d · 추가 %d · 도구 장비 정리 %d · 기존 레벨 장비 삭제 없음"),*RestoreDocument.Name,Update,Create,Excess.Num()),Path);return RestoreId;
}
bool UVirtualSensorLayoutSubsystem::CancelLayoutRestorePreview(const FString& Id)
{
    auto* Op=Operations.Find(Id);
    if(!Op)return false;
    if(Op->State==EVirtualSensorLayoutOperation::Cancelled)return true;
    if(Id!=RestoreId||Op->State!=EVirtualSensorLayoutOperation::ReadyToApply)return false;
    Op->State=EVirtualSensorLayoutOperation::Cancelled;Op->Message=TEXT("복원 취소 · 현재 배치 유지");
    RestoreId.Reset();RestoreDocument=FVirtualSensorLayoutDocument();RestorePoses.Reset();RestoreRevisions.Reset();return true;
}
bool UVirtualSensorLayoutSubsystem::ApplyLayoutRestore(const FString& Id,FString& Reason)
{
    if(Id.IsEmpty()||Id!=RestoreId||PendingWrite||bEnding||Revision!=RestoreInventoryRevision){Reason=TEXT("배치 미리보기가 오래됐습니다. 다시 확인하세요.");return false;}
    for(const auto& P:RestorePoses)if(!P.Key.IsValid()||!P.Key->GetActorTransform().Equals(P.Value,.001)||P.Key->GetConfigurationRevision()!=RestoreRevisions.FindRef(P.Key)){Reason=TEXT("장비가 변경됐습니다. 다시 미리보기를 확인하세요.");return false;}
    TArray<AVirtualSensorActorBase*> Matches,Excess;if(!ValidateDocument(RestoreDocument,Matches,Excess,Reason))return false;
    FVirtualSensorLayoutDocument Before;if(!BuildLayoutSnapshot(Before,Reason))return false;
    TMap<AVirtualSensorActorBase*,FVirtualSensorEditableState> OriginalStates;
    for(auto* S:Coordinator()->GetSensorActors())if(IsValid(S)){FVirtualSensorEditableState State;bool P=false;S->ReadCollectionState(State,P);OriginalStates.Add(S,State);}
    TArray<AVirtualSensorActorBase*> Created,Modified;
    auto Rollback=[&](){for(auto* S:Created){S->StopSensor();S->Destroy();}for(const auto& E:Before.Sensors)for(auto* S:Modified)if((E.bToolCreated&&Owned.Find(S)&&*Owned.Find(S)==E.LayoutGuid)||(!E.bToolCreated&&CanonicalActorPath(S)==E.ActorPath)){FString Ignored;S->StopSensor();S->ApplyEditableState(OriginalStates.FindChecked(S),Ignored);if(auto* L=Cast<AVirtualLidarSensorActor>(S))L->ScanComponent->AcquisitionBackend=E.LidarBackend;if(E.bPreviewEnabled)S->StartSensor();}};
    for(int32 I=0;I<Matches.Num();++I)if(!Matches[I]){const auto& E=RestoreDocument.Sensors[I];auto* S=SpawnStopped(E.State.TargetKind==EVirtualSensorTargetKind::Camera?EVirtualSensorKind::Camera:EVirtualSensorKind::Lidar,E.State.ActorTransform,Reason);if(!S){Rollback();return false;}Matches[I]=S;Created.Add(S);}
    for(int32 I=0;I<Matches.Num();++I)
    {
        auto* S=Matches[I];const auto& E=RestoreDocument.Sensors[I];if(!Created.Contains(S))Modified.Add(S);S->OnDataCollectionStarting.Broadcast();S->EndInteractiveManipulation();S->StopSensor();
        if(!S->ApplyEditableState(E.State,Reason)){Rollback();return false;}if(auto* L=Cast<AVirtualLidarSensorActor>(S))L->ScanComponent->AcquisitionBackend=E.LidarBackend;
    }
    for(int32 I=0;I<Matches.Num();++I){auto* S=Matches[I];const auto& E=RestoreDocument.Sensors[I];if(Created.Contains(S)){Owned.Add(S,E.LayoutGuid);Coordinator()->RegisterSensorActor(S);}if(E.bPreviewEnabled)S->StartSensor();GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->BeginPlacementPreviewRefresh(S);}
    for(auto* S:Excess){FString Ignored;DeleteSensor(S,Ignored);}
    if(!Matches.IsEmpty())Coordinator()->SelectSensorActor(Matches[0]);
    if(auto* Op=Operations.Find(Id)){Op->State=EVirtualSensorLayoutOperation::Applied;Op->Message=TEXT("전체 배치 복원 완료 · PCD 송신은 별도로 시작하세요.");}
    RestoreId.Reset();Changed();return true;
}
