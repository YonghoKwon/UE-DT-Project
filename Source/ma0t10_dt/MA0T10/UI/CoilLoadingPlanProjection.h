#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingModel.h"

namespace CoilPlan
{
struct MA0T10_DT_API FGlyph
{
    int32 Index=INDEX_NONE;
    bool bGhost=false,bAnnulus=false,bSection=false,bForeAft=true,bCold=false,bHighlighted=false;
    FVector2D Center=FVector2D::ZeroVector,Half=FVector2D::ZeroVector,NetHalf=FVector2D::ZeroVector;
    double Radius=0,Bore=0,NetRadius=0;
    FLinearColor Fill,Edge;
    FString Information,ShortLabel;
    bool Contains(FVector2D Point) const;
};
struct MA0T10_DT_API FProjection
{
    TArray<FGlyph> Glyphs;
    double Scale=1;
    FVector2D Min=FVector2D::ZeroVector,Max=FVector2D::ZeroVector,Origin=FVector2D::ZeroVector;
    bool bSection=false;
    FVector2D Project(FVector2D Meters) const;
    int32 Hit(FVector2D Point) const;
};
MA0T10_DT_API FString Describe(const CoilLoading::FPlacement& Placement);
/** Keep the native render target full-sized; frame its visible UI region off-axis. */
MA0T10_DT_API void FitPlayerProjection(FMatrix& Projection,const FIntRect& Full,const FVector4& NormalizedRect);
MA0T10_DT_API FProjection Build(const CoilLoading::FSettings& Settings,const TArray<CoilLoading::FPlacement>& Plan,const CoilLoading::FPlacement& Next,const CoilLoading::FResult& Analysis,const TArray<int32>& Highlighted,FVector2D Size,bool Section,int32 Row);
}
