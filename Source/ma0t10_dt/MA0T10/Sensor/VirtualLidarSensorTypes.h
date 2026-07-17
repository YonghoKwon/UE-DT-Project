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

/** 2D diagnostic projection. This is intentionally independent from colour mapping. */
UENUM(BlueprintType)
enum class ELidarMonitorProjectionMode : uint8
{
    RangeImage UMETA(DisplayName = "거리 영상"),
    TopDown UMETA(DisplayName = "조감도"),
    Elevation UMETA(DisplayName = "고도 단면"),
    Split UMETA(DisplayName = "거리 영상 + 조감도")
};

/** Colour policy shared by the monitor textures and the world point renderer. */
UENUM(BlueprintType)
enum class ELidarColorMode : uint8
{
    DistanceTurbo UMETA(DisplayName = "거리 Turbo"),
    DistanceViridis UMETA(DisplayName = "거리 Viridis"),
    RelativeHeight UMETA(DisplayName = "상대 높이"),
    SemanticLabel UMETA(DisplayName = "의미 분류"),
    VerticalChannel UMETA(DisplayName = "수직 채널"),
    ReturnIndex UMETA(DisplayName = "Return Index"),
    HitMask UMETA(DisplayName = "검출 마스크"),
    DistanceGray UMETA(DisplayName = "거리 회색조")
};

/** Runtime policy for the world-space LiDAR point renderer. */
UENUM(BlueprintType)
enum class ELidarPointCloudRenderPolicy : uint8
{
    AutoPreferNiagara UMETA(DisplayName = "자동 (Niagara 우선)"),
    ForceNiagara UMETA(DisplayName = "Niagara 강제"),
    ForceCpu UMETA(DisplayName = "CPU 강제")
};

/** Observable renderer state. Uploading an array alone is not considered active. */
UENUM(BlueprintType)
enum class ELidarPointCloudRendererState : uint8
{
    Disabled UMETA(DisplayName = "꺼짐"),
    Starting UMETA(DisplayName = "시작 중"),
    NiagaraActive UMETA(DisplayName = "Niagara 활성"),
    CpuFallback UMETA(DisplayName = "CPU fallback"),
    Error UMETA(DisplayName = "오류")
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarRendererTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Visualization")
    ELidarPointCloudRendererState State = ELidarPointCloudRendererState::Disabled;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Visualization")
    int32 MeasuredPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Visualization")
    int32 HitPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Visualization")
    int32 UploadedPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Visualization")
    int32 VisiblePointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Visualization")
    FString RendererName;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Visualization")
    FString Message;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarVisualizationSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization")
    ELidarMonitorProjectionMode ProjectionMode = ELidarMonitorProjectionMode::RangeImage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization")
    ELidarColorMode ColorMode = ELidarColorMode::DistanceTurbo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization")
    bool bUseAdaptiveDistance = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization")
    bool bShowGrid = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization")
    bool bShowDepthEdges = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization")
    bool bShowWorldPointCloud = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization", meta = (ClampMin = "0.25", ClampMax = "12.0"))
    float PointSize = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization", meta = (ClampMin = "128", ClampMax = "2048"))
    int32 TopDownResolution = 512;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization", meta = (ClampMin = "128", ClampMax = "2048"))
    int32 ElevationWidth = 512;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization", meta = (ClampMin = "64", ClampMax = "1024"))
    int32 ElevationHeight = 256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
    FVector2D TopDownPanCm = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View", meta = (ClampMin = "0.1", ClampMax = "20.0"))
    float TopDownZoom = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
    float TopDownRotationDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
    FVector2D ElevationPanCm = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View", meta = (ClampMin = "0.1", ClampMax = "20.0"))
    float ElevationZoom = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
    float ElevationRotationDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarPoint
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
struct MA0T10_DT_API FVirtualLidarSlabAnalysisResult
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
struct MA0T10_DT_API FVirtualSensorRuntimeStatus
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

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    float LastAcquisitionDurationMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    float LastPostProcessDurationMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    float MeasuredCompletionRateHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    bool bAcquisitionInFlight = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    bool bDerivedWorkInFlight = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    int32 DroppedAcquisitionFrameCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    int32 DroppedDerivedFrameCount = 0;
};

UENUM(BlueprintType)
enum class EVirtualLidarOutputMode : uint8
{
    None UMETA(DisplayName = "None"),
    LogOnly UMETA(DisplayName = "Log Only"),
    SaveJson UMETA(DisplayName = "Save JSON"),
    HttpPost UMETA(DisplayName = "HTTP POST")
};
