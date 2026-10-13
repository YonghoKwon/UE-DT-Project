#include "SlabLabelLayout.h"

void SlabLabelLayout::Place(const FSlabLabelCandidate* Candidates,const int32* Priority,
    const FBox2D& Safe,const TArray<FBox2D>& Exclusions,FSlabLabelPlacement* Result)
{
    for(int32 I=0;I<5;++I)Result[I]=FSlabLabelPlacement();
    if(!Safe.bIsValid||Safe.GetSize().GetMin()<=0)return;
    for(int32 P=0;P<5;++P)
    {
        const int32 I=Priority[P];const auto& C=Candidates[I];
        if(!C.bEnabled||C.HalfSize.GetMin()<=0||C.HalfSize.X*2>Safe.GetSize().X||C.HalfSize.Y*2>Safe.GetSize().Y)continue;
        // Closest valid anchor wins; higher-priority labels own their rectangles first.
        const double Dx=C.HalfSize.X*2+12,Dy=C.HalfSize.Y*2+10;
        const FVector2D Moves[]={FVector2D::ZeroVector,{0,-Dy},{0,Dy},{-Dx,0},{Dx,0},
            {-Dx,-Dy},{Dx,-Dy},{-Dx,Dy},{Dx,Dy},{0,-Dy*2},{0,Dy*2}};
        for(const FVector2D& Move:Moves)
        {
            FVector2D At=C.Anchor+Move;
            At.X=FMath::Clamp(At.X,Safe.Min.X+C.HalfSize.X,Safe.Max.X-C.HalfSize.X);
            At.Y=FMath::Clamp(At.Y,Safe.Min.Y+C.HalfSize.Y,Safe.Max.Y-C.HalfSize.Y);
            const FBox2D Rect(At-C.HalfSize,At+C.HalfSize),Padded(Rect.Min-FVector2D(5,5),Rect.Max+FVector2D(5,5));
            bool Blocked=false;
            for(const auto& B:Exclusions)if(B.bIsValid&&Padded.Intersect(B)){Blocked=true;break;}
            if(!Blocked)for(int32 J=0;J<5;++J)if(Result[J].bVisible&&Padded.Intersect(Result[J].Rect)){Blocked=true;break;}
            if(!Blocked){Result[I].Rect=Rect;Result[I].bVisible=true;break;}
        }
    }
}
float SlabLabelLayout::DistanceFade(double Depth)
{
    if(!FMath::IsFinite(Depth)||Depth<=0)return 0;
    return float(1-FMath::SmoothStep(3500.,12000.,Depth));
}
