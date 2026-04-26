#pragma once

#include "CoreMinimal.h"
#include "VirtualLidarSensorTypes.generated.h"

USTRUCT(BlueprintType)
struct M7AT10_DT_API FVirtualLidarPoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    FVector LocalDirection = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    float Distance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    bool bHit = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    FName HitActorName = NAME_None;
};

UENUM(BlueprintType)
enum class EVirtualLidarOutputMode : uint8
{
    None UMETA(DisplayName = "None"),
    LogOnly UMETA(DisplayName = "Log Only"),
    SaveJson UMETA(DisplayName = "Save JSON"),
    HttpPost UMETA(DisplayName = "HTTP POST")
};
