#pragma once
#include "CoilLoadingModel.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

/** Ref6 photo proportions, not certified structural or crane-capacity inputs. */
namespace CoilQuay
{
constexpr double RailX=-8,FootHalfX=4,TopHalfX=3.4,LegBaseZ=1.4,LegKneeZ=16,LegTopZ=28;
constexpr double SeaKneeInset=.6,SeaTopInset=5,LandTopInset=2;
constexpr double PortalTubeLevels[]={6.5,12.,17.5,23.};
constexpr double PortalTubeDiameter=.62;
constexpr double WheelTreadRadius=.41,WheelFlangeRadius=.45,WheelCenterZ=.57;
inline double PhotoSeaInset()
{
    // Editor automation only: photograph angle candidates use the same complete
    // construction path. Shipping builds always use the selected 5m offset.
    double Inset=SeaTopInset;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    FParse::Value(FCommandLine::Get(),TEXT("CoilPhotoSeaInset="),Inset);
    Inset=FMath::Clamp(Inset,3.8,6.2);
#endif
    return Inset;
}
inline FVector LegNode(const CoilLoading::FSettings& S,double XSign,int RailIndex,double Z)
{
    const double T=FMath::Clamp((Z-LegBaseZ)/(LegTopZ-LegBaseZ),0.,1.);
    const double FootY=S.QuayEdgeY()+S.QuaySide*(S.RailSetback+RailIndex*S.RailGauge);
    const double Scale=FMath::Min(1.,S.RailGauge/12.);
    const double SeaInset=Z<=LegKneeZ?SeaKneeInset*FMath::Clamp((Z-LegBaseZ)/(LegKneeZ-LegBaseZ),0.,1.):FMath::Lerp(SeaKneeInset,PhotoSeaInset(),FMath::Clamp((Z-LegKneeZ)/(LegTopZ-LegKneeZ),0.,1.));
    const double Inset=RailIndex?-LandTopInset*T:SeaInset;
    return {RailX+XSign*FMath::Lerp(FootHalfX,TopHalfX,T),FootY+S.QuaySide*Inset*Scale,Z};
}
inline double GirderSea(const CoilLoading::FSettings& S){return S.RailCenterY()-S.QuaySide*S.MaxRadius;}
inline double GirderLand(const CoilLoading::FSettings& S){return S.RailCenterY()+S.QuaySide*(S.RailGauge*.5+2);}
}
