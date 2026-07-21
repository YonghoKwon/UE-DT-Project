#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"

#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Core/DTCoreSettings.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/CameraJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarCsvReplaySourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarHttpJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLinesReplaySourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarUdpJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualCameraStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualLidarStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"
#include "ma0t10_dt/ma0t10_dt.h"
#include "WebSocket/TransactionCodeMessage.h"

AVirtualSensorExternalSourceHostActor::AVirtualSensorExternalSourceHostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	LidarCsvReplay = CreateDefaultSubobject<ULidarCsvReplaySourceComponent>(TEXT("LidarCsvReplay"));
	LidarJsonLinesReplay = CreateDefaultSubobject<ULidarJsonLinesReplaySourceComponent>(TEXT("LidarJsonLinesReplay"));
	LidarBufferedJson = CreateDefaultSubobject<ULidarJsonLiveSourceComponent>(TEXT("LidarBufferedJson"));
	LidarHttpJson = CreateDefaultSubobject<ULidarHttpJsonLiveSourceComponent>(TEXT("LidarHttpJson"));
	LidarUdpJson = CreateDefaultSubobject<ULidarUdpJsonLiveSourceComponent>(TEXT("LidarUdpJson"));
	CameraBufferedJson = CreateDefaultSubobject<UCameraJsonLiveSourceComponent>(TEXT("CameraBufferedJson"));
	for (URealSensorSourceComponent* Source : { static_cast<URealSensorSourceComponent*>(LidarCsvReplay), static_cast<URealSensorSourceComponent*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComponent*>(LidarBufferedJson), static_cast<URealSensorSourceComponent*>(LidarHttpJson), static_cast<URealSensorSourceComponent*>(LidarUdpJson), static_cast<URealSensorSourceComponent*>(CameraBufferedJson) })
	{
		Source->SetupAttachment(Root);
		Source->bAutoStartSource = false;
		Source->bSendTransportByDefault = false;
	}
	LidarCsvReplay->SourceId = TEXT("Demo-LiDAR-CSV");
	LidarCsvReplay->CsvFilePath = TEXT("Samples/slab_replay_sample.csv");
	LidarJsonLinesReplay->SourceId = TEXT("Demo-LiDAR-JSONL");
	LidarBufferedJson->SourceId = TEXT("Live-LiDAR-Buffered");
	LidarJsonLinesReplay->JsonLinesFilePath = TEXT("Samples/slab_replay_sample.jsonl");
	LidarHttpJson->SourceId = TEXT("Live-LiDAR-HTTP");
	LidarHttpJson->ListenPort = 8082;
	LidarHttpJson->RoutePath = TEXT("/ma0t10/lidar/live");
	LidarUdpJson->SourceId = TEXT("Live-LiDAR-UDP");
	LidarUdpJson->BindAddress = TEXT("127.0.0.1");
	LidarUdpJson->BindPort = 0;
	CameraBufferedJson->SourceId = TEXT("Live-Camera-JSON");
}

void AVirtualSensorExternalSourceHostActor::BeginPlay()
{
	Super::BeginPlay();
	bEndingPlay = false;
	AVirtualLidarSensorActor* Lidar = nullptr;
	AVirtualCameraSensorActor* Camera = nullptr;
	for (TActorIterator<AVirtualLidarSensorActor> It(GetWorld()); It; ++It) { Lidar = *It; break; }
	for (TActorIterator<AVirtualCameraSensorActor> It(GetWorld()); It; ++It) { Camera = *It; break; }
	for (URealSensorSourceComponent* Source : { static_cast<URealSensorSourceComponent*>(LidarCsvReplay), static_cast<URealSensorSourceComponent*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComponent*>(LidarBufferedJson), static_cast<URealSensorSourceComponent*>(LidarHttpJson), static_cast<URealSensorSourceComponent*>(LidarUdpJson) })
	{
		Source->TargetSensorActor = Lidar;
		Source->TargetLidar = Lidar ? Lidar->ScanComponent : nullptr;
	}
	CameraBufferedJson->TargetSensorActor = Camera;
	CameraBufferedJson->TargetCamera = Camera ? Camera->CaptureComponent : nullptr;

	InitializeTopicReceiverRuntime();
	if (bAutoStartTopicReceivers)
	{
		bTopicReceiversRequested = true;
		GetWorldTimerManager().SetTimer(
			TopicReceiverRetryTimer,
			this,
			&AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions,
			FMath::Max(0.01f, InitialSubscribeDelaySeconds),
			false);
	}
}

void AVirtualSensorExternalSourceHostActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopTopicReceivers();
	Super::EndPlay(EndPlayReason);
}

void AVirtualSensorExternalSourceHostActor::InitializeTopicReceiverRuntime()
{
	if (!LidarTopicHandler) LidarTopicHandler = NewObject<UVirtualLidarStreamReceiverTC>(this, TEXT("LidarTopicHandler"));
	if (!CameraTopicHandler) CameraTopicHandler = NewObject<UVirtualCameraStreamReceiverTC>(this, TEXT("CameraTopicHandler"));
	if (!PointCloudTopicHandler) PointCloudTopicHandler = NewObject<UVirtualPointCloudStreamReceiverTC>(this, TEXT("PointCloudTopicHandler"));
	if (ReceiverRuntimes.Num() == 3) return;
	ReceiverRuntimes.Reset();
	for (const EVirtualSensorTopicReceiveKind Kind : { EVirtualSensorTopicReceiveKind::Lidar, EVirtualSensorTopicReceiveKind::Camera, EVirtualSensorTopicReceiveKind::PointCloud })
	{
		FReceiverRuntime& Runtime = ReceiverRuntimes.AddDefaulted_GetRef();
		Runtime.Status.Kind = Kind;
		Runtime.Status.Topic = GetTopic(Kind);
		Runtime.Status.State = EVirtualSensorTopicReceiverState::Stopped;
	}
}

void AVirtualSensorExternalSourceHostActor::StartTopicReceivers()
{
	InitializeTopicReceiverRuntime();
	bTopicReceiversRequested = true;
	AttemptTopicSubscriptions();
}

void AVirtualSensorExternalSourceHostActor::StopTopicReceivers()
{
	bTopicReceiversRequested = false;
	GetWorldTimerManager().ClearTimer(TopicReceiverRetryTimer);
	UDxWebSocketSubsystem* WebSocket = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>() : nullptr;
	for (FReceiverRuntime& Runtime : ReceiverRuntimes)
	{
		if (WebSocket && !Runtime.SubscriptionId.IsEmpty())
		{
			WebSocket->Unsubscribe(Runtime.SubscriptionId, FSTOMPRequestCompleted());
		}
		Runtime.SubscriptionId.Reset();
		Runtime.PendingBody.Reset();
		Runtime.bSubscriptionPending = false;
		Runtime.SubscriptionStartedSeconds = 0.0;
		Runtime.RetryAttempt = 0;
		Runtime.Status.State = EVirtualSensorTopicReceiverState::Stopped;
		Runtime.Status.LastMessage = TEXT("수신 구독을 중지했습니다.");
	}
}

void AVirtualSensorExternalSourceHostActor::ReconnectTopicReceivers()
{
	StopTopicReceivers();
	bEndingPlay = false;
	bTopicReceiversRequested = true;
	GetWorldTimerManager().SetTimerForNextTick(this, &AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions);
}

void AVirtualSensorExternalSourceHostActor::ConfigureReceiverTopics(
	const FString& InLidarTopic,
	const FString& InCameraTopic,
	const FString& InPointCloudTopic)
{
	const FString NewLidar = InLidarTopic.TrimStartAndEnd();
	const FString NewCamera = InCameraTopic.TrimStartAndEnd();
	const FString NewPointCloud = InPointCloudTopic.TrimStartAndEnd();
	const bool bChanged = LidarReceiveTopic != NewLidar || CameraReceiveTopic != NewCamera || PointCloudReceiveTopic != NewPointCloud;
	LidarReceiveTopic = NewLidar;
	CameraReceiveTopic = NewCamera;
	PointCloudReceiveTopic = NewPointCloud;
	for (FReceiverRuntime& Runtime : ReceiverRuntimes) Runtime.Status.Topic = GetTopic(Runtime.Status.Kind);
	if (bChanged && bTopicReceiversRequested) ReconnectTopicReceivers();
}

TArray<FVirtualSensorTopicReceiverStatus> AVirtualSensorExternalSourceHostActor::GetTopicReceiverStatuses() const
{
	TArray<FVirtualSensorTopicReceiverStatus> Result;
	Result.Reserve(ReceiverRuntimes.Num());
	for (const FReceiverRuntime& Runtime : ReceiverRuntimes) Result.Add(Runtime.Status);
	return Result;
}

FString AVirtualSensorExternalSourceHostActor::GetReceiverBrokerUrl() const
{
	FString RuntimeUrl;
	if (GConfig && GConfig->GetString(TEXT("DTCoreRuntimeOverride"), TEXT("WebSocketUrl"), RuntimeUrl, GGameIni) && !RuntimeUrl.IsEmpty()) return RuntimeUrl;
	const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();
	return Settings ? Settings->WebSocketUrl : FString();
}

AVirtualSensorExternalSourceHostActor::FReceiverRuntime* AVirtualSensorExternalSourceHostActor::FindRuntime(EVirtualSensorTopicReceiveKind Kind)
{
	return ReceiverRuntimes.FindByPredicate([Kind](const FReceiverRuntime& Runtime) { return Runtime.Status.Kind == Kind; });
}

const AVirtualSensorExternalSourceHostActor::FReceiverRuntime* AVirtualSensorExternalSourceHostActor::FindRuntime(EVirtualSensorTopicReceiveKind Kind) const
{
	return ReceiverRuntimes.FindByPredicate([Kind](const FReceiverRuntime& Runtime) { return Runtime.Status.Kind == Kind; });
}

UTransactionCodeMessage* AVirtualSensorExternalSourceHostActor::FindHandler(EVirtualSensorTopicReceiveKind Kind) const
{
	switch (Kind)
	{
	case EVirtualSensorTopicReceiveKind::Lidar: return LidarTopicHandler;
	case EVirtualSensorTopicReceiveKind::Camera: return CameraTopicHandler;
	case EVirtualSensorTopicReceiveKind::PointCloud: return PointCloudTopicHandler;
	default: return nullptr;
	}
}

FString AVirtualSensorExternalSourceHostActor::GetTopic(EVirtualSensorTopicReceiveKind Kind) const
{
	switch (Kind)
	{
	case EVirtualSensorTopicReceiveKind::Lidar: return LidarReceiveTopic;
	case EVirtualSensorTopicReceiveKind::Camera: return CameraReceiveTopic;
	case EVirtualSensorTopicReceiveKind::PointCloud: return PointCloudReceiveTopic;
	default: return FString();
	}
}

void AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions()
{
	if (bEndingPlay || !bTopicReceiversRequested) return;
	InitializeTopicReceiverRuntime();
	const double Now = FPlatformTime::Seconds();
	UDxWebSocketSubsystem* WebSocket = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>() : nullptr;
	for (FReceiverRuntime& Runtime : ReceiverRuntimes)
	{
		if (Runtime.bSubscriptionPending && Runtime.SubscriptionStartedSeconds > 0.0 && Now - Runtime.SubscriptionStartedSeconds >= 5.0)
		{
			if (WebSocket && !Runtime.SubscriptionId.IsEmpty()) WebSocket->Unsubscribe(Runtime.SubscriptionId, FSTOMPRequestCompleted());
			Runtime.SubscriptionId.Reset();
			Runtime.bSubscriptionPending = false;
			Runtime.SubscriptionStartedSeconds = 0.0;
			++Runtime.RetryAttempt;
			Runtime.Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
			Runtime.Status.LastMessage = TEXT("구독 응답 시간 초과, 다시 시도합니다.");
		}
	}
	for (const EVirtualSensorTopicReceiveKind Kind : { EVirtualSensorTopicReceiveKind::Lidar, EVirtualSensorTopicReceiveKind::Camera, EVirtualSensorTopicReceiveKind::PointCloud })
	{
		SubscribeRuntime(Kind);
	}
	ScheduleSubscriptionRetry();
}

void AVirtualSensorExternalSourceHostActor::SubscribeRuntime(EVirtualSensorTopicReceiveKind Kind)
{
	FReceiverRuntime* Runtime = FindRuntime(Kind);
	if (!Runtime || !Runtime->SubscriptionId.IsEmpty() || Runtime->bSubscriptionPending) return;
	Runtime->Status.Topic = GetTopic(Kind);
	if (Runtime->Status.Topic.IsEmpty())
	{
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Error;
		Runtime->Status.LastMessage = TEXT("수신 Topic이 비어 있습니다.");
		return;
	}
	UDxWebSocketSubsystem* WebSocket = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>() : nullptr;
	if (!WebSocket)
	{
		Runtime->Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
		Runtime->Status.LastMessage = TEXT("DTCore WebSocket Subsystem 대기 중");
		++Runtime->RetryAttempt;
		return;
	}
	FSTOMPSubscriptionEvent Event;
	FSTOMPRequestCompleted Completion;
	switch (Kind)
	{
	case EVirtualSensorTopicReceiveKind::Lidar:
		Event.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandleLidarTopicMessage);
		Completion.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandleLidarSubscriptionCompleted);
		break;
	case EVirtualSensorTopicReceiveKind::Camera:
		Event.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandleCameraTopicMessage);
		Completion.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandleCameraSubscriptionCompleted);
		break;
	case EVirtualSensorTopicReceiveKind::PointCloud:
		Event.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandlePointCloudTopicMessage);
		Completion.BindDynamic(this, &AVirtualSensorExternalSourceHostActor::HandlePointCloudSubscriptionCompleted);
		break;
	default: return;
	}
	Runtime->bSubscriptionPending = true;
	Runtime->SubscriptionStartedSeconds = FPlatformTime::Seconds();
	Runtime->Status.State = EVirtualSensorTopicReceiverState::Subscribing;
	Runtime->Status.LastMessage = TEXT("DTCore STOMP 구독 요청 중");
	Runtime->SubscriptionId = WebSocket->Subscribe(Runtime->Status.Topic, Event, Completion);
	if (Runtime->SubscriptionId.IsEmpty())
	{
		Runtime->bSubscriptionPending = false;
		Runtime->SubscriptionStartedSeconds = 0.0;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
		Runtime->Status.LastMessage = TEXT("구독 요청이 시작되지 않아 재시도합니다.");
		++Runtime->RetryAttempt;
	}
}

void AVirtualSensorExternalSourceHostActor::ScheduleSubscriptionRetry()
{
	if (bEndingPlay || !bTopicReceiversRequested) return;
	bool bNeedsRetry = false;
	int32 MaxAttempt = 0;
	for (const FReceiverRuntime& Runtime : ReceiverRuntimes)
	{
		bNeedsRetry |= Runtime.bSubscriptionPending || Runtime.SubscriptionId.IsEmpty();
		MaxAttempt = FMath::Max(MaxAttempt, Runtime.RetryAttempt);
	}
	if (!bNeedsRetry) return;
	const float Delay = FMath::Min(10.0f, FMath::Pow(2.0f, static_cast<float>(FMath::Clamp(MaxAttempt, 0, 4))));
	GetWorldTimerManager().SetTimer(TopicReceiverRetryTimer, this, &AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions, Delay, false);
}

void AVirtualSensorExternalSourceHostActor::CompleteSubscription(EVirtualSensorTopicReceiveKind Kind, bool bSuccess, const FString& Error)
{
	FReceiverRuntime* Runtime = FindRuntime(Kind);
	if (!Runtime || bEndingPlay || !bTopicReceiversRequested) return;
	Runtime->bSubscriptionPending = false;
	Runtime->SubscriptionStartedSeconds = 0.0;
	if (bSuccess)
	{
		Runtime->RetryAttempt = 0;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Active;
		Runtime->Status.LastMessage = TEXT("DTCore STOMP Topic 구독 완료");
		UE_LOG(LogMA0T10, Display, TEXT("[SensorTopicReceiver] subscribed topic=%s subscription=%s"), *Runtime->Status.Topic, *Runtime->SubscriptionId);
	}
	else
	{
		Runtime->SubscriptionId.Reset();
		++Runtime->RetryAttempt;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::WaitingForConnection;
		Runtime->Status.LastMessage = FString::Printf(TEXT("구독 실패, 재시도 예정: %s"), *Error.Left(256));
		ScheduleSubscriptionRetry();
	}
}

void AVirtualSensorExternalSourceHostActor::HandleLidarSubscriptionCompleted(bool bSuccess, FString Error) { CompleteSubscription(EVirtualSensorTopicReceiveKind::Lidar, bSuccess, Error); }
void AVirtualSensorExternalSourceHostActor::HandleCameraSubscriptionCompleted(bool bSuccess, FString Error) { CompleteSubscription(EVirtualSensorTopicReceiveKind::Camera, bSuccess, Error); }
void AVirtualSensorExternalSourceHostActor::HandlePointCloudSubscriptionCompleted(bool bSuccess, FString Error) { CompleteSubscription(EVirtualSensorTopicReceiveKind::PointCloud, bSuccess, Error); }

void AVirtualSensorExternalSourceHostActor::HandleLidarTopicMessage(const FWebSocketMessage& Message) { QueueTopicPayload(EVirtualSensorTopicReceiveKind::Lidar, Message.BodyString); }
void AVirtualSensorExternalSourceHostActor::HandleCameraTopicMessage(const FWebSocketMessage& Message) { QueueTopicPayload(EVirtualSensorTopicReceiveKind::Camera, Message.BodyString); }
void AVirtualSensorExternalSourceHostActor::HandlePointCloudTopicMessage(const FWebSocketMessage& Message) { QueueTopicPayload(EVirtualSensorTopicReceiveKind::PointCloud, Message.BodyString); }

void AVirtualSensorExternalSourceHostActor::QueueTopicPayload(EVirtualSensorTopicReceiveKind Kind, FString Body)
{
	if (bEndingPlay || !bTopicReceiversRequested || Body.IsEmpty()) return;
	FReceiverRuntime* Runtime = FindRuntime(Kind);
	if (!Runtime) return;
	++Runtime->Status.ReceivedCount;
	Runtime->Status.LastReceivedUtc = FDateTime::UtcNow();
	const int32 Bytes = FTCHARToUTF8(*Body).Length();
	Runtime->Status.LastMessageBytes = Bytes;
	if (Bytes > FMath::Clamp(MaxReceiverMessageBytes, 1024, 16777216))
	{
		++Runtime->Status.ValidationFailureCount;
		Runtime->Status.State = EVirtualSensorTopicReceiverState::Error;
		Runtime->Status.LastMessage = FString::Printf(TEXT("수신 크기 제한 초과: %d bytes"), Bytes);
		return;
	}
	if (Runtime->PendingBody.IsSet()) ++Runtime->Status.ReplacedPendingCount;
	Runtime->PendingBody = MoveTemp(Body);
	TryStartQueuedParses();
}

void AVirtualSensorExternalSourceHostActor::TryStartQueuedParses()
{
	if (bEndingPlay || !bTopicReceiversRequested || ReceiverRuntimes.IsEmpty()) return;
	const int32 Limit = FMath::Clamp(MaxConcurrentReceiverParses, 1, 3);
	for (int32 Attempt = 0; Attempt < ReceiverRuntimes.Num() && ActiveReceiverParseCount < Limit; ++Attempt)
	{
		const int32 Index = (ReceiverRoundRobinCursor + Attempt) % ReceiverRuntimes.Num();
		FReceiverRuntime& Runtime = ReceiverRuntimes[Index];
		if (Runtime.bParsing || !Runtime.PendingBody.IsSet()) continue;
		UTransactionCodeMessage* Handler = FindHandler(Runtime.Status.Kind);
		if (!Handler) continue;
		FString Body = MoveTemp(Runtime.PendingBody.GetValue());
		Runtime.PendingBody.Reset();
		Runtime.bParsing = true;
		++ActiveReceiverParseCount;
		ReceiverRoundRobinCursor = (Index + 1) % ReceiverRuntimes.Num();
		const EVirtualSensorTopicReceiveKind Kind = Runtime.Status.Kind;
		const TWeakObjectPtr<AVirtualSensorExternalSourceHostActor> WeakThis(this);
		const TWeakObjectPtr<UTransactionCodeMessage> WeakHandler(Handler);
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, WeakHandler, Kind, Body = MoveTemp(Body)]()
		{
			const double Started = FPlatformTime::Seconds();
			TSharedPtr<FTransactionCodeDataBase> Parsed;
			if (WeakHandler.IsValid()) Parsed = WeakHandler->ParseToStruct(Body);
			const float LatencyMs = static_cast<float>((FPlatformTime::Seconds() - Started) * 1000.0);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Kind, Parsed = MoveTemp(Parsed), LatencyMs]()
			{
				if (WeakThis.IsValid()) WeakThis->CompleteTopicParse(Kind, Parsed, LatencyMs);
			});
		});
		Attempt = -1;
	}
}

void AVirtualSensorExternalSourceHostActor::CompleteTopicParse(
	EVirtualSensorTopicReceiveKind Kind,
	const TSharedPtr<FTransactionCodeDataBase>& ParsedData,
	float ParseLatencyMs)
{
	ActiveReceiverParseCount = FMath::Max(0, ActiveReceiverParseCount - 1);
	FReceiverRuntime* Runtime = FindRuntime(Kind);
	if (!Runtime) return;
	Runtime->bParsing = false;
	if (bEndingPlay || !bTopicReceiversRequested)
	{
		Runtime->PendingBody.Reset();
		return;
	}
	const TSharedPtr<FVirtualSensorTopicReceivedDataBase> Data = StaticCastSharedPtr<FVirtualSensorTopicReceivedDataBase>(ParsedData);
	if (!Data.IsValid())
	{
		++Runtime->Status.ValidationFailureCount;
		Runtime->Status.LastMessage = TEXT("수신 Handler가 결과를 반환하지 않았습니다.");
	}
	else
	{
		Runtime->Status.LastSensorId = Data->SensorId;
		Runtime->Status.LastFrameId = Data->FrameId;
		Runtime->Status.LastMessageBytes = Data->MessageBytes;
		Runtime->Status.LastParseLatencyMs = ParseLatencyMs;
		Runtime->Status.LastMessage = Data->Message;
		Runtime->Status.State = Data->bValid ? EVirtualSensorTopicReceiverState::Active : EVirtualSensorTopicReceiverState::Error;
		if (Data->bValid) ++Runtime->Status.ValidatedCount;
		else ++Runtime->Status.ValidationFailureCount;
		if (Data->bDeepValidated && Data->bValid) ++Runtime->Status.DeepValidationCount;
		if (UTransactionCodeMessage* Handler = FindHandler(Kind)) Handler->ProcessStructData(ParsedData);
		AddReceiveLog(*Runtime, *Data, ParseLatencyMs);
	}
	TryStartQueuedParses();
}

void AVirtualSensorExternalSourceHostActor::AddReceiveLog(
	const FReceiverRuntime& Runtime,
	const FVirtualSensorTopicReceivedDataBase& Data,
	float ParseLatencyMs)
{
	FVirtualSensorTopicReceiveLogEntry Entry;
	Entry.TimestampUtc = FDateTime::UtcNow();
	Entry.Kind = Runtime.Status.Kind;
	Entry.Topic = Runtime.Status.Topic;
	Entry.SensorId = Data.SensorId;
	Entry.FrameId = Data.FrameId;
	Entry.MessageBytes = Data.MessageBytes;
	Entry.ParseLatencyMs = ParseLatencyMs;
	Entry.bValid = Data.bValid;
	Entry.bDeepValidated = Data.bDeepValidated;
	Entry.Message = Data.Message.Left(512);
	RecentTopicReceiveLogs.Insert(MoveTemp(Entry), 0);
	if (RecentTopicReceiveLogs.Num() > 200) RecentTopicReceiveLogs.SetNum(200, false);
}
