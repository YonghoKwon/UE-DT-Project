#include "ma0t10_dt/MA0T10/Common/ProjectDeviceAnalysisPolicy.h"
#include "Device/Sensor/DevicePointCloudAnalysis.h"
namespace
{
class FProjectDeviceAnalysisPolicy final : public IDevicePointCloudAnalysisProvider
{
public:
FVirtualLidarSlabAnalysisResult Analyze(const TArray<FVirtualLidarPoint>& Points,FName SlabSemanticLabel,float ReferenceSlabYawDegrees,int32 MinSlabPointsForAnalysis) const override
{
    FVirtualLidarSlabAnalysisResult Result;
    Result.ReferenceYawDegrees = ReferenceSlabYawDegrees;

    TArray<FVector> SlabPoints;
    for (const FVirtualLidarPoint& Point : Points)
    {
        if (Point.bHit && Point.SemanticLabel == SlabSemanticLabel)
        {
            if (SlabPoints.IsEmpty()) SlabPoints.Reserve(Points.Num());
            SlabPoints.Add(Point.WorldLocation);
        }
    }

    Result.SlabHitPointCount = SlabPoints.Num();
    if (SlabPoints.Num() < FMath::Max(3, MinSlabPointsForAnalysis))
    {
        Result.StatusMessage = FString::Printf(TEXT("insufficient slab points: %d/%d"), SlabPoints.Num(), FMath::Max(3, MinSlabPointsForAnalysis));
        return Result;
    }

    Result = DevicePointCloudAnalysis::Geometry(SlabPoints,ReferenceSlabYawDegrees);
    Result.bValid = true;
    Result.StatusMessage = TEXT("ok");
    return Result;
}
};
}
void RegisterProjectDeviceAnalysisPolicy()
{ DevicePointCloudAnalysis::Register(MakeShared<FProjectDeviceAnalysisPolicy,ESPMode::ThreadSafe>()); }
void UnregisterProjectDeviceAnalysisPolicy() { DevicePointCloudAnalysis::Unregister(); }
