#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarAnalysisComponent.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

UVirtualLidarAnalysisComponent::UVirtualLidarAnalysisComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualLidarAnalysisComponent::BindScanComponent(UVirtualLidarScanComponent* InScanComponent)
{
	ScanComponent = InScanComponent;
}

void UVirtualLidarAnalysisComponent::AnalyzeLatestFrame()
{
	LatestHitPointCount = 0;
	LatestSemanticCounts.Reset();
	if (!ScanComponent) return;

	for (const FVirtualLidarPoint& Point : ScanComponent->GetLastPoints())
	{
		if (!Point.bHit) continue;
		++LatestHitPointCount;
		const FString Label = Point.SemanticLabel.IsNone() ? TEXT("Unclassified") : Point.SemanticLabel.ToString();
		LatestSemanticCounts.FindOrAdd(Label)++;
	}
}
