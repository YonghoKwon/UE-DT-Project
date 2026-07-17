#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

UVirtualLidarVisualizationComponent::UVirtualLidarVisualizationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualLidarVisualizationComponent::BindScanComponent(UVirtualLidarScanComponent* InScanComponent)
{
	ScanComponent = InScanComponent;
}

void UVirtualLidarVisualizationComponent::RefreshLatestFrame()
{
	PreviewTexture = ScanComponent ? ScanComponent->GetLidarViewTexture() : nullptr;
	VisiblePointCount = ScanComponent ? ScanComponent->GetRuntimeStatus().PreviewPointCount : 0;
}
