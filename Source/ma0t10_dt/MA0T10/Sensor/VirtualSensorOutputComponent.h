#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorOutputComponent.generated.h"

class UVirtualSensorRecorderComponent;
class UVirtualSensorTransportComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVirtualSensorFrameRouted, const FString&, SensorId, int64, FrameId);

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualSensorOutputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualSensorOutputComponent();

	void SetSharedServices(UVirtualSensorTransportComponent* InTransport, UVirtualSensorRecorderComponent* InRecorder);
	bool RouteFrame(const FVirtualSensorFrameEnvelope& Frame);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorOutput")
	FString GetLastJsonPayload() const { return LastJsonPayload; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorOutput")
	int64 GetLastRoutedFrameId() const { return LastRoutedFrameId; }

	UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|SensorOutput")
	FOnVirtualSensorFrameRouted OnFrameRouted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorOutput")
	TObjectPtr<UVirtualSensorTransportComponent> TransportComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorOutput")
	TObjectPtr<UVirtualSensorRecorderComponent> RecorderComponent;

private:
	FString BuildSensorType(EVirtualSensorKind Kind) const;

	FString LastJsonPayload;
	FString LastRoutedSensorId;
	int64 LastRoutedFrameId = INDEX_NONE;
};
