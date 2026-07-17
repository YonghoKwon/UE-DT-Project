#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorExternalSourceHostActor.generated.h"

class USceneComponent;
class ULidarCsvReplaySourceComponent;
class ULidarJsonLinesReplaySourceComponent;
class ULidarJsonLiveSourceComponent;
class ULidarHttpJsonLiveSourceComponent;
class ULidarUdpJsonLiveSourceComponent;
class UCameraJsonLiveSourceComponent;

/** Test-map host that exposes every implemented external source without auto-starting network listeners. */
UCLASS(BlueprintType)
class MA0T10_DT_API AVirtualSensorExternalSourceHostActor : public AActor
{
	GENERATED_BODY()

public:
	AVirtualSensorExternalSourceHostActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarCsvReplaySourceComponent> LidarCsvReplay;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarJsonLinesReplaySourceComponent> LidarJsonLinesReplay;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarJsonLiveSourceComponent> LidarBufferedJson;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarHttpJsonLiveSourceComponent> LidarHttpJson;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<ULidarUdpJsonLiveSourceComponent> LidarUdpJson;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|ExternalSource") TObjectPtr<UCameraJsonLiveSourceComponent> CameraBufferedJson;

protected:
	virtual void BeginPlay() override;
};
