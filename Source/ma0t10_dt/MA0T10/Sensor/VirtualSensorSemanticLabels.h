#pragma once
#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;
struct FVirtualLidarSemanticClassRule;

/** Canonical project defaults. Existing per-LiDAR overrides remain supported. */
namespace VirtualSensorSemanticLabels
{
    MA0T10_DT_API const TArray<FVirtualLidarSemanticClassRule>& Defaults();
    MA0T10_DT_API bool Matches(const FVirtualLidarSemanticClassRule& Rule, const AActor* Actor, const UPrimitiveComponent* Component);
    MA0T10_DT_API FName Resolve(const UPrimitiveComponent* Component);
    // Stable IDs: 0 background/invalid, 1 Other, 2..6 original defaults, 7 Coil.
    MA0T10_DT_API uint8 ClassId(FName Label);
    MA0T10_DT_API FName Label(uint8 Id);
    MA0T10_DT_API FColor Color(uint8 Id);
}
