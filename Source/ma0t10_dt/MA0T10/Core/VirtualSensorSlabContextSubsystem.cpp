#include "VirtualSensorSlabContextSubsystem.h"
#include "EngineUtils.h"
#include "VirtualSensorStreamPublisherComponent.h"
#include "VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

TMap<FString,FString> FVirtualSlabFrameContext::ToHeaders() const
{
	TMap<FString,FString> Result;
	if (!bEligible) return Result;
	Result.Add(TEXT("x-run-uuid"), RunId);
	Result.Add(TEXT("x-mtl-no"), MtlNo);
	Result.Add(TEXT("x-slab-frame-no"), LexToString(SlabFrameNo));
	Result.Add(TEXT("x-slab-elapsed-sec"), FString::Printf(TEXT("%.6f"), ElapsedSec));
	Result.Add(TEXT("x-session-segment"), LexToString(Segment));
	return Result;
}
bool UVirtualSensorSlabContextSubsystem::CheckRun(const FString& Id)
{
	if (!IsInGameThread() || Id.IsEmpty() || Id != Status.RunId) { Status.Message=TEXT("실행 UUID가 일치하지 않습니다."); return false; }
	return true;
}
FString UVirtualSensorSlabContextSubsystem::BeginSlabSensorSession(const FString& RequestedId, const TArray<FString>& Ids, bool bPointCloudOnly)
{
	if (!IsInGameThread() || !GetWorld()) return FString();
	if (Status.State==EVirtualSlabSessionState::Ready || Status.State==EVirtualSlabSessionState::Running || Status.State==EVirtualSlabSessionState::Paused || Status.State==EVirtualSlabSessionState::Draining)
	{ Status.Message=TEXT("진행 중인 세션이 있습니다."); return FString(); }
	FGuid Guid;
	if (RequestedId.IsEmpty()) Guid=FGuid::NewGuid();
	else if (!FGuid::Parse(RequestedId,Guid) || !Guid.IsValid()) { Status.Message=TEXT("유효한 UUID가 필요합니다."); return FString(); }
	const FString Id=Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
	if (UsedRunIds.Contains(Id)) { Status.Message=TEXT("재실행에는 새로운 UUID를 사용하세요."); return FString(); }
	TArray<AVirtualSensorCoordinator*> Managers;
	for (TActorIterator<AVirtualSensorCoordinator> It(GetWorld()); It; ++It) Managers.Add(*It);
	if (Managers.Num()!=1) { Status.Message=TEXT("센서 Coordinator가 정확히 하나 필요합니다."); return FString(); }
	const auto* Transport=Managers[0]->SharedTransportComponent.Get();
	if (!Transport || Transport->TransportMode!=EVirtualSensorTransportMode::StompWebSocket || Transport->GetTransportProfile().BrokerUrl.IsEmpty())
	{ Status.Message=TEXT("세션 시작 전에 캡처/내보내기에서 STOMP 서버 설정을 적용하십시오. 로그 전용 출력을 Topic 송신으로 처리하지 않습니다."); return FString(); }
	TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>> NewTargets;
	for (auto* Actor : Managers[0]->GetSensorActors())
	{
		if (!IsValid(Actor) || (!Ids.IsEmpty() && !Ids.Contains(Actor->GetSensorId()))) continue;
		if (Actor->GetSensorId().IsEmpty() || NewTargets.Contains(Actor->GetSensorId())) { Status.Message=TEXT("중복 또는 빈 SensorId입니다."); return FString(); }
		NewTargets.Add(Actor->GetSensorId(),Actor);
	}
	if (NewTargets.IsEmpty()) { Status.Message=TEXT("대상 센서가 없습니다."); return FString(); }
	if(bPointCloudOnly)
	{
		bool bHasLidar=false; for(const auto& P:NewTargets) bHasLidar|=P.Value.IsValid()&&P.Value->GetSensorKind()==EVirtualSensorKind::Lidar;
		if(!bHasLidar) { Status.Message=TEXT("PCD 전용 세션에는 LiDAR 대상이 필요합니다."); return FString(); }
	}
	for (const FString& SensorId : Ids) if (!NewTargets.Contains(SensorId)) { Status.Message=TEXT("요청한 SensorId를 찾을 수 없습니다."); return FString(); }
	Coordinator=Managers[0]; Targets=MoveTemp(NewTargets); PendingKeys.Reset(); StartedSensors.Reset();
	InitialStreamErrors=CountStreamErrors();
	Status=FVirtualSlabSessionStatus(); Status.RunId=Id; Status.State=EVirtualSlabSessionState::Ready;
	Status.bPointCloudOnly=bPointCloudOnly;
	Status.CurrentSlab.RunId=Id; Status.CurrentSlab.Generation=++Generation;
	Status.Message=TEXT("첫 Slab 프레임 적용 대기 중"); UsedRunIds.Add(Id);
	for (const auto& Pair : Targets) ControlledIds.Add(Pair.Key);
	for (const auto& Pair : Targets)
	{
		auto* Actor=Pair.Value.Get();
		auto* Publisher=Coordinator->StreamPublisherComponent.Get();
		if (Publisher)
		{
			Publisher->StopAllStreams(Pair.Key);
			for (auto Kind : {EVirtualSensorStreamKind::CameraImage, EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::PointCloud})
			{
				if(bPointCloudOnly && Kind!=EVirtualSensorStreamKind::PointCloud) continue;
				if ((Kind==EVirtualSensorStreamKind::CameraImage) != (Actor->GetSensorKind()==EVirtualSensorKind::Camera)) continue;
				auto Config=Publisher->GetEffectiveStreamConfig(Kind,Pair.Key);
				Config.bEnabled=true; Config.FrameStride=1; Config.ReceiptSampleInterval=1;
				Publisher->ConfigureStream(Config);
			}
		}
		if (!Actor->IsSensorRunning() && (!bPointCloudOnly || Actor->GetSensorKind()==EVirtualSensorKind::Lidar)) { StartedSensors.Add(Pair.Key); Actor->StartSensor(); }
	}
	return Id;
}
bool UVirtualSensorSlabContextSubsystem::NotifySlabFrameApplied(const FString& Id,const FString& Mtl,int64 Frame,double Seconds)
{
	if (!CheckRun(Id)) return false;
	if (Status.State!=EVirtualSlabSessionState::Ready && Status.State!=EVirtualSlabSessionState::Running) return false;
	if (Mtl.TrimStartAndEnd().IsEmpty() || Mtl.Len()>256 || Mtl.Contains(TEXT("\n")) || Mtl.Contains(TEXT("\r")) || Frame<0 || !FMath::IsFinite(Seconds) || Seconds<0)
	{ Status.Message=TEXT("Slab 프레임 값이 잘못되었습니다."); return false; }
	const auto& Previous=Status.CurrentSlab;
	if (Frame==Previous.SlabFrameNo) return Mtl==Previous.MtlNo && Seconds==Previous.ElapsedSec;
	if (Frame<Previous.SlabFrameNo || Seconds<Previous.ElapsedSec) { Status.Message=TEXT("Slab 프레임 또는 시간이 역순입니다."); return false; }
	Status.CurrentSlab.MtlNo=Mtl; Status.CurrentSlab.SlabFrameNo=Frame; Status.CurrentSlab.ElapsedSec=Seconds;
	Status.CurrentSlab.bEligible=true; Status.State=EVirtualSlabSessionState::Running; Status.Message=Status.bPointCloudOnly?TEXT("Slab 연동 PCD 전용 송신 중"):TEXT("Slab 연동 센서 송신 중");
	return true;
}
bool UVirtualSensorSlabContextSubsystem::SetSlabSensorSessionPaused(const FString& Id,bool Paused)
{
	if (!CheckRun(Id)) return false;
	if (Paused && Status.State==EVirtualSlabSessionState::Running) Status.State=EVirtualSlabSessionState::Paused;
	else if (!Paused && Status.State==EVirtualSlabSessionState::Paused) { Status.State=EVirtualSlabSessionState::Running; ++Status.CurrentSlab.Segment; }
	else return false;
	return true;
}
bool UVirtualSensorSlabContextSubsystem::EndSlabSensorSession(const FString& Id,bool Aborted)
{
	if (!CheckRun(Id)) return false;
	if (Status.State==EVirtualSlabSessionState::Draining || Status.State==EVirtualSlabSessionState::Completed) return true;
	if (Status.State!=EVirtualSlabSessionState::Running && Status.State!=EVirtualSlabSessionState::Paused && Status.State!=EVirtualSlabSessionState::Ready) return false;
	Status.bAborted=Aborted; Status.State=EVirtualSlabSessionState::Draining; DrainStarted=FPlatformTime::Seconds();
	Status.Message=TEXT("마지막 데이터 전송 중"); return true;
}
FVirtualSlabFrameContext UVirtualSensorSlabContextSubsystem::CaptureContext(const FString& SensorId,int64 FrameId)
{
	FVirtualSlabFrameContext Result;
	if (!Targets.Contains(SensorId) || Status.State!=EVirtualSlabSessionState::Running) return Result;
	Result=Status.CurrentSlab; Result.AcquisitionUtcTicks=FDateTime::UtcNow().GetTicks();
	PendingKeys.Add(SensorId+TEXT("|")+LexToString(FrameId)); Status.PendingAcquisitions=PendingKeys.Num();
	return Result;
}
void UVirtualSensorSlabContextSubsystem::CompleteAcquisition(const FString& Id,int64 FrameId,bool Success)
{
	const bool Removed=PendingKeys.Remove(Id+TEXT("|")+LexToString(FrameId))>0;
	if (Removed && !Success) ++Status.AcquisitionFailures;
	Status.PendingAcquisitions=PendingKeys.Num();
}
bool UVirtualSensorSlabContextSubsystem::AllowsFrame(const FString& Id,const FVirtualSlabFrameContext& Context) const
{
	if (!ControlsSensor(Id)) return !Context.bEligible;
	return Context.bEligible && Context.RunId==Status.RunId && Context.Generation==Generation &&
		(Status.State==EVirtualSlabSessionState::Running || Status.State==EVirtualSlabSessionState::Paused || Status.State==EVirtualSlabSessionState::Draining);
}
void UVirtualSensorSlabContextSubsystem::Tick(float DeltaTime)
{
	if (Status.State!=EVirtualSlabSessionState::Draining) return;
	int64 Waiting=PendingKeys.Num();
	Status.StreamFailures = FMath::Max<int64>(0, CountStreamErrors() - InitialStreamErrors);
	if (Coordinator.IsValid() && Coordinator->StreamPublisherComponent)
	{
		for (const auto& S : Coordinator->StreamPublisherComponent->GetStreamStatuses())
		{
			if (!Targets.Contains(S.SensorId)) continue;
			Waiting+=S.InputQueueDepth+S.PreparedQueueDepth+(S.bProcessing ? 1 : 0);
			if (S.ActiveTransportBackend!=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput) Waiting+=S.ReceiptQueueDepth;
		}
	}
	if (auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>())
	{
		Waiting+=Raw->GetPendingRunFrameCount(Status.RunId);
		Status.StreamFailures += Raw->GetFailedRunFrameCount(Status.RunId);
	}
	Status.UnfinishedFrames=Waiting;
	if (Waiting == 0)
	{
		const auto Reason = Status.AcquisitionFailures > 0 ? EVirtualSlabSessionEndReason::AcquisitionFailure
			: Status.StreamFailures > 0 ? EVirtualSlabSessionEndReason::StreamFailure
			: Status.bAborted ? EVirtualSlabSessionEndReason::Aborted : EVirtualSlabSessionEndReason::Completed;
		Finish(Reason);
	}
	else if (FPlatformTime::Seconds()-DrainStarted>10.0) Finish(EVirtualSlabSessionEndReason::DrainTimeout);
}
void UVirtualSensorSlabContextSubsystem::Finish(EVirtualSlabSessionEndReason Reason)
{
	const bool bFailed = Reason == EVirtualSlabSessionEndReason::AcquisitionFailure ||
		Reason == EVirtualSlabSessionEndReason::StreamFailure || Reason == EVirtualSlabSessionEndReason::DrainTimeout;
	PendingKeys.Reset(); Status.PendingAcquisitions=0;
	if (Coordinator.IsValid() && Coordinator->StreamPublisherComponent)
		for (const auto& Pair : Targets) Coordinator->StreamPublisherComponent->StopAllStreams(Pair.Key);
	for (const auto& Pair : Targets) if (StartedSensors.Contains(Pair.Key) && Pair.Value.IsValid()) Pair.Value->StopSensor();
	if (bFailed) if (auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()) Raw->CancelRun(Status.RunId);
	Status.EndReason = Reason;
	Status.State = bFailed ? EVirtualSlabSessionState::Incomplete : EVirtualSlabSessionState::Completed;
	switch (Reason)
	{
	case EVirtualSlabSessionEndReason::AcquisitionFailure:
		Status.Message = FString::Printf(TEXT("센서 측정 실패: %d건 · 송신 오류 %lld건 (전송 로그 확인)"), Status.AcquisitionFailures, Status.StreamFailures);
		break;
	case EVirtualSlabSessionEndReason::StreamFailure:
		Status.Message = FString::Printf(TEXT("센서 데이터 처리/송신 실패: %lld건 (크기 제한·직렬화·receipt 등 전송 로그 확인)"), Status.StreamFailures);
		break;
	case EVirtualSlabSessionEndReason::DrainTimeout:
		Status.Message = FString::Printf(TEXT("종료 제한 시간 초과: 미완료 %lld건"), Status.UnfinishedFrames);
		break;
	case EVirtualSlabSessionEndReason::Aborted:
		Status.Message = TEXT("Slab 시뮬레이션 중단: 접수된 데이터 전송 마무리 완료");
		break;
	default:
		Status.Message = TEXT("Slab 센서 세션 완료");
		break;
	}
}
TStatId UVirtualSensorSlabContextSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorSlabContextSubsystem,STATGROUP_Tickables); }
int64 UVirtualSensorSlabContextSubsystem::CountStreamErrors() const
{
	int64 Count=0;
	if (Coordinator.IsValid() && Coordinator->StreamPublisherComponent)
		for (const auto& S : Coordinator->StreamPublisherComponent->GetStreamStatuses()) if (Targets.Contains(S.SensorId)) Count+=S.OverloadCount+S.EncodeFailureCount+S.ConsumerValidationFailureCount+S.BodyLimitRejectedCount+S.DeliveryFailureCount;
	if (GetWorld()) if (auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>())
		for (const auto& S : Raw->GetStreamTelemetry()) if (Targets.Contains(S.SensorId)) Count+=S.OverloadCount+S.ValidationFailureCount;
	return Count;
}
void UVirtualSensorSlabContextSubsystem::Deinitialize() { PendingKeys.Reset(); Targets.Reset(); Super::Deinitialize(); }
