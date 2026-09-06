#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VirtualSlabFrameContext.h"
#include "VirtualSensorSlabContextSubsystem.generated.h"
class AVirtualSensorActorBase;
class AVirtualSensorCoordinator;

/** Adapter for the colleague-owned scenario player. No Topic subscription or scene mutation. */
UCLASS()
class MA0T10_DT_API UVirtualSensorSlabContextSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SlabSensorSession")
	FString BeginSlabSensorSession(const FString& RunId, const TArray<FString>& TargetSensorIds);
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
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;
private:
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
