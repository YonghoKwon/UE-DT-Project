#pragma once
#include "CoreMinimal.h"
#include "SlabExecutionTypes.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSlabContextCompatibility.h"
#include "VirtualSlabFrameContext.generated.h"

UENUM(BlueprintType)
enum class EVirtualSlabSessionState : uint8 { Idle, Ready, Running, Paused, Draining, Completed, Incomplete, Preparing };

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
	UPROPERTY(BlueprintReadOnly) ESlabExecutionPolicy ExecutionPolicy=ESlabExecutionPolicy::ObservationAllowed;
	UPROPERTY(BlueprintReadOnly) FString RequiredDataError;
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
