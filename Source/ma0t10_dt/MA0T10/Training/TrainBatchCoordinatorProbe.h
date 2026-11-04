#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TrainBatchCoordinatorProbe.generated.h"
struct FTrainCoordinatorProbeState;

/** Explicit local headless entry point for restart/worker acceptance. No listener or default work. */
UCLASS()
class MA0T10_DT_API UTrainBatchCoordinatorProbe : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;

private:
	TSharedPtr<FTrainCoordinatorProbeState> State;
	FTimerHandle Timer;
	void Pump();
	void Finish(bool Success, const FString& Error);
};
