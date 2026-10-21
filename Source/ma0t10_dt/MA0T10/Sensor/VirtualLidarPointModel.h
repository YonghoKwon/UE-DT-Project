#pragma once
#include "VirtualLidarSensorTypes.h"
#include "VirtualSensorDeviceProfileTypes.h"

namespace VirtualLidarPointModel
{
struct FSurface
{
    float Reflectivity=.2f,IntensityGain=1,DetectionProbabilityScale=1;
    bool bTwoSided=false,bAllowSaturation=true;
};
/** Value-only acquisition options; safe to use on a worker. CPU and GPU share Apply. */
struct MA0T10_DT_API FConfig
{
    float MaxDistance=0,AmbientLightLux=0,RangeNoiseStandardDeviationMillimeters=0,MaximumRangeErrorMillimeters=0;
    bool bEnableMaterialResponse=false,bEnableRangeNoise=false;
    EVirtualSensorFidelityMode FidelityMode=EVirtualSensorFidelityMode::IdealTruth;
    uint32 RandomSequence=0;
    float DeterministicUnitRandom(int32 RayIndex,int32 ReturnIndex,uint32 Salt) const;
    float DeterministicGaussian(int32 RayIndex,int32 ReturnIndex) const;
    bool Apply(FVirtualLidarPoint& Point,const FSurface& Surface,const FVector& SafeDirection,
        const FTransform& AcquisitionTransform,float ModelRange,float ModelIncidence,int32 RayIndex,int32 ReturnIndex) const;
};
}
