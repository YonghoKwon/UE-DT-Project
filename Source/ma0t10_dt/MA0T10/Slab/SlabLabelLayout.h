#pragma once
#include "CoreMinimal.h"

/** Presentation-only screen geometry. No UI types, sensor state or persisted settings. */
struct FSlabLabelCandidate
{
    FVector2D Anchor=FVector2D::ZeroVector;
    FVector2D HalfSize=FVector2D::ZeroVector;
    bool bEnabled=false;
};
struct FSlabLabelPlacement
{
    FBox2D Rect=FBox2D(ForceInit);
    bool bVisible=false;
};
namespace SlabLabelLayout
{
    MA0T10_DT_API void Place(const FSlabLabelCandidate* Candidates,const int32* Priority,
        const FBox2D& SafeView,const TArray<FBox2D>& Exclusions,FSlabLabelPlacement* Result);
    MA0T10_DT_API float DistanceFade(double DepthCm);
}
