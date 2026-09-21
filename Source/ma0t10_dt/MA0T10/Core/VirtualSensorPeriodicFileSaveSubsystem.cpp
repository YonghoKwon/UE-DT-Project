#include "VirtualSensorPeriodicFileSaveSubsystem.h"
#include "VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

struct FVirtualSensorPeriodicFileSaveSession
{
	FVirtualSensorPeriodicFileSaveStatus Status;
	TWeakObjectPtr<AVirtualSensorActorBase> Sensor;
	uint64 Revision=0;
	double Origin=0;
	int64 NextDeadlineIndex=0;
	int64 LastRequestedFrameId=-1;
	bool bInvalidationReported=false;
	double NextRevisionCheck=0;
};

void UVirtualSensorPeriodicFileSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UVirtualSensorFileSaveSubsystem>();
	Super::Initialize(Collection);
	if(auto* Files=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>():nullptr)Files->OnRequestUpdated.AddUniqueDynamic(this,&ThisClass::HandleFileSaveUpdated);
}
int64 UVirtualSensorPeriodicFileSaveSubsystem::AdvanceDeadline(double Origin,double Interval,double Now,int64& Next)
{
	if(!FMath::IsFinite(Origin)||!FMath::IsFinite(Interval)||!FMath::IsFinite(Now)||Interval<=0||Now<Origin)return 0;
	Next=FMath::Max<int64>(0,Next);
	if(Now+1.e-9<Origin+static_cast<double>(Next)*Interval)return 0;
	const double Current=(Now-Origin)/Interval;
	if(Current>static_cast<double>(MAX_int64/2))return 0;
	const int64 LastDue=FMath::Max(Next,static_cast<int64>(FMath::FloorToDouble(Current+1.e-9)));
	const int64 Count=LastDue-Next+1;
	Next=LastDue+1;
	return Count;
}
bool UVirtualSensorPeriodicFileSaveSubsystem::IsNewFrame(int64 Frame,int64 Previous){return Frame>=0&&Frame>Previous;}
bool UVirtualSensorPeriodicFileSaveSubsystem::ValidateSelection(EVirtualSensorKind Kind,const FVirtualSensorCaptureSelection& S,FString& Error)
{
	if(!FMath::IsFinite(S.IntervalSeconds)||S.IntervalSeconds<.05f||S.IntervalSeconds>3600.f)
	{Error=TEXT("저장 주기는 0.05~3600초의 유한한 값이어야 합니다.");return false;}
	if(Kind==EVirtualSensorKind::Camera&&!(S.bCameraImage||S.bCameraPayload))
	{Error=TEXT("Camera JPEG 또는 Camera Payload 출력을 선택하세요.");return false;}
	if(Kind==EVirtualSensorKind::Lidar&&!(S.bPointCloud||S.bLidarPayload))
	{Error=TEXT("Point Cloud 또는 LiDAR Payload 출력을 선택하세요.");return false;}
	if(Kind==EVirtualSensorKind::Lidar&&S.bPointCloud&&(S.PointCloudFormat<EVirtualSensorExportKind::PointCloudCsv||S.PointCloudFormat>EVirtualSensorExportKind::PointCloudLaz))
	{Error=TEXT("지원하는 Point Cloud 저장 형식을 선택하세요.");return false;}
	Error.Empty();return true;
}
FString UVirtualSensorPeriodicFileSaveSubsystem::StartPeriodicSave(AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection)
{
	check(IsInGameThread());
	auto Session=MakeShared<FVirtualSensorPeriodicFileSaveSession>();
	auto& S=Session->Status;S.SessionId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);S.Selection=Selection;S.IntervalSeconds=Selection.IntervalSeconds;
	S.SensorId=IsValid(Sensor)?Sensor->GetSensorId():FString();
	Sessions.Add(S.SessionId,Session);Order.Add(S.SessionId);
	auto Reject=[this,&Session](const FString& Reason){Session->Status.LastMessage=Reason;Session->Status.FailedCount=1;Publish(Session);TrimHistory();return Session->Status.SessionId;};
	if(bShuttingDown||!GetWorld()||!IsValid(Sensor)||Sensor->IsActorBeingDestroyed()||Sensor->GetWorld()!=GetWorld())return Reject(TEXT("같은 월드의 유효한 센서를 선택하세요."));
	if(S.SensorId.IsEmpty())return Reject(TEXT("SensorId가 비어 있습니다."));
	FString Error;if(!ValidateSelection(Sensor->GetSensorKind(),Selection,Error))return Reject(Error);
	int32 Active=0;
	for(const auto& P:Sessions)
	{
		if(P.Value==Session||(!P.Value->Status.bActive&&!P.Value->Status.bPending))continue;
		++Active;if(P.Value->Sensor.Get()==Sensor)return Reject(TEXT("이 센서의 주기 저장 또는 마지막 파일 저장이 이미 진행 중입니다."));
	}
	if(Active>=MaxConcurrentSessions)return Reject(TEXT("동시 주기 저장 세션 한도(16개)에 도달했습니다."));
	Session->Sensor=Sensor;Session->Revision=UVirtualSensorFileSaveSubsystem::GetSensorSaveRevision(Sensor);Session->Origin=FPlatformTime::Seconds();
	const FDateTime Now=FDateTime::UtcNow();
	const FString Folder=FString::Printf(TEXT("%s_%03d_%s"),*Now.ToString(TEXT("%Y%m%dT%H%M%SZ")),Now.GetMillisecond(),*S.SessionId.Left(8));
	S.Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("SensorCaptures/LocalTimedCapture")/Folder);
	S.bActive=true;S.LastMessage=TEXT("주기 저장 시작 · 완료된 최신 프레임만 저장합니다. 센서 측정을 추가하지 않습니다.");
	Publish(Session);TrimHistory();return S.SessionId;
}
bool UVirtualSensorPeriodicFileSaveSubsystem::StopPeriodicSave(const FString& Id)
{
	check(IsInGameThread());const auto* Found=Sessions.Find(Id);if(!Found)return false;
	auto& S=(*Found)->Status;
	S.bActive=false;S.bWaitingForLatestFrame=false;S.WaitReason=EVirtualSensorPeriodicSaveWaitReason::None;
	S.LastMessage=S.bPending?TEXT("주기 저장 중지 · 이미 접수한 파일을 마무리합니다."):TEXT("주기 저장 중지 완료");
	Publish(*Found);return true;
}
FVirtualSensorPeriodicFileSaveStatus UVirtualSensorPeriodicFileSaveSubsystem::GetPeriodicStatus(const FString& Id) const
{
	if(const auto* S=Sessions.Find(Id))return (*S)->Status;
	FVirtualSensorPeriodicFileSaveStatus Result;Result.SessionId=Id;Result.LastMessage=TEXT("주기 저장 세션을 찾을 수 없습니다.");return Result;
}
TArray<FVirtualSensorPeriodicFileSaveStatus> UVirtualSensorPeriodicFileSaveSubsystem::GetRecentPeriodicSessions() const
{
	TArray<FVirtualSensorPeriodicFileSaveStatus> Result;Result.Reserve(Order.Num());
	for(int32 I=Order.Num()-1;I>=0;--I)if(const auto* S=Sessions.Find(Order[I]))Result.Add((*S)->Status);return Result;
}
void UVirtualSensorPeriodicFileSaveSubsystem::Publish(const TSharedPtr<FVirtualSensorPeriodicFileSaveSession>& S){if(!bShuttingDown)OnPeriodicUpdated.Broadcast(S->Status);}
void UVirtualSensorPeriodicFileSaveSubsystem::CompleteRequest(const TSharedPtr<FVirtualSensorPeriodicFileSaveSession>& Session,const FVirtualSensorFileSaveStatus& R)
{
	auto& S=Session->Status;if(!S.bPending||!R.IsTerminal()||R.RequestId!=S.LastRequestId)return;
	S.bPending=false;
	if(R.State==EVirtualSensorFileSaveState::Succeeded)
	{++S.CompletedCount;S.LastFrameId=R.FrameId;S.LastMessage=S.bActive?TEXT("최신 완료 프레임 파일 저장 완료"):TEXT("주기 저장 중지 완료 · 마지막 파일 저장을 마쳤습니다.");}
	else{if(!Session->bInvalidationReported)++S.FailedCount;S.bActive=false;S.bWaitingForLatestFrame=false;S.WaitReason=EVirtualSensorPeriodicSaveWaitReason::None;S.LastMessage=TEXT("주기 저장 종료: ")+R.Message;}
	Publish(Session);
}
void UVirtualSensorPeriodicFileSaveSubsystem::HandleFileSaveUpdated(const FVirtualSensorFileSaveStatus& R)
{
	if(bShuttingDown||!R.IsTerminal())return;
	TSharedPtr<FVirtualSensorPeriodicFileSaveSession> Target;
	for(const auto& P:Sessions)if(P.Value->Status.bPending&&P.Value->Status.LastRequestId==R.RequestId){Target=P.Value;break;}
	// Capture the completion before the file service's bounded history can evict it.
	if(Target)CompleteRequest(Target,R);
}
void UVirtualSensorPeriodicFileSaveSubsystem::TrimHistory()
{
	for(int32 I=0;Order.Num()>MaxRetainedSessions&&I<Order.Num();)
	{
		const auto* S=Sessions.Find(Order[I]);
		if(!S||(!(*S)->Status.bActive&&!(*S)->Status.bPending)){Sessions.Remove(Order[I]);Order.RemoveAt(I);}else ++I;
	}
}
void UVirtualSensorPeriodicFileSaveSubsystem::ProcessSession(const TSharedPtr<FVirtualSensorPeriodicFileSaveSession>& Session,UVirtualSensorFileSaveSubsystem* Files,double Now)
{
	auto& S=Session->Status;
	if(S.bPending)
	{
		const auto R=Files->GetRequestStatus(S.LastRequestId);
		CompleteRequest(Session,R);
	}
	if(!S.bActive)return;
	auto* Sensor=Session->Sensor.Get();
	const int64 Due=AdvanceDeadline(Session->Origin,S.IntervalSeconds,Now,Session->NextDeadlineIndex);
	const bool CheckRevision=Due>0||Now>=Session->NextRevisionCheck;
	if(CheckRevision)Session->NextRevisionCheck=Now+.2;
	if(!Sensor||Sensor->IsActorBeingDestroyed()||Sensor->GetWorld()!=GetWorld()||Sensor->GetSensorId()!=S.SensorId||(CheckRevision&&UVirtualSensorFileSaveSubsystem::GetSensorSaveRevision(Sensor)!=Session->Revision))
	{
		S.bActive=false;S.bWaitingForLatestFrame=false;S.WaitReason=EVirtualSensorPeriodicSaveWaitReason::None;++S.FailedCount;Session->bInvalidationReported=true;
		S.LastMessage=TEXT("대상 센서가 삭제되었거나 측정 설정 revision이 변경되어 주기 저장을 종료했습니다.");
		if(S.bPending)Files->CancelRequest(S.LastRequestId);
		Publish(Session);return;
	}
	if(Due<=0)return;
	S.SkippedCount+=Due-1; // A hitch never starts a burst of catch-up writes.
	if(S.bPending||!Files->CanAcceptSave(Sensor))
	{++S.SkippedCount;S.bWaitingForLatestFrame=true;S.WaitReason=EVirtualSensorPeriodicSaveWaitReason::SaveBusy;S.LastMessage=TEXT("저장 작업 진행 중 · 이번 주기를 생략하고 다음 주기에 최신 프레임을 확인합니다.");Publish(Session);return;}
	const int64 Frame=UVirtualSensorFileSaveSubsystem::GetAvailableFrameId(Sensor);
	if(Frame<0)
	{
		++S.SkippedCount;S.bWaitingForLatestFrame=true;S.WaitReason=EVirtualSensorPeriodicSaveWaitReason::NoCompletedFrame;
		S.LastMessage=TEXT("완료된 센서 프레임이 아직 없어 대기합니다. 대체 이미지나 추가 스캔은 생성하지 않습니다.");
		const FString Diagnostic=Sensor->GetSensorRuntimeStatus().LastMessage;
		if(!Diagnostic.IsEmpty())S.LastMessage+=TEXT(" 센서 입력/측정 진단: ")+Diagnostic;
		Publish(Session);return;
	}
	if(!UVirtualSensorFileSaveSubsystem::CanSaveCurrent(Sensor,S.Selection))
	{
		++S.SkippedCount;S.bWaitingForLatestFrame=true;S.WaitReason=EVirtualSensorPeriodicSaveWaitReason::SelectedOutputUnavailable;
		S.LastMessage=Sensor->GetSensorKind()==EVirtualSensorKind::Camera&&S.Selection.bCameraImage
			?TEXT("프레임은 수신했지만 선택한 JPEG 파일 출력이 없어 대기합니다. JSON 전용 외부 입력이라면 JPEG를 해제한 새 주기 저장을 시작하세요. 대체 이미지는 만들지 않습니다.")
			:TEXT("현재 프레임이 선택한 파일 출력을 지원하지 않아 대기합니다. 파싱·직렬화 오류는 저장 요청의 실패 진단과 별개입니다.");
		Publish(Session);return;
	}
	if(!IsNewFrame(Frame,Session->LastRequestedFrameId))
	{++S.SkippedCount;S.bWaitingForLatestFrame=true;S.WaitReason=EVirtualSensorPeriodicSaveWaitReason::NoNewFrame;S.LastMessage=TEXT("새 완료 프레임 대기 · 같은 센서 프레임을 중복 저장하지 않습니다.");Publish(Session);return;}
	S.bWaitingForLatestFrame=false;S.WaitReason=EVirtualSensorPeriodicSaveWaitReason::None;Session->LastRequestedFrameId=Frame;
	S.LastRequestId=Files->RequestSaveInCaptureSession(Sensor,S.Selection,S.Folder);
	S.bPending=true;S.LastMessage=TEXT("최신 완료 프레임 저장 접수 · 파일 완료 대기");Publish(Session);
}
void UVirtualSensorPeriodicFileSaveSubsystem::Tick(float)
{
	if(bShuttingDown||Sessions.IsEmpty()||!GetWorld())return;
	auto* Files=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>();if(!Files)return;
	const double Now=FPlatformTime::Seconds();
	// Delegates may start/stop another session; iterate a stable list of shared session records.
	TArray<TSharedPtr<FVirtualSensorPeriodicFileSaveSession>> Snapshot;Snapshot.Reserve(Sessions.Num());
	for(const auto& P:Sessions)Snapshot.Add(P.Value);
	for(const auto& S:Snapshot)if(S->Status.bActive||S->Status.bPending)ProcessSession(S,Files,Now);
	TrimHistory();
}
TStatId UVirtualSensorPeriodicFileSaveSubsystem::GetStatId() const{RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorPeriodicFileSaveSubsystem,STATGROUP_Tickables);}
void UVirtualSensorPeriodicFileSaveSubsystem::Deinitialize()
{
	// Tests and world teardown may converge here; never deinitialize the base twice.
	if(bShuttingDown)return;
	bShuttingDown=true;
	if(auto* Files=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>():nullptr)
	{
		Files->OnRequestUpdated.RemoveDynamic(this,&ThisClass::HandleFileSaveUpdated);
		for(const auto& P:Sessions)if(P.Value->Status.bPending)Files->CancelRequest(P.Value->Status.LastRequestId);
	}
	Sessions.Reset();Order.Reset();OnPeriodicUpdated.Clear();Super::Deinitialize();
}
