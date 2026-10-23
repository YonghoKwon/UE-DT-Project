#pragma once
#include "CoreMinimal.h"

/** Presentation only; inputs and output are in the monitor's viewport UI units. */
namespace CoilLoadingPresentation
{
    inline FBox2D AvailableArea(FVector2D Viewport, FVector2D ToolsInset)
    {
        const double Left=FMath::Clamp(ToolsInset.X,0.,FMath::Max(0.,Viewport.X-1.));
        const double Bottom=FMath::Clamp(ToolsInset.Y,0.,FMath::Max(0.,Viewport.Y-1.));
        return FBox2D(FVector2D(Left,0),FVector2D(Viewport.X,Viewport.Y-Bottom));
    }
}
