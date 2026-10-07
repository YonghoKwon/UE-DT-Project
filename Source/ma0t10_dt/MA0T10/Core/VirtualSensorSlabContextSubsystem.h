#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VirtualSlabFrameContext.h"
#include "VirtualSensorSlabContextSubsystem.generated.h"
class AVirtualSensorActorBase;
class AVirtualSensorCoordinator;
enum class EVirtualSensorStreamKind : uint8;

/** Adapter for the colleague-owned scenario player. No Topic subscription or scene mutation. */
UCLASS()
class MA0T10_DT_API UVirtualSensorSlabContextSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SlabSensorSession")
	FString BeginSlabSensorSession(const FString& RunId, const TArray<FString>& TargetSensorIds, UPARAM(DisplayName="PCD만 송신") bool bPointCloudOnly=false);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SlabSensorSession")
	FString BeginReplaySensorSession(const FString& RunId,const TArray<FString>& TargetSensorIds,const FString& ScenarioUUID,bool bSendPcd=false);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SlabSensorSession")
	FString BeginScenarioSensorSession(const FString& RunId,const TArray<FString>& TargetSensorIds,const FString& ScenarioUUID,const FVirtualSlabSensorOutputSelection& Outputs);
	/** Read-only configuration check; does not imply a Broker connection/receipt. */
	UFUNCTION(BlueprintPure, Category="DigitalTwin|SlabSensorSession")
	bool ValidateScenarioOutputs(const TArray<FString>& TargetSensorIds,const FVirtualSlabSensorOutputSelection& Outputs,FString& Reason) const;
	/** Motion-only fallback: never discovers, starts or stops sensors/streams. */
	FString BeginUnboundObservationSession(const FString& RunId,const FString& ScenarioUUID);
	bool PrepareScenarioTransmission(const FString& RunId,const TArray<FString>& Ids,const FString& ScenarioUUID,const FVirtualSlabSensorOutputSelection& Outputs,const FSlabExecutionOptions& Options,FString& Error);
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SlabSensorSession") bool IsSensorConfigurationLocked(const FString& SensorId) const;
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SlabSensorSession")
	bool NotifySlabFrameApplied(const FString& RunId, const FString& MtlNo, int64 SlabFrameNo, double ElapsedSec);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SlabSensorSession")
	bool SetSlabSensorSessionPaused(const FString& RunId, bool bPaused);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SlabSensorSession")
	bool EndSlabSensorSession(const FString& RunId, bool bAborted);
	UFUNCTION(BlueprintPure, Category="DigitalTwin|SlabSensorSession")
	FVirtualSlabSessionStatus GetSlabSensorSessionStatus() const { return Status; }
	FVirtualSlabFrameContext CaptureContext(const FString& SensorId, int64 SensorFrameId);
	void CompleteAcquisition(const FString& SensorId, int64 SensorFrameId, bool bSuccess=true);
	bool ControlsSensor(const FString& SensorId) const { return ControlledIds.Contains(SensorId); }
	bool AllowsFrame(const FString& SensorId, const FVirtualSlabFrameContext& Context) const;
	bool AllowsStreamFrame(const FString& SensorId, EVirtualSensorStreamKind Kind, const FVirtualSlabFrameContext& Context) const;
	bool AllowsStreamDemand(const FString& SensorId, EVirtualSensorStreamKind Kind) const;
	bool HasSelectedOutputForSensor(const FString& SensorId) const;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;
private:
	TArray<FString> PreparationIds;
	FSlabExecutionOptions ExecutionOptions;
	TMap<FString,FString> ConfigurationSnapshots;
	uint32 TransportConfigurationHash=0;
	bool bRequireRaw=false,bRequireEngine=false;
	int64 RawConnectionRevision=0,EngineConnectionRevision=0;
	double PreparationStarted=0,RunningStartedWorld=0;
	float RequiredCheckAccumulator=0;
	bool IsTransmissionReady() const;
	FString CheckRequiredDataHealth() const;
	static FString SensorConfiguration(const AVirtualSensorActorBase* Sensor);
	uint32 CurrentTransportHash() const;
	void FailPreparation(const FString& Error);
	FString BeginSessionInternal(const FString& RunId,const TArray<FString>& TargetSensorIds,const FVirtualSlabSensorOutputSelection& Outputs,const FString& ScenarioUUID,bool bRequireEachRequestedKind=true);
	bool IsOutputSelected(EVirtualSensorStreamKind Kind) const;
	bool CheckRun(const FString& RunId);
	void Finish(EVirtualSlabSessionEndReason Reason);
	int64 CountStreamErrors() const;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSlabSessionFailureTest;
#endif
	UPROPERTY(Transient) FVirtualSlabSessionStatus Status;
	TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>> Targets;
	TSet<FString> ControlledIds;
	TSet<FString> PendingKeys;
	TSet<FString> StartedSensors;
	TSet<FString> UsedRunIds;
	TWeakObjectPtr<AVirtualSensorCoordinator> Coordinator;
	int32 Generation = 0;
	double DrainStarted = 0;
	int64 InitialStreamErrors=0;
};
