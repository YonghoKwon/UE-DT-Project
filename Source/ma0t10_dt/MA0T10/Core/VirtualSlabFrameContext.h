#pragma once
#include "CoreMinimal.h"
#include "VirtualSlabFrameContext.generated.h"

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSlabFrameContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString RunId;
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

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSlabSessionStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString RunId;
	UPROPERTY(BlueprintReadOnly) EVirtualSlabSessionState State = EVirtualSlabSessionState::Idle;
	UPROPERTY(BlueprintReadOnly) FVirtualSlabFrameContext CurrentSlab;
	UPROPERTY(BlueprintReadOnly) FString Message;
	UPROPERTY(BlueprintReadOnly) int32 PendingAcquisitions = 0;
	UPROPERTY(BlueprintReadOnly) int64 UnfinishedFrames = 0;
	UPROPERTY(BlueprintReadOnly) bool bAborted = false;
	UPROPERTY(BlueprintReadOnly) int32 AcquisitionFailures = 0;
};
