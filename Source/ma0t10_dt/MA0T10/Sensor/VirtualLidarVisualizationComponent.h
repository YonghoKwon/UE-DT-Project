#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/StatusVisualizerCompBase.h"
#include "VirtualLidarVisualizationComponent.generated.h"

class UTexture2D;
class UVirtualLidarScanComponent;

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualLidarVisualizationComponent : public UStatusVisualizerCompBase
{
	GENERATED_BODY()

public:
	UVirtualLidarVisualizationComponent();
	void BindScanComponent(UVirtualLidarScanComponent* InScanComponent);
	void RefreshLatestFrame();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
	UTexture2D* GetPreviewTexture() const { return PreviewTexture; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Visualization")
	int32 GetVisiblePointCount() const { return VisiblePointCount; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UVirtualLidarScanComponent> ScanComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PreviewTexture;

	int32 VisiblePointCount = 0;
};
