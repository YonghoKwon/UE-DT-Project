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
