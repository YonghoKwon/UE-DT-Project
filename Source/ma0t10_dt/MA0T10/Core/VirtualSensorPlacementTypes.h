#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorPlacementTypes.generated.h"

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorPoseBookmark
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString BookmarkId;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString MapPackage;
    UPROPERTY(BlueprintReadOnly) FString SensorId;
    UPROPERTY(BlueprintReadOnly) EVirtualSensorKind SensorKind=EVirtualSensorKind::Camera;
    /** World cm/quaternion, unit scale. Recall preserves the live Actor scale. */
    UPROPERTY(BlueprintReadOnly) FTransform Pose=FTransform::Identity;
    UPROPERTY(BlueprintReadOnly) int32 Revision=1;
    UPROPERTY(BlueprintReadOnly) FDateTime CreatedUtc;
    UPROPERTY(BlueprintReadOnly) FDateTime ModifiedUtc;
};
UENUM(BlueprintType)
enum class EVirtualSensorPoseSaveState:uint8 { Rejected, Writing, Saved, Failed };
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorPoseSaveStatus
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString RequestId;
    UPROPERTY(BlueprintReadOnly) FString BookmarkId;
    UPROPERTY(BlueprintReadOnly) EVirtualSensorPoseSaveState State=EVirtualSensorPoseSaveState::Rejected;
    UPROPERTY(BlueprintReadOnly) FString Message;
    UPROPERTY(BlueprintReadOnly) FString Path;
};
UENUM(BlueprintType)
enum class EVirtualSensorPlacementPreviewState:uint8 { Idle, Waiting, Ready, Stopped, Delayed, Cancelled };
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorPlacementPreviewStatus
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) EVirtualSensorPlacementPreviewState State=EVirtualSensorPlacementPreviewState::Idle;
    UPROPERTY(BlueprintReadOnly) int64 RequestId=0;
    UPROPERTY(BlueprintReadOnly) int64 CompletedFrameId=-1;
    UPROPERTY(BlueprintReadOnly) double ElapsedSeconds=0;
    UPROPERTY(BlueprintReadOnly) FString Message;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorPlacementSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString MapPackage;
    UPROPERTY(BlueprintReadOnly) FString SensorId;
    UPROPERTY(BlueprintReadOnly) EVirtualSensorKind SensorKind=EVirtualSensorKind::Camera;
    UPROPERTY(BlueprintReadOnly) FTransform WorldTransform=FTransform::Identity;
    UPROPERTY(BlueprintReadOnly) FString BookmarkId;
    UPROPERTY(BlueprintReadOnly) FString BookmarkName;
    UPROPERTY(BlueprintReadOnly) int32 BookmarkRevision=0;
    UPROPERTY(BlueprintReadOnly) bool bDirectPlacement=true;
    UPROPERTY(BlueprintReadOnly) FDateTime CapturedUtc;
    UPROPERTY(BlueprintReadOnly) bool bChangedDuringRun=false;
};
