#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "VirtualSensorRuntimeTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorKind : uint8
{
	Camera UMETA(DisplayName = "Camera"),
	Lidar UMETA(DisplayName = "LiDAR")
};

UENUM(BlueprintType)
enum class EVirtualSensorStreamKind : uint8
{
	LidarPayload UMETA(DisplayName = "LiDAR Payload"),
	CameraImage UMETA(DisplayName = "Camera Image"),
	PointCloud UMETA(DisplayName = "Point Cloud")
};

UENUM(BlueprintType)
enum class EVirtualPointCloudStreamFormat : uint8
{
	CSV,
	JSONL,
	PCD,
	LAS,
	LAZ
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorStreamConfig
{
	GENERATED_BODY()

	/** Empty means every sensor of the matching kind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream")
	FString SensorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream")
	EVirtualSensorStreamKind StreamKind = EVirtualSensorStreamKind::LidarPayload;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 FrameStride = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream")
	EVirtualPointCloudStreamFormat PointCloudFormat = EVirtualPointCloudStreamFormat::CSV;

	/** Automatic streams request a broker receipt every N submitted frames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 ReceiptSampleInterval = 10;

	/** True LAZ compression is capped to this rate even when acquisition is faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MaxLazHz = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream")
	FString LazCompressorPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Stream")
	FString LazCompressorArguments = TEXT("-i {input} -o {output}");
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorStreamStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	FString SensorId;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	EVirtualSensorStreamKind StreamKind = EVirtualSensorStreamKind::LidarPayload;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	bool bProcessing = false;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	bool bPendingLatestFrame = false;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 LastInputFrameId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 LastSubmittedFrameId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 InputFrameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 SubmittedFrameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 ReplacedPendingFrameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 BandwidthDeferredFrameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 EncodeFailureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 StaleResultDiscardCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int32 ConfigRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 ReceiptTimeoutCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 ReceiptReceivedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	int64 TotalSubmittedBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	float InputHz = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	float SubmittedHz = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	float LastReceiptLatencyMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	FString LastRequestId;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	FString Destination;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Stream")
	FString Message;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorTransportLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	FDateTime TimestampUtc;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	FString SensorId;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	EVirtualSensorStreamKind StreamKind = EVirtualSensorStreamKind::LidarPayload;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	FString RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	FString Destination;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	FString Protocol;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	FString State;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	int64 FrameId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	int32 Bytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Transport")
	float LatencyMs = 0.0f;
};

/** Temporary acquisition budget used while an operator moves a sensor in PIE. */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorInteractionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Interaction")
	FIntPoint CameraPreviewResolution = FIntPoint(640, 360);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Interaction", meta = (ClampMin = "0.05"))
	float CameraPreviewInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Interaction", meta = (ClampMin = "1"))
	int32 LidarHorizontalSamples = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Interaction", meta = (ClampMin = "1"))
	int32 LidarVerticalChannels = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Interaction", meta = (ClampMin = "0.05"))
	float LidarPreviewInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualSensor|Interaction")
	bool bSuppressDerivedOutput = true;
};

/**
 * 센서 수집부와 출력부 사이에서 사용하는 불변 프레임 스냅샷입니다.
 * 큰 배열은 공유 포인터로 전달해 자동 수집 경로에서 불필요한 복사를 피합니다.
 */
struct MA0T10_DT_API FVirtualSensorFrameEnvelope
{
	FString SensorId;
	EVirtualSensorKind SensorKind = EVirtualSensorKind::Camera;
	int64 FrameId = 0;
	FDateTime TimestampUtc;
	FString SchemaVersion;
	TSharedPtr<const FString, ESPMode::ThreadSafe> JsonPayload;
	TSharedPtr<const TArray64<uint8>, ESPMode::ThreadSafe> BinaryPayload;
	TSharedPtr<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe> PointSnapshot;
	bool bSendTransport = true;
	bool bRecord = true;

	bool HasJsonPayload() const
	{
		return JsonPayload.IsValid() && !JsonPayload->IsEmpty();
	}
};

struct MA0T10_DT_API FVirtualSensorScheduleContext
{
	double NowSeconds = 0.0;
	float GameThreadBudgetMs = 0.0f;
	bool bAllowNewAcquisition = true;
	bool bPreferPreview = false;
};

class MA0T10_DT_API IVirtualSensorScheduledTask
{
public:
	virtual ~IVirtualSensorScheduledTask() = default;
	virtual EVirtualSensorKind GetScheduledSensorKind() const = 0;
	virtual bool IsScheduledTaskActive() const = 0;
	virtual bool TickScheduledTask(const FVirtualSensorScheduleContext& Context) = 0;
};
