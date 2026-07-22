#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorStreamPublisherComponent.generated.h"

class UVirtualSensorTransportComponent;
struct FVirtualSensorTransportResult;

/**
 * Bounded, latest-frame-first publisher shared by every V2 sensor.
 * Acquisition never waits for serialization or network IO: each stream key owns
 * at most one in-flight item and one replaceable latest item.
 */
UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualSensorStreamPublisherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualSensorStreamPublisherComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetTransportComponent(UVirtualSensorTransportComponent* InTransportComponent);
	void SubmitFrame(const FVirtualSensorFrameEnvelope& Frame);
	void PumpPublisherOnce(double NowSeconds);
	static bool SerializePointCloudForTesting(
		const FVirtualSensorFrameEnvelope& Frame,
		const FVirtualSensorStreamConfig& Config,
		FString& OutExtension,
		TArray<uint8>& OutBytes,
		int32& OutPointCount,
		FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Stream")
	void ConfigureStream(const FVirtualSensorStreamConfig& Config);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Stream")
	void StartStream(EVirtualSensorStreamKind StreamKind, const FString& SensorId);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Stream")
	void StopStream(EVirtualSensorStreamKind StreamKind, const FString& SensorId);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Stream")
	void StartAllStreams(const FString& SensorId);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Stream")
	void StopAllStreams(const FString& SensorId);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualSensor|Stream")
	bool IsStreamEnabled(EVirtualSensorStreamKind StreamKind, const FString& SensorId) const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualSensor|Stream")
	TArray<FVirtualSensorStreamStatus> GetStreamStatuses() const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualSensor|Stream")
	TArray<FVirtualSensorTransportLogEntry> GetRecentLogEntries() const { return RecentLogEntries; }

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualSensor|Stream")
	bool ExportDiagnosticReport(FString& OutJsonPath, FString& OutMarkdownPath) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxSubmissionsPerFrame = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream", meta = (ClampMin = "1.0", ClampMax = "1024.0"))
	float BandwidthLimitMegabytesPerSecond = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float ReceiptTimeoutSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float ReconnectCooldownSeconds = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	TObjectPtr<UVirtualSensorTransportComponent> TransportComponent;

private:
	struct FPreparedMessage
	{
		FString SensorId;
		EVirtualSensorStreamKind StreamKind = EVirtualSensorStreamKind::LidarPayload;
		int64 FrameId = 0;
		FString Json;
		int32 ByteCount = 0;
		int32 ConfigRevision = 0;
	};

	struct FStreamRuntime
	{
		FVirtualSensorStreamConfig Config;
		FVirtualSensorStreamStatus Status;
		TOptional<FVirtualSensorFrameEnvelope> PendingFrame;
		TOptional<FPreparedMessage> PreparedMessage;
		double FirstInputSeconds = 0.0;
		double FirstSubmitSeconds = 0.0;
		double LastLazSubmitSeconds = -DBL_MAX;
		double NextSubmitAttemptSeconds = 0.0;
		bool bSerializationInFlight = false;
		int32 ConfigRevision = 0;
	};

	struct FReceiptWait
	{
		FString StreamKey;
		double SubmittedSeconds = 0.0;
	};

	FString MakeStreamKey(EVirtualSensorStreamKind StreamKind, const FString& SensorId) const;
	FStreamRuntime& FindOrAddRuntime(EVirtualSensorStreamKind StreamKind, const FString& SensorId);
	void QueueFrameForRuntime(const FString& StreamKey, FStreamRuntime& Runtime, const FVirtualSensorFrameEnvelope& Frame);
	void StartPointCloudSerialization(const FString& StreamKey, FStreamRuntime& Runtime, const FVirtualSensorFrameEnvelope& Frame);
	void CompletePointCloudSerialization(const FString& StreamKey, FPreparedMessage&& Message, const FString& Error, int32 CapturedConfigRevision);
	void PumpPreparedMessages(double NowSeconds);
	void CheckReceiptTimeouts(double NowSeconds);
	void AddLog(const FString& StreamKey, const FString& State, const FString& Message, const FVirtualSensorTransportResult* Result = nullptr, int64 FrameId = 0);
	void HandleTransportResult(const FVirtualSensorTransportResult& Result);
	void UpdateCameraStreamDemand();
	static bool StreamMatchesFrame(EVirtualSensorStreamKind StreamKind, EVirtualSensorKind SensorKind);

	UFUNCTION()
	void OnTransportResult(const FVirtualSensorTransportResult& Result);

	TMap<FString, FStreamRuntime> StreamRuntimes;
	TMap<FString, FReceiptWait> WaitingReceipts;
	TMap<FString, FString> RequestToStreamKey;
	TArray<FVirtualSensorTransportLogEntry> RecentLogEntries;
	int32 RoundRobinCursor = 0;
	int32 ConsecutiveReceiptTimeouts = 0;
	double LastReconnectSeconds = -DBL_MAX;
	double TokenBucketBytes = 0.0;
	double LastTokenUpdateSeconds = 0.0;
	bool bEndingPlay = false;
};
