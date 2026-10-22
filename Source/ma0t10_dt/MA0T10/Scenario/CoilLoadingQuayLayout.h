#pragma once
#include "CoilLoadingModel.h"

/** Ref6 member locations. Dimensions not published in the photographs are
 * representative visual proportions, never crane certification inputs. */
namespace CoilQuay
{
constexpr double RailX=-8,FootHalfX=4,TopHalfX=3.4,LegBaseZ=1.4,LegTopZ=28,LegInsetY=2;
constexpr double WheelTreadRadius=.41,WheelFlangeRadius=.45,WheelCenterZ=.57;
inline FVector LegNode(const CoilLoading::FSettings& S,double XSign,int RailIndex,double Z)
{
    const double T=FMath::Clamp((Z-LegBaseZ)/(LegTopZ-LegBaseZ),0.,1.);
    const double FootY=S.QuayEdgeY()+S.QuaySide*(S.RailSetback+RailIndex*S.RailGauge);
    const double Inset=FMath::Min(LegInsetY,S.RailGauge*.2);
    return {RailX+XSign*FMath::Lerp(FootHalfX,TopHalfX,T),FootY+S.QuaySide*(RailIndex?-Inset:Inset)*T,Z};
}
inline double GirderSea(const CoilLoading::FSettings& S){return S.RailCenterY()-S.QuaySide*S.MaxRadius;}
inline double GirderLand(const CoilLoading::FSettings& S){return S.RailCenterY()+S.QuaySide*(S.RailGauge*.5+2);}
}
