#pragma once
#include "CoreMinimal.h"
#include "VirtualLidarSensorTypes.h"

/** Immutable frame policy. Implementations may select business points; device math has no labels. */
class MA0T10_DT_API IDevicePointCloudAnalysisProvider
{
public:
    virtual ~IDevicePointCloudAnalysisProvider() = default;
    virtual FVirtualLidarSlabAnalysisResult Analyze(const TArray<FVirtualLidarPoint>& Points,
        FName Label,float ReferenceYawDegrees,int32 MinPoints) const = 0;
};
namespace DevicePointCloudAnalysis
{
    MA0T10_DT_API void Register(TSharedRef<const IDevicePointCloudAnalysisProvider,ESPMode::ThreadSafe> Provider);
    MA0T10_DT_API void Unregister();
    MA0T10_DT_API TSharedPtr<const IDevicePointCloudAnalysisProvider,ESPMode::ThreadSafe> Snapshot();
    /** Pure XY PCA/bounds. Caller owns selection, minimum count and status wording. */
    MA0T10_DT_API FVirtualLidarSlabAnalysisResult Geometry(TConstArrayView<FVector> SelectedPoints,float ReferenceYawDegrees);
}
