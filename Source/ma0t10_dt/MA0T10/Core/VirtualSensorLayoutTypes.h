#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorLayoutTypes.generated.h"
USTRUCT(BlueprintType)
struct FVirtualSensorLayoutEntry
{
    GENERATED_BODY()
    UPROPERTY() FString LayoutGuid;
    UPROPERTY() FString ActorPath;
    UPROPERTY() bool bToolCreated=false;
    UPROPERTY() FVirtualSensorEditableState State;
    UPROPERTY() EVirtualLidarAcquisitionBackend LidarBackend=EVirtualLidarAcquisitionBackend::Auto;
    UPROPERTY() bool bPreviewEnabled=false;
};
USTRUCT()
struct FVirtualSensorLayoutDocument
{
    GENERATED_BODY()
    UPROPERTY() int32 Version=1;
    UPROPERTY() FString Schema=TEXT("ma0t10.sensor-layout.v1");
    UPROPERTY() FString Name;
    UPROPERTY() FString MapPackage;
    UPROPERTY() FDateTime SavedUtc;
    UPROPERTY() TArray<FVirtualSensorLayoutEntry> Sensors;
};
UENUM(BlueprintType)
enum class EVirtualSensorLayoutOperation:uint8 { Failed, Writing, Saved, ReadyToApply, Applied, Cancelled };
USTRUCT(BlueprintType)
struct FVirtualSensorLayoutOperationStatus
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString RequestId;
    UPROPERTY(BlueprintReadOnly) EVirtualSensorLayoutOperation State=EVirtualSensorLayoutOperation::Failed;
    UPROPERTY(BlueprintReadOnly) FString Message;
    UPROPERTY(BlueprintReadOnly) FString Path;
};
