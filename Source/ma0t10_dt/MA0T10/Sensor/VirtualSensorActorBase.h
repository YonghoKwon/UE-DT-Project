#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorActorBase.generated.h"

class UTexture;
class UVirtualSensorOutputComponent;
class UVirtualSensorRecorderComponent;
class UVirtualSensorTransportComponent;
struct FVirtualSensorEditableState;

UCLASS(Abstract, BlueprintType)
class MA0T10_DT_API AVirtualSensorActorBase : public AActor
{
	GENERATED_BODY()

public:
	AVirtualSensorActorBase();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor")
	virtual FString GetSensorId() const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor")
	virtual EVirtualSensorKind GetSensorKind() const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor")
	virtual bool IsSensorRunning() const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor")
	virtual void StartSensor();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor")
	virtual void StopSensor();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor")
	virtual void CaptureSensorOnce();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor")
	virtual void RefreshSensorPreview();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor")
	virtual UTexture* GetSensorPreviewTexture() const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor")
	virtual FVirtualSensorRuntimeStatus GetSensorRuntimeStatus() const;

	virtual bool SubmitExternalFrame(const FVirtualSensorFrameEnvelope& Frame, bool bSendTransport);
	virtual bool ReadEditableState(FVirtualSensorEditableState& OutState) const;
	virtual bool ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const;
	virtual bool ApplyEditableState(const FVirtualSensorEditableState& State, FString& OutError);
	virtual bool ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError);

	void SetSharedOutputServices(UVirtualSensorTransportComponent* Transport, UVirtualSensorRecorderComponent* Recorder);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|Sensor")
	TObjectPtr<UVirtualSensorOutputComponent> OutputComponent;
};
