#pragma once

#include "CoreMinimal.h"
#include "VirtualLidarSensorTypes.h"
#include "VirtualLidarPayloadCodec.generated.h"

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarPayloadDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec")
	FString SensorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec")
	FString Manufacturer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec")
	FString Model;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec")
	float HorizontalFovDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec")
	float VerticalFovDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec")
	float MaxRangeMeters = 0.0f;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualLidarV2EncodeOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 PointStride = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec", meta = (ClampMin = "0", ClampMax = "1000000"))
	int32 MaxPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec")
	bool bIncludeInvalidPoints = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Codec")
	bool bIncludeDigitalTwinExtensions = false;
};

/**
 * Stateless physical LiDAR codecs. The compact format is a project contract,
 * not a claim that the bytes match proprietary ML-X network packets.
 */
class MA0T10_DT_API FVirtualLidarPayloadCodec
{
public:
	static FString EncodeV2Json(
		const FVirtualLidarFrameSnapshot& Frame,
		const FVirtualLidarPayloadDescriptor& Descriptor,
		const FVirtualLidarV2EncodeOptions& Options = FVirtualLidarV2EncodeOptions());

	static bool EncodeCompactBinary(
		const FVirtualLidarFrameSnapshot& Frame,
		const FVirtualLidarV2EncodeOptions& Options,
		TArray64<uint8>& OutBytes,
		int32& OutEncodedPointCount);

	static FString GetCompactBinaryContentType()
	{
		return TEXT("application/vnd.ma0t10.virtual-lidar-v2");
	}
};

