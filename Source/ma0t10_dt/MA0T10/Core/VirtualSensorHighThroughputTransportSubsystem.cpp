#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"

#include "ma0t10_dt/MA0T10/Core/VirtualSensorStompProtocol.h"
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
	Overload
};

struct FWorkerEvent
{
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
	}

	void Stop() override
	{
		bStopRequested.Store(true);
	}

	uint32 Run() override
	{
		while (!bStopRequested.Load())
		{
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
		if (QueueCounts[Index].Load() >= Limit)
		{
			OutError = FString::Printf(TEXT("High-throughput %s queue reached %d frames."), *Frame.SensorId, Limit);
			FWorkerEvent Event;
			Event.Type = EWorkerEventType::Overload;
			Event.StreamKind = Frame.StreamKind;
			Event.SensorId = Frame.SensorId;
			Event.FrameId = Frame.FrameId;
			Event.QueueDepth = QueueCounts[Index].Load();
			Event.Message = OutError;
			Events.Enqueue(MoveTemp(Event));
			return false;
		}
		++QueueCounts[Index];
		Frames.Enqueue(Frame);
		return true;
	}

	bool DequeueEvent(FWorkerEvent& OutEvent)
	{
		return Events.Dequeue(OutEvent);
	}

	bool IsRunning() const { return Thread != nullptr && !bStopRequested.Load(); }

private:
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
		SendSubscribe(TEXT("ma0t10-camera-loopback"), Profile.CameraTopic);
		SendSubscribe(TEXT("ma0t10-lidar-loopback"), Profile.LidarTopic);
		SendSubscribe(TEXT("ma0t10-pcd-loopback"), Profile.PointCloudTopic);
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

	void PumpSend()
	{
		int32 SentThisPass = 0;
		FVirtualSensorBinaryFrame Frame;
		while (SentThisPass < 8 && Frames.Dequeue(Frame))
		{
			const int32 Index = static_cast<int32>(Frame.StreamKind);
			--QueueCounts[Index];
			if (!SendFrame(Frame, 0))
			{
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
		FString Header = FString::Printf(
			TEXT("SEND\ndestination:%s\ncontent-type:%s\ncontent-length:%lld\nreceipt:%s\npersistent:true\ndestination-type:MULTICAST\nschema:%s\nsensor-id:%s\nframe-id:%lld\ntimestamp-utc:%s\nrequest-id:%s\nx-sensor-id:%s\nx-frame-id:%lld\nx-request-id:%s\nx-data-kind:%s\nx-sensor-type:%s\n"),
			*FVirtualSensorStompParser::EscapeHeader(Frame.Destination),
			*FVirtualSensorStompParser::EscapeHeader(Frame.ContentType),
			Frame.NumBytes(),
			*FVirtualSensorStompParser::EscapeHeader(RequestId),
			*FVirtualSensorStompParser::EscapeHeader(Frame.Schema),
			*FVirtualSensorStompParser::EscapeHeader(Frame.SensorId),
			Frame.FrameId,
			*FVirtualSensorStompParser::EscapeHeader(Frame.TimestampUtc.ToIso8601()),
			*FVirtualSensorStompParser::EscapeHeader(RequestId),
			*FVirtualSensorStompParser::EscapeHeader(Frame.SensorId),
			Frame.FrameId,
			*FVirtualSensorStompParser::EscapeHeader(RequestId),
			Frame.StreamKind == EVirtualSensorStreamKind::PointCloud ? TEXT("pointcloud-stream")
				: Frame.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("camera-stream") : TEXT("lidar-stream"),
			Frame.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("camera") : TEXT("lidar"));
		for (const TPair<FString, FString>& Pair : Frame.Headers)
		{
			if (Pair.Key.Equals(TEXT("schema"), ESearchCase::IgnoreCase) ||
				Pair.Key.Equals(TEXT("sensor-id"), ESearchCase::IgnoreCase) ||
				Pair.Key.Equals(TEXT("frame-id"), ESearchCase::IgnoreCase)) continue;
			Header += FVirtualSensorStompParser::EscapeHeader(Pair.Key);
			Header += TEXT(":");
			Header += FVirtualSensorStompParser::EscapeHeader(Pair.Value);
			Header += TEXT("\n");
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

		FWorkerEvent Event;
		Event.Type = EWorkerEventType::Submitted;
		Event.StreamKind = Frame.StreamKind;
		Event.SensorId = Frame.SensorId;
		Event.FrameId = Frame.FrameId;
		Event.Bytes = static_cast<int32>(FMath::Min<int64>(MAX_int32, Frame.NumBytes()));
		Event.QueueDepth = QueueCounts[static_cast<int32>(Frame.StreamKind)].Load();
		Event.ReceiptDepth = CountPendingReceipts(Frame.StreamKind);
		Event.LatencyMs = static_cast<float>((LastSocketActivitySeconds - Started) * 1000.0);
		Event.Message = TEXT("Raw TCP STOMP frame submitted; waiting for broker receipt.");
		Events.Enqueue(MoveTemp(Event));
		return true;
	}

	bool SendAll(const uint8* Data, int64 NumBytes, double TimeoutSeconds)
	{
		if (!Socket || !Data || NumBytes < 0) return false;
		int64 Offset = 0;
		const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
		while (Offset < NumBytes && !bStopRequested.Load() && FPlatformTime::Seconds() < Deadline)
		{
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
				Event.Bytes = static_cast<int32>(FMath::Min<int64>(MAX_int32, Pending->Frame.NumBytes()));
				Event.ReceiptDepth = FMath::Max(0, CountPendingReceipts(Pending->Frame.StreamKind) - 1);
				Event.LatencyMs = static_cast<float>((FPlatformTime::Seconds() - Pending->SubmittedSeconds) * 1000.0);
				Event.Message = TEXT("Broker receipt received.");
				Events.Enqueue(MoveTemp(Event));
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
		const FString Schema = HeaderValue(Frame.Headers, TEXT("schema"));
		EVirtualSensorStreamKind Kind = EVirtualSensorStreamKind::LidarPayload;
		if (Schema == TEXT("virtual-camera.jpeg.v1")) Kind = EVirtualSensorStreamKind::CameraImage;
		else if (Schema == TEXT("virtual-pointcloud.pcd.v1")) Kind = EVirtualSensorStreamKind::PointCloud;
		else if (Schema != TEXT("virtual-lidar.telemetry.v1")) return;

		const FString SensorId = HeaderValue(Frame.Headers, TEXT("sensor-id"));
		int64 FrameId = 0;
		const bool bFrameIdValid = LexTryParseString(FrameId, *HeaderValue(Frame.Headers, TEXT("frame-id")));
		bool bValid = bFrameIdValid && !SensorId.IsEmpty() && !Frame.Body.IsEmpty();
		FString ValidationMessage;
		if (bValid && Kind == EVirtualSensorStreamKind::CameraImage)
		{
			bValid = Frame.Body.Num() >= 4 && Frame.Body[0] == 0xff && Frame.Body[1] == 0xd8 &&
				Frame.Body[Frame.Body.Num() - 2] == 0xff && Frame.Body.Last() == 0xd9;
			ValidationMessage = bValid ? TEXT("JPEG signature and size validated.") : TEXT("JPEG signature validation failed.");
		}
		else if (bValid && Kind == EVirtualSensorStreamKind::PointCloud)
		{
			const int32 HeaderProbe = FMath::Min(Frame.Body.Num(), 2048);
			FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Frame.Body.GetData()), HeaderProbe);
			const FString PcdHeader(Text.Length(), Text.Get());
			bValid = PcdHeader.Contains(TEXT(".PCD v0.7")) && PcdHeader.Contains(TEXT("DATA binary"));
			ValidationMessage = bValid ? TEXT("Binary PCD header and body validated.") : TEXT("Binary PCD header validation failed.");
		}
		else if (bValid)
		{
			FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Frame.Body.GetData()), Frame.Body.Num());
			const FString Json(Text.Length(), Text.Get());
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
			bValid = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid() &&
				Root->GetStringField(TEXT("schema")) == TEXT("virtual-lidar.telemetry.v1");
			ValidationMessage = bValid ? TEXT("LiDAR telemetry schema validated.") : TEXT("LiDAR telemetry schema validation failed.");
		}

		const FString ExpectedChecksum = HeaderValue(Frame.Headers, TEXT("checksum"));
		if (bValid && !ExpectedChecksum.IsEmpty())
		{
			bValid = Sha1Hex(Frame.Body.GetData(), Frame.Body.Num()).Equals(ExpectedChecksum, ESearchCase::IgnoreCase);
			if (!bValid) ValidationMessage = TEXT("Frame checksum validation failed.");
		}

		FWorkerEvent Event;
		Event.Type = bValid ? EWorkerEventType::Consumed : EWorkerEventType::ValidationFailed;
		Event.StreamKind = Kind;
		Event.SensorId = SensorId;
		Event.FrameId = FrameId;
		Event.Bytes = Frame.Body.Num();
		Event.Message = ValidationMessage;
		const int32 Index = static_cast<int32>(Kind);
		const int64 Previous = LastConsumerFrameIds[Index];
		if (bValid && Previous > 0)
		{
			if (FrameId == Previous) Event.DuplicateDelta = 1;
			else if (FrameId > Previous + 1) Event.GapDelta = FrameId - Previous - 1;
		}
		if (bValid && FrameId > Previous) LastConsumerFrameIds[Index] = FrameId;
		FDateTime SourceUtc;
		if (FDateTime::ParseIso8601(*HeaderValue(Frame.Headers, TEXT("timestamp-utc")), SourceUtc))
		{
			Event.LatencyMs = static_cast<float>(FMath::Max(0.0, (FDateTime::UtcNow() - SourceUtc).GetTotalMilliseconds()));
		}
		Events.Enqueue(MoveTemp(Event));
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
				FWorkerEvent Event;
				Event.Type = EWorkerEventType::Overload;
				Event.StreamKind = Pending.Frame.StreamKind;
				Event.SensorId = Pending.Frame.SensorId;
				Event.FrameId = Pending.Frame.FrameId;
				Event.Message = TEXT("Broker receipt retry limit was exhausted.");
				Events.Enqueue(MoveTemp(Event));
				continue;
			}
			FWorkerEvent RetryEvent;
			RetryEvent.Type = EWorkerEventType::Retry;
			RetryEvent.StreamKind = Pending.Frame.StreamKind;
			RetryEvent.SensorId = Pending.Frame.SensorId;
			RetryEvent.FrameId = Pending.Frame.FrameId;
			RetryEvent.Message = TEXT("Broker receipt timed out; retrying the idempotent frame.");
			Events.Enqueue(MoveTemp(RetryEvent));
			if (!SendFrame(Pending.Frame, Pending.RetryAttempt + 1))
			{
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
		Events.Enqueue(MoveTemp(Event));
	}

	void CloseSocket()
	{
		bConnected = false;
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
		ActivePasscode == SessionPasscode;
	if (bSameProfile && Worker->IsRunning()) return true;
	StopHighThroughputTransport();
	ActiveProfile = Profile;
	ActivePasscode = SessionPasscode;
	Worker = new FVirtualSensorHighThroughputTransportWorker(Profile, SessionPasscode);
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
	if (Worker)
	{
		Worker->StopWorker();
		delete Worker;
		Worker = nullptr;
	}
}

bool UVirtualSensorHighThroughputTransportSubsystem::IsHighThroughputTransportRunning() const
{
	return Worker != nullptr && Worker->IsRunning();
}

bool UVirtualSensorHighThroughputTransportSubsystem::EnqueueBinaryFrame(
	const FVirtualSensorBinaryFrame& Frame,
	FString& OutError)
{
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
		++Telemetry.OverloadCount;
		Telemetry.State = TEXT("overload");
		Telemetry.Message = OutError;
		return false;
	}
	++Telemetry.EnqueuedCount;
	return true;
}

TArray<FVirtualSensorStreamTelemetry> UVirtualSensorHighThroughputTransportSubsystem::GetStreamTelemetry() const
{
	TArray<FVirtualSensorStreamTelemetry> Result;
	TelemetryByKey.GenerateValueArray(Result);
	return Result;
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
		if (Event.Type == EWorkerEventType::Connected || Event.Type == EWorkerEventType::Disconnected)
		{
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
		default: break;
		}
	}
}
