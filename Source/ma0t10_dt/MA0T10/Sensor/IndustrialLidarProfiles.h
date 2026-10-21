#pragma once
#include "VirtualSensorDeviceProfileTypes.h"

/** Published envelopes, not a vendor calibration or packet implementation. */
namespace IndustrialLidar
{
struct FSpec
{
    EVirtualLidarDeviceProfile Profile;
    const TCHAR *Maker,*Model,*Source,*Pattern,*Conditions;
    int32 Beams,Columns,Rows,PointRate,Echoes;
    float HFov,Bottom,Top,MinMeters,RangeMeters,Hz,PrecisionMM,Wavelength;
};
MA0T10_DT_API TConstArrayView<FSpec> Specs();
MA0T10_DT_API const FSpec* Find(EVirtualLidarDeviceProfile Profile);
MA0T10_DT_API bool Resolve(EVirtualLidarDeviceProfile Profile,EVirtualSensorSimulationQuality Quality,FVirtualLidarProfilePreset& Out);
/** Physical channel layout for spinning sensors; deterministic nonrepeating
 * envelope approximation for Livox. Frame phase is fixed at acquisition start. */
MA0T10_DT_API FRotator Beam(EVirtualLidarDeviceProfile Profile,int32 Ray,int32 Width,int32 Height,int64 Frame,float Pitch,float Yaw);
}
