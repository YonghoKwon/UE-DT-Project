#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorAcquisitionContext.h"
#include "VirtualSlabContextCompatibility.generated.h"

// Legacy reflected value contracts: keep names, properties, defaults and Blueprint pins.
// Scenario session state stays in Scenario/; internal frames use the native context.
/** Per-run automatic Topic output policy. Local capture/recording is independent. */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSlabSensorOutputSelection
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPointCloud = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCameraImage = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLidarTelemetry = false;
	bool HasAnyOutput() const { return bPointCloud || bCameraImage || bLidarTelemetry; }
	static FVirtualSlabSensorOutputSelection ObservationOnly() { FVirtualSlabSensorOutputSelection V; V.bPointCloud=false; return V; }
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSlabFrameContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString RunId;
	UPROPERTY(BlueprintReadOnly) FString ScenarioUUID;
	UPROPERTY(BlueprintReadOnly) FString MtlNo;
	UPROPERTY(BlueprintReadOnly) int64 SlabFrameNo = -1;
	UPROPERTY(BlueprintReadOnly) double ElapsedSec = 0;
	UPROPERTY(BlueprintReadOnly) int32 Generation = 0;
	UPROPERTY(BlueprintReadOnly) int32 Segment = 0;
	UPROPERTY(BlueprintReadOnly) bool bEligible = false;
	UPROPERTY(BlueprintReadOnly) int64 AcquisitionUtcTicks = 0;
	TMap<FString,FString> ToHeaders() const;
	FVirtualSlabFrameContext() = default;
	FVirtualSlabFrameContext(const FVirtualSensorAcquisitionContext& Context)
	{
		RunId = Context.RunId;
		ScenarioUUID = Context.ScenarioUUID;
		MtlNo = Context.MtlNo;
		SlabFrameNo = Context.SlabFrameNo;
		ElapsedSec = Context.ElapsedSec;
		Generation = Context.Generation;
		Segment = Context.Segment;
		bEligible = Context.bEligible;
		AcquisitionUtcTicks = Context.AcquisitionUtcTicks;
	}
	operator FVirtualSensorAcquisitionContext() const
	{
		FVirtualSensorAcquisitionContext Context;
		Context.RunId = RunId;
		Context.ScenarioUUID = ScenarioUUID;
		Context.MtlNo = MtlNo;
		Context.SlabFrameNo = SlabFrameNo;
		Context.ElapsedSec = ElapsedSec;
		Context.Generation = Generation;
		Context.Segment = Segment;
		Context.bEligible = bEligible;
		Context.AcquisitionUtcTicks = AcquisitionUtcTicks;
		return Context;
	}

};
