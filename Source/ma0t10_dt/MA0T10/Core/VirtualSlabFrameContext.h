#pragma once
#include "CoreMinimal.h"
#include "VirtualSlabFrameContext.generated.h"

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
};

UENUM(BlueprintType)
enum class EVirtualSlabSessionState : uint8 { Idle, Ready, Running, Paused, Draining, Completed, Incomplete };

UENUM(BlueprintType)
enum class EVirtualSlabSessionEndReason : uint8
{
	None, Completed, Aborted, AcquisitionFailure, StreamFailure, DrainTimeout
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSlabSessionStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FVirtualSlabSensorOutputSelection Outputs;
	UPROPERTY(BlueprintReadOnly) bool bObservationOnly=false;
	UPROPERTY(BlueprintReadOnly) FString RunId;
	UPROPERTY(BlueprintReadOnly) EVirtualSlabSessionState State = EVirtualSlabSessionState::Idle;
	UPROPERTY(BlueprintReadOnly) FVirtualSlabFrameContext CurrentSlab;
	UPROPERTY(BlueprintReadOnly) FString Message;
	UPROPERTY(BlueprintReadOnly) int32 PendingAcquisitions = 0;
	UPROPERTY(BlueprintReadOnly) int64 UnfinishedFrames = 0;
	UPROPERTY(BlueprintReadOnly) bool bAborted = false;
	UPROPERTY(BlueprintReadOnly) bool bPointCloudOnly = false;
	UPROPERTY(BlueprintReadOnly) int32 AcquisitionFailures = 0;
	UPROPERTY(BlueprintReadOnly) int64 StreamFailures = 0;
	UPROPERTY(BlueprintReadOnly) EVirtualSlabSessionEndReason EndReason = EVirtualSlabSessionEndReason::None;
};
