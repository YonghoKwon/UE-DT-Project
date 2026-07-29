#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualLidarSurfaceResponseComponent.generated.h"

/**
 * Optional 940 nm optical response override for virtual LiDAR hits.
 *
 * The component belongs on the hit Actor. Values are intentionally material
 * properties only; sensor noise and range response remain owned by the sensor.
 */
UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualLidarSurfaceResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualLidarSurfaceResponseComponent();

	/** Diffuse reflectivity at the ML-X wavelength. 0.2 means 20 percent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Optical", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float Reflectivity940Nm = 0.2f;

	/** Multiplier applied after the physical intensity estimate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Optical", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float IntensityGain = 1.0f;

	/** Detection probability multiplier used for unusual coatings or glass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Optical", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DetectionProbabilityScale = 1.0f;

	/** Treat back-face hits as having the same incidence response as front faces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Optical")
	bool bTwoSided = false;

	/** Allows high-gain retroreflective surfaces to report saturation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|Optical")
	bool bAllowSaturation = true;
};

