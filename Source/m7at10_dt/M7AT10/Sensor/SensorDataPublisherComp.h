#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SensorDataPublisherComp.generated.h"

UENUM(BlueprintType)
enum class ESensorTransportMode : uint8
{
	Disabled UMETA(DisplayName = "Disabled"),
	LogOnly UMETA(DisplayName = "Log Only"),
	HttpPost UMETA(DisplayName = "HTTP POST")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSensorPacketPublished, FName, SensorType, const FString&, SensorName, bool, bSuccess, const FString&, Message);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API USensorDataPublisherComp : public UActorComponent
{
	GENERATED_BODY()

public:
	USensorDataPublisherComp();

	UFUNCTION(BlueprintCallable, Category = "Sensor|Publisher")
	void PublishPacket(FName SensorType, const FString& SensorName, const FString& JsonPayload);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Publisher")
	ESensorTransportMode TransportMode = ESensorTransportMode::LogOnly;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Publisher", meta = (EditCondition = "TransportMode == ESensorTransportMode::HttpPost"))
	FString EndpointUrl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Publisher", meta = (EditCondition = "TransportMode == ESensorTransportMode::HttpPost"))
	TMap<FString, FString> AdditionalHeaders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Publisher", meta = (EditCondition = "TransportMode == ESensorTransportMode::HttpPost", ClampMin = "0.5", UIMin = "0.5"))
	float HttpTimeoutSeconds = 5.0f;

	UPROPERTY(BlueprintAssignable, Category = "Sensor|Publisher")
	FOnSensorPacketPublished OnPacketPublished;
};
