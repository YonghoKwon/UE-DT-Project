#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "SlabScenarioReplayTestAdapter.generated.h"
/** Test-only adapter. Production projects supply their own playback implementation. */
UCLASS(NotPlaceable,Transient)
class MA0T10_DT_API ASlabScenarioReplayTestAdapter : public AActor, public ISlabScenarioPlaybackAdapter
{
	GENERATED_BODY()
public:
	ASlabScenarioReplayTestAdapter();
	virtual bool StartScenarioPlayback_Implementation(const FString& Json,const FString& ScenarioUUID,const FString& RunUUID) override;
	virtual void Tick(float Delta) override;
	static FString MakeBulkJson(const FString& UUID);
	int32 CompletedRuns=0;
	int32 AppliedCount=0;
	bool bFailed=false;
	FString LastReceivedJson,LastScenarioUUID,LastRunUUID;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Mesh;
	TArray<TSharedPtr<class FJsonValue>> Rows;
	double StartedSeconds=0;
	int32 LastIndex=-1;
	bool bPlaying=false;
};
