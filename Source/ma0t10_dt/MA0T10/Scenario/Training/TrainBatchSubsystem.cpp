#include "TrainBatchSubsystem.h"
#include "TrainBatchStorage.h"
#include "ma0t10_dt/MA0T10/Contracts/ConsumeReadyFuture.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetPcdPreview.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/TrainBatchScenarioTC.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UObject/Class.h"

namespace
{
enum class EWork { None,Parse,Receive,Import,Prepare,Finish };
struct FWorkResult
{
    FString Error;
    TSharedPtr<FTrainBatchTransaction> Transaction;
    TSharedPtr<FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Plan;
    TArray<FTrainBatchJobView> Views;
};
struct FInput {FString Text;bool File=false;int64 Bytes=0;};
bool WorldBusy(UWorld* World,ASlabActor* Slab)
{
    if(!Slab||Slab->IsSimulationActive()||Slab->HasPendingScenarioInput())return true;
    const auto S=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus().State;
    return S==EVirtualSlabSessionState::Preparing||S==EVirtualSlabSessionState::Ready||S==EVirtualSlabSessionState::Running||S==EVirtualSlabSessionState::Paused||S==EVirtualSlabSessionState::Draining;
}
FString Unit(ESlabInputUnit U)
{return U==ESlabInputUnit::Millimeters?TEXT("mm"):U==ESlabInputUnit::Meters?TEXT("m"):TEXT("cm");}
}
struct FTrainBatchState
{
    bool Enabled=false,Subscribing=false,CancelRequested=false;
    FString Message=TEXT("학습 기능 꺼짐"),Subscription,Preset,ActiveJob;
    int32 ActiveIndex=INDEX_NONE;
    int64 Revision=0,QueuedBytes=0,WorkBytes=0,ParsedBytes=0;
    uint64 SubscriptionEpoch=0;
    TWeakObjectPtr<ASlabActor> Slab;
    TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
    TWeakObjectPtr<AVirtualCameraSensorActor> Camera;
    FVirtualSensorEditableState LidarPreset,CameraPreset;
    TWeakObjectPtr<UDxWebSocketSubsystem> WebSocket;
    TArray<FInput> Inputs;
    TOptional<FTrainBatchMessage> Parsed;
    TArray<FTrainBatchJobView> Views;
    TSharedRef<FTrainBatchStorage,ESPMode::ThreadSafe> Storage=MakeShared<FTrainBatchStorage,ESPMode::ThreadSafe>();
    TSharedRef<std::atomic<bool>,ESPMode::ThreadSafe> Cancel=MakeShared<std::atomic<bool>,ESPMode::ThreadSafe>(false);
    TSharedRef<std::atomic<bool>,ESPMode::ThreadSafe> RunCancel=MakeShared<std::atomic<bool>,ESPMode::ThreadSafe>(false);
    TFuture<FWorkResult> Work;
    EWork Kind=EWork::None;
    TSharedPtr<FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Prepared;
    TSharedPtr<FAgentTestSetPcdPreview> Preview;
    void Say(const FString& S){if(Message!=S){Message=S;++Revision;}}
    bool TargetsMatch(bool WithCamera) const
    {
        FVirtualSensorEditableState Current;
        if(!Lidar.IsValid()||!Lidar->ReadEditableState(Current)||!FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&Current,&LidarPreset,0))return false;
        return !WithCamera||(Camera.IsValid()&&Camera->ReadEditableState(Current)&&FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&Current,&CameraPreset,0));
    }
};
bool UTrainBatchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{const auto* W=Cast<UWorld>(Outer);return W&&(W->WorldType==EWorldType::Game||W->WorldType==EWorldType::PIE);}
bool UTrainBatchSubsystem::IsEnabled() const{return State&&State->Enabled;}
const TArray<FTrainBatchJobView>& UTrainBatchSubsystem::Jobs() const
{static const TArray<FTrainBatchJobView> Empty;return State?State->Views:Empty;}
int64 UTrainBatchSubsystem::Revision() const{return State?State->Revision:0;}
FString UTrainBatchSubsystem::GetStatus() const{return State?State->Message:TEXT("학습 기능 꺼짐");}
bool UTrainBatchSubsystem::SetEnabled(bool Enabled,FString& Error)
{
    check(IsInGameThread());
    if(Enabled)
    {
        if(IsEnabled())return true;if(!State)State=MakeShared<FTrainBatchState>();State->Enabled=true;State->Cancel->store(false);
        Handler=NewObject<UTrainBatchScenarioTC>(this);
        if(auto* GI=GetWorld()->GetGameInstance())
        {
            State->WebSocket=GI->GetSubsystem<UDxWebSocketSubsystem>();
            if(auto* WS=State->WebSocket.Get())WS->OnConnectionStateChanged.AddUniqueDynamic(this,&UTrainBatchSubsystem::OnConnectionState);
        }
        State->Say(TEXT("학습 입력 대기 · 생성은 시작 버튼으로 실행"));Subscribe();return true;
    }
    if(!State)return true;
    if(State->Work.IsValid()||!State->ActiveJob.IsEmpty()){Error=TEXT("진행 작업을 중단하고 정리가 끝난 뒤 기능을 끄세요");return false;}
    DisconnectOwnedSubscription();StopPreview();State->Enabled=false;State->Inputs.Reset();State->Parsed.Reset();State->QueuedBytes=0;
    GetWorld()->GetTimerManager().ClearTimer(Timer);Handler=nullptr;State->Say(TEXT("학습 기능 꺼짐"));return true;
}
void UTrainBatchSubsystem::Subscribe()
{
    if(!IsEnabled()||State->Subscribing||!State->Subscription.IsEmpty())return;
    auto* WS=State->WebSocket.Get();if(!WS||WS->GetConnectionState()!=EDxWebSocketConnectionState::Ready)return;
    Handler=NewObject<UTrainBatchScenarioTC>(this);Handler->SubscriptionEpoch=++State->SubscriptionEpoch;
    FSTOMPSubscriptionEvent Event;Event.BindDynamic(Handler.Get(),&UTrainBatchScenarioTC::OnSubscriptionMessage);
    FSTOMPRequestCompleted Done;Done.BindDynamic(Handler.Get(),&UTrainBatchScenarioTC::OnSubscriptionComplete);State->Subscribing=true;
    State->Subscription=WS->Subscribe(TrainBatchContract::Topic,Event,Done);if(State->Subscription.IsEmpty())State->Subscribing=false;
}
void UTrainBatchSubsystem::DisconnectOwnedSubscription()
{
    if(!State)return;++State->SubscriptionEpoch;if(auto* WS=State->WebSocket.Get())
    {
        WS->OnConnectionStateChanged.RemoveDynamic(this,&UTrainBatchSubsystem::OnConnectionState);
        if(!State->Subscription.IsEmpty())WS->Unsubscribe(State->Subscription,FSTOMPRequestCompleted());
    }
    State->Subscription.Reset();State->Subscribing=false;
}
void UTrainBatchSubsystem::OnConnectionState(EDxWebSocketConnectionState Connection,FString)
{if(!IsEnabled())return;if(Connection==EDxWebSocketConnectionState::Ready)Subscribe();else{++State->SubscriptionEpoch;State->Subscription.Reset();State->Subscribing=false;}}
bool UTrainBatchSubsystem::IsSubscriptionCurrent(uint64 Epoch) const
{return IsEnabled()&&State->SubscriptionEpoch==Epoch;}
void UTrainBatchSubsystem::OnSubscribed(bool Success,FString)
{if(!IsEnabled())return;State->Subscribing=false;if(!Success){State->Subscription.Reset();State->Say(TEXT("학습 Topic 구독 실패 · 다시 켜서 재시도"));}}
void UTrainBatchSubsystem::OnMessage(const FWebSocketMessage& Message)
{if(!IsEnabled()||Message.Destination!=TrainBatchContract::Topic)return;FString Error;if(!SubmitEnvelope(Message.BodyString,Error))State->Say(Error);}
bool UTrainBatchSubsystem::SubmitEnvelope(const FString& Json,FString& Error)
{
    check(IsInGameThread());const int64 Bytes=int64(Json.Len())*sizeof(TCHAR);
    if(!IsEnabled()){Error=TEXT("학습 기능을 먼저 켜세요");return false;}
    if(Json.IsEmpty()||Json.Len()>TrainBatchContract::EnvelopeBytes||Bytes>TrainBatchContract::QueueBytes-State->QueuedBytes||State->Inputs.Num()>=256)
    {Error=TEXT("학습 수신 대기열/메시지 상한 초과");return false;}
    State->Inputs.Add({Json,false,Bytes});State->QueuedBytes+=Bytes;EnsurePump();return true;
}
void UTrainBatchSubsystem::AcceptTransaction(const FTrainBatchTransaction& Transaction)
{
    check(IsInGameThread());if(!IsEnabled())return;
    if(!Transaction.Error.IsEmpty()){State->Say(Transaction.Error);return;}
    if(State->Parsed.IsSet()){State->Say(TEXT("학습 admission 작업 사용 중 · 재전송 필요"));return;}
    const auto& M=Transaction.Message;int64 Bytes=int64(M.Job.OriginalJson.Len())*sizeof(TCHAR);
    if(M.Scenario)Bytes+=int64(M.Scenario->OriginalJson.Len())*sizeof(TCHAR)+int64(M.Scenario->Rows.Num())*(sizeof(FSlabScenarioRow)+64);
    if(Bytes>TrainBatchContract::QueueBytes-State->QueuedBytes){State->Say(TEXT("파싱된 학습 입력의 64MiB 예산 초과 · 재전송 필요"));return;}
    State->Parsed=M;State->ParsedBytes=Bytes;State->QueuedBytes+=Bytes;EnsurePump();
}
bool UTrainBatchSubsystem::ImportManifest(const FString& Path,FString& Error)
{
    if(!IsEnabled()||FPaths::IsRelative(Path)||Path.Len()>2048||State->Inputs.Num()>=256){Error=TEXT("학습 기능 활성화와 로컬 절대 경로를 확인하세요");return false;}
    State->Inputs.Add({Path,true,0});EnsurePump();return true;
}
bool UTrainBatchSubsystem::ConfigureTargets(const FString& PresetId,ASlabActor* Slab,AVirtualLidarSensorActor* Lidar,AVirtualCameraSensorActor* Camera,FString& Error)
{
    if(!IsEnabled()||!State->ActiveJob.IsEmpty()||State->Work.IsValid()||!TrainBatchCodec::SafeId(PresetId)||!IsValid(Slab)||!IsValid(Lidar)||!Lidar->ScanComponent||Slab->GetWorld()!=GetWorld()||Lidar->GetWorld()!=GetWorld()||(Camera&&(!Camera->CaptureComponent||Camera->GetWorld()!=GetWorld())))
    {Error=TEXT("대기 상태에서 같은 World의 Slab/LiDAR와 preset을 명시하세요");return false;}
    if(!Lidar->ReadEditableState(State->LidarPreset)||(Camera&&!Camera->ReadEditableState(State->CameraPreset))){Error=TEXT("센서 preset을 읽을 수 없습니다");return false;}
    State->Preset=PresetId;State->Slab=Slab;State->Lidar=Lidar;State->Camera=Camera;State->Say(TEXT("학습 대상 적용됨"));return true;
}
bool UTrainBatchSubsystem::StartJob(const FString& JobId,FString& Error)
{
    if(!IsEnabled()||!State->ActiveJob.IsEmpty()||State->Work.IsValid()||State->Parsed.IsSet()||!State->Inputs.IsEmpty())
    {Error=TEXT("접수/실행 정리 뒤 시작하세요");return false;}
    const auto* J=State->Views.FindByPredicate([&](const auto& V){return V.Spec.Id==JobId;});auto* Slab=State->Slab.Get();
    if(!J||!J->IsReady()||!Slab||!State->Lidar.IsValid()||J->Spec.PresetId!=State->Preset||J->Spec.DimensionUnit!=Unit(Slab->GetDimensionUnit())||J->Spec.PositionUnit!=Unit(Slab->GetPositionUnit())||(J->Spec.CaptureCamera&&!State->Camera.IsValid()))
    {Error=TEXT("입력 완료·preset·단위·대상 센서를 확인하세요");return false;}
    if(!State->TargetsMatch(J->Spec.CaptureCamera)){Error=TEXT("적용한 센서 preset이 변경되었습니다 · 다시 명시 적용하세요");return false;}
    if(!J->Items.ContainsByPredicate([](const auto& I){return I.State==ETrainBatchItemState::Waiting;})){Error=TEXT("대기 시나리오가 없습니다");return false;}
    State->ActiveJob=JobId;State->CancelRequested=false;State->RunCancel->store(false);State->ActiveIndex=INDEX_NONE;State->Say(TEXT("순차 생성 대기 · 1A 현재 프로세스 기준선"));EnsurePump();return true;
}
void UTrainBatchSubsystem::CancelJob()
{
    if(!State||State->ActiveJob.IsEmpty())return;State->CancelRequested=true;State->RunCancel->store(true);
    if(State->ActiveIndex!=INDEX_NONE)
        if(auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>())
            if(const auto* J=State->Views.FindByPredicate([&](const auto& V){return V.Spec.Id==State->ActiveJob;}))
                if(J->Items.IsValidIndex(State->ActiveIndex)&&D->OwnsPlannedRun(J->Items[State->ActiveIndex].RunId))D->Stop();
    State->Say(TEXT("학습 생성 중단 · 파일 정리 대기"));EnsurePump();
}
void UTrainBatchSubsystem::EnsurePump()
{if(GetWorld()&&!GetWorld()->GetTimerManager().IsTimerActive(Timer))GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Pump,.05f,true);}
void UTrainBatchSubsystem::Pump()
{
    if(!IsEnabled())return;auto& S=*State;
    if(S.Work.IsValid())
    {
        if(!S.Work.IsReady())return;auto R=MA0T10::ConsumeReadyFuture(S.Work);const EWork Finished=S.Kind;S.Kind=EWork::None;
        if(Finished==EWork::Parse){S.QueuedBytes-=S.WorkBytes;S.WorkBytes=0;if(Handler&&R.Transaction)Handler->ProcessStructData(R.Transaction);}
        else {S.Views=MoveTemp(R.Views);++S.Revision;if(!R.Error.IsEmpty())S.Say(R.Error);}
        if((Finished==EWork::Receive||Finished==EWork::Import)&&R.Error.IsEmpty())S.Say(TEXT("입력 저장 완료 · Job 선택 후 시작"));
        if(Finished==EWork::Receive){S.QueuedBytes-=S.WorkBytes;S.WorkBytes=0;}
        if(Finished==EWork::Prepare)
        {S.Prepared=MoveTemp(R.Plan);if(!S.Prepared)S.ActiveIndex=INDEX_NONE;}
        if(Finished==EWork::Finish){S.ActiveIndex=INDEX_NONE;if(S.CancelRequested)S.ActiveJob.Reset();}
    }
    const auto Storage=S.Storage;const auto Cancel=S.Cancel;const auto RunCancel=S.RunCancel;
    if(S.Parsed.IsSet())
    {
        auto M=MoveTemp(S.Parsed.GetValue());S.Parsed.Reset();S.Kind=EWork::Receive;S.WorkBytes=S.ParsedBytes;S.ParsedBytes=0;
        S.Work=Async(EAsyncExecution::ThreadPool,[Storage,M=MoveTemp(M),Cancel]()
        {FWorkResult R;if(Cancel->load())R.Error=TEXT("입력 접수 취소");else if(!Storage->Receive(M,R.Error)&&R.Error.IsEmpty())R.Error=TEXT("학습 입력 저장 거절");R.Views=Storage->Jobs;return R;});return;
    }
    if(!S.Inputs.IsEmpty())
    {
        auto In=MoveTemp(S.Inputs[0]);S.Inputs.RemoveAt(0);S.WorkBytes=In.Bytes;
        if(In.File)
        {
            S.Kind=EWork::Import;S.Work=Async(EAsyncExecution::ThreadPool,[Storage,Path=MoveTemp(In.Text),Cancel]()
            {FWorkResult R;if(!Storage->Import(Path,*Cancel,R.Error)&&R.Error.IsEmpty())R.Error=TEXT("로컬 manifest 가져오기 실패");R.Views=Storage->Jobs;return R;});
        }
        else {S.Kind=EWork::Parse;S.Work=Async(EAsyncExecution::ThreadPool,[Text=MoveTemp(In.Text)](){FWorkResult R;R.Transaction=UTrainBatchScenarioTC::ParsePayload(Text);return R;});}
        return;
    }
    if(S.Preview)S.Preview->Pump(GetWorld());
    if(!S.ActiveJob.IsEmpty())
    {
        auto* J=S.Views.FindByPredicate([&](const auto& V){return V.Spec.Id==S.ActiveJob;});auto* D=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>();
        if(!J){S.ActiveJob.Reset();return;}
        if(!S.Slab.IsValid()||!S.Lidar.IsValid()||(J->Spec.CaptureCamera&&!S.Camera.IsValid()))
        {S.CancelRequested=true;S.RunCancel->store(true);if(S.ActiveIndex==INDEX_NONE){S.ActiveJob.Reset();S.Say(TEXT("학습 대상이 삭제되어 중단됨"));return;}if(D->OwnsPlannedRun(J->Items[S.ActiveIndex].RunId))D->Stop();}
        const bool Busy=WorldBusy(GetWorld(),S.Slab.Get());
        if(S.Prepared)
        {
            if(S.CancelRequested){const FString Id=S.ActiveJob;const int32 Index=S.ActiveIndex;S.Prepared.Reset();S.Kind=EWork::Finish;
                S.Work=Async(EAsyncExecution::ThreadPool,[Storage,Id,Index,Cancel=RunCancel](){FWorkResult R;Storage->Finish(Id,Index,false,TEXT("사용자 중단"),*Cancel,R.Error);R.Views=Storage->Jobs;return R;});return;}
            if(Busy||D->IsActive()){S.Say(TEXT("일반 시나리오/데이터셋 우선 · 학습 대기"));return;}
            if(!S.TargetsMatch(J->Spec.CaptureCamera)){S.CancelRequested=true;S.RunCancel->store(true);S.Say(TEXT("센서 preset 변경으로 학습 중단"));return;}
            FString Available;
            if(!S.Lidar.IsValid()||S.Lidar->IsSensorRunning()||!S.Lidar->CanEditSensorConfiguration(Available)||!S.Lidar->CanEditSensorPlacement(Available)||
               (J->Spec.CaptureCamera&&(!S.Camera.IsValid()||S.Camera->IsSensorRunning()||!S.Camera->CanEditSensorConfiguration(Available)||!S.Camera->CanEditSensorPlacement(Available))))
            {S.Say(TEXT("선택 센서의 측정/출력/저장을 중지한 뒤 학습을 진행하세요"));return;}
            FVirtualDatasetSettings C;C.Enabled=true;C.Name=TEXT("train_batch");C.Scenes=S.Prepared->Frames.Num();C.SettleFrames=1;C.CameraTruth=J->Spec.CameraTruth;
            FString Error;
            if(!S.Slab.IsValid()||S.Slab->GetMetricsConfigurationHash()!=S.Prepared->SlabConfiguration||!D->Start(C,S.Slab.Get(),S.Lidar.Get(),J->Spec.CaptureCamera?S.Camera.Get():nullptr,nullptr,Error,S.Prepared))
            {
                const FString Id=S.ActiveJob;const int32 Index=S.ActiveIndex;S.Prepared.Reset();S.Kind=EWork::Finish;
                S.Work=Async(EAsyncExecution::ThreadPool,[Storage,Id,Index,Error,Cancel=RunCancel](){FWorkResult R;Storage->Finish(Id,Index,false,Error,*Cancel,R.Error);R.Views=Storage->Jobs;return R;});return;
            }
            S.Prepared.Reset();S.Say(TEXT("학습 시간축 생성 중"));
        }
        if(S.ActiveIndex!=INDEX_NONE)
        {
            auto& I=J->Items[S.ActiveIndex];
            if(D->OwnsPlannedRun(I.RunId)&&D->IsActive())
            {
                if(Busy)D->YieldPlannedCapture();
                else if(D->IsPaused()&&!S.CancelRequested){FString E;if(!S.TargetsMatch(J->Spec.CaptureCamera)||!D->Resume(E)){D->Stop();S.Say(E.IsEmpty()?TEXT("센서 preset 변경으로 중단"):E);}}
                if(I.CompletedFrames!=D->CompletedScenes()){I.CompletedFrames=D->CompletedScenes();++S.Revision;}return;
            }
            const FString Id=S.ActiveJob,Reason=D->Message;const int32 Index=S.ActiveIndex;const bool Success=D->OwnsPlannedRun(I.RunId)&&D->HasSucceeded()&&!S.CancelRequested;
            S.Kind=EWork::Finish;S.Work=Async(EAsyncExecution::ThreadPool,[Storage,Id,Index,Success,Reason,Cancel=RunCancel]()
            {FWorkResult R;Storage->Finish(Id,Index,Success,Reason,*Cancel,R.Error);R.Views=Storage->Jobs;return R;});S.Say(TEXT("저장 결과 검증 중"));return;
        }
        if(S.CancelRequested){S.ActiveJob.Reset();S.Say(TEXT("학습 중단됨 · 미실행 항목 보존"));}
        else if(!Busy&&!D->IsActive())
        {
            if(!S.TargetsMatch(J->Spec.CaptureCamera)){S.ActiveJob.Reset();S.Say(TEXT("센서 preset이 변경되어 남은 항목 중단"));return;}
            const int32 Index=J->Items.IndexOfByPredicate([](const auto& I){return I.State==ETrainBatchItemState::Waiting;});
            if(Index==INDEX_NONE){S.ActiveJob.Reset();S.Say(J->CompleteCount()==J->Items.Num()?TEXT("Job 전체 완료"):TEXT("Job 종료 · 실패 항목 확인"));}
            else
            {
                S.ActiveIndex=Index;S.Kind=EWork::Prepare;const FString Id=S.ActiveJob;const auto Track=S.Slab->GetDatasetTrackTransform();const auto Dimension=S.Slab->GetDimensionUnit(),Position=S.Slab->GetPositionUnit();const uint32 Config=S.Slab->GetMetricsConfigurationHash();
                const auto* Scan=S.Lidar->ScanComponent;const int64 Points=int64(Scan->HorizontalSamples)*Scan->VerticalChannels*FMath::Max(1,Scan->MaxHitsPerRay);
                const int64 Pixels=J->Spec.CaptureCamera&&S.Camera.IsValid()?int64(S.Camera->CaptureComponent->CaptureResolution.X)*S.Camera->CaptureComponent->CaptureResolution.Y:0;
                const int64 FrameBytes=Points*38+Pixels*64+2ll*1024*1024;
                S.Work=Async(EAsyncExecution::ThreadPool,[Storage,Id,Index,Track,Dimension,Position,Config,FrameBytes,Cancel=RunCancel](){FWorkResult R;R.Plan=Storage->Prepare(Id,Index,Track,Dimension,Position,Config,FrameBytes,R.Error);if(!R.Plan){const FString Why=R.Error;Storage->Finish(Id,Index,false,Why,*Cancel,R.Error);}R.Views=Storage->Jobs;return R;});return;
            }
        }
        else S.Say(TEXT("일반 시나리오/데이터셋 우선 · 학습 대기"));
    }
    if(S.ActiveJob.IsEmpty()&&(!S.Preview||!S.Preview->NeedsPump()))GetWorld()->GetTimerManager().ClearTimer(Timer);
}
bool UTrainBatchSubsystem::SelectPreview(const FString& JobId,int32 Index,FString& Error)
{
    if(!IsEnabled()){Error=TEXT("학습 기능을 먼저 켜세요");return false;}StopPreview();const auto* J=State->Views.FindByPredicate([&](const auto& V){return V.Spec.Id==JobId;});
    if(!J||!J->Items.IsValidIndex(Index)||J->Items[Index].State!=ETrainBatchItemState::Completed){Error=TEXT("검증 완료한 저장 결과만 볼 수 있습니다");return false;}
    const auto& I=J->Items[Index];FAgentTestSetItem Source;Source.ScenarioId=I.Spec.Id;Source.RunId=I.RunId;Source.Directory=FPaths::GetPath(I.RunDirectory);Source.Frames=I.CompletedFrames;Source.LocalStorageRoot=TrainBatchCodec::JobsRoot();
    State->Preview=MakeShared<FAgentTestSetPcdPreview>();State->Preview->Start(GetWorld(),Source,Index);EnsurePump();return true;
}
void UTrainBatchSubsystem::StopPreview(){if(State&&State->Preview){State->Preview->Stop();State->Preview.Reset();}}
UTexture2D* UTrainBatchSubsystem::PreviewTexture() const{return State&&State->Preview?State->Preview->Texture():nullptr;}
FString UTrainBatchSubsystem::PreviewStatus() const{return State&&State->Preview?State->Preview->Status().Message:TEXT("완료 항목을 선택하면 저장 PCD를 봅니다");}
void UTrainBatchSubsystem::Deinitialize()
{
    DisconnectOwnedSubscription();CancelJob();StopPreview();if(State){State->Cancel->store(true);State->Enabled=false;}
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);Handler=nullptr;State.Reset();Super::Deinitialize();
}
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
bool UTrainBatchSubsystem::HasBackgroundWorkForTests() const
{return State&&(State->Work.IsValid()||!State->Inputs.IsEmpty()||State->Parsed.IsSet()||!State->Subscription.IsEmpty()||GetWorld()->GetTimerManager().IsTimerActive(Timer));}
#endif
