#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchModel.h"

namespace ShipHatch
{
/** Presentation only. Hold-local SI -> screen (right, down).
 * Matches the top camera: yaw +90, pitch -90, roll 0. Screen right=-X,
 * screen up=+Y (sea). Storage stays row=+Y, column=+X; no payload/UV flip.
 * Section length looks from land -Y toward +Y; width from +X toward -X.
 */
struct FViewTransform
{
    FVector Right,Down,Center;
    FVector2D Span;
    static FRotator TopRotation() { return FRotator(-90,90,0); }
    static FViewTransform Top(const FSettings& S)
    { return {FVector(-1,0,0),FVector(0,-1,0),FVector::ZeroVector,FVector2D(S.OpeningLength*1.1,S.OpeningWidth*1.1)}; }
    static FViewTransform Section(bool AlongLength,double Width,double Height)
    { return {AlongLength?FVector(-1,0,0):FVector(0,-1,0),FVector(0,0,-1),FVector(0,0,Height*.5),FVector2D(Width,Height)}; }
    FVector2D ToUnit(FVector P) const
    { const auto D=P-Center;return FVector2D(.5+FVector::DotProduct(D,Right)/Span.X,.5+FVector::DotProduct(D,Down)/Span.Y); }
    FVector FromUnit(FVector2D P) const
    { return Center+Right*((P.X-.5)*Span.X)+Down*((P.Y-.5)*Span.Y); }
    FVector2D ToPixel(FVector P,const FSlateRect& R) const
    { const auto U=ToUnit(P);return FVector2D(R.Left+U.X*(R.Right-R.Left),R.Top+U.Y*(R.Bottom-R.Top)); }
    FVector FromPixel(FVector2D P,const FSlateRect& R) const
    { return FromUnit(FVector2D((P.X-R.Left)/(R.Right-R.Left),(P.Y-R.Top)/(R.Bottom-R.Top))); }
    FVector2D DirectionToPixel(FVector D,const FSlateRect& R) const
    { return ToPixel(Center+D,R)-ToPixel(Center,R); }
    FVector2D WorldToPixel(FVector WorldCm,const FTransform& HoldPose,const FSlateRect& R) const
    { return ToPixel(HoldPose.InverseTransformPosition(WorldCm)*.01,R); }
    FVector PixelToWorld(FVector2D P,const FTransform& HoldPose,const FSlateRect& R) const
    { return HoldPose.TransformPosition(FromPixel(P,R)*100); }
    FSlateRect Bounds(FVector A,FVector B,const FSlateRect& R) const
    { const auto P=ToPixel(A,R),Q=ToPixel(B,R);return FSlateRect(FMath::Min(P.X,Q.X),FMath::Min(P.Y,Q.Y),FMath::Max(P.X,Q.X),FMath::Max(P.Y,Q.Y)); }
    FSlateRect Fit(FVector2D Size,bool Heat,double FontScale) const
    {
        const double AvailableW=FMath::Max(1.,Size.X-(Heat?60:24)),Top=24*FontScale,AvailableH=FMath::Max(1.,Size.Y-Top-22*FontScale);
        const double Aspect=Span.X/Span.Y,W=FMath::Max(1.,FMath::Min(AvailableW,AvailableH*Aspect)),H=W/Aspect;
        const double X=12+(AvailableW-W)*.5,Y=Top+(AvailableH-H)*.5;
        return FSlateRect(X,Y,X+W,Y+H);
    }
};
}
