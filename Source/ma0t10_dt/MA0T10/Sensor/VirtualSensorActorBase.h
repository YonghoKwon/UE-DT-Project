#pragma once

#include "CoreMinimal.h"
#include "InteractableActor/InteractableActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorActorBase.generated.h"

class UTexture;
class UVirtualSensorOutputComponent;
class UVirtualSensorRecorderComponent;
class UVirtualSensorTransportComponent;
class UVirtualSensorStreamPublisherComponent;
class AVirtualSensorActorBase;
class UStaticMeshComponent;
DECLARE_MULTICAST_DELEGATE_OneParam(FVirtualSensorActivated,AVirtualSensorActorBase*);
struct FVirtualSensorEditableState;

UCLASS(Abstract, BlueprintType)
class MA0T10_DT_API AVirtualSensorActorBase : public AInteractableActor
{
	GENERATED_BODY()

public:
	AVirtualSensorActorBase();
	virtual void Click() override;
	FVirtualSensorActivated OnSensorActivated;
	/** Explicit workspace opt-in; query-only placement marker never participates in sensor traces/captures. */
	void SetPlacementPickingEnabled(bool Enabled);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor")
	virtual FString GetSensorId() const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor")
	virtual EVirtualSensorKind GetSensorKind() const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor")
	virtual bool IsSensorRunning() const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|Sensor") bool CanEditSensorConfiguration(FString& OutReason) const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|Sensor|Placement") bool CanEditSensorPlacement(FString& OutReason) const;
	/** Commits the operator gesture and restores acquisition settings before data admission. */
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|Sensor|Placement") bool PrepareForDataCollection(FString& OutReason);
	FSimpleMulticastDelegate OnDataCollectionStarting;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|Sensor") int32 GetConfigurationRevision() const { return ConfigurationRevision; }

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor")
	virtual void StartSensor();
	uint64 GetRunControlRevision() const { return SensorRunControlRevision; }

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
	/** Intended acquisition configuration, without ending lightweight placement preview. */
	virtual bool ReadCollectionState(FVirtualSensorEditableState& OutState, bool& bPreviewEnabled) const;
	virtual bool ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const;
	virtual bool ApplyEditableState(const FVirtualSensorEditableState& State, FString& OutError);
	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor|Interaction")
	virtual bool ApplyEditableTransform(const FTransform& Transform, FString& OutError);
	virtual bool ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor|Interaction")
	virtual bool BeginInteractiveManipulation(const FVirtualSensorInteractionRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor|Interaction")
	virtual bool UpdateInteractiveTransform(const FTransform& Transform);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|Sensor|Interaction")
	virtual void EndInteractiveManipulation();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|Sensor|Interaction")
	bool IsInteractiveManipulationActive() const { return bInteractiveManipulationActive; }

	void SetSharedOutputServices(UVirtualSensorTransportComponent* Transport, UVirtualSensorRecorderComponent* Recorder, UVirtualSensorStreamPublisherComponent* StreamPublisher = nullptr);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|Sensor")
	TObjectPtr<UVirtualSensorOutputComponent> OutputComponent;

protected:
	uint64 SensorRunControlRevision=0;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> PlacementPickMarker;
	int32 ConfigurationRevision=0;
	bool bSettingsTransaction=false;
	bool bInteractiveManipulationActive = false;
};
