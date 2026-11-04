#pragma once
#include "CoreMinimal.h"

/** COIL-01 SI-only deterministic screening. Basis: docs/research/COIL-01-basis.md.
 * Unknown approval inputs remain unknown; rigid-body confirmation never overrides this result. */
namespace CoilLoading
{
constexpr double Gravity=9.81;
constexpr int32 MaximumTier=5; // User decision 2026-10-22; load warnings remain independent.
enum class ESeverity:uint8 { Normal, Caution, Warning };
enum class ECoilType:uint8 { HotRolled, ColdRolled };
struct MA0T10_DT_API FSpec
{
    FString CoilId=TEXT("HR-0001");
    double OD=1.6,ID=.762,Width=1.25,Density=7.85,Fill=.97,MassOverride=0,Mu=.3,SteelMu=.1;
    ECoilType Type=ECoilType::HotRolled;
    // Synthetic display/geometry inputs, not a supplier packaging standard.
    double PackageRadial=.0015,PackageEnd=.002;
    bool bPackagedFrictionConfirmed=false;
    double OuterDiameter() const { return OD+(Type==ECoilType::ColdRolled?2*PackageRadial:0); }
    double ClearBore() const { return ID-(Type==ECoilType::ColdRolled?2*PackageRadial:0); }
    double OverallWidth() const { return Width+(Type==ECoilType::ColdRolled?2*PackageEnd:0); }
    static FSpec Representative(ECoilType Type);
    double GrossMass() const;
    double CalculatedMass() const;
    bool Validate(FString& Error) const;
};
struct FCylinderFit
{
    bool bValid=false;
    FVector Center=FVector::ZeroVector;
    double ResidualP95=0;
    int32 UsedPoints=0;
};
struct MA0T10_DT_API FPlacement
{
    FSpec Spec;
    FVector Center=FVector::ZeroVector; // metres, X forward / Y port / Z above tanktop
    int32 Tier=1,Row=1,Column=1,SupportA=INDEX_NONE,SupportB=INDEX_NONE;
    bool bDunnage=true,bChocks=true,bLashing=false,bEyeForeAft=true;
    double ChockCapacityKN=0,LashingCapacityKN=0,DiametralStiffnessKNPerM=0,OvalityLimit=0;
    double RollingResistanceM=0; // effective contact moment arm; 0 = ideal rigid cylinder
};
struct MA0T10_DT_API FSettings
{
    // Wagenborg N-class, working aft hold: published box-hold/hatch dimensions.
    double LOA=174.10,Beam=22,Depth=12.80,Draft=6.20,HoldLength=24.80,HoldWidth=18.80,HoldHeight=14;
    double OpeningLength=24.80,OpeningWidth=18.80,Coaming=1.4,UDLLimit=14.5;
    int32 HoldCount=4,DunnageLines=3;
    double IntermediateHoldLength=27.4,HoldGap=4; // gap is a layout assumption
    bool bDeckCranes=true,bFoldingCovers=true;
    double DunnageWidth=.2,DunnageThickness=.05,ContactLength=1.6;
    double ApprovedPatchArea=0,LocalPressureLimit=0; // 0 = not provided, never a green approval
    double Displacement=30000,KG=7,KM=9,FreeSurfaceCorrection=.2,MCT1cm=0,KeelToTanktop=.2;
    double LBP=166,SpeedKnots=14,PositionFromAP=.25; // synthetic navigation inputs
    double TPC=0,WaterplaneCoefficient=.85,WaterDensity=1.025,SummerDraftLimit=9.38;
    bool bTPCConfirmed=false; // positive certified TPC required; otherwise data needed
    double LCFFromAP=0; // metres from AP; unavailable until explicitly confirmed
    bool bLCFConfirmed=false;
    double BerthGap=2,RailSetback=3,RailGauge=12;
    int32 QuaySide=-1; // -1 starboard / +1 port, in scenario coordinates
    double HookSWL=40,GearMass=2,MinRadius=11,MaxRadius=35,GearHeadroom=3,GearWidth=2.2,GearLength=2.5;
    double LegAngleDegrees=30,LegWLLTonnes=0,Outreach=0,ClearanceMargin=.2,WindMps=0; // radius0 = derive with rail aligned to load X
    bool bCHook=false,bWetSteel=false,bLoadingManual=false,bUseCSS=false;
    double Ax=0,Ay=0,Az=0;
    double QuayEdgeY() const { return QuaySide*(Beam*.5+BerthGap); }
    double RailCenterY() const { return QuayEdgeY()+QuaySide*(RailSetback+RailGauge*.5); }
    double CraneRadius(const FVector& Load) const { return FMath::Abs(Load.Y-RailCenterY()); }
    double HoldLengthAt(int32 I) const { return I>0&&I<HoldCount-1&&IntermediateHoldLength>0?IntermediateHoldLength:HoldLength; }
    double HoldCenterX(int32 Index) const { double X=0;for(int32 I=1;I<=Index;++I)X+=(HoldLengthAt(I-1)+HoldLengthAt(I))*.5+HoldGap;return X; }
    double VesselCenterX() const { return (HoldCenterX(HoldCount-1)+(HoldLengthAt(HoldCount-1)-HoldLength)*.5)*.5; }
    double WeatherDeckZ() const { return HoldHeight-Coaming; }
    double DeckCraneInnerY() const { return Beam*.5-.8-.37; }
    bool Validate(FString& Error) const;
};
struct MA0T10_DT_API FIssue
{
    FName Code;
    ESeverity Severity=ESeverity::Caution;
    FString Reason;
    int32 Coil=INDEX_NONE;
    double Value=0,Limit=0;
    bool bKnown=true;
};
struct MA0T10_DT_API FKeyGeometry
{
    bool bValid=false;
    FVector2D Center=FVector2D::ZeroVector;
    double Height=0,AngleDegrees=0,PenetrationRatio=0,NormalKN=0,SpreadKN=0;
};
struct MA0T10_DT_API FDraftResponse
{
    double AddedMass=0,TonnesPerCm=0,Sinkage=0,Draft=0,WaterplaneArea=0;
    bool bConfirmed=false;
};
MA0T10_DT_API FDraftResponse DraftResponse(const FSettings& Settings,double AddedMass);
struct MA0T10_DT_API FResult
{
    TArray<FIssue> Issues;
    TArray<double> VerticalTonnes,SpreadKN,ContactPressure,PatchPressure,Ovality;
    ESeverity Severity=ESeverity::Normal;
    bool bComplete=true;
    double TotalMass=0,FloorReaction=0,KG=0,GM=0,HeelDegrees=0,TrimCm=0,TrimMomentTonnesM=0,MaxUtilization=0;
    FDraftResponse Hydro;
    int32 UnknownCount=0;
    void Add(FName Code,ESeverity Level,const FString& Reason,int32 Coil=INDEX_NONE,double Value=0,double Limit=0,bool Known=true);
};
MA0T10_DT_API FKeyGeometry KeyGeometry(double LowerRadius,double UpperRadius,double Gap,double DownwardKN=0);
MA0T10_DT_API bool UnequalKey(FVector2D A,double RA,FVector2D B,double RB,double R,FVector2D& Center);
MA0T10_DT_API bool CSSAcceleration(const FSettings& Settings,FVector& Acceleration,FString& Error);
MA0T10_DT_API FResult Analyze(const FSettings& Settings,const TArray<FPlacement>& Plan);
MA0T10_DT_API double Heel(double Displacement,double GM,double TransverseMoment);
MA0T10_DT_API double PolygonRadialError(double Radius,int32 Sides);
/** Analytic finite annulus intersection for the two supported horizontal eye axes. */
MA0T10_DT_API bool CylindersInterfere(const FPlacement& A,const FPlacement& B,double Tolerance=.003);
/** Robust fixed-axis annulus translation fit; all inputs/outputs in scenario-local metres. */
MA0T10_DT_API FCylinderFit FitCylinder(const FPlacement& Placement,const TArray<FVector>& Points);
MA0T10_DT_API FString PlanToJson(const FSettings& Settings,const TArray<FPlacement>& Plan);
MA0T10_DT_API bool PlanFromJson(const FString& Json,FSettings& Settings,TArray<FPlacement>& Plan,FString& Error);
MA0T10_DT_API bool ImportManifest(const FString& Text,bool Json,TArray<FSpec>& Coils,FString& Error);
}
