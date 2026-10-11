#include "VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Json.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "Engine/World.h"

namespace { FCriticalSection PoseLibraryWriter;constexpr int64 MaxLibraryBytes=1024*1024; }
FString UVirtualSensorPlacementSubsystem::CanonicalMap(UWorld* W)
{return W?UWorld::RemovePIEPrefix(W->GetOutermost()->GetName()):FString();}
bool UVirtualSensorPlacementSubsystem::SamePose(const FTransform& A,const FTransform& B)
{return A.GetLocation().Equals(B.GetLocation(),.1)&&A.GetRotation().AngularDistance(B.GetRotation())<=FMath::DegreesToRadians(.01);}
FString UVirtualSensorPlacementSubsystem::StoragePath() const
{
    FString Root=FPaths::ProjectSavedDir()/VirtualSensorDefaults::PlacementDirectory;
#if WITH_DEV_AUTOMATION_TESTS
    if(!StorageRootOverrideForTests.IsEmpty())Root=StorageRootOverrideForTests;
#endif
    return FPaths::ConvertRelativePathToFull(Root/(FMD5::HashAnsiString(*CanonicalMap(GetWorld()))+TEXT(".poses.v1.json")));
}
bool UVirtualSensorPlacementSubsystem::CheckSensor(const AVirtualSensorActorBase* S,FString& Reason) const
{
    if(bEnding||!IsValid(S)||S->IsActorBeingDestroyed()||S->GetWorld()!=GetWorld()||!S->GetRootComponent())
    {Reason=TEXT("같은 월드의 유효한 센서를 선택하세요.");return false;}
    if(S->GetSensorId().TrimStartAndEnd().IsEmpty()){Reason=TEXT("SensorId가 비어 있습니다.");return false;}
    int32 Matches=0;for(TActorIterator<AVirtualSensorActorBase> It(GetWorld());It;++It)if(!It->IsActorBeingDestroyed()&&It->GetSensorId()==S->GetSensorId())++Matches;
    if(Matches!=1){Reason=TEXT("중복 SensorId를 먼저 수정하세요. 저장 위치를 추측해 연결하지 않습니다.");return false;}
    if(S->GetActorTransform().ContainsNaN()){Reason=TEXT("현재 센서 자세가 유효하지 않습니다.");return false;}
    Reason.Reset();return true;
}
bool UVirtualSensorPlacementSubsystem::EnsureLoaded()
{
    if(bLoaded)return bLoadValid;bLoaded=true;LoadedMap=CanonicalMap(GetWorld());Bookmarks.Reset();
    const FString Path=StoragePath();if(!IFileManager::Get().FileExists(*Path)){LibraryMessage=TEXT("저장 위치가 없습니다.");return true;}
    FString Text;TSharedPtr<FJsonObject> Root;double Version=0;FString Schema,Map;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    auto Fail=[&](){bLoadValid=false;LibraryMessage=TEXT("저장 위치 파일의 버전·맵·내용이 유효하지 않습니다. 원본 파일을 보존했습니다.");return false;};
    if(IFileManager::Get().FileSize(*Path)>MaxLibraryBytes||!FFileHelper::LoadFileToString(Text,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)||!Root.IsValid()||
       !Root->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("ma0t10.sensor-poses.v1")||!Root->TryGetNumberField(TEXT("version"),Version)||Version!=1||
       !Root->TryGetStringField(TEXT("mapPackage"),Map)||Map!=LoadedMap||!Root->TryGetArrayField(TEXT("bookmarks"),Rows))return Fail();
    TArray<FVirtualSensorPoseBookmark> Loaded;TSet<FString> Ids,Names;TMap<FString,int32> Counts;
    for(const auto& Row:*Rows)
    {
        FVirtualSensorPoseBookmark B;FGuid Guid;const auto Object=Row.IsValid()?Row->AsObject():nullptr;
        if(!Object.IsValid()||!FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(),&B)||!FGuid::Parse(B.BookmarkId,Guid)||!Guid.IsValid()||Ids.Contains(B.BookmarkId)||
           B.MapPackage!=LoadedMap||B.SensorId.IsEmpty()||B.Name.TrimStartAndEnd().IsEmpty()||B.Name.Len()>64||B.Revision<1||B.Pose.ContainsNaN()||!B.Pose.GetRotation().IsNormalized()||
           (B.SensorKind!=EVirtualSensorKind::Camera&&B.SensorKind!=EVirtualSensorKind::Lidar))return Fail();
        const FString Key=LexToString(int32(B.SensorKind))+TEXT("|")+B.SensorId;const FString NameKey=Key+TEXT("|")+B.Name.ToLower();
        if(Names.Contains(NameKey)||++Counts.FindOrAdd(Key)>MaxBookmarksPerSensor)return Fail();
        B.Pose.SetScale3D(FVector::OneVector);Ids.Add(B.BookmarkId);Names.Add(NameKey);Loaded.Add(MoveTemp(B));
    }
    Bookmarks=MoveTemp(Loaded);LibraryMessage=TEXT("저장 위치 목록을 불러왔습니다. 맵 배치는 그대로 유지합니다.");++LibraryRevision;return true;
}
bool UVirtualSensorPlacementSubsystem::CheckName(const FString& Name,const AVirtualSensorActorBase* S,const FString& Except,FString& Reason) const
{
    if(Name.IsEmpty()||Name.Len()>64){Reason=TEXT("위치 이름은 1~64자여야 합니다.");return false;}
    for(TCHAR C:Name)if(C<32){Reason=TEXT("위치 이름에 제어 문자를 사용할 수 없습니다.");return false;}
    for(const auto& B:Bookmarks)if(B.SensorId==S->GetSensorId()&&B.SensorKind==S->GetSensorKind()&&B.BookmarkId!=Except&&B.Name.Equals(Name,ESearchCase::IgnoreCase))
    {Reason=TEXT("같은 이름이 있습니다. 항목의 덮어쓰기를 사용하거나 다른 이름을 입력하세요.");return false;}
    return true;
}
FString UVirtualSensorPlacementSubsystem::Reject(const FString& Reason)
{
    FVirtualSensorPoseSaveStatus S;S.RequestId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);S.Message=Reason;SaveStatuses.Add(S.RequestId,S);RequestOrder.Add(S.RequestId);
    while(RequestOrder.Num()>40){SaveStatuses.Remove(RequestOrder[0]);RequestOrder.RemoveAt(0);}return S.RequestId;
}
FVirtualSensorPoseSaveStatus UVirtualSensorPlacementSubsystem::GetPoseSaveStatus(const FString& Id) const
{if(const auto* S=SaveStatuses.Find(Id))return *S;FVirtualSensorPoseSaveStatus S;S.RequestId=Id;S.Message=TEXT("저장 요청을 찾을 수 없습니다.");return S;}
TArray<FVirtualSensorPoseBookmark> UVirtualSensorPlacementSubsystem::GetSensorPoseBookmarks(AVirtualSensorActorBase* S)
{
    TArray<FVirtualSensorPoseBookmark> Result;FString Error;if(!CheckSensor(S,Error)||!EnsureLoaded())return Result;
    for(const auto& B:Bookmarks)if(B.SensorId==S->GetSensorId()&&B.SensorKind==S->GetSensorKind())Result.Add(B);
    Result.Sort([](const auto& A,const auto& B){return A.ModifiedUtc>B.ModifiedUtc;});return Result;
}
FString UVirtualSensorPlacementSubsystem::RequestSaveSensorPose(AVirtualSensorActorBase* S,const FString& RawName,const FString& Existing)
{
    FString Error;if(!CheckSensor(S,Error))return Reject(Error);if(!EnsureLoaded())return Reject(LibraryMessage);
    const FString Name=RawName.TrimStartAndEnd();if(!CheckName(Name,S,Existing,Error))return Reject(Error);
    auto Candidate=Bookmarks;auto* B=Existing.IsEmpty()?nullptr:Candidate.FindByPredicate([&](const auto& Item){return Item.BookmarkId==Existing&&Item.SensorId==S->GetSensorId()&&Item.SensorKind==S->GetSensorKind();});
    if(!Existing.IsEmpty()&&!B)return Reject(TEXT("선택 센서의 저장 위치를 찾을 수 없습니다."));
    if(!B)
    {
        int32 Count=0;for(const auto& Item:Candidate)Count+=Item.SensorId==S->GetSensorId()&&Item.SensorKind==S->GetSensorKind();
        if(Count>=MaxBookmarksPerSensor)return Reject(TEXT("센서별 저장 위치 한도(20개)에 도달했습니다."));
        B=&Candidate.AddDefaulted_GetRef();B->BookmarkId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);B->MapPackage=LoadedMap;B->SensorId=S->GetSensorId();B->SensorKind=S->GetSensorKind();B->CreatedUtc=FDateTime::UtcNow();
    }
    else ++B->Revision;
    B->Name=Name;B->ModifiedUtc=FDateTime::UtcNow();B->Pose=S->GetActorTransform();B->Pose.SetScale3D(FVector::OneVector);const FString Id=B->BookmarkId;
    return StartWrite(MoveTemp(Candidate),Id);
}
FString UVirtualSensorPlacementSubsystem::RenameSensorPoseBookmark(AVirtualSensorActorBase* S,const FString& Id,const FString& RawName)
{
    FString Error;if(!CheckSensor(S,Error))return Reject(Error);if(!EnsureLoaded())return Reject(LibraryMessage);const FString Name=RawName.TrimStartAndEnd();if(!CheckName(Name,S,Id,Error))return Reject(Error);
    auto Candidate=Bookmarks;auto* B=Candidate.FindByPredicate([&](const auto& Item){return Item.BookmarkId==Id&&Item.SensorId==S->GetSensorId()&&Item.SensorKind==S->GetSensorKind();});if(!B)return Reject(TEXT("저장 위치를 찾을 수 없습니다."));
    B->Name=Name;++B->Revision;B->ModifiedUtc=FDateTime::UtcNow();return StartWrite(MoveTemp(Candidate),Id);
}
FString UVirtualSensorPlacementSubsystem::DeleteSensorPoseBookmark(AVirtualSensorActorBase* S,const FString& Id)
{
    FString Error;if(!CheckSensor(S,Error))return Reject(Error);if(!EnsureLoaded())return Reject(LibraryMessage);auto Candidate=Bookmarks;
    if(Candidate.RemoveAll([&](const auto& B){return B.BookmarkId==Id&&B.SensorId==S->GetSensorId()&&B.SensorKind==S->GetSensorKind();})!=1)return Reject(TEXT("저장 위치를 찾을 수 없습니다."));
    return StartWrite(MoveTemp(Candidate),Id);
}
FString UVirtualSensorPlacementSubsystem::StartWrite(TArray<FVirtualSensorPoseBookmark> Candidate,const FString& Bookmark)
{
    if(bEnding||PendingWrite)return Reject(TEXT("저장 위치 파일을 쓰는 중입니다. 완료 후 다시 요청하세요."));
    auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("schema"),TEXT("ma0t10.sensor-poses.v1"));Root->SetNumberField(TEXT("version"),1);Root->SetStringField(TEXT("mapPackage"),LoadedMap);
    TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& B:Candidate)Rows.Add(MakeShared<FJsonValueObject>(FJsonObjectConverter::UStructToJsonObject(B)));Root->SetArrayField(TEXT("bookmarks"),Rows);
    FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));FTCHARToUTF8 Bytes(*Json);if(Bytes.Length()>MaxLibraryBytes)return Reject(TEXT("저장 위치 파일의 1MiB 한도를 초과했습니다."));
    FVirtualSensorPoseSaveStatus Status;Status.RequestId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);Status.BookmarkId=Bookmark;Status.State=EVirtualSensorPoseSaveState::Writing;Status.Message=TEXT("저장 위치 파일 쓰는 중");Status.Path=StoragePath();
    SaveStatuses.Add(Status.RequestId,Status);RequestOrder.Add(Status.RequestId);while(RequestOrder.Num()>40){SaveStatuses.Remove(RequestOrder[0]);RequestOrder.RemoveAt(0);}
    PendingWrite=MakeUnique<FPendingWrite>();PendingWrite->Request=Status.RequestId;PendingWrite->Candidate=MoveTemp(Candidate);
    const FString Path=Status.Path;PendingWrite->Future=Async(EAsyncExecution::ThreadPool,[Path,Json](){return WriteLibrary(Path,Json);});return Status.RequestId;
}
UVirtualSensorPlacementSubsystem::FWriteResult UVirtualSensorPlacementSubsystem::WriteLibrary(const FString& Path,const FString& Json)
{
    FScopeLock Lock(&PoseLibraryWriter);FWriteResult R;const FString Directory=FPaths::GetPath(Path),Temp=Path+TEXT(".writing"),Backup=Path+TEXT(".previous");
    if(!IFileManager::Get().MakeDirectory(*Directory,true)||!FFileHelper::SaveStringToFile(Json,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {R.Error=TEXT("저장 위치 파일 쓰기에 실패했습니다.");return R;}
    const bool HadOriginal=IFileManager::Get().FileExists(*Path);
    if(HadOriginal&&IFileManager::Get().Copy(*Backup,*Path,true,true)!=COPY_OK){IFileManager::Get().Delete(*Temp);R.Error=TEXT("기존 저장 위치 파일을 보호할 수 없어 저장을 거절했습니다.");return R;}
    R.Success=IFileManager::Get().Move(*Path,*Temp,true,true);
    if(!R.Success)
    {if(HadOriginal)IFileManager::Get().Copy(*Path,*Backup,true,true);IFileManager::Get().Delete(*Temp);R.Error=TEXT("저장 위치 확정에 실패했습니다. 이전 파일과 백업을 보존했습니다.");}
    return R;
}
void UVirtualSensorPlacementSubsystem::FinishWrite(FWriteResult Result,bool Broadcast)
{
    if(!PendingWrite)return;auto* Status=SaveStatuses.Find(PendingWrite->Request);
    if(Result.Success){Bookmarks=MoveTemp(PendingWrite->Candidate);++LibraryRevision;LibraryMessage=TEXT("저장 위치 파일 저장 완료");}
    else LibraryMessage=Result.Error;
    if(Status){Status->State=Result.Success?EVirtualSensorPoseSaveState::Saved:EVirtualSensorPoseSaveState::Failed;Status->Message=LibraryMessage;}
    PendingWrite.Reset();if(Broadcast&&!bEnding)OnPoseLibraryChanged.Broadcast();
}
void UVirtualSensorPlacementSubsystem::Tick(float)
{
    if(PendingWrite&&PendingWrite->Future.IsReady())FinishWrite(PendingWrite->Future.Get(),true);
    for(auto It=Histories.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
    for(auto It=AppliedBookmarks.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
    for(auto It=PreviewRequests.CreateIterator();It;++It)
    {
        auto* S=It.Key().Get();if(!IsValid(S)){It.RemoveCurrent();continue;}
        auto& R=It.Value();if(R.Status.State!=EVirtualSensorPlacementPreviewState::Waiting)continue;
        R.Status.ElapsedSeconds=FMath::Max(0.,double(GetWorld()->GetTimeSeconds())-R.StartedWorld);
        if(S->GetSensorId()!=R.SensorId||S->GetConfigurationRevision()!=R.ConfigurationRevision||!SamePose(S->GetActorTransform(),R.ActorPose))
        {R.Status.State=EVirtualSensorPlacementPreviewState::Cancelled;R.Status.Message=TEXT("미리보기 확인 취소 · 센서 또는 설정·자세 변경");continue;}
        if(!S->IsSensorRunning()){R.Status.State=EVirtualSensorPlacementPreviewState::Stopped;R.Status.Message=TEXT("센서 정지 · 새 측정 없이 위치 유지");continue;}
        if(auto* L=Cast<AVirtualLidarSensorActor>(S))
        {
            if(L->ScanComponent->GetFileCaptureRevision()!=R.Generation){R.Status.State=EVirtualSensorPlacementPreviewState::Cancelled;R.Status.Message=TEXT("미리보기 확인 취소 · 측정 재시작");continue;}
            if(const auto F=L->ScanComponent->GetLastFrameSnapshot())ObservePreviewFrame(S,F->FrameId,F->AcquisitionTransform,F->AcquisitionStartedMonotonic,F->PipelineGeneration,F->HorizontalSamples,F->VerticalChannels);
        }
        if(auto* C=Cast<AVirtualCameraSensorActor>(S))
        {
            if(C->CaptureComponent->GetFileCaptureRevision()!=R.Generation){R.Status.State=EVirtualSensorPlacementPreviewState::Cancelled;R.Status.Message=TEXT("미리보기 확인 취소 · 측정 재시작");continue;}
            FVirtualCameraPreviewCompletion F;if(C->CaptureComponent->GetPlacementPreviewCompletion(F))ObservePreviewFrame(S,F.Acquisition.FrameId,F.AcquisitionPose,F.StartedMonotonic,F.Generation,F.Acquisition.Width,F.Acquisition.Height);
        }
        if(R.Status.State==EVirtualSensorPlacementPreviewState::Waiting&&R.Status.ElapsedSeconds>R.Timeout)
        {R.Status.State=EVirtualSensorPlacementPreviewState::Delayed;R.Status.Message=TEXT("새 위치 미리보기 지연 · 측정 상태를 확인하세요");if(auto* C=Cast<AVirtualCameraSensorActor>(S))C->CaptureComponent->CancelPlacementPreviewProof();}
    }
}
TStatId UVirtualSensorPlacementSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorPlacementSubsystem,STATGROUP_Tickables);}
void UVirtualSensorPlacementSubsystem::Deinitialize()
{
    bEnding=true;if(PendingWrite)FinishWrite(PendingWrite->Future.Get(),false);Histories.Reset();AppliedBookmarks.Reset();
    for(const auto& P:PreviewRequests)if(auto* C=Cast<AVirtualCameraSensorActor>(P.Key.Get()))C->CaptureComponent->CancelPlacementPreviewProof();
    PreviewRequests.Reset();Super::Deinitialize();
}
bool UVirtualSensorPlacementSubsystem::CanEditSensorPlacement(AVirtualSensorActorBase* S,FString& Reason) const
{return CheckSensor(S,Reason)&&S->CanEditSensorPlacement(Reason);}
bool UVirtualSensorPlacementSubsystem::ApplyPose(AVirtualSensorActorBase* S,const FTransform& Pose,FString& Reason)
{
    if(!CanEditSensorPlacement(S,Reason)||Pose.ContainsNaN()||!Pose.GetRotation().IsNormalized())
    {if(Reason.IsEmpty())Reason=TEXT("저장 자세가 유효하지 않습니다.");return false;}
    auto Applied=Pose;Applied.SetScale3D(S->GetActorScale3D());
    if(!S->ApplyEditableTransform(Applied,Reason))return false;
    BeginPlacementPreviewRefresh(S);
    return true;
}
bool UVirtualSensorPlacementSubsystem::IsPreviewRefreshPending(AVirtualSensorActorBase* S) const
{return GetPlacementPreviewStatus(S).State==EVirtualSensorPlacementPreviewState::Waiting;}
FVirtualSensorPlacementPreviewStatus UVirtualSensorPlacementSubsystem::GetPlacementPreviewStatus(AVirtualSensorActorBase* S) const
{if(IsValid(S))if(const auto* R=PreviewRequests.Find(S))return R->Status;return FVirtualSensorPlacementPreviewStatus();}
void UVirtualSensorPlacementSubsystem::CancelPlacementPreviewRefresh(AVirtualSensorActorBase* S)
{if(auto* C=Cast<AVirtualCameraSensorActor>(S))C->CaptureComponent->CancelPlacementPreviewProof();PreviewRequests.Remove(S);}
void UVirtualSensorPlacementSubsystem::BeginPlacementPreviewRefresh(AVirtualSensorActorBase* S)
{
    FString Reason;if(!CheckSensor(S,Reason))return;CancelPlacementPreviewRefresh(S);
    auto& R=PreviewRequests.Add(S);R.Status.RequestId=++NextPreviewRequest;R.SensorId=S->GetSensorId();R.ActorPose=S->GetActorTransform();R.ConfigurationRevision=S->GetConfigurationRevision();
    R.StartedMonotonic=FPlatformTime::Seconds();R.StartedWorld=GetWorld()->GetTimeSeconds();R.BaselineFrame=S->GetSensorRuntimeStatus().FrameId;
    if(!S->IsSensorRunning()){R.Status.State=EVirtualSensorPlacementPreviewState::Stopped;R.Status.Message=TEXT("센서 정지 · 새 측정 없이 위치 유지");return;}
    R.Status.State=EVirtualSensorPlacementPreviewState::Waiting;R.Status.Message=TEXT("새 위치 미리보기 갱신 중");
    if(auto* C=Cast<AVirtualCameraSensorActor>(S))
    {auto* Comp=C->CaptureComponent.Get();R.AcquisitionPose=Comp->GetComponentTransform();R.Width=Comp->CaptureResolution.X;R.Height=Comp->CaptureResolution.Y;R.Generation=Comp->GetFileCaptureRevision();R.Timeout=FMath::Max(1.5,double(Comp->CaptureInterval)*3);Comp->RequestPlacementPreviewProof();Comp->RequestImmediateScheduledCapture();}
    if(auto* L=Cast<AVirtualLidarSensorActor>(S))
    {auto* Comp=L->ScanComponent.Get();R.AcquisitionPose=Comp->GetComponentTransform();R.Width=Comp->HorizontalSamples;R.Height=Comp->VerticalChannels;R.Generation=Comp->GetFileCaptureRevision();R.Timeout=FMath::Max(1.5,double(Comp->ScanInterval)*3);Comp->RequestImmediateScheduledScan();}
}
void UVirtualSensorPlacementSubsystem::ObservePreviewFrame(AVirtualSensorActorBase* S,int64 Frame,const FTransform& Pose,double Started,int32 Generation,int32 Width,int32 Height)
{
    auto* R=PreviewRequests.Find(S);if(!R||R->Status.State!=EVirtualSensorPlacementPreviewState::Waiting||Frame<=R->BaselineFrame||Started<R->StartedMonotonic||Generation!=R->Generation||Width!=R->Width||Height!=R->Height||!SamePose(Pose,R->AcquisitionPose))return;
    R->Status.State=EVirtualSensorPlacementPreviewState::Ready;R->Status.CompletedFrameId=Frame;
    R->Status.ElapsedSeconds=FMath::Max(0.,double(GetWorld()->GetTimeSeconds())-R->StartedWorld);R->Status.Message=TEXT("새 위치 미리보기 준비 완료");
}
bool UVirtualSensorPlacementSubsystem::ApplySensorPoseBookmark(AVirtualSensorActorBase* S,const FString& Id,FString& Reason)
{
    if(!CheckSensor(S,Reason)||!EnsureLoaded()){if(Reason.IsEmpty())Reason=LibraryMessage;return false;}
    const auto* B=Bookmarks.FindByPredicate([&](const auto& Item){return Item.BookmarkId==Id&&Item.SensorId==S->GetSensorId()&&Item.SensorKind==S->GetSensorKind()&&Item.MapPackage==CanonicalMap(GetWorld());});
    if(!B){Reason=TEXT("현재 맵·선택 센서와 일치하는 저장 위치가 없습니다.");return false;}
    const FTransform Before=S->GetActorTransform();const FString Previous=AppliedBookmarks.FindRef(S);
    if(!ApplyPose(S,B->Pose,Reason))return false;
    AppliedBookmarks.Add(S,Id);RecordCommand(S,Before,S->GetActorTransform(),Previous,Id);return true;
}
UVirtualSensorPlacementSubsystem::FHistory& UVirtualSensorPlacementSubsystem::HistoryFor(AVirtualSensorActorBase* S)
{auto& H=Histories.FindOrAdd(S);if(H.SensorId!=S->GetSensorId()){H=FHistory();H.SensorId=S->GetSensorId();}return H;}
void UVirtualSensorPlacementSubsystem::BeginGesture(AVirtualSensorActorBase* S)
{if(IsValid(S)){auto& H=HistoryFor(S);if(!H.GestureBefore.IsSet()){H.GestureBefore=S->GetActorTransform();H.GestureBookmark=AppliedBookmarks.FindRef(S);}}}
void UVirtualSensorPlacementSubsystem::CommitGesture(AVirtualSensorActorBase* S)
{if(IsValid(S)){auto& H=HistoryFor(S);if(H.GestureBefore.IsSet()){const auto Before=H.GestureBefore.GetValue();const auto Bookmark=H.GestureBookmark;H.GestureBefore.Reset();RecordCommand(S,Before,S->GetActorTransform(),Bookmark,FString());}}}
void UVirtualSensorPlacementSubsystem::RecordCommand(AVirtualSensorActorBase* S,const FTransform& Before,const FTransform& After,const FString& OldBookmark,const FString& NewBookmark)
{
    if(bApplyingHistory||SamePose(Before,After))return;auto& H=HistoryFor(S);if(H.Index<H.Commands.Num())H.Commands.SetNum(H.Index);
    H.Commands.Add({Before,After,OldBookmark,NewBookmark});if(H.Commands.Num()>MaxHistoryPerSensor)H.Commands.RemoveAt(0);H.Index=H.Commands.Num();
    if(NewBookmark.IsEmpty())AppliedBookmarks.Remove(S);
}
void UVirtualSensorPlacementSubsystem::RecordPoseChange(AVirtualSensorActorBase* S,const FTransform& Before,const FTransform& After)
{if(IsValid(S)&&!HistoryFor(S).GestureBefore.IsSet())RecordCommand(S,Before,After,AppliedBookmarks.FindRef(S),FString());}
void UVirtualSensorPlacementSubsystem::ForgetSensorIdentity(AVirtualSensorActorBase* S)
{Histories.Remove(S);AppliedBookmarks.Remove(S);CancelPlacementPreviewRefresh(S);}
bool UVirtualSensorPlacementSubsystem::CanUndoSensorPlacement(AVirtualSensorActorBase* S) const
{FString R;const auto* H=Histories.Find(S);return IsValid(S)&&H&&H->SensorId==S->GetSensorId()&&H->Index>0&&CanEditSensorPlacement(S,R);}
bool UVirtualSensorPlacementSubsystem::CanRedoSensorPlacement(AVirtualSensorActorBase* S) const
{FString R;const auto* H=Histories.Find(S);return IsValid(S)&&H&&H->SensorId==S->GetSensorId()&&H->Index<H->Commands.Num()&&CanEditSensorPlacement(S,R);}
bool UVirtualSensorPlacementSubsystem::UndoSensorPlacement(AVirtualSensorActorBase* S,FString& Reason)
{
    if(!CanEditSensorPlacement(S,Reason))return false;auto& H=HistoryFor(S);if(H.Index<=0){Reason=TEXT("되돌릴 위치가 없습니다.");return false;}
    const auto C=H.Commands[H.Index-1];if(!SamePose(S->GetActorTransform(),C.After)){Reason=TEXT("외부에서 위치가 변경되어 이력을 적용할 수 없습니다.");return false;}
    TGuardValue<bool> Guard(bApplyingHistory,true);if(!ApplyPose(S,C.Before,Reason))return false;--H.Index;AppliedBookmarks.Add(S,C.BeforeBookmark);return true;
}
bool UVirtualSensorPlacementSubsystem::RedoSensorPlacement(AVirtualSensorActorBase* S,FString& Reason)
{
    if(!CanEditSensorPlacement(S,Reason))return false;auto& H=HistoryFor(S);if(H.Index>=H.Commands.Num()){Reason=TEXT("다시 적용할 위치가 없습니다.");return false;}
    const auto C=H.Commands[H.Index];if(!SamePose(S->GetActorTransform(),C.Before)){Reason=TEXT("외부에서 위치가 변경되어 이력을 적용할 수 없습니다.");return false;}
    TGuardValue<bool> Guard(bApplyingHistory,true);if(!ApplyPose(S,C.After,Reason))return false;++H.Index;AppliedBookmarks.Add(S,C.AfterBookmark);return true;
}
FVirtualSensorPlacementSnapshot UVirtualSensorPlacementSubsystem::GetCurrentPlacementSnapshot(AVirtualSensorActorBase* S)
{
    FVirtualSensorPlacementSnapshot Snapshot;FString Error;if(!CheckSensor(S,Error))return Snapshot;
    Snapshot.MapPackage=CanonicalMap(GetWorld());Snapshot.SensorId=S->GetSensorId();Snapshot.SensorKind=S->GetSensorKind();Snapshot.WorldTransform=S->GetActorTransform();Snapshot.CapturedUtc=FDateTime::UtcNow();
    EnsureLoaded();const FString Id=AppliedBookmarks.FindRef(S);const auto* B=Bookmarks.FindByPredicate([&](const auto& Item){return Item.BookmarkId==Id&&Item.SensorId==Snapshot.SensorId&&Item.SensorKind==Snapshot.SensorKind;});
    if(B&&SamePose(Snapshot.WorldTransform,B->Pose)){Snapshot.BookmarkId=B->BookmarkId;Snapshot.BookmarkName=B->Name;Snapshot.BookmarkRevision=B->Revision;Snapshot.bDirectPlacement=false;}return Snapshot;
}
