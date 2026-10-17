#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "VirtualPointCloudCoordinates.h"
#include "VirtualSensorWireHeaders.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorAcquisitionContext.h"

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



void UVirtualSensorStreamPublisherComponent::SubmitFrame(const FVirtualSensorFrameEnvelope& Frame)
{
	if (GetWorld()) if (auto* Slab=FVirtualSensorAcquisitionContextProviders::Get(GetWorld()))
		if (!Slab->AllowsAcquisitionFrame(Frame.SensorId,Frame.SlabContext)) return;
	if (bEndingPlay || Frame.SensorId.IsEmpty() || IsLocalCaptureSuspended(Frame.SensorId)) return;
	TArray<FString> Keys;
	StreamRuntimes.GetKeys(Keys);
	for (const FString& Key : Keys)
	{
		FStreamRuntime* Runtime = StreamRuntimes.Find(Key);
		if (!Runtime || !Runtime->Config.bEnabled || !StreamMatchesFrame(Runtime->Config.StreamKind, Frame.SensorKind)) continue;
		if(Frame.bLidarCompatibilityDerived&&(Runtime->Config.StreamKind!=EVirtualSensorStreamKind::LidarPayload||
			(Runtime->Config.TransportBackend==EVirtualSensorStreamTransportBackend::TcpStompHighThroughput&&CanUseHighThroughputTransport())))continue;
		if (!Runtime->Config.SensorId.IsEmpty() && !Runtime->Config.SensorId.Equals(Frame.SensorId, ESearchCase::CaseSensitive)) continue;
		if (GetWorld()) if (const auto* Slab=FVirtualSensorAcquisitionContextProviders::Get(GetWorld()))
			if(!Slab->AllowsOutputFrame(Frame.SensorId,VirtualSensorAcquisitionOutput(Runtime->Config.StreamKind),Frame.SlabContext)) continue;
		if (Runtime->Config.SensorId.IsEmpty())
		{
			const FStreamRuntime* Exact = StreamRuntimes.Find(MakeStreamKey(Runtime->Config.StreamKind, Frame.SensorId));
			if (Exact && (Exact->Config.bEnabled || Exact->Status.bOverloadSuspended)) continue;
		}
		QueueFrameForRuntime(Key, *Runtime, Frame);
	}
}

void UVirtualSensorStreamPublisherComponent::PumpPreparedMessages(double NowSeconds)
{
	if (!TransportComponent || StreamRuntimes.IsEmpty()) return;
	TArray<FString> Keys;
	StreamRuntimes.GetKeys(Keys);
	Keys.Sort();
	int32 SubmittedThisFrame = 0;
    // Keep this traversal stable: advancing RoundRobinCursor after a successful
    // submission must not change the candidates still to visit in this pass.
    const int32 StartingCursor=RoundRobinCursor;
    const int32 SubmissionBudget=FMath::Max(1,MaxSubmissionsPerFrame);
	for (int32 Attempt = 0; Attempt < Keys.Num()*SubmissionBudget && SubmittedThisFrame < SubmissionBudget; ++Attempt)
	{
		const int32 Index = (StartingCursor + Attempt) % Keys.Num();
		FStreamRuntime* Runtime = StreamRuntimes.Find(Keys[Index]);
		if (!Runtime || !Runtime->Config.bEnabled || NowSeconds < Runtime->NextSubmitAttemptSeconds) continue;
		const bool bHighThroughputQueued = Runtime->Config.TransportBackend == EVirtualSensorStreamTransportBackend::TcpStompHighThroughput &&
			!Runtime->PreparedMessageQueue.IsEmpty();
		const bool bNoLoss = bHighThroughputQueued ||
			(Runtime->Config.StreamKind == EVirtualSensorStreamKind::PointCloud &&
			 Runtime->Config.DeliveryMode == EVirtualPointCloudDeliveryMode::ConnectedNoLoss);
		RefreshQueueTelemetry(*Runtime);
		if (bNoLoss && Runtime->Status.ReceiptQueueDepth >= Runtime->Config.MaxBufferedFrames) continue;
		FPreparedMessage* MessagePtr = bNoLoss
			? (Runtime->PreparedMessageQueue.IsEmpty() ? nullptr : &Runtime->PreparedMessageQueue[0])
			: (Runtime->PreparedMessage.IsSet() ? &Runtime->PreparedMessage.GetValue() : nullptr);
		if (!MessagePtr) continue;
		FPreparedMessage& Message = *MessagePtr;
		if (Message.bHighThroughputBinary && !CanUseHighThroughputTransport())
		{
			RecordSlabFailure(Message.SlabContext.RunId,Message.SensorId,Message.StreamKind,Message.FrameId);
			// A broker/backend change invalidates the old binary-only camera/telemetry
			// derivation. Never downgrade WSS to TCP or retry the incompatible body forever.
			++Runtime->Status.StaleResultDiscardCount;
			AddLog(Keys[Index], TEXT("backend-changed"), TEXT("서버 방식 변경 전의 파생 프레임 폐기; 다음 프레임은 호환 Payload 사용"), nullptr, Message.FrameId);
			if (bNoLoss) Runtime->PreparedMessageQueue.RemoveAt(0, 1, false); else Runtime->PreparedMessage.Reset();
			RefreshQueueTelemetry(*Runtime);
			continue;
		}
		if (GetWorld()&&Message.StreamKind!=EVirtualSensorStreamKind::Imu) if (auto* Slab=FVirtualSensorAcquisitionContextProviders::Get(GetWorld()))
		{
			if (!Slab->AllowsOutputFrame(Message.SensorId,VirtualSensorAcquisitionOutput(Message.StreamKind),Message.SlabContext))
			{
				++Runtime->Status.StaleResultDiscardCount;
				if (bNoLoss) Runtime->PreparedMessageQueue.RemoveAt(0,1,false); else Runtime->PreparedMessage.Reset();
				TryStartNextPointCloudSerialization(Keys[Index],*Runtime);
				RefreshQueueTelemetry(*Runtime);
				continue;
			}
		}
		FString BodyError;
		if (!ValidateBinaryBodySize(Message.ByteCount, TransportComponent->GetTransportProfile().MaxMessageBytes, BodyError))
		{
			StopForBodyLimit(Keys[Index], *Runtime, BodyError, Message.FrameId);
			continue;
		}
		double& ActiveTokenBucket = Message.bBinaryPcd ? PointCloudTokenBucketBytes : TokenBucketBytes;
		if (Message.ByteCount > ActiveTokenBucket)
		{
			++Runtime->Status.BandwidthDeferredFrameCount;
			continue;
		}
		const bool bReceipt = Message.bBinaryPcd || ((Runtime->Status.SubmittedFrameCount + 1) % Runtime->Config.ReceiptSampleInterval) == 0;
		const FString SensorType = Message.StreamKind==EVirtualSensorStreamKind::Imu?TEXT("imu"):Message.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("camera") : TEXT("lidar");
		const FString DataKind = Message.StreamKind == EVirtualSensorStreamKind::PointCloud ? TEXT("pointcloud-stream")
			: Message.StreamKind==EVirtualSensorStreamKind::Imu?TEXT("imu-stream"):Message.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("camera-stream") : TEXT("lidar-stream");
		const bool bUseHighThroughput = Runtime->Config.TransportBackend == EVirtualSensorStreamTransportBackend::TcpStompHighThroughput &&
			CanUseHighThroughputTransport() &&
			(Message.bBinaryPcd || Message.bHighThroughputBinary);
		if (bUseHighThroughput)
		{
			FString SubmitError;
			if (!TrySubmitHighThroughput(Message, *Runtime, SubmitError))
			{
				Runtime->NextSubmitAttemptSeconds = NowSeconds + 0.5;
				Runtime->Status.Message = SubmitError;
				Runtime->Status.ActiveTransportBackend = EVirtualSensorStreamTransportBackend::EngineStompCompatibility;
				AddLog(Keys[Index], TEXT("raw-tcp-submit-failed"), SubmitError, nullptr, Message.FrameId);
				break;
			}

			Runtime->NextSubmitAttemptSeconds = 0.0;
			ActiveTokenBucket -= Message.ByteCount;
			Runtime->Status.ActiveTransportBackend = EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;
			Runtime->Status.LastSubmittedFrameId = Message.FrameId;
			Runtime->Status.LastFrameBytes = Message.ByteCount;
			Runtime->Status.LastPointCount = Message.BinaryMetadata.PointCount;
			Runtime->Status.LastSourcePointCount = Message.BinaryMetadata.SourcePointCount;
			Runtime->Status.Message = TEXT("Raw TCP worker queue accepted Binary PCD.");
			Runtime->PreparedMessageQueue.RemoveAt(0, 1, false);
			TryStartNextPointCloudSerialization(Keys[Index], *Runtime);
			RefreshQueueTelemetry(*Runtime);
			++SubmittedThisFrame;
			RoundRobinCursor = (Index + 1) % Keys.Num();
			continue;
		}
		const FVirtualSensorTransportResult Result = Message.bBinaryPcd
			? TransportComponent->SendStompBinaryStreamRequest(*Message.BinaryBody, Message.BinaryMetadata)
			: TransportComponent->SendJsonStreamRequest(Message.SensorId, SensorType, DataKind, Message.FrameId, Message.Json, bReceipt, Message.BinaryHeaders);
		Runtime->Status.LastRequestId = Result.RequestId;
		Runtime->Status.Destination = Result.Destination;
		Runtime->Status.Message = Result.Message;
		if (Result.bSubmitted)
		{
			Runtime->NextSubmitAttemptSeconds = 0.0;
			ActiveTokenBucket -= Message.ByteCount;
			if (Message.RetryAttempt == 0) ++Runtime->Status.SubmittedFrameCount;
			Runtime->Status.LastSubmittedFrameId = Message.FrameId;
			if (Message.RetryAttempt == 0) Runtime->Status.TotalSubmittedBytes += Message.ByteCount;
			Runtime->Status.LastFrameBytes = Message.ByteCount;
			if (Message.bBinaryPcd)
			{
				Runtime->Status.LastPointCount = Message.BinaryMetadata.PointCount;
				Runtime->Status.LastSourcePointCount = Message.BinaryMetadata.SourcePointCount;
			}
			if (Runtime->FirstSubmitSeconds <= 0.0) Runtime->FirstSubmitSeconds = NowSeconds;
			Runtime->Status.SubmittedHz = static_cast<float>(Runtime->Status.SubmittedFrameCount / FMath::Max(0.001, NowSeconds - Runtime->FirstSubmitSeconds));
			Runtime->Status.SubmittedMegabytesPerSecond = static_cast<float>(Runtime->Status.TotalSubmittedBytes /
				(FMath::Max(0.001, NowSeconds - Runtime->FirstSubmitSeconds) * 1024.0 * 1024.0));
			if (Runtime->Config.PointCloudFormat == EVirtualPointCloudStreamFormat::LAZ) Runtime->LastLazSubmitSeconds = NowSeconds;
			if (bReceipt && !Result.RequestId.IsEmpty()) WaitingReceipts.Add(Result.RequestId, {Keys[Index], NowSeconds, Message});
			if (!Result.RequestId.IsEmpty())
			{
				RequestToStreamKey.Add(Result.RequestId, Keys[Index]);
				if (RequestToStreamKey.Num() > 512)
				{
					for (auto It = RequestToStreamKey.CreateIterator(); It && RequestToStreamKey.Num() > 384; ++It) It.RemoveCurrent();
				}
			}
			AddLog(Keys[Index], bReceipt ? TEXT("submitted-receipt") : TEXT("submitted"), Result.Message, &Result, Message.FrameId);
			if (bNoLoss)
			{
				Runtime->PreparedMessageQueue.RemoveAt(0, 1, false);
				TryStartNextPointCloudSerialization(Keys[Index], *Runtime);
			}
			else
			{
				Runtime->PreparedMessage.Reset();
			}
			RefreshQueueTelemetry(*Runtime);
			++SubmittedThisFrame;
			RoundRobinCursor = (Index + 1) % Keys.Num();
		}
		else
		{
			// Do not retry a disconnected broker on every game frame. The latest
			// prepared message remains bounded to one item while acquisition continues.
			Runtime->NextSubmitAttemptSeconds = NowSeconds + 0.5;
			AddLog(Keys[Index], TEXT("submit-failed"), Result.Message, &Result, Message.FrameId);
			break;
		}
	}
}

void UVirtualSensorStreamPublisherComponent::UpdateCameraStreamDemand()
{
	if (AVirtualSensorCoordinator* Coordinator = Cast<AVirtualSensorCoordinator>(GetOwner()))
	{
		for (AVirtualSensorActorBase* SensorActor : Coordinator->GetSensorActors())
		{
			if (!SensorActor || SensorActor->GetSensorKind() != EVirtualSensorKind::Camera) continue;
			if (UVirtualCameraCaptureComponent* Camera = SensorActor->FindComponentByClass<UVirtualCameraCaptureComponent>())
			{
				const FStreamRuntime* Global = StreamRuntimes.Find(MakeStreamKey(EVirtualSensorStreamKind::CameraImage, FString()));
				const FStreamRuntime* Exact = StreamRuntimes.Find(MakeStreamKey(EVirtualSensorStreamKind::CameraImage, SensorActor->GetSensorId()));
				const FStreamRuntime* Active = Exact && Exact->Config.bEnabled ? Exact : (Global && Global->Config.bEnabled ? Global : nullptr);
				if(IsLocalCaptureSuspended(SensorActor->GetSensorId()))Active=nullptr;
			if(GetWorld()) if(const auto* Slab=FVirtualSensorAcquisitionContextProviders::Get(GetWorld()))
					if(!Slab->AllowsOutputDemand(SensorActor->GetSensorId(),VirtualSensorAcquisitionOutput(EVirtualSensorStreamKind::CameraImage))) Active=nullptr;
				Camera->SetRuntimeStreamOutputDemand(
					Active != nullptr,
					Active && Active->Config.TransportBackend == EVirtualSensorStreamTransportBackend::TcpStompHighThroughput && CanUseHighThroughputTransport());
			}
		}
	}
}
bool UVirtualSensorStreamPublisherComponent::NeedsCompatibilityLidarOutput(const FString& Id,const FVirtualSlabFrameContext& Context) const
{
    return NeedsCompatibilityLidarOutputForContext(Id,static_cast<FVirtualSensorAcquisitionContext>(Context));
}
bool UVirtualSensorStreamPublisherComponent::NeedsCompatibilityLidarOutputForContext(const FString& Id,const FVirtualSensorAcquisitionContext& Context) const
{
    if(IsLocalCaptureSuspended(Id))return false;
    const auto* Slab=GetWorld()?FVirtualSensorAcquisitionContextProviders::Get(GetWorld()):nullptr;
    if(Slab&&!Slab->AllowsAcquisitionFrame(Id,Context))return false;
    for(const auto& Pair:StreamRuntimes){const auto& C=Pair.Value.Config;
        if(C.bEnabled&&C.StreamKind==EVirtualSensorStreamKind::LidarPayload&&(C.SensorId.IsEmpty()||C.SensorId==Id)&&
            !(C.TransportBackend==EVirtualSensorStreamTransportBackend::TcpStompHighThroughput&&CanUseHighThroughputTransport()))
        {
            if(C.SensorId.IsEmpty()){const auto* Exact=StreamRuntimes.Find(MakeStreamKey(C.StreamKind,Id));if(Exact&&Exact->Config.bEnabled)continue;}
            if(!Slab||Slab->AllowsOutputFrame(Id,VirtualSensorAcquisitionOutput(C.StreamKind),Context))return true;
        }}
    return false;
}
void UVirtualSensorStreamPublisherComponent::RecordSlabFailure(const FString& RunId,const FString& SensorId,EVirtualSensorStreamKind Kind,int64 FrameId,const FString& Reason)
{
	if(RunId.IsEmpty()) return;
	if(auto* Provider=FVirtualSensorAcquisitionContextProviders::Get(GetWorld()))Provider->RecordAcquisitionOutputFailure(RunId,SensorId,VirtualSensorAcquisitionOutput(Kind),FrameId,Reason);
	if(!SlabRunFailures.Contains(RunId)&&SlabRunFailures.Num()>=32)
		for(auto It=SlabRunFailures.CreateIterator();It;++It) if(It.Key()!=RunId) { It.RemoveCurrent(); break; }
	SlabRunFailures.FindOrAdd(RunId).Add(SensorId+TEXT("|")+LexToString(static_cast<uint8>(Kind))+TEXT("|")+LexToString(FrameId));
}
