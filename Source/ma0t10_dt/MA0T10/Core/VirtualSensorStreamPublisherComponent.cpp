#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "VirtualPointCloudCoordinates.h"
#include "VirtualSensorWireHeaders.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Base64.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarPayloadCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"



UVirtualSensorStreamPublisherComponent::UVirtualSensorStreamPublisherComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}
void UVirtualSensorStreamPublisherComponent::BeginPlay()
{
	Super::BeginPlay();
	LastTokenUpdateSeconds = FPlatformTime::Seconds();
	TokenBucketBytes = BandwidthLimitMegabytesPerSecond * 1024.0 * 1024.0;
	PointCloudTokenBucketBytes = PointCloudBandwidthLimitMegabytesPerSecond * 1024.0 * 1024.0;
	if (TransportComponent) TransportComponent->OnDataSent.AddUniqueDynamic(this, &UVirtualSensorStreamPublisherComponent::OnTransportResult);
	FString HighThroughputError;
	EnsureHighThroughputTransport(HighThroughputError);
}

void UVirtualSensorStreamPublisherComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	bHasPointCloudRecoveryWork = false;
	if (TransportComponent) TransportComponent->OnDataSent.RemoveDynamic(this, &UVirtualSensorStreamPublisherComponent::OnTransportResult);
	WaitingReceipts.Reset();
	RequestToStreamKey.Reset();
	StreamRuntimes.Reset();
	SlabRunFailures.Reset();
	if (GetWorld())
	{
		if (UVirtualSensorHighThroughputTransportSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>())
		{
			Subsystem->StopHighThroughputTransport();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UVirtualSensorStreamPublisherComponent::SetTransportComponent(UVirtualSensorTransportComponent* InTransportComponent)
{
	if (TransportComponent == InTransportComponent) return;
	if (TransportComponent) TransportComponent->OnDataSent.RemoveDynamic(this, &UVirtualSensorStreamPublisherComponent::OnTransportResult);
	TransportComponent = InTransportComponent;
	if (TransportComponent) TransportComponent->OnDataSent.AddUniqueDynamic(this, &UVirtualSensorStreamPublisherComponent::OnTransportResult);
}

FString UVirtualSensorStreamPublisherComponent::MakeStreamKey(EVirtualSensorStreamKind StreamKind, const FString& SensorId) const
{
	return FString::Printf(TEXT("%d|%s"), static_cast<int32>(StreamKind), SensorId.IsEmpty() ? TEXT("*") : *SensorId);
}

UVirtualSensorStreamPublisherComponent::FStreamRuntime& UVirtualSensorStreamPublisherComponent::FindOrAddRuntime(EVirtualSensorStreamKind StreamKind, const FString& SensorId)
{
	const FString Key = MakeStreamKey(StreamKind, SensorId);
	FStreamRuntime& Runtime = StreamRuntimes.FindOrAdd(Key);
	Runtime.Config.StreamKind = StreamKind;
	Runtime.Config.SensorId = SensorId;
	Runtime.Status.StreamKind = StreamKind;
	Runtime.Status.SensorId = SensorId;
	return Runtime;
}

FVirtualSensorStreamConfig UVirtualSensorStreamPublisherComponent::NormalizeLiveConfig(FVirtualSensorStreamConfig Config)
{
	if (Config.StreamKind == EVirtualSensorStreamKind::PointCloud)
	{
		Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
		Config.PcdDataMode = EVirtualPcdDataMode::Binary;
		Config.DeliveryMode = EVirtualPointCloudDeliveryMode::ConnectedNoLoss;
		Config.FrameStride = 1;
		Config.ReceiptSampleInterval = 1;
	}
	return Config;
}

FVirtualSensorStreamConfig UVirtualSensorStreamPublisherComponent::GetEffectiveStreamConfig(EVirtualSensorStreamKind Kind, const FString& SensorId) const
{
	if (const FStreamRuntime* Runtime = StreamRuntimes.Find(MakeStreamKey(Kind, SensorId))) return Runtime->Config;
	FVirtualSensorStreamConfig Config;
	Config.StreamKind = Kind;
	Config.SensorId = SensorId;
	return NormalizeLiveConfig(Config);
}

bool UVirtualSensorStreamPublisherComponent::ValidateBinaryBodySize(int64 Bytes, int64 Limit, FString& Error)
{
	if (Bytes <= 0 || Bytes > Limit)
	{
		Error = FString::Printf(TEXT("[메시지 크기] Binary body=%lld bytes, 설정 한도=%lld bytes. 센서 점 수와 메시지 한도를 확인하세요."), Bytes, Limit);
		return false;
	}
	Error.Reset();
	return true;
}

void UVirtualSensorStreamPublisherComponent::ConfigureStream(const FVirtualSensorStreamConfig& RequestedConfig)
{
	const FVirtualSensorStreamConfig Config = NormalizeLiveConfig(RequestedConfig);
	if (FStreamRuntime* Existing = StreamRuntimes.Find(MakeStreamKey(Config.StreamKind, Config.SensorId.TrimStartAndEnd())))
		CancelPointCloudOverloadResume(*Existing);
	FString PlacementError;if(Config.bEnabled&&!PrepareCollectionTargets(Config.StreamKind,Config.SensorId,PlacementError))
	{AddLog(MakeStreamKey(Config.StreamKind,Config.SensorId),TEXT("start-rejected"),PlacementError);return;}
	FStreamRuntime& Runtime = FindOrAddRuntime(Config.StreamKind, Config.SensorId.TrimStartAndEnd());
	const bool bWasEnabled = Runtime.Config.bEnabled;
	const bool bSerializationContractChanged = Runtime.Config.PointCloudFormat != Config.PointCloudFormat ||
		Runtime.Config.PcdDataMode != Config.PcdDataMode ||
		Runtime.Config.DeliveryMode != Config.DeliveryMode ||
		Runtime.Config.PointCloudFilter.Revision != Config.PointCloudFilter.Revision ||
		Runtime.Config.FrameStride != Config.FrameStride ||
		Runtime.Config.LazCompressorPath != Config.LazCompressorPath ||
		Runtime.Config.LazCompressorArguments != Config.LazCompressorArguments;
	if (bSerializationContractChanged)
	{
		RecordDiscardedSlabFrames(Runtime);
		++Runtime.ConfigRevision;
		Runtime.PendingFrame.Reset();
		Runtime.PreparedMessage.Reset();
		Runtime.PendingFrameQueue.Reset();
		Runtime.PreparedMessageQueue.Reset();
		Runtime.Status.bPendingLatestFrame = false;
	}
	Runtime.Config = Config;
	Runtime.Config.SensorId = Config.SensorId.TrimStartAndEnd();
	Runtime.Config.FrameStride = FMath::Max(1, Config.FrameStride);
	Runtime.Config.ReceiptSampleInterval = FMath::Max(1, Config.ReceiptSampleInterval);
	Runtime.Config.MaxBufferedFrames = FMath::Clamp(Config.MaxBufferedFrames, 1, 120);
	Runtime.Config.MaxReceiptRetries = FMath::Clamp(Config.MaxReceiptRetries, 0, 10);
	if (Runtime.Config.StreamKind == EVirtualSensorStreamKind::PointCloud &&
		Runtime.Config.DeliveryMode == EVirtualPointCloudDeliveryMode::ConnectedNoLoss)
	{
		Runtime.Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
		Runtime.Config.PcdDataMode = EVirtualPcdDataMode::Binary;
		Runtime.Config.FrameStride = 1;
		Runtime.Config.ReceiptSampleInterval = 1;
	}
	Runtime.Config.MaxLazHz = FMath::Clamp(Config.MaxLazHz, 0.1f, 1.0f);
	Runtime.Status.bEnabled = Runtime.Config.bEnabled;
	Runtime.Status.bOverloaded = false;
	Runtime.Status.ConfigRevision = Runtime.ConfigRevision;
	RefreshQueueTelemetry(Runtime);
	if (bWasEnabled != Runtime.Config.bEnabled) UpdateCameraStreamDemand();
	OnStreamConfigurationChanged.Broadcast();
}

void UVirtualSensorStreamPublisherComponent::StartStream(EVirtualSensorStreamKind StreamKind, const FString& SensorId)
{
	FString PlacementError;if(!PrepareCollectionTargets(StreamKind,SensorId,PlacementError))
	{AddLog(MakeStreamKey(StreamKind,SensorId),TEXT("start-rejected"),PlacementError);return;}
	FStreamRuntime& Runtime = FindOrAddRuntime(StreamKind, SensorId.TrimStartAndEnd());
	CancelPointCloudOverloadResume(Runtime);
	Runtime.Config = NormalizeLiveConfig(Runtime.Config);
	Runtime.Config.bEnabled = true;
	Runtime.Status.bEnabled = true;
	Runtime.Status.bOverloaded = false;
	Runtime.Status.Message = TEXT("실시간 전송 대기 중");
	AddLog(MakeStreamKey(StreamKind, SensorId.TrimStartAndEnd()), TEXT("started"), TEXT("스트림을 시작했습니다."));
	UpdateCameraStreamDemand();
	OnStreamConfigurationChanged.Broadcast();
}

void UVirtualSensorStreamPublisherComponent::StopStream(EVirtualSensorStreamKind StreamKind, const FString& SensorId)
{
	const FString Key = MakeStreamKey(StreamKind, SensorId.TrimStartAndEnd());
	if (FStreamRuntime* Runtime = StreamRuntimes.Find(Key))
	{
		CancelPointCloudOverloadResume(*Runtime);
		++Runtime->ConfigRevision; // Invalidate serialization already running at user stop.
		Runtime->Status.ConfigRevision = Runtime->ConfigRevision;
		RecordDiscardedSlabFrames(*Runtime);
		Runtime->Config.bEnabled = false;
		Runtime->Status.bEnabled = false;
		Runtime->Status.bOverloaded = false;
		Runtime->PendingFrame.Reset();
		Runtime->PreparedMessage.Reset();
		Runtime->PendingFrameQueue.Reset();
		Runtime->PreparedMessageQueue.Reset();
		Runtime->Status.bPendingLatestFrame = false;
		RefreshQueueTelemetry(*Runtime);
		Runtime->Status.Message = TEXT("중지됨");
		AddLog(Key, TEXT("stopped"), TEXT("스트림을 중지했습니다."));
	}
	UpdateCameraStreamDemand();
	OnStreamConfigurationChanged.Broadcast();
}

void UVirtualSensorStreamPublisherComponent::StartAllStreams(const FString& SensorId)
{
	StartStream(EVirtualSensorStreamKind::LidarPayload, SensorId);
	StartStream(EVirtualSensorStreamKind::CameraImage, SensorId);
	StartStream(EVirtualSensorStreamKind::PointCloud, SensorId);
}

void UVirtualSensorStreamPublisherComponent::StopAllStreams(const FString& SensorId)
{
	if (SensorId.TrimStartAndEnd().IsEmpty())
	{
		for (TPair<FString, FStreamRuntime>& Pair : StreamRuntimes)
		{
			CancelPointCloudOverloadResume(Pair.Value);
			++Pair.Value.ConfigRevision;
			Pair.Value.Status.ConfigRevision = Pair.Value.ConfigRevision;
			RecordDiscardedSlabFrames(Pair.Value);
			Pair.Value.Config.bEnabled = false;
			Pair.Value.Status.bEnabled = false;
			Pair.Value.Status.bOverloaded = false;
			Pair.Value.PendingFrame.Reset();
			Pair.Value.PreparedMessage.Reset();
			Pair.Value.PendingFrameQueue.Reset();
			Pair.Value.PreparedMessageQueue.Reset();
			Pair.Value.Status.bPendingLatestFrame = false;
			RefreshQueueTelemetry(Pair.Value);
			Pair.Value.Status.Message = TEXT("중지됨");
		}
		UpdateCameraStreamDemand();
		OnStreamConfigurationChanged.Broadcast();
		return;
	}
	StopStream(EVirtualSensorStreamKind::LidarPayload, SensorId);
	StopStream(EVirtualSensorStreamKind::CameraImage, SensorId);
	StopStream(EVirtualSensorStreamKind::PointCloud, SensorId);
}

bool UVirtualSensorStreamPublisherComponent::IsStreamEnabled(EVirtualSensorStreamKind StreamKind, const FString& SensorId) const
{
	if (const FStreamRuntime* Runtime = StreamRuntimes.Find(MakeStreamKey(StreamKind, SensorId.TrimStartAndEnd()))) return Runtime->Config.bEnabled;
	return false;
}

bool UVirtualSensorStreamPublisherComponent::IsStreamActiveOrAwaitingResume(EVirtualSensorStreamKind Kind, const FString& SensorId) const
{
	if (const FStreamRuntime* Runtime = StreamRuntimes.Find(MakeStreamKey(Kind, SensorId.TrimStartAndEnd())))
		return Runtime->Config.bEnabled || Runtime->Status.bOverloadSuspended;
	return false;
}

bool UVirtualSensorStreamPublisherComponent::StreamMatchesFrame(EVirtualSensorStreamKind StreamKind, EVirtualSensorKind SensorKind)
{
	return StreamKind == EVirtualSensorStreamKind::CameraImage
		? SensorKind == EVirtualSensorKind::Camera
		: SensorKind == EVirtualSensorKind::Lidar;
}

bool UVirtualSensorStreamPublisherComponent::ResolveActiveStreamForSensor(EVirtualSensorStreamKind Kind,const FString& Id,FVirtualSensorStreamConfig& Out) const
{
	const auto* Exact=StreamRuntimes.Find(MakeStreamKey(Kind,Id));
	if(Exact&&Exact->Status.bOverloadSuspended)return false;
	if(Exact&&Exact->Config.bEnabled){Out=Exact->Config;return true;}
	const auto* Wildcard=StreamRuntimes.Find(MakeStreamKey(Kind,FString()));
	if(Wildcard&&Wildcard->Config.bEnabled){Out=Wildcard->Config;return true;}
	return false;
}

void UVirtualSensorStreamPublisherComponent::RefreshQueueTelemetry(FStreamRuntime& Runtime)
{
	Runtime.Status.InputQueueDepth = Runtime.PendingFrameQueue.Num() + (Runtime.PendingFrame.IsSet() ? 1 : 0);
	Runtime.Status.PreparedQueueDepth = Runtime.PreparedMessageQueue.Num() + (Runtime.PreparedMessage.IsSet() ? 1 : 0);
	int32 ReceiptDepth = 0;
	for (const TPair<FString, FReceiptWait>& Pair : WaitingReceipts)
	{
		if (Pair.Value.StreamKey == MakeStreamKey(Runtime.Config.StreamKind, Runtime.Config.SensorId)) ++ReceiptDepth;
	}
	Runtime.Status.ReceiptQueueDepth = ReceiptDepth;
	Runtime.Status.bPendingLatestFrame = Runtime.Status.InputQueueDepth > 0 || Runtime.Status.PreparedQueueDepth > 0;
}

void UVirtualSensorStreamPublisherComponent::StopForBodyLimit(const FString& StreamKey, FStreamRuntime& Runtime, const FString& Reason, int64 FrameId)
{
	RecordSlabFailure(Runtime.LastInputSlabContext.RunId,Runtime.Config.SensorId,Runtime.Config.StreamKind,FrameId);
	RecordDiscardedSlabFrames(Runtime);
	// Stop clears unsent queues but retains receipt tracking for accepted frames.
	// Persist a failure counter even after Finish() replaces the current UI message.
	const FString SensorId = Runtime.Config.SensorId;
	StopStream(Runtime.Config.StreamKind, SensorId);
	++Runtime.Status.BodyLimitRejectedCount;
	Runtime.Status.Message = Reason;
	AddLog(StreamKey, TEXT("body-limit-rejected"), Reason, nullptr, FrameId);
}

void UVirtualSensorStreamPublisherComponent::StopForPointCloudOverload(
	const FString& StreamKey,
	FStreamRuntime& Runtime,
	const FString& Reason,
	const FPreparedMessage* RejectedMessage)
{
	const FString& RejectedSensor = RejectedMessage ? RejectedMessage->SensorId : Runtime.LastInputSensorId;
	const int64 RejectedFrame = RejectedMessage ? RejectedMessage->FrameId : Runtime.Status.LastInputFrameId;
	const auto& RejectedContext = RejectedMessage ? RejectedMessage->SlabContext : Runtime.LastInputSlabContext;
	RecordSlabFailure(RejectedContext.RunId, RejectedSensor, Runtime.Config.StreamKind, RejectedFrame);
	RecordDiscardedSlabFrames(Runtime);
	TSet<FString> Discarded;
	auto Count = [&Discarded](const FString& Sensor, int64 Frame) { Discarded.Add(Sensor + TEXT("|") + LexToString(Frame)); };
	Count(RejectedSensor, RejectedFrame);
	if (Runtime.bSerializationInFlight) Count(Runtime.SerializationSensorId, Runtime.SerializationFrameId);
	if (Runtime.PendingFrame.IsSet()) Count(Runtime.PendingFrame->SensorId, Runtime.PendingFrame->FrameId);
	if (Runtime.PreparedMessage.IsSet()) Count(Runtime.PreparedMessage->SensorId, Runtime.PreparedMessage->FrameId);
	for (const auto& Frame : Runtime.PendingFrameQueue) Count(Frame.SensorId, Frame.FrameId);
	for (const auto& Message : Runtime.PreparedMessageQueue) Count(Message.SensorId, Message.FrameId);
	for (const auto& Pair : WaitingReceipts) if (Pair.Value.StreamKey == StreamKey) Count(Pair.Value.Message.SensorId, Pair.Value.Message.FrameId);
	Runtime.Status.OverloadDiscardedFrameCount += Discarded.Num();
	++Runtime.ConfigRevision;
	Runtime.Status.ConfigRevision = Runtime.ConfigRevision;
	++Runtime.Status.OverloadCount;
	Runtime.Status.bOverloaded = true;
	Runtime.Status.bEnabled = false;
	Runtime.Config.bEnabled = false;
	Runtime.Status.Message = Reason;
	Runtime.HealthyDeliverySinceSeconds = -1.0;
	Runtime.Status.bOverloadSuspended = bAutoResumePointCloudAfterOverload &&
		Runtime.Config.StreamKind == EVirtualSensorStreamKind::PointCloud;
	if (Runtime.Status.bOverloadSuspended)
	{
		bHasPointCloudRecoveryWork = true;
		const double Maximum = FMath::Clamp<double>(PointCloudOverloadMaxResumeDelaySeconds, 0.1, 30.0);
		const double Initial = FMath::Clamp<double>(PointCloudOverloadResumeDelaySeconds, 0.1, Maximum);
		Runtime.OverloadBackoffSeconds = Runtime.OverloadBackoffSeconds > 0.0
			? FMath::Min(Maximum, Runtime.OverloadBackoffSeconds * 2.0) : Initial;
		Runtime.OverloadResumeAtSeconds = FPlatformTime::Seconds() + Runtime.OverloadBackoffSeconds;
		Runtime.NextOverloadRecoveryCheckSeconds = Runtime.OverloadResumeAtSeconds;
		Runtime.Status.OverloadResumeDelaySeconds = Runtime.OverloadBackoffSeconds;
		Runtime.Status.EarliestOverloadResumeUtc = FDateTime::UtcNow() + FTimespan::FromSeconds(Runtime.OverloadBackoffSeconds);
		Runtime.Status.Message = TEXT("PCD overload suspended; automatic resume awaits cooldown, connection and drain.");
	}
	Runtime.PendingFrame.Reset();
	Runtime.PendingFrameQueue.Reset();
	Runtime.PreparedMessage.Reset();
	Runtime.PreparedMessageQueue.Reset();
	TArray<FString> ReceiptIdsToRemove;
	for (const TPair<FString, FReceiptWait>& Pair : WaitingReceipts)
	{
		if (Pair.Value.StreamKey == StreamKey) ReceiptIdsToRemove.Add(Pair.Key);
	}
	for (const FString& RequestId : ReceiptIdsToRemove)
	{
		WaitingReceipts.Remove(RequestId);
		RequestToStreamKey.Remove(RequestId);
	}
	RefreshQueueTelemetry(Runtime);
	AddLog(StreamKey, TEXT("overload-stopped"), Reason);
	if (Runtime.Status.bOverloadSuspended)
	{
		const FString Detail = FString::Printf(TEXT("Automatic PCD resume eligible at %s (%.1fs cooldown), resume count=%lld; connection and drain required."),
			*Runtime.Status.EarliestOverloadResumeUtc.ToIso8601(), Runtime.OverloadBackoffSeconds, Runtime.Status.ResumeCount);
		AddLog(StreamKey, TEXT("overload-resume-scheduled"), Detail);
		UE_LOG(LogTemp, Display, TEXT("[PCD-RESUME] %s %s"), *StreamKey, *Detail);
	}
}

void UVirtualSensorStreamPublisherComponent::CancelPointCloudOverloadResume(FStreamRuntime& Runtime)
{
	Runtime.Status.bOverloadSuspended = false;
	Runtime.Status.OverloadResumeDelaySeconds = 0.0f;
	Runtime.Status.EarliestOverloadResumeUtc = FDateTime();
	Runtime.OverloadResumeAtSeconds = Runtime.NextOverloadRecoveryCheckSeconds = Runtime.OverloadBackoffSeconds = 0.0;
	Runtime.HealthyDeliverySinceSeconds = -1.0;
	Runtime.LastObservedReceiptCount = Runtime.Status.ReceiptReceivedCount;
}

bool UVirtualSensorStreamPublisherComponent::CanResumePointCloudStream(const FString& StreamKey, const FStreamRuntime& Runtime) const
{
	if (!TransportComponent || Runtime.bSerializationInFlight || Runtime.PendingFrame.IsSet() ||
		Runtime.PreparedMessage.IsSet() || !Runtime.PendingFrameQueue.IsEmpty() || !Runtime.PreparedMessageQueue.IsEmpty()) return false;
	for (const auto& Pair : WaitingReceipts) if (Pair.Value.StreamKey == StreamKey) return false;
	if (Runtime.Config.TransportBackend == EVirtualSensorStreamTransportBackend::TcpStompHighThroughput && CanUseHighThroughputTransport())
	{
		const auto* Raw = GetWorld() ? GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>() : nullptr;
		return Raw && Raw->CanResumePointCloudStream(Runtime.Config.SensorId);
	}
	// WSS stays on the existing Engine STOMP route; never downgrade it to TCP.
	return TransportComponent->IsStompConnected();
}

bool UVirtualSensorStreamPublisherComponent::UpdatePointCloudOverloadRecovery(const FString& StreamKey, FStreamRuntime& Runtime, double Now, bool bPathReady)
{
	if (!Runtime.Status.bOverloadSuspended) return false;
	if (!bAutoResumePointCloudAfterOverload)
	{
		CancelPointCloudOverloadResume(Runtime);
		Runtime.Status.Message = TEXT("PCD overload stopped; automatic resume disabled.");
		AddLog(StreamKey, TEXT("overload-resume-cancelled"), Runtime.Status.Message);
		return false;
	}
	if (Now < Runtime.OverloadResumeAtSeconds || !bPathReady || Runtime.bSerializationInFlight) return false;
	Runtime.Status.bOverloadSuspended = false;
	Runtime.Status.EarliestOverloadResumeUtc = FDateTime();
	Runtime.Status.OverloadResumeDelaySeconds = 0.0f;
	Runtime.Config.bEnabled = Runtime.Status.bEnabled = true;
	Runtime.Status.bOverloaded = false;
	Runtime.NextSubmitAttemptSeconds = 0.0;
	Runtime.HealthyDeliverySinceSeconds = -1.0;
	Runtime.LastObservedReceiptCount = Runtime.Status.ReceiptReceivedCount;
	++Runtime.Status.ResumeCount;
	Runtime.Status.Message = TEXT("PCD automatically resumed; waiting for a new frame.");
	AddLog(StreamKey, TEXT("overload-resumed"), Runtime.Status.Message);
	UE_LOG(LogTemp, Display, TEXT("[PCD-RESUME] %s resumed count=%lld"), *StreamKey, Runtime.Status.ResumeCount);
	return true;
}

void UVirtualSensorStreamPublisherComponent::PumpPointCloudOverloadRecovery(double Now)
{
	bool Resumed = false, RecoveryNeeded = false;
	for (auto& Pair : StreamRuntimes)
	{
		auto& Runtime = Pair.Value;
		if (Runtime.Config.StreamKind != EVirtualSensorStreamKind::PointCloud) continue;
		if (Runtime.Status.bOverloadSuspended)
		{
			if (bAutoResumePointCloudAfterOverload && Now < Runtime.NextOverloadRecoveryCheckSeconds)
			{
				RecoveryNeeded = true;
				continue;
			}
			Runtime.NextOverloadRecoveryCheckSeconds = Now + 0.2;
			Resumed |= UpdatePointCloudOverloadRecovery(Pair.Key, Runtime, Now, CanResumePointCloudStream(Pair.Key, Runtime));
			RecoveryNeeded |= Runtime.Status.bOverloadSuspended || (Runtime.Config.bEnabled && Runtime.OverloadBackoffSeconds > 0.0);
			continue;
		}
		if (!Runtime.Config.bEnabled || Runtime.OverloadBackoffSeconds <= 0.0) continue;
		const auto* Raw = GetWorld() ? GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>() : nullptr;
		const bool Connected = Runtime.Config.TransportBackend == EVirtualSensorStreamTransportBackend::TcpStompHighThroughput && CanUseHighThroughputTransport()
			? Raw && Raw->IsTransportConnected() && !Raw->IsSendDelayed() : TransportComponent && TransportComponent->IsStompConnected();
		UpdatePointCloudDeliveryProgress(Runtime, Now, Connected);
		RecoveryNeeded |= Runtime.OverloadBackoffSeconds > 0.0;
	}
	bHasPointCloudRecoveryWork = RecoveryNeeded;
	if (Resumed) OnStreamConfigurationChanged.Broadcast();
}

void UVirtualSensorStreamPublisherComponent::UpdatePointCloudDeliveryProgress(FStreamRuntime& Runtime, double Now, bool bConnected)
{
	if (!bConnected || Now - Runtime.LastDeliveryProgressSeconds > 2.0) Runtime.HealthyDeliverySinceSeconds = -1.0;
	if (bConnected && Runtime.Status.ReceiptReceivedCount > Runtime.LastObservedReceiptCount)
	{
		Runtime.LastDeliveryProgressSeconds = Now;
		if (Runtime.HealthyDeliverySinceSeconds < 0.0) Runtime.HealthyDeliverySinceSeconds = Now;
		if (Now - Runtime.HealthyDeliverySinceSeconds >= FMath::Max(1.0f, PointCloudOverloadBackoffResetSeconds))
		{
			Runtime.OverloadBackoffSeconds = 0.0;
			AddLog(MakeStreamKey(Runtime.Config.StreamKind, Runtime.Config.SensorId), TEXT("overload-backoff-reset"), TEXT("PCD receipts progressed normally; overload backoff reset."));
		}
	}
	Runtime.LastObservedReceiptCount = Runtime.Status.ReceiptReceivedCount;
}

void UVirtualSensorStreamPublisherComponent::TryStartNextPointCloudSerialization(
	const FString& StreamKey,
	FStreamRuntime& Runtime)
{
	if (!Runtime.Config.bEnabled || Runtime.bSerializationInFlight || Runtime.PendingFrameQueue.IsEmpty()) return;
	if (Runtime.PreparedMessageQueue.Num() >= Runtime.Config.MaxBufferedFrames) return;
	FVirtualSensorFrameEnvelope Next = MoveTemp(Runtime.PendingFrameQueue[0]);
	Runtime.PendingFrameQueue.RemoveAt(0, 1, false);
	StartPointCloudSerialization(StreamKey, Runtime, Next);
	RefreshQueueTelemetry(Runtime);
}

void UVirtualSensorStreamPublisherComponent::CompletePointCloudSerialization(const FString& StreamKey, FPreparedMessage&& Message, const FString& Error, int32 CapturedConfigRevision)
{
	FStreamRuntime* Runtime = StreamRuntimes.Find(StreamKey);
	if (!Runtime) return;
	Runtime->bSerializationInFlight = false;
	Runtime->SerializationRunId.Reset();
	Runtime->Status.bProcessing = false;
	if (!Runtime->Config.bEnabled || Runtime->Status.bOverloaded)
	{
		// An overload stop deliberately clears every retained frame. A worker
		// that was already running must not repopulate the prepared queue after
		// that stop, otherwise the UI would report a drained error state while a
		// megabyte-scale body remains retained indefinitely.
		RefreshQueueTelemetry(*Runtime);
		return;
	}
	const bool bNoLoss = Runtime->Config.DeliveryMode == EVirtualPointCloudDeliveryMode::ConnectedNoLoss;
	if (CapturedConfigRevision != Runtime->ConfigRevision)
	{
		RecordSlabFailure(Message.SlabContext.RunId,Message.SensorId,Message.StreamKind,Message.FrameId);
		++Runtime->Status.StaleResultDiscardCount;
		Runtime->Status.Message = TEXT("설정 변경 전에 시작된 Point Cloud 결과를 폐기했습니다.");
		if (bNoLoss)
		{
			TryStartNextPointCloudSerialization(StreamKey, *Runtime);
		}
		else if (Runtime->Config.bEnabled && Runtime->PendingFrame.IsSet())
		{
			FVirtualSensorFrameEnvelope Pending = MoveTemp(Runtime->PendingFrame.GetValue());
			Runtime->PendingFrame.Reset();
			StartPointCloudSerialization(StreamKey, *Runtime, Pending);
		}
		RefreshQueueTelemetry(*Runtime);
		return;
	}
	if (!Error.IsEmpty() || (Message.Json.IsEmpty() && !Message.BinaryBody.IsValid()))
	{
		RecordSlabFailure(Message.SlabContext.RunId,Message.SensorId,Message.StreamKind,Message.FrameId);
		++Runtime->Status.EncodeFailureCount;
		Runtime->Status.Message = Error.IsEmpty() ? TEXT("포인트 클라우드 직렬화 실패") : Error;
		AddLog(StreamKey, TEXT("encode-failed"), Runtime->Status.Message, nullptr, Message.FrameId);
	}
	else
	{
		if (bNoLoss)
		{
			if (Runtime->PreparedMessageQueue.Num() >= Runtime->Config.MaxBufferedFrames)
			{
				StopForPointCloudOverload(StreamKey, *Runtime, TEXT("Binary PCD prepared-body queue reached its limit."), &Message);
				return;
			}
			Runtime->PreparedMessageQueue.Add(MoveTemp(Message));
		}
		else
		{
			if (Runtime->PreparedMessage.IsSet()) ++Runtime->Status.ReplacedPendingFrameCount;
			Runtime->PreparedMessage = MoveTemp(Message);
		}
	}
	if (bNoLoss)
	{
		TryStartNextPointCloudSerialization(StreamKey, *Runtime);
	}
	else if (Runtime->Config.bEnabled && Runtime->PendingFrame.IsSet())
	{
		FVirtualSensorFrameEnvelope Pending = MoveTemp(Runtime->PendingFrame.GetValue());
		Runtime->PendingFrame.Reset();
		StartPointCloudSerialization(StreamKey, *Runtime, Pending);
	}
	RefreshQueueTelemetry(*Runtime);
}

void UVirtualSensorStreamPublisherComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PumpPublisherOnce(FPlatformTime::Seconds());
}

void UVirtualSensorStreamPublisherComponent::PumpPublisherOnce(double Now)
{
	const bool bRawAvailable = CanUseHighThroughputTransport();
	if (!LastRawTransportAvailable.IsSet() || LastRawTransportAvailable.GetValue() != bRawAvailable)
	{
		LastRawTransportAvailable = bRawAvailable;
		UpdateCameraStreamDemand();
	}
	const double RateBytes = FMath::Max(1.0f, BandwidthLimitMegabytesPerSecond) * 1024.0 * 1024.0;
	const double PointCloudRateBytes = FMath::Max(1.0f, PointCloudBandwidthLimitMegabytesPerSecond) * 1024.0 * 1024.0;
	TokenBucketBytes = FMath::Min(RateBytes, TokenBucketBytes + (Now - LastTokenUpdateSeconds) * RateBytes);
	PointCloudTokenBucketBytes = FMath::Min(PointCloudRateBytes, PointCloudTokenBucketBytes + (Now - LastTokenUpdateSeconds) * PointCloudRateBytes);
	LastTokenUpdateSeconds = Now;
	PumpPreparedMessages(Now);
	CheckReceiptTimeouts(Now);
	MergeHighThroughputTelemetry();
	if (!bEndingPlay && bHasPointCloudRecoveryWork) PumpPointCloudOverloadRecovery(Now);
}

bool UVirtualSensorStreamPublisherComponent::CanUseHighThroughputTransport() const
{
	return TransportComponent && TransportComponent->TransportMode == EVirtualSensorTransportMode::StompWebSocket &&
		UVirtualSensorHighThroughputTransportSubsystem::CanUseRawTcp(TransportComponent->GetTransportProfile().BrokerUrl);
}

bool UVirtualSensorStreamPublisherComponent::EnsureHighThroughputTransport(FString& OutError)
{
	OutError.Reset();
	if (!TransportComponent || TransportComponent->TransportMode != EVirtualSensorTransportMode::StompWebSocket || !GetWorld())
	{
		OutError = TEXT("STOMP transport is not active.");
		return false;
	}
	UVirtualSensorHighThroughputTransportSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
	if (!Subsystem)
	{
		OutError = TEXT("High-throughput transport subsystem is unavailable.");
		return false;
	}
	const FVirtualSensorTransportProfile& Profile = TransportComponent->GetTransportProfile();
	if (!UVirtualSensorHighThroughputTransportSubsystem::CanUseRawTcp(Profile.BrokerUrl, &OutError)) return false;
	if (!Subsystem->StartHighThroughputTransport(
		UVirtualSensorHighThroughputTransportSubsystem::MakeProfile(Profile),
		TransportComponent->GetSessionPasscodeForHighThroughput()))
	{
		OutError = TEXT("Failed to start Raw TCP STOMP worker.");
		return false;
	}
	return true;
}

bool UVirtualSensorStreamPublisherComponent::TrySubmitHighThroughput(
	const FPreparedMessage& Message,
	const FStreamRuntime& Runtime,
	FString& OutError)
{
	if ((!Message.BinaryBody.IsValid() && !Message.BinaryBody64.IsValid()) || !EnsureHighThroughputTransport(OutError)) return false;
	UVirtualSensorHighThroughputTransportSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
	FVirtualSensorBinaryFrame Frame;
	Frame.StreamKind = Message.StreamKind;
	Frame.SensorId = Message.SensorId;
	Frame.FrameId = Message.FrameId;
	Frame.TimestampUtc = Message.TimestampUtc;
	if (Frame.TimestampUtc.GetTicks() <= 0 && !Message.BinaryMetadata.TimestampUtc.IsEmpty())
	{
		FDateTime::ParseIso8601(*Message.BinaryMetadata.TimestampUtc, Frame.TimestampUtc);
	}
	if (Frame.TimestampUtc.GetTicks() <= 0) Frame.TimestampUtc = FDateTime::UtcNow();
	Frame.Schema = Message.bBinaryPcd ? Message.BinaryMetadata.Schema : Message.BinarySchema;
	Frame.ContentType = Message.bBinaryPcd ? TEXT("application/vnd.pcd") : Message.BinaryContentType;
	const FString SensorType = Message.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("camera") : TEXT("lidar");
	const FString DataKind = Message.StreamKind == EVirtualSensorStreamKind::PointCloud ? TEXT("pointcloud-stream")
		: Message.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("camera-stream") : TEXT("lidar-stream");
	Frame.Destination = TransportComponent->ResolveDestination(SensorType, DataKind);
	Frame.Body32 = Message.BinaryBody;
	Frame.Body64 = Message.BinaryBody64;
	Frame.Headers = Message.BinaryHeaders;
	const FString Checksum = Message.bBinaryPcd ? Message.BinaryMetadata.ChecksumSha1 : Message.BinaryHeaders.FindRef(TEXT("checksum"));
	Frame.RequestId = FString::Printf(TEXT("%s-%lld-%s"), *Message.SensorId, Message.FrameId, *Checksum);
	if (Message.bBinaryPcd)
	{
		Frame.Headers = FVirtualSensorWireHeaders::RawPcd(Message.BinaryMetadata);
	}
	return Subsystem->EnqueueBinaryFrame(Frame, OutError);
}

void UVirtualSensorStreamPublisherComponent::MergeHighThroughputTelemetry()
{
	if (!GetWorld()) return;
	const UVirtualSensorHighThroughputTransportSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
	if (!Subsystem) return;
	const TArray<FVirtualSensorStreamTelemetry> Telemetry = Subsystem->GetStreamTelemetry();
	MergeHighThroughputTelemetry(Telemetry);
}

void UVirtualSensorStreamPublisherComponent::MergeHighThroughputTelemetry(const TArray<FVirtualSensorStreamTelemetry>& Telemetry)
{
    TMap<FString,FVirtualSensorStreamTelemetry> ByRuntime;
	for(const auto& Item:Telemetry)
    {
        FString Key=MakeStreamKey(Item.StreamKind,Item.SensorId);
        if(!StreamRuntimes.Contains(Key))Key=MakeStreamKey(Item.StreamKind,FString());
        const auto* Runtime=StreamRuntimes.Find(Key);
        if(!Runtime||Runtime->Config.TransportBackend!=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput)continue;
        if(!ByRuntime.Contains(Key)){ByRuntime.Add(Key,Item);continue;}
        auto& Sum=ByRuntime.FindChecked(Key);
        Sum.SubmittedCount+=Item.SubmittedCount;Sum.SubmittedBytes+=Item.SubmittedBytes;
        Sum.ReceiptCount+=Item.ReceiptCount;Sum.ConsumerReceivedCount+=Item.ConsumerReceivedCount;
        Sum.ValidationFailureCount+=Item.ValidationFailureCount;Sum.DeliveryFailureCount+=Item.DeliveryFailureCount;
        Sum.FrameGapCount+=Item.FrameGapCount;Sum.DuplicateCount+=Item.DuplicateCount;
        Sum.InputQueueDepth+=Item.InputQueueDepth;Sum.ReceiptQueueDepth+=Item.ReceiptQueueDepth;
        Sum.OutstandingFrameCount+=Item.OutstandingFrameCount;Sum.OutstandingBytes+=Item.OutstandingBytes;
        // Process-wide bytes are shared, not a per-sensor allocation to sum.
        Sum.GlobalOutstandingBytes=FMath::Max(Sum.GlobalOutstandingBytes,Item.GlobalOutstandingBytes);
        Sum.SubmittedHz+=Item.SubmittedHz;Sum.SubmittedMegabytesPerSecond+=Item.SubmittedMegabytesPerSecond;Sum.ConsumerHz+=Item.ConsumerHz;
        Sum.LastSocketWriteLatencyMs=FMath::Max(Sum.LastSocketWriteLatencyMs,Item.LastSocketWriteLatencyMs);
        Sum.LastReceiptLatencyMs=FMath::Max(Sum.LastReceiptLatencyMs,Item.LastReceiptLatencyMs);
        Sum.LastEndToEndLatencyMs=FMath::Max(Sum.LastEndToEndLatencyMs,Item.LastEndToEndLatencyMs);
        // Conservative worst sensor p95, not a pooled percentile.
        Sum.EndToEndP95LatencyMs=FMath::Max(Sum.EndToEndP95LatencyMs,Item.EndToEndP95LatencyMs);
        if(!Item.LastDeliveryFailureMessage.IsEmpty()){Sum.LastDeliveryFailureMessage=Item.LastDeliveryFailureMessage;Sum.Message=Item.Message;}
    }
	for (const auto& Pair : ByRuntime)
	{
		const auto& Item=Pair.Value;
		FStreamRuntime* Runtime = StreamRuntimes.Find(Pair.Key);
		if (!Runtime || Runtime->Config.TransportBackend != EVirtualSensorStreamTransportBackend::TcpStompHighThroughput) continue;
		Runtime->Status.ActiveTransportBackend = EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;
		Runtime->Status.SubmittedFrameCount = Item.SubmittedCount;
		Runtime->Status.SubmittedHz = Item.SubmittedHz;
		Runtime->Status.SubmittedMegabytesPerSecond = Item.SubmittedMegabytesPerSecond;
		Runtime->Status.ReceiptReceivedCount = Item.ReceiptCount;
		Runtime->Status.ConsumerReceivedCount = Item.ConsumerReceivedCount;
		Runtime->Status.ConsumerReceivedHz = Item.ConsumerHz;
		Runtime->Status.ConsumerValidationFailureCount = Item.ValidationFailureCount;
		Runtime->Status.RawDeliveryFailureCount=Item.DeliveryFailureCount;
		Runtime->Status.LastRawDeliveryFailureMessage=Item.LastDeliveryFailureMessage;
		Runtime->Status.ConsumerFrameGapCount = Item.FrameGapCount;
		Runtime->Status.ConsumerDuplicateCount = Item.DuplicateCount;
		Runtime->Status.InputQueueDepth = Runtime->PendingFrameQueue.Num() + (Runtime->PendingFrame.IsSet() ? 1 : 0) + Item.InputQueueDepth;
		Runtime->Status.PreparedQueueDepth = Runtime->PreparedMessageQueue.Num() + (Runtime->PreparedMessage.IsSet() ? 1 : 0);
		Runtime->Status.ReceiptQueueDepth = Item.ReceiptQueueDepth;
		Runtime->Status.RawOutstandingFrameCount=Item.OutstandingFrameCount;
		Runtime->Status.RawOutstandingBytes=Item.OutstandingBytes;
		Runtime->Status.RawGlobalOutstandingBytes=Item.GlobalOutstandingBytes;
		Runtime->Status.LastSocketWriteLatencyMs = Item.LastSocketWriteLatencyMs;
		Runtime->Status.LastReceiptLatencyMs = Item.LastReceiptLatencyMs;
		Runtime->Status.LastConsumerLatencyMs = Item.LastEndToEndLatencyMs;
		Runtime->Status.EndToEndP95LatencyMs = Item.EndToEndP95LatencyMs;
		if (!Runtime->Status.bOverloaded) Runtime->Status.Message = Item.Message;
	}
}

void UVirtualSensorStreamPublisherComponent::CheckReceiptTimeouts(double NowSeconds)
{
	TArray<FString> TimedOut;
	for (const TPair<FString, FReceiptWait>& Pair : WaitingReceipts)
	{
		const float EffectiveTimeoutSeconds = Pair.Value.Message.bBinaryPcd
			? FMath::Max(ReceiptTimeoutSeconds, BinaryPcdReceiptTimeoutSeconds)
			: ReceiptTimeoutSeconds;
		if (NowSeconds - Pair.Value.SubmittedSeconds >= EffectiveTimeoutSeconds) TimedOut.Add(Pair.Key);
	}
	for (const FString& RequestId : TimedOut)
	{
		const FReceiptWait Wait = WaitingReceipts.FindAndRemoveChecked(RequestId);
		RequestToStreamKey.Remove(RequestId);
		if (FStreamRuntime* Runtime = StreamRuntimes.Find(Wait.StreamKey))
		{
			++Runtime->Status.ReceiptTimeoutCount;
			Runtime->Status.Message = TEXT("Broker receipt 제한 시간 초과");
		}
		if (RequeueReceiptForRetry(Wait, TEXT("Broker receipt timeout"))) continue;
		RecordSlabFailure(Wait.Message.SlabContext.RunId,Wait.Message.SensorId,Wait.Message.StreamKind,Wait.Message.FrameId);
		if (FStreamRuntime* Runtime = StreamRuntimes.Find(Wait.StreamKey)) ++Runtime->Status.DeliveryFailureCount;
		++ConsecutiveReceiptTimeouts;
		AddLog(Wait.StreamKey, TEXT("receipt-timeout"), TEXT("Broker receipt가 5초 안에 도착하지 않았습니다."));
	}
	if (ConsecutiveReceiptTimeouts >= 3 && TransportComponent && NowSeconds - LastReconnectSeconds >= ReconnectCooldownSeconds)
	{
		LastReconnectSeconds = NowSeconds;
		ConsecutiveReceiptTimeouts = 0;
		TransportComponent->RequestStompReconnect();
		AddLog(FString(), TEXT("reconnect"), TEXT("연속 receipt timeout으로 STOMP 재연결을 시작했습니다."));
	}
}

bool UVirtualSensorStreamPublisherComponent::RequeueReceiptForRetry(const FReceiptWait& Wait, const FString& Error)
{
	FStreamRuntime* Runtime = StreamRuntimes.Find(Wait.StreamKey);
	if (!Runtime || !Runtime->Config.bEnabled ||
		Runtime->Config.DeliveryMode != EVirtualPointCloudDeliveryMode::ConnectedNoLoss ||
		!Wait.Message.bBinaryPcd)
	{
		return false;
	}
	if (Wait.Message.RetryAttempt >= Runtime->Config.MaxReceiptRetries)
	{
		RecordSlabFailure(Wait.Message.SlabContext.RunId,Wait.Message.SensorId,Wait.Message.StreamKind,Wait.Message.FrameId);
		StopForPointCloudOverload(Wait.StreamKey, *Runtime,
			FString::Printf(TEXT("Binary PCD receipt retry limit exhausted: %s"), *Error), &Wait.Message);
		return true;
	}
	if (Runtime->PreparedMessageQueue.Num() >= Runtime->Config.MaxBufferedFrames)
	{
		RecordSlabFailure(Wait.Message.SlabContext.RunId,Wait.Message.SensorId,Wait.Message.StreamKind,Wait.Message.FrameId);
		StopForPointCloudOverload(Wait.StreamKey, *Runtime,
			FString::Printf(TEXT("Binary PCD retry queue is full: %s"), *Error), &Wait.Message);
		return true;
	}
	FPreparedMessage RetryMessage = Wait.Message;
	++RetryMessage.RetryAttempt;
	Runtime->PreparedMessageQueue.Insert(MoveTemp(RetryMessage), 0);
	++Runtime->Status.RetryCount;
	Runtime->NextSubmitAttemptSeconds = FPlatformTime::Seconds() + 0.05;
	Runtime->Status.Message = FString::Printf(TEXT("Binary PCD receipt retry %d/%d: %s"),
		Wait.Message.RetryAttempt + 1,
		Runtime->Config.MaxReceiptRetries,
		*Error);
	RefreshQueueTelemetry(*Runtime);
	AddLog(Wait.StreamKey, TEXT("receipt-retry-queued"), Runtime->Status.Message, nullptr, Wait.Message.FrameId);
	return true;
}

void UVirtualSensorStreamPublisherComponent::OnTransportResult(const FVirtualSensorTransportResult& Result)
{
	HandleTransportResult(Result);
}

void UVirtualSensorStreamPublisherComponent::HandleTransportResult(const FVirtualSensorTransportResult& Result)
{
	if (Result.RequestId.IsEmpty()) return;
	const FString* KnownStreamKey = RequestToStreamKey.Find(Result.RequestId);
	if (Result.bConsumerAckReceived)
	{
		if (KnownStreamKey) AddLog(*KnownStreamKey, TEXT("consumer-ack"), Result.Message, &Result);
		RequestToStreamKey.Remove(Result.RequestId);
		return;
	}
	FReceiptWait Wait;
	if (!WaitingReceipts.RemoveAndCopyValue(Result.RequestId, Wait))
	{
		if (KnownStreamKey && Result.bReceiptReceived)
		{
			if (FStreamRuntime* Runtime = StreamRuntimes.Find(*KnownStreamKey)) ++Runtime->Status.DuplicateReceiptCount;
		}
		if (KnownStreamKey && !Result.bSubmitted) AddLog(*KnownStreamKey, TEXT("transport-error"), Result.Message, &Result);
		return;
	}
	if (!Result.bAccepted)
	{
		RequestToStreamKey.Remove(Result.RequestId);
		AddLog(Wait.StreamKey, TEXT("receipt-failed"), Result.Message, &Result, Wait.Message.FrameId);
		if (RequeueReceiptForRetry(Wait, Result.Message)) return;
		RecordSlabFailure(Wait.Message.SlabContext.RunId,Wait.Message.SensorId,Wait.Message.StreamKind,Wait.Message.FrameId);
		if (FStreamRuntime* Runtime = StreamRuntimes.Find(Wait.StreamKey)) ++Runtime->Status.DeliveryFailureCount;
	}
	if (FStreamRuntime* Runtime = StreamRuntimes.Find(Wait.StreamKey))
	{
		Runtime->Status.LastReceiptLatencyMs = Result.LatencyMs;
		Runtime->Status.Message = Result.Message;
		if (Result.bReceiptReceived) ++Runtime->Status.ReceiptReceivedCount;
		RefreshQueueTelemetry(*Runtime);
	}
	ConsecutiveReceiptTimeouts = Result.bAccepted ? 0 : ConsecutiveReceiptTimeouts + 1;
	AddLog(Wait.StreamKey, Result.bAccepted ? TEXT("receipt") : TEXT("receipt-failed"), Result.Message, &Result);
	if (TransportComponent && TransportComponent->GetTransportProfile().AckTopic.IsEmpty()) RequestToStreamKey.Remove(Result.RequestId);
}

void UVirtualSensorStreamPublisherComponent::AddLog(const FString& StreamKey, const FString& State, const FString& Message, const FVirtualSensorTransportResult* Result, int64 FrameId)
{
	FVirtualSensorTransportLogEntry Entry;
	Entry.TimestampUtc = FDateTime::UtcNow();
	Entry.State = State;
	Entry.Message = Message;
	Entry.FrameId = FrameId;
	if (const FStreamRuntime* Runtime = StreamRuntimes.Find(StreamKey))
	{
		Entry.SensorId = Runtime->Status.SensorId;
		Entry.StreamKind = Runtime->Status.StreamKind;
	}
	if (Result)
	{
		Entry.SensorId = Result->SensorId;
		Entry.RequestId = Result->RequestId;
		Entry.Destination = Result->Destination;
		Entry.Protocol = Result->Protocol;
		Entry.Bytes = Result->DataLength;
		Entry.LatencyMs = Result->LatencyMs;
	}
	RecentLogEntries.Insert(MoveTemp(Entry), 0);
	if (RecentLogEntries.Num() > 200) RecentLogEntries.SetNum(200);
}

TArray<FVirtualSensorStreamStatus> UVirtualSensorStreamPublisherComponent::GetStreamStatuses() const
{
	TArray<FVirtualSensorStreamStatus> Result;
	for (const TPair<FString, FStreamRuntime>& Pair : StreamRuntimes) Result.Add(Pair.Value.Status);
	Result.Sort([](const FVirtualSensorStreamStatus& A, const FVirtualSensorStreamStatus& B)
	{
		if (A.StreamKind != B.StreamKind) return static_cast<uint8>(A.StreamKind) < static_cast<uint8>(B.StreamKind);
		return A.SensorId < B.SensorId;
	});
	return Result;
}

bool UVirtualSensorStreamPublisherComponent::ExportDiagnosticReport(FString& OutJsonPath, FString& OutMarkdownPath) const
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), VirtualSensorDefaults::ReportsDirectory);
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	OutJsonPath = FPaths::Combine(Directory, FString::Printf(TEXT("sensor_stream_%s.json"), *Timestamp));
	OutMarkdownPath = FPaths::Combine(Directory, FString::Printf(TEXT("sensor_stream_%s.md"), *Timestamp));
	TArray<TSharedPtr<FJsonValue>> StatusValues;
	FString Markdown = TEXT("# Sensor Stream Diagnostic\n\n| Sensor | Stream | Input | Sent | Replaced | Deferred | Encode fail | Receipt timeout | Message |\n|---|---:|---:|---:|---:|---:|---:|---:|---|\n");
	for (const FVirtualSensorStreamStatus& Status : GetStreamStatuses())
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("sensorId"), Status.SensorId);
		Object->SetNumberField(TEXT("streamKind"), static_cast<int32>(Status.StreamKind));
		Object->SetBoolField(TEXT("enabled"), Status.bEnabled);
		Object->SetBoolField(TEXT("overloadSuspended"), Status.bOverloadSuspended);
		Object->SetNumberField(TEXT("resumeCount"), static_cast<double>(Status.ResumeCount));
		Object->SetNumberField(TEXT("overloadDiscardedFrames"), static_cast<double>(Status.OverloadDiscardedFrameCount));
		Object->SetNumberField(TEXT("overloadResumeDelaySeconds"), Status.OverloadResumeDelaySeconds);
		Object->SetStringField(TEXT("earliestOverloadResumeUtc"), Status.bOverloadSuspended ? Status.EarliestOverloadResumeUtc.ToIso8601() : FString());
		Object->SetNumberField(TEXT("inputFrames"), static_cast<double>(Status.InputFrameCount));
		Object->SetNumberField(TEXT("submittedFrames"), static_cast<double>(Status.SubmittedFrameCount));
		Object->SetNumberField(TEXT("replacedPending"), static_cast<double>(Status.ReplacedPendingFrameCount));
		Object->SetNumberField(TEXT("bandwidthDeferred"), static_cast<double>(Status.BandwidthDeferredFrameCount));
		Object->SetNumberField(TEXT("encodeFailures"), static_cast<double>(Status.EncodeFailureCount));
		Object->SetNumberField(TEXT("bodyLimitRejected"), static_cast<double>(Status.BodyLimitRejectedCount));
		Object->SetNumberField(TEXT("deliveryFailures"), static_cast<double>(Status.DeliveryFailureCount));
		Object->SetNumberField(TEXT("receiptTimeouts"), static_cast<double>(Status.ReceiptTimeoutCount));
		Object->SetNumberField(TEXT("receipts"), static_cast<double>(Status.ReceiptReceivedCount));
		Object->SetStringField(TEXT("message"), Status.Message);
		StatusValues.Add(MakeShared<FJsonValueObject>(Object));
		Markdown += FString::Printf(TEXT("| %s | %d | %lld | %lld | %lld | %lld | %lld | %lld | %s |\n"), *Status.SensorId, static_cast<int32>(Status.StreamKind), Status.InputFrameCount, Status.SubmittedFrameCount, Status.ReplacedPendingFrameCount, Status.BandwidthDeferredFrameCount, Status.EncodeFailureCount, Status.ReceiptTimeoutCount, *Status.Message.Replace(TEXT("|"), TEXT("/")));
	}
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("generatedUtc"), FDateTime::UtcNow().ToIso8601());
	Root->SetNumberField(TEXT("bandwidthLimitMBps"), BandwidthLimitMegabytesPerSecond);
	Root->SetArrayField(TEXT("streams"), StatusValues);
	FString Json;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return FFileHelper::SaveStringToFile(Json, *OutJsonPath) && FFileHelper::SaveStringToFile(Markdown, *OutMarkdownPath);
}

FVirtualSensorResourceSnapshot UVirtualSensorStreamPublisherComponent::GetResourceSnapshot() const
{
    check(IsInGameThread());
    FVirtualSensorResourceSnapshot S; S.Owner=GetName()+TEXT("/publisher");
    int64 Pending=0,Prepared=0,Serializing=0,SharedBytes=0;TSet<const void*> Bodies;
    auto CountBody=[&](const FPreparedMessage& M){
        if(M.BinaryBody.IsValid()&&!Bodies.Contains(M.BinaryBody.Get())){Bodies.Add(M.BinaryBody.Get());SharedBytes+=M.BinaryBody->GetAllocatedSize();}
        if(M.BinaryBody64.IsValid()&&!Bodies.Contains(M.BinaryBody64.Get())){Bodies.Add(M.BinaryBody64.Get());SharedBytes+=M.BinaryBody64->GetAllocatedSize();}};
    for(const auto& P:StreamRuntimes){const auto& R=P.Value;Pending+=R.PendingFrameQueue.Num()+R.PendingFrame.IsSet();Prepared+=R.PreparedMessageQueue.Num()+R.PreparedMessage.IsSet();Serializing+=R.bSerializationInFlight;
        if(R.PreparedMessage.IsSet())CountBody(*R.PreparedMessage);for(const auto& M:R.PreparedMessageQueue)CountBody(M);}
    for(const auto& P:WaitingReceipts)CountBody(P.Value.Message);
    S.Counts={{TEXT("stream_keys"),StreamRuntimes.Num()},{TEXT("pending_frames"),Pending},{TEXT("prepared_messages"),Prepared},
        {TEXT("serializing"),Serializing},{TEXT("waiting_receipts"),WaitingReceipts.Num()},{TEXT("failure_run_ids"),SlabRunFailures.Num()},
        {TEXT("recent_logs"),RecentLogEntries.Num()}};
    S.SharedCpuBytes.Add(TEXT("unique_prepared_bodies"),SharedBytes);
    S.OwnedCpuBytes.Add(TEXT("runtime_containers"),StreamRuntimes.GetAllocatedSize()+WaitingReceipts.GetAllocatedSize()+RequestToStreamKey.GetAllocatedSize());
    return S;
}
bool UVirtualSensorStreamPublisherComponent::NeedsCompatibilityLidarOutput(const FString& Id) const
{
    // Configuration-only compatibility API. Acquisition callers use the context overload.
    for(const auto& Pair:StreamRuntimes){const auto& C=Pair.Value.Config;
        if(!C.bEnabled||C.StreamKind!=EVirtualSensorStreamKind::LidarPayload||(!C.SensorId.IsEmpty()&&C.SensorId!=Id))continue;
        if(C.SensorId.IsEmpty()){const auto* Exact=StreamRuntimes.Find(MakeStreamKey(C.StreamKind,Id));if(Exact&&Exact->Config.bEnabled)continue;}
        if(!(C.TransportBackend==EVirtualSensorStreamTransportBackend::TcpStompHighThroughput&&CanUseHighThroughputTransport()))return true;
    }
    return false;
}

bool UVirtualSensorStreamPublisherComponent::PrepareCollectionTargets(EVirtualSensorStreamKind Kind,const FString& Id,FString& Error)
{
	if(GetWorld())for(TActorIterator<AVirtualSensorActorBase> It(GetWorld());It;++It)
		if((Id.IsEmpty()||It->GetSensorId()==Id)&&StreamMatchesFrame(Kind,It->GetSensorKind())&&
			It->OutputComponent&&It->OutputComponent->StreamPublisherComponent==this&&!It->PrepareForDataCollection(Error))return false;
	return true;
}

bool UVirtualSensorStreamPublisherComponent::HasSensorPlacementWork(const FString& Id,EVirtualSensorKind Kind) const
{
	for(const auto& Pair:StreamRuntimes)
	{
		const auto& R=Pair.Value;if(!StreamMatchesFrame(R.Config.StreamKind,Kind))continue;
		if(R.Config.bEnabled&&(R.Config.SensorId.IsEmpty()||R.Config.SensorId==Id))return true;
		if(R.bSerializationInFlight&&R.SerializationSensorId==Id)return true;
		if(R.PendingFrame.IsSet()&&R.PendingFrame.GetValue().SensorId==Id)return true;
		if(R.PreparedMessage.IsSet()&&R.PreparedMessage.GetValue().SensorId==Id)return true;
		for(const auto& F:R.PendingFrameQueue)if(F.SensorId==Id)return true;
		for(const auto& F:R.PreparedMessageQueue)if(F.SensorId==Id)return true;
	}
	for(const auto& Pair:WaitingReceipts)if(Pair.Value.Message.SensorId==Id)return true;
	return false;
}

int64 UVirtualSensorStreamPublisherComponent::GetPendingSlabRunCount(const FString& RunId) const
{
	if(RunId.IsEmpty()) return 0;
	int64 Count=0;
	for(const auto& Pair:StreamRuntimes)
	{
		const auto& R=Pair.Value;
		if(R.bSerializationInFlight&&R.SerializationRunId==RunId) ++Count;
		if(R.PendingFrame.IsSet()&&R.PendingFrame->SlabContext.RunId==RunId) ++Count;
		if(R.PreparedMessage.IsSet()&&R.PreparedMessage->SlabContext.RunId==RunId) ++Count;
		for(const auto& F:R.PendingFrameQueue) if(F.SlabContext.RunId==RunId) ++Count;
		for(const auto& M:R.PreparedMessageQueue) if(M.SlabContext.RunId==RunId) ++Count;
	}
	for(const auto& Pair:WaitingReceipts) if(Pair.Value.Message.SlabContext.RunId==RunId) ++Count;
	return Count;
}
void UVirtualSensorStreamPublisherComponent::GatherPendingSlabRunKeys(const FString& RunId,TSet<FString>& Outputs,TSet<FString>& SensorFrames,int64* Preparation,int64* Receipts) const
{
	if(RunId.IsEmpty())return;
	TSet<FString> StageKeys;
	auto Add=[&](EVirtualSensorStreamKind Kind,const FString& Sensor,int64 Frame){const auto Key=Sensor+TEXT("|")+LexToString(Frame);SensorFrames.Add(Key);const auto OutputKey=LexToString(static_cast<int32>(Kind))+TEXT("|")+Key;Outputs.Add(OutputKey);StageKeys.Add(OutputKey);};
	for(const auto& Pair:StreamRuntimes)
	{
		const auto& R=Pair.Value;
		if(R.bSerializationInFlight&&R.SerializationRunId==RunId)Add(R.Config.StreamKind,R.SerializationSensorId,R.SerializationFrameId);
		auto Item=[&](const auto& M){if(M.SlabContext.RunId==RunId)Add(R.Config.StreamKind,M.SensorId,M.FrameId);};
		if(R.PendingFrame.IsSet())Item(*R.PendingFrame);if(R.PreparedMessage.IsSet())Item(*R.PreparedMessage);
		for(const auto& M:R.PendingFrameQueue)Item(M);for(const auto& M:R.PreparedMessageQueue)Item(M);
	}
	if(Preparation)*Preparation=StageKeys.Num();StageKeys.Reset();
	for(const auto& Pair:WaitingReceipts){const auto& M=Pair.Value.Message;if(M.SlabContext.RunId==RunId)Add(M.StreamKind,M.SensorId,M.FrameId);}
	if(Receipts)*Receipts=StageKeys.Num();
}
int64 UVirtualSensorStreamPublisherComponent::GetFailedSlabRunCount(const FString& RunId) const
{
	const auto* Failures=SlabRunFailures.Find(RunId); return Failures?Failures->Num():0;
}
void UVirtualSensorStreamPublisherComponent::RecordDiscardedSlabFrames(const FStreamRuntime& R)
{
	if(R.bSerializationInFlight) RecordSlabFailure(R.SerializationRunId,R.SerializationSensorId,R.Config.StreamKind,R.SerializationFrameId);
	auto Record=[this,&R](const auto& Item){ RecordSlabFailure(Item.SlabContext.RunId,Item.SensorId,R.Config.StreamKind,Item.FrameId); };
	if(R.PendingFrame.IsSet()) Record(R.PendingFrame.GetValue());
	if(R.PreparedMessage.IsSet()) Record(R.PreparedMessage.GetValue());
	for(const auto& F:R.PendingFrameQueue) Record(F);
	for(const auto& M:R.PreparedMessageQueue) Record(M);
	for(const auto& Pair:WaitingReceipts) if(Pair.Value.StreamKey==MakeStreamKey(R.Config.StreamKind,R.Config.SensorId)) Record(Pair.Value.Message);
}
