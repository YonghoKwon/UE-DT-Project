#pragma once

#include "CoreMinimal.h"
#include "VirtualLidarSensorTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualLidarPreset : uint8
{
    LowDebug UMETA(DisplayName = "Low Debug"),
    MediumPreview UMETA(DisplayName = "Medium Preview"),
    HighQuality UMETA(DisplayName = "High Quality"),
    Custom UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
enum class EVirtualLidarViewMode : uint8
{
    IntensityGray UMETA(DisplayName = "Gray / Legacy"),
    HitMask UMETA(DisplayName = "Hit Mask - White/Black"),
    DepthGradient UMETA(DisplayName = "Depth Color"),
    ActorClassColor UMETA(DisplayName = "Semantic Color - Tag/Class")
};

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
    int32 Row = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    int32 Col = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    int32 ReturnIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    bool bHasGridCoord = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    FName HitActorName = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    FName HitActorClassName = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    TArray<FName> HitActorTags;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    FName SemanticLabel = NAME_None;
};

USTRUCT(BlueprintType)
struct M7AT10_DT_API FVirtualLidarSlabAnalysisResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    bool bValid = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    int32 SlabHitPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    FVector BoundsMin = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    FVector BoundsMax = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    FVector Center = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    float EstimatedYawDegrees = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    float ReferenceYawDegrees = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    float AngleDeviationDegrees = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    float Confidence = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SlabAnalysis")
    FString StatusMessage;
};

USTRUCT(BlueprintType)
struct M7AT10_DT_API FVirtualSensorRuntimeStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    FString SensorId;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    FString SensorType;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    int64 FrameId = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    FDateTime LastUpdateUtc;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    int32 LastPayloadLength = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    int32 TotalPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    int32 HitPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    int32 ServerPayloadPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    int32 PreviewPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    FString PerformanceWarning;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    FVirtualLidarSlabAnalysisResult SlabAnalysis;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor")
    FString LastMessage;
};

UENUM(BlueprintType)
enum class EVirtualLidarOutputMode : uint8
{
    None UMETA(DisplayName = "None"),
    LogOnly UMETA(DisplayName = "Log Only"),
    SaveJson UMETA(DisplayName = "Save JSON"),
    HttpPost UMETA(DisplayName = "HTTP POST")
};
