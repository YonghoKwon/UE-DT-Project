#pragma once
#include "CoreMinimal.h"

/** Read-only, on-demand diagnostics. Shared bytes are observations, not additive ownership. */
struct FVirtualSensorResourceSnapshot
{
    FString Owner;
    TMap<FString, int64> Counts;
    TMap<FString, int64> OwnedCpuBytes;
    TMap<FString, int64> SharedCpuBytes;
    TMap<FString, int64> EstimatedGpuBytes;
};
