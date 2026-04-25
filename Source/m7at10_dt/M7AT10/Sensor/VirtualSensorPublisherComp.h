#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualSensorTypes.h"
#include "VirtualSensorPublisherComp.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualSensorPublisherComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualSensorPublisherComp();

	UFUNCTION(BlueprintCallable, Category = "VirtualSensor|Publish")
	void PublishSensorPacket(const FVirtualSensorPacket& Packet);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor|Publish")
	bool bEnableHttpPublish = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor|Publish")
	FString EndpointUrl = TEXT("http://127.0.0.1:8080/digital-twin/sensor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor|Publish")
	FString HttpMethod = TEXT("POST");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor|Publish")
	TMap<FString, FString> AdditionalHeaders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor|Publish")
	bool bLogPayload = false;

private:
	void OnPublishResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
};
