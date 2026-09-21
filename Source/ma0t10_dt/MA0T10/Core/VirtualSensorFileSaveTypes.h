#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorFileSaveTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorFileSaveMode : uint8 { NewFrame, CurrentFrame };

UENUM(BlueprintType)
enum class EVirtualSensorFileSaveState : uint8 { Accepted, Waiting, Saving, Succeeded, Failed, Cancelled };

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorFileSaveStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString RequestId;
	UPROPERTY(BlueprintReadOnly) EVirtualSensorFileSaveMode Mode=EVirtualSensorFileSaveMode::CurrentFrame;
	UPROPERTY(BlueprintReadOnly) EVirtualSensorFileSaveState State=EVirtualSensorFileSaveState::Failed;
	UPROPERTY(BlueprintReadOnly) FString SensorId;
	UPROPERTY(BlueprintReadOnly) EVirtualSensorKind SensorKind=EVirtualSensorKind::Lidar;
	UPROPERTY(BlueprintReadOnly) int64 FrameId=-1;
	UPROPERTY(BlueprintReadOnly) FVirtualSensorCaptureSelection Selection;
	UPROPERTY(BlueprintReadOnly) FDateTime RequestedUtc;
	UPROPERTY(BlueprintReadOnly) FDateTime AcquisitionUtc;
	UPROPERTY(BlueprintReadOnly) FDateTime CompletedUtc;
	UPROPERTY(BlueprintReadOnly) TArray<FVirtualSensorExportResult> FileResults;
	UPROPERTY(BlueprintReadOnly) FString Message;
	bool IsTerminal() const { return State==EVirtualSensorFileSaveState::Succeeded||State==EVirtualSensorFileSaveState::Failed||State==EVirtualSensorFileSaveState::Cancelled; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVirtualSensorFileSaveUpdated,const FVirtualSensorFileSaveStatus&,Status);
DECLARE_MULTICAST_DELEGATE_OneParam(FVirtualSensorFileSaveUpdatedNative,const FVirtualSensorFileSaveStatus&);
