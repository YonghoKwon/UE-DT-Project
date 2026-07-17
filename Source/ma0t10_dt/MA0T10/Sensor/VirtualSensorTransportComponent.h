#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualSensorTransportComponent.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorTransportMode : uint8
{
    None UMETA(DisplayName = "None"),
    LogOnly UMETA(DisplayName = "Log Only"),
    SaveToFile UMETA(DisplayName = "Save To File"),
	HttpPost UMETA(DisplayName = "HTTP POST"),
	StompWebSocket UMETA(DisplayName = "Artemis STOMP over WebSocket")
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorTransportProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
	FString BrokerUrl = TEXT("ws://127.0.0.1:61616");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
	FString CameraTopic = TEXT("topic.virtual.sensor.camera.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
	FString LidarTopic = TEXT("topic.virtual.sensor.lidar.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
	FString ExportTopic = TEXT("topic.virtual.sensor.export.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
	FString AckTopic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
	FString UserName = TEXT("artemis");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
	FString HttpEndpoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport", meta = (ClampMin = "1024"))
	int32 MaxMessageBytes = 8388608;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport", meta = (ClampMin = "1", ClampMax = "120"))
	int32 TimeoutSeconds = 30;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorTransportResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    bool bSubmitted = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    bool bAccepted = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    bool bBackpressureRejected = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    bool bRetryExhausted = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    int32 DataLength = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    int32 HttpStatusCode = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    int32 RetryAttemptCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    FString SavedFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    FString ResponseBody;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
	FString Protocol;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
	FString RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
	FString SensorId;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
	FString SensorType;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
	FString DataKind;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
	FString Destination;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
	float LatencyMs = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorDataSent, const FVirtualSensorTransportResult&, Result);

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualSensorTransportComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVirtualSensorTransportComponent();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorTransport")
    FVirtualSensorTransportResult SendJson(const FString& SensorId, const FString& SensorType, const FString& JsonText);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorTransport")
    FVirtualSensorTransportResult SendBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorTransport")
	FVirtualSensorTransportResult SendJsonRequest(const FString& SensorId, const FString& SensorType, const FString& DataKind, int64 FrameId, const FString& JsonText, bool bManualRequest = false);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorTransport")
	void ConfigureTransportProfile(const FVirtualSensorTransportProfile& InProfile);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorTransport")
	const FVirtualSensorTransportProfile& GetTransportProfile() const { return TransportProfile; }

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorTransport")
	void SetSessionCredentials(const FString& InPasscode, const FString& InBearerToken);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorTransport")
	FVirtualSensorTransportResult TestConnection();

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|SensorTransport")
    FOnVirtualSensorDataSent OnDataSent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
    EVirtualSensorTransportMode TransportMode = EVirtualSensorTransportMode::LogOnly;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport", meta = (EditCondition = "TransportMode == EVirtualSensorTransportMode::HttpPost"))
    FString HttpEndpoint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
    FString SaveSubDirectory = TEXT("SensorCaptures");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
    int32 HttpTimeoutSeconds = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport", meta = (ClampMin = "1", EditCondition = "TransportMode == EVirtualSensorTransportMode::HttpPost"))
    int32 MaxInFlightHttpRequests = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport", meta = (ClampMin = "0", ClampMax = "5", EditCondition = "TransportMode == EVirtualSensorTransportMode::HttpPost"))
    int32 MaxHttpRetryAttempts = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport", meta = (EditCondition = "TransportMode == EVirtualSensorTransportMode::HttpPost"))
    bool bRetryOnConnectionFailure = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport", meta = (EditCondition = "TransportMode == EVirtualSensorTransportMode::HttpPost"))
    bool bRetryOnServerError = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
    bool bLogHttpResponse = true;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    int32 InFlightHttpRequestCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    int32 BackpressureRejectedRequestCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    int32 TotalHttpRetryAttemptCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    int32 FailedHttpRequestCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    int32 RetryExhaustedRequestCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    FVirtualSensorTransportResult LastResult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorTransport")
	FVirtualSensorTransportProfile TransportProfile;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorTransport")
	bool IsStompConnected() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FVirtualSensorTransportResult SendHttp(const FString& SensorId, const FString& SensorType, const FString& JsonText);
    bool SubmitHttpAttempt(const FString& SensorId, const FString& SensorType, const FString& JsonText, int32 AttemptIndex, int32 SubmittedDataLength);
    FVirtualSensorTransportResult SendHttpBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes);
    FVirtualSensorTransportResult SaveJson(const FString& SensorId, const FString& SensorType, const FString& JsonText) const;
    FVirtualSensorTransportResult SaveBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes) const;
    FString BuildSavePath(const FString& SensorId, const FString& SensorType, const FString& Extension) const;
	FVirtualSensorTransportResult SendStomp(const FString& SensorId, const FString& SensorType, const FString& DataKind, int64 FrameId, const FString& JsonText);
	void EnsureStompClient();
	void HandleStompConnected(const FString& ProtocolVersion, const FString& SessionId, const FString& ServerString);
	void HandleStompFailure(const FString& Error);
	FString ResolveTopic(const FString& SensorType, const FString& DataKind) const;

	TSharedPtr<class IStompClient> StompClient;
	FString SessionPasscode;
	FString SessionBearerToken;
	double LastStompSubmitSeconds = 0.0;
};
