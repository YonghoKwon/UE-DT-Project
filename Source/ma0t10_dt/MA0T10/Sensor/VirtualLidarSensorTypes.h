#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDeviceProfileTypes.h"
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
	Elevation UMETA(DisplayName = "방사 거리-높이 프로파일"),
	Split UMETA(DisplayName = "거리 영상 + 조감도"),
	/** Appended for serialized enum compatibility. */
	ForwardSlice UMETA(DisplayName = "전방 수직 슬라이스"),
	/** World-space floor plan. Appended for serialized enum compatibility. */
	WorldTopDown UMETA(DisplayName = "월드 XY 조감도")
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

/** One canonical policy shared by 2D projection, Niagara and CPU fallback previews. */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarPreviewPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Preview", meta = (ClampMin = "1", ClampMax = "100"))
	int32 PointStride = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Preview", meta = (ClampMin = "0", ClampMax = "1000000"))
	int32 MaxPoints = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Preview")
	bool bHitOnly = true;
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

	/** World-space pan axes are screen axes: X = World Y, Y = World X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
	FVector2D WorldTopDownPanCm = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float WorldTopDownZoom = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
	float WorldTopDownRotationDegrees = 0.0f;

	/** Fits the viewport to the current hit-point bounds, independent of sensor rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
	bool bWorldTopDownAutoFit = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
    FVector2D ElevationPanCm = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View", meta = (ClampMin = "0.1", ClampMax = "20.0"))
    float ElevationZoom = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View")
    float ElevationRotationDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Visualization|View", meta = (ClampMin = "1.0", ClampMax = "10000.0"))
	float ForwardSliceThicknessCm = 100.0f;
};

UENUM(BlueprintType)
enum class EVirtualLidarEchoType : uint8
{
    None UMETA(DisplayName = "None"),
    Single UMETA(DisplayName = "Single"),
    First UMETA(DisplayName = "First"),
    Strongest UMETA(DisplayName = "Strongest"),
    Last UMETA(DisplayName = "Last")
};

UENUM(BlueprintType)
enum class EVirtualLidarPointValidity : uint8
{
    Valid UMETA(DisplayName = "Valid"),
    NoReturn UMETA(DisplayName = "No Return"),
    BelowSignalThreshold UMETA(DisplayName = "Below Signal Threshold"),
    Saturated UMETA(DisplayName = "Saturated"),
    OutOfRange UMETA(DisplayName = "Out Of Range"),
    InvalidCalibration UMETA(DisplayName = "Invalid Calibration")
};

UENUM(BlueprintType)
enum class EVirtualLidarTimeSyncState : uint8
{
    Unsynchronized UMETA(DisplayName = "Unsynchronized"),
    SimulationClock UMETA(DisplayName = "Simulation Clock"),
    PtpSimulated UMETA(DisplayName = "PTP Simulated"),
    HardwarePtpLocked UMETA(DisplayName = "Hardware PTP Locked")
};

UENUM(BlueprintType)
enum class EVirtualLidarEchoSelectionPolicy : uint8
{
    FirstOnly UMETA(DisplayName = "First Only"),
    StrongestOnly UMETA(DisplayName = "Strongest Only"),
    FirstAndLast UMETA(DisplayName = "First And Last"),
    AllAvailable UMETA(DisplayName = "All Available")
};

/**
 * Hardware-shaped measurement fields. Coordinates and ranges intentionally use
 * SI-friendly sensor-local units so they can be compared with a real device.
 */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualPhysicalLidarPoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    FVector SensorLocalPositionMeters = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 RangeMillimeters = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 RawIntensity = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    float NormalizedIntensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 Ring = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 HorizontalIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 EchoIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 EchoCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    EVirtualLidarEchoType EchoType = EVirtualLidarEchoType::None;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int64 PointTimeOffsetNanoseconds = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    EVirtualLidarPointValidity Validity = EVirtualLidarPointValidity::NoReturn;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    float Confidence = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    float SurfaceReflectivity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    float IncidenceCosine = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    float AmbientLux = 0.0f;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarPoint : public FVirtualPhysicalLidarPoint
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

/**
 * Immutable hardware-shaped frame header. Points are shared with the legacy
 * digital-twin frame so preview, codecs and exports do not copy full frames.
 */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualPhysicalLidarFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    FString SchemaVersion = TEXT("virtual-lidar.v2");

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    FString ProfileKey = TEXT("generic");

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    FString CalibrationId = TEXT("none");

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    FString FirmwareVersion = TEXT("simulation");

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    FString CoordinateConvention = TEXT("sensor-local RH X-forward Y-left Z-up, meters");

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int64 AcquisitionStartUnixNanoseconds = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int64 AcquisitionEndUnixNanoseconds = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 RequestedRayCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 ValidPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 InvalidPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 FirstEchoCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    int32 SecondEchoCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical") float MinRangeMeters = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical") float MaxRangeMeters = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical") float MeanRangeMeters = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical") float MinIntensity = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical") float MaxIntensity = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical") float MeanIntensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    EVirtualSensorFidelityMode FidelityMode = EVirtualSensorFidelityMode::IdealTruth;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    EVirtualLidarAcquisitionBackend AcquisitionBackend = EVirtualLidarAcquisitionBackend::AccurateCpuTrace;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    EVirtualLidarTimeSyncState TimeSyncState = EVirtualLidarTimeSyncState::SimulationClock;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar|Physical")
    bool bProtocolVerifiedAgainstHardware = false;

    TSharedPtr<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe> Points;
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
    float RequestedAcquisitionRateHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    float MeasuredAcquisitionRateHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    float MeasuredOutputRateHz = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    int32 DeadlineMissCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
	float CadenceStartJitterP95Ms = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
	float CadenceIntervalErrorP95Ms = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    FString RequestedAcquisitionBackend;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    FString ActiveAcquisitionBackend;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    FString AcquisitionBackendMessage;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    bool bAcquisitionInFlight = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    bool bDerivedWorkInFlight = false;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    int32 DroppedAcquisitionFrameCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    int32 DroppedDerivedFrameCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    int32 BudgetSkippedAcquisitionFrameCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    int32 FailedAcquisitionFrameCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Performance")
    int32 QueueOverflowCount = 0;
};

/** Immutable point frame and the exact pose/settings used to acquire it. */
struct MA0T10_DT_API FVirtualLidarFrameSnapshot : public FVirtualPhysicalLidarFrame
{
	FTransform AcquisitionTransform = FTransform::Identity;
	int64 ScheduledUnixNanoseconds = 0;
	int64 FrameId = 0;
	int32 HorizontalSamples = 1;
	int32 VerticalChannels = 1;
	float MaxDistanceCm = 10000.0f;
	uint32 SettingsRevision = 0;

	bool IsValid() const { return Points.IsValid(); }
};

UENUM(BlueprintType)
enum class EVirtualLidarOutputMode : uint8
{
    None UMETA(DisplayName = "None"),
    LogOnly UMETA(DisplayName = "Log Only"),
    SaveJson UMETA(DisplayName = "Save JSON"),
    HttpPost UMETA(DisplayName = "HTTP POST")
};
