#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TrainBatchWorkerSubsystem.generated.h"
struct FTrainBatchWorkerState;

/** Only created by the explicit detached worker command line. No UI/ordinary input subscription. */
UCLASS()
class MA0T10_DT_API UTrainBatchWorkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;

private:
	TSharedPtr<FTrainBatchWorkerState> State;
	FTimerHandle Timer;
	void Pump();
	void Finish(const FString& Phase, const FString& Error = FString());
};
