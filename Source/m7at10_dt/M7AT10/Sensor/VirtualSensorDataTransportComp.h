#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualSensorDataTransportComp.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorTransportMode : uint8
{
    None UMETA(DisplayName = "None"),
    LogOnly UMETA(DisplayName = "Log Only"),
    SaveToFile UMETA(DisplayName = "Save To File"),
    HttpPost UMETA(DisplayName = "HTTP POST")
};

USTRUCT(BlueprintType)
struct M7AT10_DT_API FVirtualSensorTransportResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    bool bSubmitted = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    bool bAccepted = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorTransport")
    bool bBackpressureRejected = false;

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
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorDataSent, const FVirtualSensorTransportResult&, Result);

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualSensorDataTransportComp : public UActorComponent
{
    GENERATED_BODY()

public:
    UVirtualSensorDataTransportComp();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorTransport")
    FVirtualSensorTransportResult SendJson(const FString& SensorId, const FString& SensorType, const FString& JsonText);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorTransport")
    FVirtualSensorTransportResult SendBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes);

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
    FVirtualSensorTransportResult LastResult;

private:
    FVirtualSensorTransportResult SendHttp(const FString& SensorId, const FString& SensorType, const FString& JsonText);
    bool SubmitHttpAttempt(const FString& SensorId, const FString& SensorType, const FString& JsonText, int32 AttemptIndex, int32 SubmittedDataLength);
    FVirtualSensorTransportResult SaveJson(const FString& SensorId, const FString& SensorType, const FString& JsonText) const;
    FVirtualSensorTransportResult SaveBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes) const;
    FString BuildSavePath(const FString& SensorId, const FString& SensorType, const FString& Extension) const;
};
