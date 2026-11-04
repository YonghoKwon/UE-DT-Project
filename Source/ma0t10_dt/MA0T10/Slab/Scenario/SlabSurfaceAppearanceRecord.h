#pragma once
#include "CoreMinimal.h"

/** Native optional provenance, excluded from existing reflected fields/wire contracts. */
struct FSlabSurfaceAppearanceRecord
{
    FString Preset;
    bool bLidarLinked=false,bHot=false,bHasOpticalResponse=false;
    float Oxidation=0,Roughness=0,EmissiveStrength=0,Reflectivity940Nm=0,IntensityGain=1;
    int64 SlabFrame=INDEX_NONE;
    double ElapsedSec=0;
};
struct FSlabSurfaceRunUsage
{
    bool bPresent=false;
    FSlabSurfaceAppearanceRecord Initial,Latest;
    TArray<FString> UsedPresets; // At most the five presentation identifiers.
    int32 ChangeCount=0;
};
