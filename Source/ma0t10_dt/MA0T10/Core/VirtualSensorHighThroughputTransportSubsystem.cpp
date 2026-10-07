#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"

#include "ma0t10_dt/MA0T10/Core/VirtualSensorStompProtocol.h"
#include "VirtualSensorWireHeaders.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualCameraStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualLidarStreamReceiverTC.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "IPAddress.h"
#include "Dom/JsonObject.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
enum class EWorkerEventType : uint8
{
	Connected,
	Disconnected,
	Submitted,
	Receipt,
	Consumed,
	ValidationFailed,
	Retry,
	Overload,
	DeliveryFailed
};

struct FWorkerEvent
{
	TSharedPtr<FVirtualSensorTopicReceivedDataBase> ReceivedData;
	EWorkerEventType Type = EWorkerEventType::Disconnected;
	EVirtualSensorStreamKind StreamKind = EVirtualSensorStreamKind::LidarPayload;
	FString SensorId;
	int64 FrameId = 0;
	int32 Bytes = 0;
	int32 QueueDepth = 0;
	int32 ReceiptDepth = 0;
	int64 GapDelta = 0;
	int64 DuplicateDelta = 0;
	float LatencyMs = 0.0f;
	FString RequestId;
	FString RunId;
	FDateTime AcquisitionUtc;
	FDateTime ObservedUtc;
	double ObservedSeconds=0;
	FString Message;
};

struct FPendingReceipt
{
	FVirtualSensorBinaryFrame Frame;
	double SubmittedSeconds = 0.0;
	int32 RetryAttempt = 0;
};

FString TelemetryKey(EVirtualSensorStreamKind Kind, const FString& SensorId)
{
	return FString::Printf(TEXT("%d|%s"), static_cast<int32>(Kind), *SensorId);
}

void AppendUtf8(TArray<uint8>& Out, const FString& Text)
{
	FTCHARToUTF8 Utf8(*Text);
	Out.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

FString HeaderValue(const TMap<FString, FString>& Headers, const TCHAR* Key)
{
	if (const FString* Exact = Headers.Find(Key)) return *Exact;
	for (const TPair<FString, FString>& Pair : Headers)
	{
		if (Pair.Key.Equals(Key, ESearchCase::IgnoreCase)) return Pair.Value;
	}
	return FString();
}

FString Sha1Hex(const uint8* Data, int64 NumBytes)
{
	if (!Data || NumBytes < 0 || NumBytes > MAX_uint32) return FString();
	uint8 Hash[FSHA1::DigestSize];
	FSHA1::HashBuffer(Data, static_cast<uint32>(NumBytes), Hash);
	return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
}

bool ParseEndpoint(const FString& Url, FString& OutHost, int32& OutPort, FString& OutError)
{
	FString Value = Url.TrimStartAndEnd();
	if (Value.StartsWith(TEXT("wss://"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("wss:// requires the Engine STOMP compatibility backend.");
		return false;
	}
	Value.RemoveFromStart(TEXT("tcp://"), ESearchCase::IgnoreCase);
	Value.RemoveFromStart(TEXT("ws://"), ESearchCase::IgnoreCase);
	int32 Slash = INDEX_NONE;
	if (Value.FindChar(TEXT('/'), Slash)) Value = Value.Left(Slash);
	int32 Colon = INDEX_NONE;
	if (Value.FindLastChar(TEXT(':'), Colon))
	{
		OutHost = Value.Left(Colon);
		if (!LexTryParseString(OutPort, *Value.Mid(Colon + 1)))
		{
			OutError = TEXT("Broker port is invalid.");
			return false;
		}
	}
	else
	{
		OutHost = Value;
		OutPort = 61616;
	}
	if (OutHost.Equals(TEXT("localhost"), ESearchCase::IgnoreCase)) OutHost = TEXT("127.0.0.1");
	if (OutHost.IsEmpty() || OutPort <= 0 || OutPort > 65535)
	{
		OutError = TEXT("Broker endpoint is invalid.");
		return false;
	}
	return true;
}
}

class FVirtualSensorHighThroughputTransportWorker final : public FRunnable
{
public:
	FVirtualSensorHighThroughputTransportWorker(
		const FVirtualSensorHighThroughputProfile& InProfile,
		FString InPasscode)
		: Profile(InProfile)
		, Passcode(InPasscode.IsEmpty() ? InProfile.UserName : MoveTemp(InPasscode))
	{
		for (TAtomic<int32>& Count : QueueCounts) Count.Store(0);
	}

	~FVirtualSensorHighThroughputTransportWorker() override
	{
		StopWorker();
	}

	bool Start()
	{
		if (Thread) return true;
		bStopRequested.Store(false);
		Thread = FRunnableThread::Create(this, TEXT("VirtualSensorRawTcpStomp"), 0, TPri_AboveNormal);
		return Thread != nullptr;
	}

	void StopWorker()
	{
		bStopRequested.Store(true);
		if (Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}
		CloseSocket();
		FVirtualSensorBinaryFrame Abandoned;
		while (DequeueFrame(Abandoned))
		{
			--QueueCounts[static_cast<int32>(Abandoned.StreamKind)];
			FailFrame(Abandoned,TEXT("Transport stopped before socket submission."));
		}
	}

	void Stop() override
	{
		bStopRequested.Store(true);
	}

	uint32 Run() override
	{
		while (!bStopRequested.Load())
		{
			FVirtualSensorReceiveSelection NextSelection;
			bool bChanged=false;
			while(ReceiverChanges.Dequeue(NextSelection)) { ReceiveSelection=MoveTemp(NextSelection); bChanged=true; }
			if(bChanged && bConnected) SyncSubscriptions();
			ProcessRunCancellations();
			CollectRetiredRuns();
			if (!bConnected)
			{
				const double Now = FPlatformTime::Seconds();
				if (Now >= NextConnectSeconds)
				{
					if (!Connect())
					{
						NextConnectSeconds = Now + FMath::Min(10.0, FMath::Pow(2.0, FMath::Min(4, ReconnectAttempt++)));
					}
				}
				FPlatformProcess::SleepNoStats(0.01f);
				continue;
			}

			PumpReceive();
			PumpSend();
			CheckReceiptTimeouts();
			SendHeartbeatIfDue();
			FPlatformProcess::SleepNoStats(0.001f);
		}
		CloseSocket();
		return 0;
	}

	bool Enqueue(const FVirtualSensorBinaryFrame& Frame, FString& OutError)
	{
		const int32 Index = static_cast<int32>(Frame.StreamKind);
		if (Index < 0 || Index >= UE_ARRAY_COUNT(QueueCounts) || !Frame.GetData() || Frame.NumBytes() <= 0)
		{
			OutError = TEXT("Binary frame is empty or has an invalid stream kind.");
			return false;
		}
		const int32 Limit = Frame.StreamKind == EVirtualSensorStreamKind::CameraImage ? 8 : 20;
		FVirtualSensorBinaryFrame Normalized=Frame;
		{
			FScopeLock Lock(&RunPendingMutex);
			const FString Run=HeaderValue(Frame.Headers,TEXT("x-run-uuid"));
			if(const auto* Token=KnownRunGenerations.Find(Run))
			{
				if(!ActiveRunGenerations.Contains(Run)||(Frame.ManagedRunGeneration&&Frame.ManagedRunGeneration!=*Token))
				{OutError=TEXT("종료되었거나 이전 generation인 실행 프레임입니다.");return false;}
				Normalized.ManagedRunGeneration=*Token;
			}
		}
		if (Normalized.RequestId.IsEmpty()) Normalized.RequestId=FString::Printf(TEXT("%d-%s-%lld-%s"),Index,*Frame.SensorId,Frame.FrameId,*HeaderValue(Frame.Headers,TEXT("checksum")));
		bool Accepted=false;
		{
			FScopeLock Lock(&OutstandingMutex);
			const FString Key=TelemetryKey(Frame.StreamKind,Frame.SensorId);
			if (!bStopRequested.Load() && QueueCounts[Index].Load()<Limit && OutstandingCounts.FindRef(Key)<Limit &&
				Frame.NumBytes()<=Profile.MaxOutstandingBytes && HeldBytes<=Profile.MaxOutstandingBytes-Frame.NumBytes() &&
				Frame.NumBytes()<=Profile.MaxOutstandingBytesPerStream && HeldBytesPerStream.FindRef(Key)<=Profile.MaxOutstandingBytesPerStream-Frame.NumBytes() && !OutstandingFrames.Contains(Normalized.RequestId))
			{
				OutstandingFrames.Add(Normalized.RequestId,{Key,Frame.NumBytes(),true,false,HeaderValue(Frame.Headers,TEXT("x-run-uuid")),Frame.FrameId});
				++OutstandingCounts.FindOrAdd(Key); HeldBytes+=Frame.NumBytes(); HeldBytesPerStream.FindOrAdd(Key)+=Frame.NumBytes(); ++QueueCounts[Index]; Accepted=true;
			}
		}
		if (!Accepted)
		{
			OutError = FString::Printf(TEXT("High-throughput %s outstanding frame/byte limit reached (frames=%d, global bytes=%lld), or duplicate request ID."),*Frame.SensorId,Limit,Profile.MaxOutstandingBytes);
			AdjustRunPending(Frame,0,true);
			FWorkerEvent Event;
			Event.Type = EWorkerEventType::Overload;
			Event.RunId=HeaderValue(Frame.Headers,TEXT("x-run-uuid"));Event.RequestId=Normalized.RequestId;
			Event.StreamKind = Frame.StreamKind;
			Event.SensorId = Frame.SensorId;
			Event.FrameId = Frame.FrameId;
			Event.QueueDepth = QueueCounts[Index].Load();
			Event.Message = OutError;
			QueueEvent(MoveTemp(Event));
			return false;
		}
		AdjustRunPending(Frame,1);
		Frames.Enqueue(MoveTemp(Normalized));
		return true;
	}
	void SnapshotOutstanding(EVirtualSensorStreamKind Kind,const FString& Sensor,int32& FramesOut,int64& BytesOut,int64& GlobalOut,int32& QueuedOut) const
	{
		FScopeLock Lock(&OutstandingMutex); FramesOut=0; BytesOut=0; QueuedOut=0; GlobalOut=HeldBytes;
		const FString Key=TelemetryKey(Kind,Sensor);
		for(const auto& Pair:OutstandingFrames) if(Pair.Value.Key==Key) { ++FramesOut; BytesOut+=Pair.Value.Bytes; QueuedOut+=Pair.Value.bQueued?1:0; }
	}
	void CancelRun(const FString& RunId)
	{
		{FScopeLock Lock(&RunPendingMutex);if(!KnownRunGenerations.Contains(RunId))KnownRunGenerations.Add(RunId,0);ActiveRunGenerations.Remove(RunId);RetiringRuns.Add(RunId);}
		RunCancellations.Enqueue(RunId);
	}
	void RegisterRun(const FString& RunId,uint64 Token,bool Active)
	{
		FScopeLock Lock(&RunPendingMutex);KnownRunGenerations.Add(RunId,Token);
		if(Active)ActiveRunGenerations.Add(RunId,Token);
	}
	void RetireRun(const FString& RunId)
	{
		FScopeLock Lock(&RunPendingMutex);ActiveRunGenerations.Remove(RunId);RetiringRuns.Add(RunId);
	}
	FVirtualSensorRunRetentionSnapshot SnapshotRetention(const FString& RunId) const
	{
		FVirtualSensorRunRetentionSnapshot S;
		{FScopeLock Lock(&RunPendingMutex);S.ActiveRuns=ActiveRunGenerations.Num();S.RetiringRuns=RetiringRuns.Num();S.CompletedRuns=CompletedRunOrder.Num();S.KnownRunIds=KnownRunGenerations.Num();S.SequenceEntries=ConsumerSequenceIds.Num();S.LateRunEvents=LateRunEvents;
		 for(const auto& P:RunInvalidFrames)S.InvalidFrameEntries+=P.Value.Num();
		 if(RunId.IsEmpty()){for(const auto& P:RunEventCounts)S.PendingEvents+=P.Value;}else S.PendingEvents=RunEventCounts.FindRef(RunId);}
		{FScopeLock Lock(&OutstandingMutex);for(const auto& P:OutstandingFrames)if(RunId.IsEmpty()||P.Value.RunId==RunId)
		 {S.OutstandingBytes+=P.Value.Bytes;if(P.Value.bQueued)++S.QueuedFrames;else if(P.Value.bSubmitted)++S.ReceiptWaitingFrames;else ++S.SocketWritingFrames;}}
		return S;
	}
	void GatherPendingKeys(const FString& RunId,TSet<FString>& Outputs,TSet<FString>& SensorFrames) const
	{
		FScopeLock Lock(&OutstandingMutex);
		for(const auto& P:OutstandingFrames)if(P.Value.RunId==RunId)
		{Outputs.Add(P.Value.Key+TEXT("|")+LexToString(P.Value.FrameId));int32 Separator;if(P.Value.Key.FindChar(TEXT('|'),Separator))SensorFrames.Add(P.Value.Key.Mid(Separator+1)+TEXT("|")+LexToString(P.Value.FrameId));}
	}
	int32 GetRunPending(const FString& RunId) const { FScopeLock Lock(&RunPendingMutex); return RunPendingCounts.FindRef(RunId); }
	int32 GetRunFailures(const FString& RunId) const { FScopeLock Lock(&RunPendingMutex); return RunFailureCounts.FindRef(RunId); }
	void AdjustRunPending(const FVirtualSensorBinaryFrame& Frame,int32 Delta,bool bFailed = false)
	{
		const FString RunId=HeaderValue(Frame.Headers,TEXT("x-run-uuid"));
		if (RunId.IsEmpty()) return;
		FScopeLock Lock(&RunPendingMutex);
		if (bFailed) ++RunFailureCounts.FindOrAdd(RunId);
		int32& Count=RunPendingCounts.FindOrAdd(RunId); Count=FMath::Max(0,Count+Delta);
		if (Count==0) RunPendingCounts.Remove(RunId);
	}

	bool DequeueEvent(FWorkerEvent& OutEvent)
	{
		if(!Events.Dequeue(OutEvent)) return false;
		if(OutEvent.ReceivedData) --PendingReceiveEvents;
		if(!OutEvent.RunId.IsEmpty()) {FScopeLock Lock(&RunPendingMutex);auto* Count=RunEventCounts.Find(OutEvent.RunId);if(Count&&--*Count<=0)RunEventCounts.Remove(OutEvent.RunId);}
		return true;
	}
	void ConfigureReceiver(const FVirtualSensorReceiveSelection& S) { ReceiverChanges.Enqueue(S); }
	int64 GetDroppedReceiveEvents() const { return DroppedReceiveEvents.Load(); }

	bool IsRunning() const { return Thread != nullptr && !bStopRequested.Load(); }

private:
	friend class FSensorRawTerminalLedgerTest;
	friend class FSensorRawOutstandingLimitTest;
	friend class FSensorRawReceiptProbeTest;
	friend class FSensorRawRunRetentionTest;
	struct FOutstanding { FString Key; int64 Bytes=0; bool bQueued=true; bool bSubmitted=false; FString RunId; int64 FrameId=0; };
	mutable FCriticalSection OutstandingMutex;
	TMap<FString,FOutstanding> OutstandingFrames;
	TMap<FString,int32> OutstandingCounts;
	TMap<FString,int64> HeldBytesPerStream;
	int64 HeldBytes=0;
	void QueueEvent(FWorkerEvent&& Event)
	{
		if(!Event.RunId.IsEmpty()){FScopeLock Lock(&RunPendingMutex);++RunEventCounts.FindOrAdd(Event.RunId);}
		Events.Enqueue(MoveTemp(Event));
	}
	bool IsClosedManagedFrame(const FVirtualSensorBinaryFrame& Frame) const
	{
		if(!Frame.ManagedRunGeneration)return false;
		FScopeLock Lock(&RunPendingMutex);
		return ActiveRunGenerations.FindRef(HeaderValue(Frame.Headers,TEXT("x-run-uuid")))!=Frame.ManagedRunGeneration;
	}
	void ProcessRunCancellations()
	{
		FString Run;
		while(RunCancellations.Dequeue(Run))
		{
			CancelledRuns.Add(Run);
			for(auto It=PendingReceipts.CreateIterator();It;++It)if(HeaderValue(It.Value().Frame.Headers,TEXT("x-run-uuid"))==Run)
			{ReleaseOutstanding(It.Value().Frame);AdjustRunPending(It.Value().Frame,-1);It.RemoveCurrent();}
			TArray<FVirtualSensorBinaryFrame> Keep;FVirtualSensorBinaryFrame Frame;
			while(DequeueFrame(Frame))
			{
				if(HeaderValue(Frame.Headers,TEXT("x-run-uuid"))==Run){--QueueCounts[static_cast<int32>(Frame.StreamKind)];ReleaseOutstanding(Frame);AdjustRunPending(Frame,-1);}
				else Keep.Add(MoveTemp(Frame));
			}
			DeferredFrames=MoveTemp(Keep); // Older unrelated frames stay ahead of concurrent enqueue.
		}
	}
	void CollectRetiredRuns()
	{
		FScopeLock Lock(&RunPendingMutex);
		for(auto It=RetiringRuns.CreateIterator();It;++It)
		{
			const auto& Run=*It;
			if(RunPendingCounts.FindRef(Run)||RunEventCounts.FindRef(Run))continue;
			CancelledRuns.Remove(Run);RunInvalidFrames.Remove(Run);RunPendingCounts.Remove(Run);
			for(auto Sequence=ConsumerSequenceIds.CreateIterator();Sequence;++Sequence)if(Sequence.Value().RunId==Run)Sequence.RemoveCurrent();
			if(!CompletedRunOrder.Contains(Run))CompletedRunOrder.Add(Run);It.RemoveCurrent();
		}
		while(CompletedRunOrder.Num()>20){RunFailureCounts.Remove(CompletedRunOrder[0]);CompletedRunOrder.RemoveAt(0);}
	}
	void ReleaseOutstanding(const FVirtualSensorBinaryFrame& Frame)
	{
		FScopeLock Lock(&OutstandingMutex); FOutstanding Removed;
		if(OutstandingFrames.RemoveAndCopyValue(Frame.RequestId,Removed))
		{ HeldBytes-=Removed.Bytes; HeldBytesPerStream.FindOrAdd(Removed.Key)-=Removed.Bytes; int32& Count=OutstandingCounts.FindOrAdd(Removed.Key); if(--Count<=0){OutstandingCounts.Remove(Removed.Key);HeldBytesPerStream.Remove(Removed.Key);} }
	}
	void FailFrame(const FVirtualSensorBinaryFrame& Frame, const FString& Reason)
	{
		ReleaseOutstanding(Frame);
		// Record failure atomically with the decrement; the game thread must not
		// observe zero pending and declare success before it drains the event queue.
		AdjustRunPending(Frame, -1, true);
		FWorkerEvent Event;
		Event.Type = EWorkerEventType::DeliveryFailed;
		Event.RunId=HeaderValue(Frame.Headers,TEXT("x-run-uuid"));Event.RequestId=Frame.RequestId;
		Event.StreamKind = Frame.StreamKind;
		Event.SensorId = Frame.SensorId;
		Event.FrameId = Frame.FrameId;
		Event.Message = Reason;
		QueueEvent(MoveTemp(Event));
	}
	bool Connect()
	{
		CloseSocket();
		FString Host;
		int32 Port = 0;
		FString Error;
		if (!ParseEndpoint(Profile.BrokerUrl, Host, Port, Error))
		{
			PushConnectionEvent(false, Error);
			return false;
		}
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!SocketSubsystem)
		{
			PushConnectionEvent(false, TEXT("Socket subsystem is unavailable."));
			return false;
		}
		TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
		bool bIpValid = false;
		Address->SetIp(*Host, bIpValid);
		Address->SetPort(Port);
		if (!bIpValid)
		{
			PushConnectionEvent(false, TEXT("Raw TCP backend currently requires an IPv4 broker address."));
			return false;
		}
		Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("VirtualSensorRawStomp"), Address->GetProtocolType());
		if (!Socket)
		{
			PushConnectionEvent(false, TEXT("Failed to create STOMP TCP socket."));
			return false;
		}
		Socket->SetNoDelay(true);
		Socket->SetSendBufferSize(16 * 1024 * 1024, SendBufferBytes);
		Socket->SetReceiveBufferSize(16 * 1024 * 1024, ReceiveBufferBytes);
		Socket->SetNonBlocking(true);
		if (!Socket->Connect(*Address))
		{
			PushConnectionEvent(false, FString::Printf(TEXT("Raw TCP connect failed: %s:%d"), *Host, Port));
			CloseSocket();
			return false;
		}
		Socket->SetNonBlocking(true);

		FString ConnectFrame = FString::Printf(
			TEXT("CONNECT\naccept-version:1.2\nhost:%s\nlogin:%s\npasscode:%s\nheart-beat:%d,%d\n\n"),
			*FVirtualSensorStompParser::EscapeHeader(Host),
			*FVirtualSensorStompParser::EscapeHeader(Profile.UserName),
			*FVirtualSensorStompParser::EscapeHeader(Passcode),
			Profile.HeartbeatIntervalMs,
			Profile.HeartbeatIntervalMs);
		TArray<uint8> Bytes;
		AppendUtf8(Bytes, ConnectFrame);
		Bytes.Add(0);
		if (!SendAll(Bytes.GetData(), Bytes.Num(), 5.0))
		{
			PushConnectionEvent(false, TEXT("Failed to send STOMP CONNECT."));
			CloseSocket();
			return false;
		}

		Parser.Reset();
		const double Deadline = FPlatformTime::Seconds() + FMath::Clamp(Profile.ConnectTimeoutSeconds, 1, 120);
		while (!bStopRequested.Load() && FPlatformTime::Seconds() < Deadline)
		{
			ProcessRunCancellations();CollectRetiredRuns();
			TArray<FVirtualSensorStompFrame> Parsed;
			if (!ReceiveFrames(Parsed, Error))
			{
				PushConnectionEvent(false, Error);
				CloseSocket();
				return false;
			}
			for (FVirtualSensorStompFrame& Frame : Parsed)
			{
				if (Frame.Command == TEXT("CONNECTED"))
				{
					bConnected = true;
					ReconnectAttempt = 0;
					LastSocketActivitySeconds = FPlatformTime::Seconds();
					SubscribeAll();
					PushConnectionEvent(true, TEXT("Raw TCP STOMP connected; binary loopback subscriptions active."));
					return true;
				}
				if (Frame.Command == TEXT("ERROR"))
				{
					PushConnectionEvent(false, TEXT("Broker rejected STOMP CONNECT."));
					CloseSocket();
					return false;
				}
			}
			FPlatformProcess::SleepNoStats(0.005f);
		}
		PushConnectionEvent(false, TEXT("Timed out waiting for STOMP CONNECTED."));
		CloseSocket();
		return false;
	}

	void SubscribeAll()
	{
		SyncSubscriptions();
	}

	void SendSubscribe(const FString& Id, const FString& Destination)
	{
		const FString Text = FString::Printf(
			TEXT("SUBSCRIBE\nid:%s\ndestination:%s\nack:auto\nsubscription-type:MULTICAST\n\n"),
			*FVirtualSensorStompParser::EscapeHeader(Id),
			*FVirtualSensorStompParser::EscapeHeader(Destination));
		TArray<uint8> Bytes;
		AppendUtf8(Bytes, Text);
		Bytes.Add(0);
		SendAll(Bytes.GetData(), Bytes.Num(), 2.0);
	}

	void SyncSubscriptions()
	{
		const FString Topics[]={Profile.LidarTopic,Profile.CameraTopic,Profile.PointCloudTopic};
		for(int32 Kind=0;Kind<3;++Kind)
		{
			const FString Id=TEXT("ma0t10-shared-receiver-")+LexToString(Kind);
			if(ReceiveSelection.WantsKind(Kind) && !SubscribedKinds.Contains(Kind))
			{ SendSubscribe(Id,Topics[Kind]); SubscribedKinds.Add(Kind); }
			else if(!ReceiveSelection.WantsKind(Kind) && SubscribedKinds.Contains(Kind))
			{
				TArray<uint8> B; AppendUtf8(B,TEXT("UNSUBSCRIBE\nid:")+Id+TEXT("\n\n")); B.Add(0); SendAll(B.GetData(),B.Num(),2.0); SubscribedKinds.Remove(Kind);
			}
		}
	}

	void PumpSend()
	{
		int32 SentThisPass = 0;
		FVirtualSensorBinaryFrame Frame;
		while (SentThisPass < 8 && DequeueFrame(Frame))
		{
			const int32 Index = static_cast<int32>(Frame.StreamKind);
			--QueueCounts[Index];
			if (IsClosedManagedFrame(Frame)||CancelledRuns.Contains(HeaderValue(Frame.Headers,TEXT("x-run-uuid")))) { ReleaseOutstanding(Frame); AdjustRunPending(Frame,-1); continue; }
			{ FScopeLock Lock(&OutstandingMutex); if(auto* Held=OutstandingFrames.Find(Frame.RequestId)) Held->bQueued=false; }
			if (!SendFrame(Frame, 0))
			{
				FailFrame(Frame, TEXT("Socket write failed; frame has no remaining delivery path."));
				HandleDisconnect(TEXT("Socket write failed; acquisition remains active and stream will reconnect."));
				return;
			}
			++SentThisPass;
		}
	}

	bool SendFrame(const FVirtualSensorBinaryFrame& Frame, int32 RetryAttempt)
	{
		const FString RequestId = Frame.RequestId.IsEmpty()
			? FString::Printf(TEXT("%s-%lld-%s"), *Frame.SensorId, Frame.FrameId, *HeaderValue(Frame.Headers, TEXT("checksum")))
			: Frame.RequestId;
		FString Header = TEXT("SEND\n");
		for (const auto& Pair : FVirtualSensorWireHeaders::RawFrame(Frame))
		{
			Header += FVirtualSensorStompParser::EscapeHeader(Pair.Key) + TEXT(":") + FVirtualSensorStompParser::EscapeHeader(Pair.Value) + TEXT("\n");
		}
		Header += TEXT("\n");
		TArray<uint8> HeaderBytes;
		AppendUtf8(HeaderBytes, Header);
		const uint8 Terminator = 0;
		const double Started = FPlatformTime::Seconds();
		if (!SendAll(HeaderBytes.GetData(), HeaderBytes.Num(), 2.0) ||
			!SendAll(Frame.GetData(), Frame.NumBytes(), 5.0) ||
			!SendAll(&Terminator, 1, 2.0))
		{
			return false;
		}
		LastSocketActivitySeconds = FPlatformTime::Seconds();
		FPendingReceipt Pending;
		Pending.Frame = Frame;
		Pending.SubmittedSeconds = LastSocketActivitySeconds;
		Pending.RetryAttempt = RetryAttempt;
		PendingReceipts.Add(RequestId, MoveTemp(Pending));
		{FScopeLock Lock(&OutstandingMutex);if(auto* Held=OutstandingFrames.Find(Frame.RequestId))Held->bSubmitted=true;}

		FWorkerEvent Event;
		Event.Type = EWorkerEventType::Submitted;
		Event.StreamKind = Frame.StreamKind;
		Event.SensorId = Frame.SensorId;
		Event.FrameId = Frame.FrameId;
		Event.Bytes = static_cast<int32>(FMath::Min<int64>(MAX_int32, Frame.NumBytes()));
		Event.RequestId=RequestId;Event.RunId=HeaderValue(Frame.Headers,TEXT("x-run-uuid"));Event.AcquisitionUtc=Frame.TimestampUtc;
		Event.ObservedUtc=FDateTime::UtcNow();Event.ObservedSeconds=LastSocketActivitySeconds;
		Event.QueueDepth = QueueCounts[static_cast<int32>(Frame.StreamKind)].Load();
		Event.ReceiptDepth = CountPendingReceipts(Frame.StreamKind);
		Event.LatencyMs = static_cast<float>((LastSocketActivitySeconds - Started) * 1000.0);
		Event.Message = TEXT("Raw TCP STOMP frame submitted; waiting for broker receipt.");
		QueueEvent(MoveTemp(Event));
		return true;
	}

	bool SendAll(const uint8* Data, int64 NumBytes, double TimeoutSeconds)
	{
		if (!Socket || !Data || NumBytes < 0) return false;
		int64 Offset = 0;
		const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
		while (Offset < NumBytes && !bStopRequested.Load() && FPlatformTime::Seconds() < Deadline)
		{
			// During connection setup there is no in-progress sensor STOMP frame.
			// Cancel queued run bodies promptly, without disrupting an active SEND.
			if(!bConnected)ProcessRunCancellations();
			int32 Sent = 0;
			const int32 Chunk = static_cast<int32>(FMath::Min<int64>(NumBytes - Offset, 1024 * 1024));
			if (Socket->Send(Data + Offset, Chunk, Sent) && Sent > 0)
			{
				Offset += Sent;
				continue;
			}
			const ESocketErrors Error = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode();
			if (Error != SE_EWOULDBLOCK && Error != SE_NO_ERROR) return false;
			FPlatformProcess::SleepNoStats(0.0005f);
		}
		return Offset == NumBytes;
	}

	void PumpReceive()
	{
		TArray<FVirtualSensorStompFrame> FramesReceived;
		FString Error;
		if (!ReceiveFrames(FramesReceived, Error))
		{
			HandleDisconnect(Error);
			return;
		}
		for (const FVirtualSensorStompFrame& Frame : FramesReceived) HandleFrame(Frame);
	}

	bool ReceiveFrames(TArray<FVirtualSensorStompFrame>& OutFrames, FString& OutError)
	{
		if (!Socket) return false;
		uint32 PendingSize = 0;
		while (Socket->HasPendingData(PendingSize) && PendingSize > 0)
		{
			TArray<uint8> Chunk;
			Chunk.SetNumUninitialized(FMath::Min<uint32>(PendingSize, 1024 * 1024));
			int32 Read = 0;
			if (!Socket->Recv(Chunk.GetData(), Chunk.Num(), Read) || Read <= 0)
			{
				const ESocketErrors Error = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode();
				if (Error == SE_EWOULDBLOCK || Error == SE_NO_ERROR) return true;
				OutError = TEXT("Raw TCP STOMP receive failed.");
				return false;
			}
			LastSocketActivitySeconds = FPlatformTime::Seconds();
			if (!Parser.Append(Chunk.GetData(), Read, OutFrames, OutError)) return false;
		}
		return true;
	}

	void HandleFrame(const FVirtualSensorStompFrame& Frame)
	{
		if (Frame.Command == TEXT("RECEIPT"))
		{
			const FString ReceiptId = HeaderValue(Frame.Headers, TEXT("receipt-id"));
			if (FPendingReceipt* Pending = PendingReceipts.Find(ReceiptId))
			{
				FWorkerEvent Event;
				Event.Type = EWorkerEventType::Receipt;
				Event.StreamKind = Pending->Frame.StreamKind;
				Event.SensorId = Pending->Frame.SensorId;
				Event.FrameId = Pending->Frame.FrameId;
				Event.RequestId=ReceiptId;Event.RunId=HeaderValue(Pending->Frame.Headers,TEXT("x-run-uuid"));Event.AcquisitionUtc=Pending->Frame.TimestampUtc;
				Event.ObservedUtc=FDateTime::UtcNow();Event.ObservedSeconds=FPlatformTime::Seconds();
				Event.Bytes = static_cast<int32>(FMath::Min<int64>(MAX_int32, Pending->Frame.NumBytes()));
				Event.ReceiptDepth = FMath::Max(0, CountPendingReceipts(Pending->Frame.StreamKind) - 1);
				Event.LatencyMs = static_cast<float>((FPlatformTime::Seconds() - Pending->SubmittedSeconds) * 1000.0);
				Event.Message = TEXT("Broker receipt received.");
				QueueEvent(MoveTemp(Event));
				AdjustRunPending(Pending->Frame,-1);
				ReleaseOutstanding(Pending->Frame);
				PendingReceipts.Remove(ReceiptId);
			}
			return;
		}
		if (Frame.Command == TEXT("MESSAGE"))
		{
			ValidateConsumedFrame(Frame);
			return;
		}
		if (Frame.Command == TEXT("ERROR"))
		{
			HandleDisconnect(TEXT("Artemis returned a STOMP ERROR frame."));
		}
	}

	void ValidateConsumedFrame(const FVirtualSensorStompFrame& Frame)
	{
		const FString Schema=HeaderValue(Frame.Headers,TEXT("schema"));
		EVirtualSensorStreamKind Kind=EVirtualSensorStreamKind::LidarPayload;
		if(Schema==TEXT("virtual-camera.jpeg.v1")) Kind=EVirtualSensorStreamKind::CameraImage;
		else if(Schema==TEXT("virtual-pointcloud.pcd.v1")) Kind=EVirtualSensorStreamKind::PointCloud;
		else if(Schema!=TEXT("virtual-lidar.telemetry.v1")) return;
		if(!ReceiveSelection.WantsKind(static_cast<int32>(Kind))) return;

		FString SensorId=HeaderValue(Frame.Headers,TEXT("sensor-id"));
		if(SensorId.IsEmpty()) SensorId=HeaderValue(Frame.Headers,TEXT("x-sensor-id"));
		int64 FrameId=-1; FString IdText=HeaderValue(Frame.Headers,TEXT("frame-id"));
		if(IdText.IsEmpty()) IdText=HeaderValue(Frame.Headers,TEXT("x-frame-id"));
		LexTryParseString(FrameId,*IdText);
		TSharedPtr<FVirtualSensorTopicReceivedDataBase> Data;
		if(Kind==EVirtualSensorStreamKind::PointCloud) Data=MakeShared<FVirtualPointCloudStreamReceiverData>();
		else if(Kind==EVirtualSensorStreamKind::CameraImage) Data=MakeShared<FVirtualCameraStreamReceiverData>();
		else Data=MakeShared<FVirtualLidarStreamReceiverData>();
		Data->Kind=static_cast<EVirtualSensorTopicReceiveKind>(Kind);
		Data->SensorId=SensorId; Data->FrameId=FrameId; Data->SchemaVersion=Schema;
		Data->bFiltered=!ReceiveSelection.Accepts(static_cast<int32>(Kind),SensorId);
		const double ParseStarted=FPlatformTime::Seconds();
		if(!Data->bFiltered && Frame.Body.Num()>ReceiveSelection.MaxMessageBytes)
		{
			Data->ErrorCode=TEXT("receive-size-limit"); Data->Message=TEXT("수신 메시지 크기 한도 초과");
		}
		else if(!Data->bFiltered && Kind==EVirtualSensorStreamKind::PointCloud)
		{
			TMap<FName,FString> Headers;
			for(const auto& P:Frame.Headers) Headers.Add(FName(*P.Key),P.Value);
			Data=StaticCastSharedPtr<FVirtualSensorTopicReceivedDataBase>(UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(Frame.Body,Headers));
			SensorId=Data->SensorId; FrameId=Data->FrameId;
		}
		else if(!Data->bFiltered)
		{
			// Existing lightweight high-throughput validation is retained for non-PCD
			// streams. The PCD path above is the authoritative full body validator.
			Data->bValid=FrameId>=0&&!SensorId.IsEmpty()&&!Frame.Body.IsEmpty();
			if(Data->bValid && Kind==EVirtualSensorStreamKind::CameraImage)
				Data->bValid=Frame.Body.Num()>=4&&Frame.Body[0]==0xff&&Frame.Body[1]==0xd8&&Frame.Body[Frame.Body.Num()-2]==0xff&&Frame.Body.Last()==0xd9;
			else if(Data->bValid)
			{
				FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Frame.Body.GetData()),Frame.Body.Num());
				TSharedPtr<FJsonObject> Json; FString ParsedSchema;
				Data->bValid=FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Text.Length(),Text.Get())),Json)&&Json.IsValid()&&
					Json->TryGetStringField(TEXT("schema"),ParsedSchema)&&ParsedSchema==Schema;
			}
			const FString Checksum=HeaderValue(Frame.Headers,TEXT("checksum"));
			if(Data->bValid&&!Checksum.IsEmpty()) Data->bValid=Sha1Hex(Frame.Body.GetData(),Frame.Body.Num()).Equals(Checksum,ESearchCase::IgnoreCase);
			Data->Message=Data->bValid?TEXT("Raw MESSAGE 기본 형식·checksum 검증 정상"):TEXT("Raw MESSAGE 형식/checksum 오류");
		}
		Data->Topic=HeaderValue(Frame.Headers,TEXT("destination")); Data->Backend=TEXT("Raw TCP 공유 수신");
		Data->RequestId=HeaderValue(Frame.Headers,TEXT("request-id")); Data->MessageBytes=Frame.Body.Num();
		Data->ReceiveGeneration=ReceiveSelection.Generation;
		if(Data->RunId.IsEmpty()) Data->RunId=HeaderValue(Frame.Headers,TEXT("x-run-uuid"));
		if(Data->bFiltered) Data->Message=TEXT("활성 송신 SensorId가 아닌 메시지: 검증 대상 외");
		FDateTime SourceUtc=Data->SourceTimestampUtc;
		if(SourceUtc.GetTicks()==0) FDateTime::ParseIso8601(*HeaderValue(Frame.Headers,TEXT("timestamp-utc")),SourceUtc);
		Data->SourceTimestampUtc=SourceUtc;
		FWorkerEvent Event;
		Event.Type=Data->bValid?EWorkerEventType::Consumed:EWorkerEventType::ValidationFailed;
		Event.StreamKind=Kind; Event.SensorId=SensorId; Event.FrameId=FrameId; Event.Bytes=Frame.Body.Num();
		Event.RequestId=Data->RequestId;Event.RunId=Data->RunId;Event.AcquisitionUtc=SourceUtc;
		Event.ObservedUtc=FDateTime::UtcNow();Event.ObservedSeconds=FPlatformTime::Seconds();
		Event.Message=Data->Message;
		Event.LatencyMs=SourceUtc.GetTicks()>0?static_cast<float>(FMath::Max(0.0,(FDateTime::UtcNow()-SourceUtc).GetTotalMilliseconds())):0;
		const FString SequenceKey=SensorId+TEXT("|")+LexToString(static_cast<int32>(Kind))+TEXT("|")+Data->RunId+TEXT("|")+
			HeaderValue(Frame.Headers,TEXT("x-session-segment"))+TEXT("|")+LexToString(ReceiveSelection.Generation);
		{
			FScopeLock Lock(&RunPendingMutex);
			const bool Closed=KnownRunGenerations.Contains(Data->RunId)&&!ActiveRunGenerations.Contains(Data->RunId);
			if(Closed)++LateRunEvents;
			else if(Data->bValid)
			{
				const auto* Previous=ConsumerSequenceIds.Find(SequenceKey);
				if(Previous){if(FrameId<=Previous->FrameId)Event.DuplicateDelta=1;else if(FrameId>Previous->FrameId+1)Event.GapDelta=FrameId-Previous->FrameId-1;}
				if(!Previous||FrameId>Previous->FrameId)ConsumerSequenceIds.Add(SequenceKey,{FrameId,Data->RunId,++SequenceOrdinal});
				if(ConsumerSequenceIds.Num()>256)
				{FString Oldest;uint64 Order=MAX_uint64;for(const auto& P:ConsumerSequenceIds)if(P.Value.Order<Order){Order=P.Value.Order;Oldest=P.Key;}ConsumerSequenceIds.Remove(Oldest);}
			}
		}
		Data->FrameGapDelta=Event.GapDelta; Data->bDuplicate=Event.DuplicateDelta>0;
		if(!Data->bFiltered&&!Data->bValid&&!Data->RunId.IsEmpty())
		{
			FScopeLock Lock(&RunPendingMutex);
			const FString Key=SensorId+TEXT("|")+LexToString(static_cast<uint8>(Kind))+TEXT("|")+LexToString(FrameId);
			if(ActiveRunGenerations.Contains(Data->RunId))
			{TSet<FString>& Invalid=RunInvalidFrames.FindOrAdd(Data->RunId);if(!Invalid.Contains(Key)){Invalid.Add(Key);++RunFailureCounts.FindOrAdd(Data->RunId);}}
		}
		Data->ParseLatencyMs=static_cast<float>((FPlatformTime::Seconds()-ParseStarted)*1000.0);
		if(PendingReceiveEvents.Load()>=256) { ++DroppedReceiveEvents; return; }
		++PendingReceiveEvents; Event.ReceivedData=MoveTemp(Data);
		QueueEvent(MoveTemp(Event));
	}

	void CheckReceiptTimeouts()
	{
		const double Now = FPlatformTime::Seconds();
		TArray<FString> TimedOut;
		for (const TPair<FString, FPendingReceipt>& Pair : PendingReceipts)
		{
			if (Now - Pair.Value.SubmittedSeconds >= 5.0) TimedOut.Add(Pair.Key);
		}
		for (const FString& Id : TimedOut)
		{
			FPendingReceipt Pending;
			if (!PendingReceipts.RemoveAndCopyValue(Id, Pending)) continue;
			if (Pending.RetryAttempt >= 3)
			{
				FailFrame(Pending.Frame, TEXT("Broker receipt retry limit was exhausted."));
				continue;
			}
			FWorkerEvent RetryEvent;
			RetryEvent.Type = EWorkerEventType::Retry;
			RetryEvent.StreamKind = Pending.Frame.StreamKind;
			RetryEvent.SensorId = Pending.Frame.SensorId;
			RetryEvent.FrameId = Pending.Frame.FrameId;
			RetryEvent.Message = TEXT("Broker receipt timed out; retrying the idempotent frame.");
			QueueEvent(MoveTemp(RetryEvent));
			if (!SendFrame(Pending.Frame, Pending.RetryAttempt + 1))
			{
				FailFrame(Pending.Frame, TEXT("Receipt retry socket write failed."));
				HandleDisconnect(TEXT("Receipt retry socket write failed."));
				return;
			}
		}
	}

	void SendHeartbeatIfDue()
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastSocketActivitySeconds < Profile.HeartbeatIntervalMs / 1000.0) return;
		const uint8 Heartbeat = '\n';
		if (!SendAll(&Heartbeat, 1, 1.0)) HandleDisconnect(TEXT("STOMP heartbeat failed."));
		else LastSocketActivitySeconds = Now;
	}

	void HandleDisconnect(const FString& Reason)
	{
		PushConnectionEvent(false, Reason);
		CloseSocket();
		NextConnectSeconds = FPlatformTime::Seconds() + 1.0;
	}

	void PushConnectionEvent(bool bNowConnected, const FString& Message)
	{
		FWorkerEvent Event;
		Event.Type = bNowConnected ? EWorkerEventType::Connected : EWorkerEventType::Disconnected;
		Event.Message = Message;
		QueueEvent(MoveTemp(Event));
	}

	void CloseSocket()
	{
		bConnected = false;
		SubscribedKinds.Reset();
		for (const auto& Pair : PendingReceipts)
			FailFrame(Pair.Value.Frame, TEXT("Connection closed before broker receipt; delivery outcome is unconfirmed."));
		PendingReceipts.Reset();
		Parser.Reset();
		if (Socket)
		{
			Socket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
			Socket = nullptr;
		}
	}

	int32 CountPendingReceipts(EVirtualSensorStreamKind Kind) const
	{
		int32 Count = 0;
		for (const TPair<FString, FPendingReceipt>& Pair : PendingReceipts)
		{
			if (Pair.Value.Frame.StreamKind == Kind) ++Count;
		}
		return Count;
	}

	FVirtualSensorHighThroughputProfile Profile;
	FVirtualSensorReceiveSelection ReceiveSelection;
	TQueue<FVirtualSensorReceiveSelection,EQueueMode::Mpsc> ReceiverChanges;
	TSet<int32> SubscribedKinds;
	TAtomic<int32> PendingReceiveEvents{0};
	TAtomic<int64> DroppedReceiveEvents{0};
	TQueue<FString,EQueueMode::Mpsc> RunCancellations;
	TSet<FString> CancelledRuns;
	struct FConsumerSequence {int64 FrameId=0;FString RunId;uint64 Order=0;};
	TMap<FString,FConsumerSequence> ConsumerSequenceIds;
	uint64 SequenceOrdinal=0;
	mutable FCriticalSection RunPendingMutex;
	TMap<FString,uint64> KnownRunGenerations,ActiveRunGenerations;
	TSet<FString> RetiringRuns;
	TArray<FString> CompletedRunOrder;
	TMap<FString,int32> RunEventCounts;
	int64 LateRunEvents=0;
	TMap<FString,int32> RunPendingCounts;
	TMap<FString,int32> RunFailureCounts;
	TMap<FString,TSet<FString>> RunInvalidFrames;
	FString Passcode;
	FRunnableThread* Thread = nullptr;
	FSocket* Socket = nullptr;
	TAtomic<bool> bStopRequested { false };
	bool bConnected = false;
	int32 ReconnectAttempt = 0;
	int32 SendBufferBytes = 0;
	int32 ReceiveBufferBytes = 0;
	double NextConnectSeconds = 0.0;
	double LastSocketActivitySeconds = 0.0;
	FVirtualSensorStompParser Parser;
	TQueue<FVirtualSensorBinaryFrame, EQueueMode::Mpsc> Frames;
	TArray<FVirtualSensorBinaryFrame> DeferredFrames;
	bool DequeueFrame(FVirtualSensorBinaryFrame& Frame)
	{
		if(DeferredFrames.IsEmpty())return Frames.Dequeue(Frame);
		Frame=MoveTemp(DeferredFrames[0]);DeferredFrames.RemoveAt(0,1,false);return true;
	}
	TQueue<FWorkerEvent, EQueueMode::Mpsc> Events;
	TAtomic<int32> QueueCounts[3];
	TMap<FString, FPendingReceipt> PendingReceipts;
	int64 LastConsumerFrameIds[3] = { 0, 0, 0 };
};

UVirtualSensorHighThroughputTransportSubsystem::UVirtualSensorHighThroughputTransportSubsystem() = default;

UVirtualSensorHighThroughputTransportSubsystem::~UVirtualSensorHighThroughputTransportSubsystem() = default;

void UVirtualSensorHighThroughputTransportSubsystem::Deinitialize()
{
	StopHighThroughputTransport();
	Super::Deinitialize();
}

void UVirtualSensorHighThroughputTransportSubsystem::Tick(float DeltaTime)
{
	DrainWorkerEvents();
}

TStatId UVirtualSensorHighThroughputTransportSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVirtualSensorHighThroughputTransportSubsystem, STATGROUP_Tickables);
}

bool UVirtualSensorHighThroughputTransportSubsystem::StartHighThroughputTransport(
	const FVirtualSensorHighThroughputProfile& Profile,
	const FString& SessionPasscode)
{
	FString Reason;
	if (!CanUseRawTcp(Profile.BrokerUrl, &Reason)) return false;
	const bool bSameProfile = Worker != nullptr &&
		ActiveProfile.BrokerUrl == Profile.BrokerUrl &&
		ActiveProfile.UserName == Profile.UserName &&
		ActiveProfile.CameraTopic == Profile.CameraTopic &&
		ActiveProfile.LidarTopic == Profile.LidarTopic &&
		ActiveProfile.PointCloudTopic == Profile.PointCloudTopic &&
		ActiveProfile.MaxOutstandingBytes == Profile.MaxOutstandingBytes &&
		ActiveProfile.MaxOutstandingBytesPerStream == Profile.MaxOutstandingBytesPerStream &&
		ActivePasscode == SessionPasscode;
	if (bSameProfile && Worker->IsRunning()) return true;
	StopHighThroughputTransport();
	ActiveProfile = Profile;
	ActivePasscode = SessionPasscode;
	Worker = new FVirtualSensorHighThroughputTransportWorker(Profile, SessionPasscode);
	Worker->ConfigureReceiver(ReceiverSelection);
	++WorkerInstanceSerial;
	for(const auto& Run:KnownManagedRuns)Worker->RegisterRun(Run.Key,Run.Value,ActiveManagedRuns.Contains(Run.Key));
	if (!Worker->Start())
	{
		delete Worker;
		Worker = nullptr;
		return false;
	}
	return true;
}

void UVirtualSensorHighThroughputTransportSubsystem::StopHighThroughputTransport()
{
	bTransportConnected=false;++ConnectionRevision;
	if (Worker)
	{
		Worker->StopWorker();
		delete Worker;
		Worker = nullptr;
	}
}

bool UVirtualSensorHighThroughputTransportSubsystem::PrepareHighThroughputTransport(const FVirtualSensorHighThroughputProfile& Profile,const FString& Passcode,FString& Error,bool bAllowIdleReplacement)
{
	Error.Reset();
	bool Held=false;for(const auto& T:GetStreamTelemetry())Held|=T.OutstandingFrameCount>0;
	if(Worker && Worker->IsRunning() && (ActiveProfile.BrokerUrl!=Profile.BrokerUrl || ActiveProfile.UserName!=Profile.UserName ||
		ActiveProfile.CameraTopic!=Profile.CameraTopic || ActiveProfile.LidarTopic!=Profile.LidarTopic || ActiveProfile.PointCloudTopic!=Profile.PointCloudTopic ||
		ActiveProfile.MaxOutstandingBytes!=Profile.MaxOutstandingBytes || ActiveProfile.MaxOutstandingBytesPerStream!=Profile.MaxOutstandingBytesPerStream || ActivePasscode!=Passcode)&&(!bAllowIdleReplacement||Held))
	{ Error=TEXT("공유 Raw TCP 연결의 설정이 다릅니다. 기존 스트림을 변경하지 않고 준비를 거절했습니다.");return false; }
	if(!StartHighThroughputTransport(Profile,Passcode)){Error=TEXT("Raw TCP 연결 준비 요청 실패");return false;}
	return true;
}

void UVirtualSensorHighThroughputTransportSubsystem::ConfigureReceiver(const FVirtualSensorReceiveSelection& Selection)
{
	ReceiverSelection=Selection;
	if(Worker) Worker->ConfigureReceiver(Selection);
}
int64 UVirtualSensorHighThroughputTransportSubsystem::GetDroppedReceiveEvents() const { return Worker?Worker->GetDroppedReceiveEvents():0; }

bool UVirtualSensorHighThroughputTransportSubsystem::IsHighThroughputTransportRunning() const
{
	return Worker != nullptr && Worker->IsRunning();
}

bool UVirtualSensorHighThroughputTransportSubsystem::EnqueueBinaryFrame(
	const FVirtualSensorBinaryFrame& Frame,
	FString& OutError)
{
	const FString RunId=HeaderValue(Frame.Headers,TEXT("x-run-uuid"));
	if(KnownManagedRuns.Contains(RunId)&&!ActiveManagedRuns.Contains(RunId))
	{OutError=TEXT("종료된 실행의 프레임은 접수할 수 없습니다.");return false;}
	if (!Worker || !Worker->IsRunning())
	{
		OutError = TEXT("High-throughput transport is not running.");
		return false;
	}
	FVirtualSensorStreamTelemetry& Telemetry = TelemetryByKey.FindOrAdd(TelemetryKey(Frame.StreamKind, Frame.SensorId));
	Telemetry.StreamKind = Frame.StreamKind;
	Telemetry.SensorId = Frame.SensorId;
	if (!Worker->Enqueue(Frame, OutError))
	{
	// Overload is counted once when the worker event is drained.
		Telemetry.State = TEXT("overload");
		Telemetry.Message = OutError;
		return false;
	}
	++Telemetry.EnqueuedCount;
	if(OnTransportObservation.IsBound())
	{
		FVirtualSensorTransportObservation O;
		O.Kind=Frame.StreamKind;O.SensorId=Frame.SensorId;O.FrameId=Frame.FrameId;O.RequestId=Frame.RequestId;O.RunId=HeaderValue(Frame.Headers,TEXT("x-run-uuid"));
		O.AcquisitionUtc=Frame.TimestampUtc;O.ObservedUtc=FDateTime::UtcNow();O.MonotonicSeconds=FPlatformTime::Seconds();
		O.bClockValid=O.AcquisitionUtc.GetTicks()>0&&O.ObservedUtc>=O.AcquisitionUtc;
		OnTransportObservation.Broadcast(O);
	}
	return true;
}

TArray<FVirtualSensorStreamTelemetry> UVirtualSensorHighThroughputTransportSubsystem::GetStreamTelemetry() const
{
	TArray<FVirtualSensorStreamTelemetry> Result;
	TelemetryByKey.GenerateValueArray(Result);
	for(auto& Item:Result)
	{
		if(Worker)
		{
			int32 Queued=0;
			Worker->SnapshotOutstanding(Item.StreamKind,Item.SensorId,Item.OutstandingFrameCount,Item.OutstandingBytes,Item.GlobalOutstandingBytes,Queued);
			Item.InputQueueDepth=Queued; Item.ReceiptQueueDepth=Item.OutstandingFrameCount-Queued;
		}
		else { Item.OutstandingFrameCount=0; Item.OutstandingBytes=0; Item.GlobalOutstandingBytes=0; Item.InputQueueDepth=0; Item.ReceiptQueueDepth=0; }
	}
	return Result;
}

void UVirtualSensorHighThroughputTransportSubsystem::CancelRun(const FString& RunId)
{
	if(RunId.IsEmpty())return;
	ActiveManagedRuns.Remove(RunId);
	if(!RunId.IsEmpty()&&!KnownManagedRuns.Contains(RunId))KnownManagedRuns.Add(RunId,++NextManagedRunGeneration);
	if (Worker) Worker->CancelRun(RunId);
}

bool UVirtualSensorHighThroughputTransportSubsystem::RegisterManagedRun(const FString& RunId)
{
	if(!IsInGameThread()||RunId.IsEmpty())return false;
	if(KnownManagedRuns.Contains(RunId))return ActiveManagedRuns.Contains(RunId);
	const uint64 Generation=++NextManagedRunGeneration;KnownManagedRuns.Add(RunId,Generation);ActiveManagedRuns.Add(RunId,Generation);
	if(Worker)Worker->RegisterRun(RunId,Generation,true);return true;
}
void UVirtualSensorHighThroughputTransportSubsystem::RetireManagedRun(const FString& RunId)
{
	if(RunId.IsEmpty())return;
	ActiveManagedRuns.Remove(RunId);if(Worker)Worker->RetireRun(RunId);
}
FVirtualSensorRunRetentionSnapshot UVirtualSensorHighThroughputTransportSubsystem::GetRunRetentionSnapshot(const FString& RunId) const
{
	auto S=Worker?Worker->SnapshotRetention(RunId):FVirtualSensorRunRetentionSnapshot();
	S.KnownRunIds=KnownManagedRuns.Num();return S;
}
void UVirtualSensorHighThroughputTransportSubsystem::FlushRunObservations(const FString& RunId)
{
	// A bounded game-thread drain; no worker waits for this callback.
	for(int32 Pass=0;Worker&&Pass<4&&Worker->SnapshotRetention(RunId).PendingEvents>0;++Pass)DrainWorkerEvents();
}
void UVirtualSensorHighThroughputTransportSubsystem::GatherPendingRunKeys(const FString& RunId,TSet<FString>& Outputs,TSet<FString>& SensorFrames) const
{if(Worker)Worker->GatherPendingKeys(RunId,Outputs,SensorFrames);}

int32 UVirtualSensorHighThroughputTransportSubsystem::GetPendingRunFrameCount(const FString& RunId) const
{
	return Worker ? Worker->GetRunPending(RunId) : 0;
}

int32 UVirtualSensorHighThroughputTransportSubsystem::GetFailedRunFrameCount(const FString& RunId) const
{
	return Worker ? Worker->GetRunFailures(RunId) : 0;
}

bool UVirtualSensorHighThroughputTransportSubsystem::CanUseRawTcp(const FString& BrokerUrl, FString* OutReason)
{
	FString Host;
	int32 Port = 0;
	FString Reason;
	const bool bValid = ParseEndpoint(BrokerUrl, Host, Port, Reason);
	if (OutReason) *OutReason = Reason;
	return bValid;
}

FVirtualSensorHighThroughputProfile UVirtualSensorHighThroughputTransportSubsystem::MakeProfile(
	const FVirtualSensorTransportProfile& Profile)
{
	FVirtualSensorHighThroughputProfile Result;
	Result.BrokerUrl = Profile.BrokerUrl;
	Result.UserName = Profile.UserName;
	Result.CameraTopic = Profile.CameraTopic;
	Result.LidarTopic = Profile.LidarTopic;
	Result.PointCloudTopic = Profile.ExportTopic;
	Result.ConnectTimeoutSeconds = Profile.TimeoutSeconds;
	return Result;
}

void UVirtualSensorHighThroughputTransportSubsystem::DrainWorkerEvents()
{
	if (!Worker) return;
	FWorkerEvent Event;
	int32 Drained = 0;
	while (Drained < 256 && Worker->DequeueEvent(Event))
	{
		++Drained;
		if(OnTransportObservation.IsBound()&&!Event.RequestId.IsEmpty()&&(!Event.ReceivedData.IsValid()||!Event.ReceivedData->bFiltered))
		{
			FVirtualSensorTransportObservation O;
			O.Phase=Event.Type==EWorkerEventType::Submitted?EVirtualSensorTransportObservationPhase::Submitted:
				Event.Type==EWorkerEventType::Receipt?EVirtualSensorTransportObservationPhase::Receipt:
				Event.Type==EWorkerEventType::Consumed?EVirtualSensorTransportObservationPhase::Consumed:Event.Type==EWorkerEventType::ValidationFailed?EVirtualSensorTransportObservationPhase::ValidationFailed:EVirtualSensorTransportObservationPhase::Failed;
			O.Kind=Event.StreamKind;O.SensorId=Event.SensorId;O.FrameId=Event.FrameId;O.RequestId=Event.RequestId;O.RunId=Event.RunId;O.Message=Event.Message;
			O.AcquisitionUtc=Event.AcquisitionUtc;O.ObservedUtc=Event.ObservedUtc;O.MonotonicSeconds=Event.ObservedSeconds;O.LatencyMs=Event.LatencyMs;
			O.bClockValid=O.AcquisitionUtc.GetTicks()>0&&O.ObservedUtc>=O.AcquisitionUtc;
			OnTransportObservation.Broadcast(O);
		}
		if(Event.ReceivedData && Event.ReceivedData->ReceiveGeneration==ReceiverSelection.Generation)
		{
			OnReceived.Broadcast(Event.ReceivedData);
			if(Event.ReceivedData->bFiltered) continue;
		}
		if (Event.Type == EWorkerEventType::Connected || Event.Type == EWorkerEventType::Disconnected)
		{
			bTransportConnected=Event.Type==EWorkerEventType::Connected;++ConnectionRevision;ConnectionMessage=Event.Message;
			for (TPair<FString, FVirtualSensorStreamTelemetry>& Pair : TelemetryByKey)
			{
				Pair.Value.State = Event.Type == EWorkerEventType::Connected ? TEXT("connected") : TEXT("reconnecting");
				Pair.Value.Message = Event.Message;
			}
			continue;
		}
		FVirtualSensorStreamTelemetry& Telemetry = TelemetryByKey.FindOrAdd(TelemetryKey(Event.StreamKind, Event.SensorId));
		Telemetry.StreamKind = Event.StreamKind;
		Telemetry.SensorId = Event.SensorId;
		Telemetry.LastFrameId = Event.FrameId;
		Telemetry.LastFrameBytes = Event.Bytes;
		Telemetry.InputQueueDepth = Event.QueueDepth;
		Telemetry.ReceiptQueueDepth = Event.ReceiptDepth;
		Telemetry.Message = Event.Message;
		switch (Event.Type)
		{
		case EWorkerEventType::Submitted:
			++Telemetry.SubmittedCount;
			Telemetry.SubmittedBytes += Event.Bytes;
			if (Telemetry.FirstSubmittedSeconds <= 0.0) Telemetry.FirstSubmittedSeconds = FPlatformTime::Seconds();
			{
				const double SubmittedSeconds = FMath::Max(0.001, FPlatformTime::Seconds() - Telemetry.FirstSubmittedSeconds);
				Telemetry.SubmittedHz = static_cast<float>(Telemetry.SubmittedCount / SubmittedSeconds);
				Telemetry.SubmittedMegabytesPerSecond = static_cast<float>(Telemetry.SubmittedBytes /
					(1024.0 * 1024.0 * SubmittedSeconds));
			}
			Telemetry.LastSocketWriteLatencyMs = Event.LatencyMs;
			Telemetry.State = TEXT("submitted");
			break;
		case EWorkerEventType::Receipt:
			++Telemetry.ReceiptCount;
			Telemetry.LastReceiptLatencyMs = Event.LatencyMs;
			Telemetry.State = TEXT("receipt");
			break;
		case EWorkerEventType::Consumed:
			++Telemetry.ConsumerReceivedCount;
			if (Telemetry.FirstConsumerSeconds <= 0.0) Telemetry.FirstConsumerSeconds = FPlatformTime::Seconds();
			Telemetry.ConsumerHz = static_cast<float>(Telemetry.ConsumerReceivedCount /
				FMath::Max(0.001, FPlatformTime::Seconds() - Telemetry.FirstConsumerSeconds));
			Telemetry.FrameGapCount += Event.GapDelta;
			Telemetry.DuplicateCount += Event.DuplicateDelta;
			Telemetry.LastEndToEndLatencyMs = Event.LatencyMs;
			Telemetry.EndToEndLatencySamples.Add(Event.LatencyMs);
			if (Telemetry.EndToEndLatencySamples.Num() > 256)
			{
				Telemetry.EndToEndLatencySamples.RemoveAt(0, Telemetry.EndToEndLatencySamples.Num() - 256, false);
			}
			{
				TArray<float> Sorted = Telemetry.EndToEndLatencySamples;
				Sorted.Sort();
				const int32 P95Index = FMath::Clamp(FMath::CeilToInt(Sorted.Num() * 0.95f) - 1, 0, Sorted.Num() - 1);
				Telemetry.EndToEndP95LatencyMs = Sorted[P95Index];
			}
			Telemetry.State = TEXT("consumer-validated");
			break;
		case EWorkerEventType::ValidationFailed:
			++Telemetry.ValidationFailureCount;
			Telemetry.State = TEXT("consumer-invalid");
			break;
		case EWorkerEventType::Retry:
			++Telemetry.RetryCount;
			Telemetry.State = TEXT("retrying");
			break;
		case EWorkerEventType::Overload:
			++Telemetry.OverloadCount;
			Telemetry.State = TEXT("overload");
			break;
		case EWorkerEventType::DeliveryFailed:
			++Telemetry.DeliveryFailureCount;
			Telemetry.LastDeliveryFailureMessage=FString::Printf(TEXT("sensor=%s frame=%lld: %s"),*Event.SensorId,Event.FrameId,*Event.Message);
			Telemetry.State = TEXT("delivery-failed");
			break;
		default: break;
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorRawRunRetentionTest,"MA0T10.SlabReliability.RawRunRetention",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorRawRunRetentionTest::RunTest(const FString&)
{
	FVirtualSensorHighThroughputProfile Profile;FVirtualSensorHighThroughputTransportWorker Worker{Profile,FString()};
	FVirtualSensorStompFrame Message;Message.Command=TEXT("MESSAGE");Message.Body={0xff,0xd8,0xff,0xd9};
	Message.Headers.Add(TEXT("schema"),TEXT("virtual-camera.jpeg.v1"));Message.Headers.Add(TEXT("sensor-id"),TEXT("retention-camera"));Message.Headers.Add(TEXT("frame-id"),TEXT("1"));
	FVirtualSensorBinaryFrame Binary;Binary.SensorId=TEXT("retention-camera");Binary.StreamKind=EVirtualSensorStreamKind::CameraImage;Binary.FrameId=1;Binary.Body32=MakeShared<const TArray<uint8>,ESPMode::ThreadSafe>(Message.Body);
	FString First;
	for(int32 I=1;I<=45;++I)
	{
		const FString Run=FGuid::NewGuid().ToString();if(I==1)First=Run;
		Worker.RegisterRun(Run,I,true);Message.Headers.Add(TEXT("x-run-uuid"),Run);Binary.Headers.Add(TEXT("x-run-uuid"),Run);
		Worker.ValidateConsumedFrame(Message);Worker.AdjustRunPending(Binary,0,true);
		Worker.RetireRun(Run);Worker.CollectRetiredRuns();TestEqual(TEXT("queued observation protects run until delivered"),Worker.SnapshotRetention(Run).RetiringRuns,1);
		FWorkerEvent Event;while(Worker.DequeueEvent(Event)){}Worker.CollectRetiredRuns();
		const auto Snapshot=Worker.SnapshotRetention(Run);
		TestEqual(TEXT("retired sequence state released"),Snapshot.SequenceEntries,0);TestTrue(TEXT("completed diagnostics bounded"),Snapshot.CompletedRuns<=20);TestEqual(TEXT("no retained event"),Snapshot.PendingEvents,0);
	}
	TestEqual(TEXT("minimal ownership IDs retained separately"),Worker.SnapshotRetention(FString()).KnownRunIds,45);
	TestEqual(TEXT("only twenty failure aggregates retained"),Worker.RunFailureCounts.Num(),20);
	FString Error;Binary.Headers.Add(TEXT("x-run-uuid"),First);Binary.RequestId=TEXT("late-frame");TestFalse(TEXT("late enqueue rejected after history eviction"),Worker.Enqueue(Binary,Error));
	Message.Headers.Add(TEXT("x-run-uuid"),First);Worker.ValidateConsumedFrame(Message);TestEqual(TEXT("late MESSAGE cannot resurrect sequence"),Worker.SnapshotRetention(FString()).SequenceEntries,0);
	FWorkerEvent Event;while(Worker.DequeueEvent(Event)){}TestTrue(TEXT("late result diagnosed"),Worker.SnapshotRetention(FString()).LateRunEvents>0);
	const FString Current=FGuid::NewGuid().ToString();Worker.RegisterRun(Current,46,true);Binary.Headers.Add(TEXT("x-run-uuid"),Current);Binary.RequestId=TEXT("cancel-queued");TestTrue(TEXT("current frame queued"),Worker.Enqueue(Binary,Error));
	FVirtualSensorBinaryFrame Independent=Binary;Independent.Headers.Reset();Independent.RequestId=TEXT("independent");TestTrue(TEXT("independent frame queued"),Worker.Enqueue(Independent,Error));
	Worker.CancelRun(Current);Worker.ProcessRunCancellations();Worker.CollectRetiredRuns();
	TestEqual(TEXT("cancelled run returns bytes even while disconnected"),Worker.SnapshotRetention(Current).OutstandingBytes,int64(0));
	TestEqual(TEXT("unrelated independent frame remains"),Worker.SnapshotRetention(FString()).QueuedFrames,1);
	Worker.StopWorker();TestEqual(TEXT("stop returns all bytes"),Worker.SnapshotRetention(FString()).OutstandingBytes,int64(0));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorRawReceiptProbeTest,"MA0T10.SensorStream.RawReceiptProbe",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorRawReceiptProbeTest::RunTest(const FString&)
{
	if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RAW_PROBE"))!=TEXT("1")) { AddInfo(TEXT("SKIP: local receipt fault probe is not enabled"));return true; }
	FVirtualSensorHighThroughputProfile Profile;Profile.BrokerUrl=TEXT("tcp://127.0.0.1:18617");
	FVirtualSensorHighThroughputTransportWorker Worker{Profile,FString()};Worker.Start();
	FVirtualSensorBinaryFrame Frame;Frame.StreamKind=EVirtualSensorStreamKind::PointCloud;Frame.SensorId=TEXT("receipt-probe");
	Frame.Body32=MakeShared<const TArray<uint8>,ESPMode::ThreadSafe>(TArray<uint8>{1,2,3});
	FString Error;
	for(int32 I=0;I<20;++I){Frame.FrameId=I+1;Frame.RequestId=FString::FromInt(I);TestTrue(TEXT("initial bounded frames admitted"),Worker.Enqueue(Frame,Error));}
	int32 Submitted=0;const double Deadline=FPlatformTime::Seconds()+4;
	while(Submitted<20&&FPlatformTime::Seconds()<Deadline)
	{
		FWorkerEvent Event;while(Worker.DequeueEvent(Event))if(Event.Type==EWorkerEventType::Submitted)++Submitted;
		FPlatformProcess::SleepNoStats(0.005f);
	}
	TestEqual(TEXT("fake broker actually receives all socket submissions"),Submitted,20);
	Frame.RequestId=TEXT("over-cap");TestFalse(TEXT("missing receipts apply admission even after input queue drains"),Worker.Enqueue(Frame,Error));
	Worker.StopWorker();int32 Count,Queued;int64 Bytes,Global;
	Worker.SnapshotOutstanding(Frame.StreamKind,Frame.SensorId,Count,Bytes,Global,Queued);
	TestEqual(TEXT("fault-probe stop releases all held bytes"),Global,int64(0));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorRawOutstandingLimitTest,"MA0T10.SensorStream.RawOutstandingLimits",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorRawOutstandingLimitTest::RunTest(const FString&)
{
	FVirtualSensorHighThroughputProfile Profile;
	FVirtualSensorHighThroughputTransportWorker Worker{Profile,FString()};
	FVirtualSensorBinaryFrame Frame; Frame.StreamKind=EVirtualSensorStreamKind::PointCloud; Frame.SensorId=TEXT("cap-test");
	Frame.Headers.Add(TEXT("x-run-uuid"),TEXT("cap-run"));
	Frame.Body32=MakeShared<const TArray<uint8>,ESPMode::ThreadSafe>(TArray<uint8>{1,2,3});
	FString Error;
	for(int32 Index=0;Index<20;++Index)
	{
		Frame.FrameId=Index+1;Frame.RequestId=LexToString(Index);
		TestTrue(TEXT("bounded FIFO accepts each available reservation"),Worker.Enqueue(Frame,Error));
		FVirtualSensorBinaryFrame Sent;Worker.Frames.Dequeue(Sent);--Worker.QueueCounts[static_cast<int32>(Sent.StreamKind)];
		Worker.OutstandingFrames.FindChecked(Sent.RequestId).bQueued=false;
		FPendingReceipt Pending;Pending.Frame=Sent;Pending.SubmittedSeconds=FPlatformTime::Seconds();Worker.PendingReceipts.Add(Sent.RequestId,Pending);
	}
	Frame.RequestId=TEXT("twenty-one");
	TestFalse(TEXT("empty input queue cannot bypass receipt-wait frame cap"),Worker.Enqueue(Frame,Error));
	int32 Count,Queued;int64 Bytes,Global;
	Worker.SnapshotOutstanding(Frame.StreamKind,Frame.SensorId,Count,Bytes,Global,Queued);
	TestEqual(TEXT("twenty bodies counted once across queue and receipt phases"),Bytes,int64(60));
	TestEqual(TEXT("receipt-only waiting has no queued frames"),Queued,0);
	FVirtualSensorStompFrame Receipt;Receipt.Command=TEXT("RECEIPT");Receipt.Headers.Add(TEXT("receipt-id"),TEXT("0"));Worker.HandleFrame(Receipt);
	TestTrue(TEXT("receipt releases admission immediately"),Worker.Enqueue(Frame,Error));
	Worker.StopWorker();Worker.SnapshotOutstanding(Frame.StreamKind,Frame.SensorId,Count,Bytes,Global,Queued);
	TestEqual(TEXT("stop retires queued and unconfirmed bodies"),Global,int64(0));
	TestEqual(TEXT("stop balances run pending"),Worker.GetRunPending(TEXT("cap-run")),0);
	Profile.MaxOutstandingBytes=4;Profile.MaxOutstandingBytesPerStream=4;
	FVirtualSensorHighThroughputTransportWorker Small{Profile,FString()};
	Frame.RequestId=TEXT("a");TestTrue(TEXT("first body fits byte budget"),Small.Enqueue(Frame,Error));
	Frame.SensorId=TEXT("other");Frame.RequestId=TEXT("b");TestFalse(TEXT("global byte budget spans different sensors"),Small.Enqueue(Frame,Error));
	Small.StopWorker();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorRawTerminalLedgerTest, "MA0T10.SensorStream.RawTerminalLedger", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSensorRawTerminalLedgerTest::RunTest(const FString& Parameters)
{
	FVirtualSensorHighThroughputTransportWorker Worker{FVirtualSensorHighThroughputProfile(), FString()};
	FVirtualSensorBinaryFrame Frame;
	Frame.SensorId = TEXT("LEDGER-TEST");
	Frame.Headers.Add(TEXT("x-run-uuid"), TEXT("run-a"));
	Frame.Body32 = MakeShared<const TArray<uint8>, ESPMode::ThreadSafe>(TArray<uint8>{1});
	FString Error;
	TestTrue(TEXT("enqueue accepts frame"), Worker.Enqueue(Frame, Error));
	Worker.PumpSend(); // no socket: deterministic terminal write failure, no network IO
	TestEqual(TEXT("failed initial write balances pending"), Worker.GetRunPending(TEXT("run-a")), 0);
	TestEqual(TEXT("failed initial write recorded before event drain"), Worker.GetRunFailures(TEXT("run-a")), 1);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FPendingReceipt Pending; Pending.Frame = Frame;
		Worker.AdjustRunPending(Frame, 1);
		Worker.PendingReceipts.Add(LexToString(Index), Pending);
	}
	Worker.CloseSocket();
	TestEqual(TEXT("disconnect retires every receipt wait"), Worker.GetRunPending(TEXT("run-a")), 0);
	TestEqual(TEXT("disconnect reports unconfirmed delivery"), Worker.GetRunFailures(TEXT("run-a")), 3);
	for (int32 RetryAttempt : {3, 0})
	{
		FPendingReceipt Pending; Pending.Frame = Frame; Pending.RetryAttempt = RetryAttempt;
		Worker.AdjustRunPending(Frame, 1);
		Worker.PendingReceipts.Add(TEXT("retry"), Pending);
		Worker.CheckReceiptTimeouts();
		TestEqual(TEXT("retry exhaustion/write failure balances pending"), Worker.GetRunPending(TEXT("run-a")), 0);
	}
	TestEqual(TEXT("all terminal paths counted once"), Worker.GetRunFailures(TEXT("run-a")), 5);
	TestEqual(TEXT("different run is not contaminated"), Worker.GetRunFailures(TEXT("run-b")), 0);
	return true;
}
#endif
