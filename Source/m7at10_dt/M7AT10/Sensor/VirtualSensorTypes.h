#pragma once

#include "CoreMinimal.h"
#include "VirtualSensorTypes.generated.h"

USTRUCT(BlueprintType)
struct FVirtualSensorPacket
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor")
	FString SensorType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor")
	FString SensorName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor")
	FString PayloadJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor")
	int64 TimestampUnixMs = 0;
};

USTRUCT(BlueprintType)
struct FVirtualLidarPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor")
	float Distance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualSensor")
	bool bHit = false;
};
