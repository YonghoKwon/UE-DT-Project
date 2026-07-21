#pragma once

#include "CoreMinimal.h"
#include "Core/DxWebSocketSubsystem.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualSensorStreamReceiverTypes.h"
#include "VirtualSensorExternalSourceHostActor.generated.h"

class USceneComponent;
class ULidarCsvReplaySourceComponent;
class ULidarJsonLinesReplaySourceComponent;
class ULidarJsonLiveSourceComponent;
class ULidarHttpJsonLiveSourceComponent;
class ULidarUdpJsonLiveSourceComponent;
class UCameraJsonLiveSourceComponent;
class UTransactionCodeMessage;
class UVirtualLidarStreamReceiverTC;
class UVirtualCameraStreamReceiverTC;
class UVirtualPointCloudStreamReceiverTC;

/** Test-map host that exposes every implemented external source without auto-starting network listeners. */
UCLASS(BlueprintType)
class MA0T10_DT_API AVirtualSensorExternalSourceHostActor : public AActor
{
	GENERATED_BODY()

public:
	AVirtualSensorExternalSourceHostActor();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	void StartTopicReceivers();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	void StopTopicReceivers();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	void ReconnectTopicReceivers();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	void ConfigureReceiverTopics(const FString& InLidarTopic, const FString& InCameraTopic, const FString& InPointCloudTopic);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	TArray<FVirtualSensorTopicReceiverStatus> GetTopicReceiverStatuses() const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	const TArray<FVirtualSensorTopicReceiveLogEntry>& GetRecentTopicReceiveLogs() const { return RecentTopicReceiveLogs; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	bool AreTopicReceiversRequested() const { return bTopicReceiversRequested; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	FString GetReceiverBrokerUrl() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarCsvReplaySourceComponent> LidarCsvReplay;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarJsonLinesReplaySourceComponent> LidarJsonLinesReplay;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarJsonLiveSourceComponent> LidarBufferedJson;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarHttpJsonLiveSourceComponent> LidarHttpJson;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarUdpJsonLiveSourceComponent> LidarUdpJson;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<UCameraJsonLiveSourceComponent> CameraBufferedJson;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	TObjectPtr<UVirtualLidarStreamReceiverTC> LidarTopicHandler;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	TObjectPtr<UVirtualCameraStreamReceiverTC> CameraTopicHandler;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	TObjectPtr<UVirtualPointCloudStreamReceiverTC> PointCloudTopicHandler;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	bool bAutoStartTopicReceivers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	FString LidarReceiveTopic = TEXT("topic.virtual.sensor.lidar.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	FString CameraReceiveTopic = TEXT("topic.virtual.sensor.camera.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ExternalSource|TopicReceiver")
	FString PointCloudReceiveTopic = TEXT("topic.virtual.sensor.export.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ExternalSource|TopicReceiver", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float InitialSubscribeDelaySeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ExternalSource|TopicReceiver", meta = (ClampMin = "1", ClampMax = "3"))
	int32 MaxConcurrentReceiverParses = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|ExternalSource|TopicReceiver", meta = (ClampMin = "1024", ClampMax = "16777216"))
	int32 MaxReceiverMessageBytes = 8388608;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FReceiverRuntime
	{
		FVirtualSensorTopicReceiverStatus Status;
		FString SubscriptionId;
		TOptional<FString> PendingBody;
		bool bSubscriptionPending = false;
		bool bParsing = false;
		int32 RetryAttempt = 0;
		double SubscriptionStartedSeconds = 0.0;
	};

	UFUNCTION()
	void HandleLidarTopicMessage(const FWebSocketMessage& Message);

	UFUNCTION()
	void HandleCameraTopicMessage(const FWebSocketMessage& Message);

	UFUNCTION()
	void HandlePointCloudTopicMessage(const FWebSocketMessage& Message);

	UFUNCTION()
	void HandleLidarSubscriptionCompleted(bool bSuccess, FString Error);

	UFUNCTION()
	void HandleCameraSubscriptionCompleted(bool bSuccess, FString Error);

	UFUNCTION()
	void HandlePointCloudSubscriptionCompleted(bool bSuccess, FString Error);

	void InitializeTopicReceiverRuntime();
	void AttemptTopicSubscriptions();
	void SubscribeRuntime(EVirtualSensorTopicReceiveKind Kind);
	void CompleteSubscription(EVirtualSensorTopicReceiveKind Kind, bool bSuccess, const FString& Error);
	void QueueTopicPayload(EVirtualSensorTopicReceiveKind Kind, FString Body);
	void TryStartQueuedParses();
	void CompleteTopicParse(EVirtualSensorTopicReceiveKind Kind, const TSharedPtr<FTransactionCodeDataBase>& ParsedData, float ParseLatencyMs);
	void ScheduleSubscriptionRetry();
	void AddReceiveLog(const FReceiverRuntime& Runtime, const FVirtualSensorTopicReceivedDataBase& Data, float ParseLatencyMs);
	FReceiverRuntime* FindRuntime(EVirtualSensorTopicReceiveKind Kind);
	const FReceiverRuntime* FindRuntime(EVirtualSensorTopicReceiveKind Kind) const;
	UTransactionCodeMessage* FindHandler(EVirtualSensorTopicReceiveKind Kind) const;
	FString GetTopic(EVirtualSensorTopicReceiveKind Kind) const;

	TArray<FReceiverRuntime> ReceiverRuntimes;
	UPROPERTY(Transient)
	TArray<FVirtualSensorTopicReceiveLogEntry> RecentTopicReceiveLogs;
	FTimerHandle TopicReceiverRetryTimer;
	int32 ActiveReceiverParseCount = 0;
	int32 ReceiverRoundRobinCursor = 0;
	bool bTopicReceiversRequested = false;
	bool bEndingPlay = false;
};
