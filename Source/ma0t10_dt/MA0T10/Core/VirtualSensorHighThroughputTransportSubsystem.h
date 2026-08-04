#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "VirtualSensorHighThroughputTransportSubsystem.generated.h"

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorHighThroughputProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Transport")
	FString BrokerUrl = TEXT("tcp://127.0.0.1:61616");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Transport")
	FString UserName = TEXT("artemis");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Transport")
	FString CameraTopic = TEXT("topic.virtual.sensor.camera.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Transport")
	FString LidarTopic = TEXT("topic.virtual.sensor.lidar.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Transport")
	FString PointCloudTopic = TEXT("topic.virtual.sensor.export.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Transport", meta = (ClampMin = "1", ClampMax = "120"))
	int32 ConnectTimeoutSeconds = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Transport", meta = (ClampMin = "100", ClampMax = "60000"))
	int32 HeartbeatIntervalMs = 5000;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorStreamTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") EVirtualSensorStreamKind StreamKind = EVirtualSensorStreamKind::LidarPayload;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") FString SensorId;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") FString State;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") FString Message;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 EnqueuedCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 SubmittedCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 ReceiptCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 ConsumerReceivedCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 ValidationFailureCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 FrameGapCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 DuplicateCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 RetryCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 OverloadCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int32 InputQueueDepth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int32 ReceiptQueueDepth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int64 LastFrameId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") int32 LastFrameBytes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") float SubmittedHz = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") float ConsumerHz = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") float LastSocketWriteLatencyMs = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") float LastReceiptLatencyMs = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") float LastEndToEndLatencyMs = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport") float EndToEndP95LatencyMs = 0.0f;

	double FirstSubmittedSeconds = 0.0;
	double FirstConsumerSeconds = 0.0;
	TArray<float> EndToEndLatencySamples;
};

/** Shared immutable frame handed directly to the socket worker. */
struct MA0T10_DT_API FVirtualSensorBinaryFrame
{
	EVirtualSensorStreamKind StreamKind = EVirtualSensorStreamKind::LidarPayload;
	FString SensorId;
	int64 FrameId = 0;
	FDateTime TimestampUtc;
	FString Schema;
	FString ContentType;
	FString Destination;
	FString RequestId;
	TMap<FString, FString> Headers;
	TSharedPtr<const TArray<uint8>, ESPMode::ThreadSafe> Body32;
	TSharedPtr<const TArray64<uint8>, ESPMode::ThreadSafe> Body64;

	int64 NumBytes() const { return Body64.IsValid() ? Body64->Num() : (Body32.IsValid() ? Body32->Num() : 0); }
	const uint8* GetData() const { return Body64.IsValid() ? Body64->GetData() : (Body32.IsValid() ? Body32->GetData() : nullptr); }
};

class FVirtualSensorHighThroughputTransportWorker;

/** World facade for a binary-safe Raw TCP STOMP worker. */
UCLASS()
class MA0T10_DT_API UVirtualSensorHighThroughputTransportSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UVirtualSensorHighThroughputTransportSubsystem();
	virtual ~UVirtualSensorHighThroughputTransportSubsystem() override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Transport")
	bool StartHighThroughputTransport(const FVirtualSensorHighThroughputProfile& Profile, const FString& SessionPasscode);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Transport")
	void StopHighThroughputTransport();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualSensor|Transport")
	bool IsHighThroughputTransportRunning() const;

	bool EnqueueBinaryFrame(const FVirtualSensorBinaryFrame& Frame, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualSensor|Transport")
	TArray<FVirtualSensorStreamTelemetry> GetStreamTelemetry() const;

	static bool CanUseRawTcp(const FString& BrokerUrl, FString* OutReason = nullptr);
	static FVirtualSensorHighThroughputProfile MakeProfile(const FVirtualSensorTransportProfile& Profile);

private:
	void DrainWorkerEvents();
	FVirtualSensorHighThroughputTransportWorker* Worker = nullptr;
	TMap<FString, FVirtualSensorStreamTelemetry> TelemetryByKey;
	FVirtualSensorHighThroughputProfile ActiveProfile;
	FString ActivePasscode;
};
