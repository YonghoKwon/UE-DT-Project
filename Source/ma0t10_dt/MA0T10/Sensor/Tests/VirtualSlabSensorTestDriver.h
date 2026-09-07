#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSlabSensorTestDriver.generated.h"

/** Transient fixture only: emulates a colleague's bulk scenario player. Never modifies the map asset. */
UCLASS(NotPlaceable, Transient)
class MA0T10_DT_API AVirtualSlabSensorTestDriver : public AActor
{
	GENERATED_BODY()
public:
	AVirtualSlabSensorTestDriver();
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool StartTest(int32 Runs, const TArray<FString>& SensorIds, bool bOnlyPointCloud=false);
	static FString MakeSyntheticBulkJson();
	bool IsFinished() const { return bFinished; }
	bool HasFailed() const { return bFailed; }
	int32 GetCompletedRuns() const { return CompletedRuns; }
	FString GetRunId() const { return RunId; }
private:
	bool BeginRun();
	void WriteReport();
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> SlabMesh;
	TArray<TSharedPtr<class FJsonValue>> Frames;
	TArray<TSharedPtr<class FJsonValue>> RunReports;
	TArray<FString> Targets;
	FString RunId;
	int32 RunCount=1;
	int32 CompletedRuns=0;
	int32 LastApplied=-1;
	int32 AppliedCount=0;
	int32 SkippedCount=0;
	double StartSeconds=0;
	bool bEndingRun=false;
	bool bFinished=false;
	bool bFailed=false;
	bool bPointCloudOnly=false;
};
