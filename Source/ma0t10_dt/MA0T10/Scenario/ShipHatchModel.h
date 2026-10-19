#pragma once
#include "CoreMinimal.h"

/** HATCH-01 SI model. X=length, Y=breadth, Z=height above tank top. No transport state. */
namespace ShipHatch
{
enum class ECargo : uint8 { IronOre, Coal };
struct MA0T10_DT_API FSettings
{
    double Length=25.8, Width=42.3, Depth=22.5;
    double OpeningLength=20, OpeningWidth=23, Coaming=2, SensorAboveDeck=8;
    double MeshCell=.5, FillMin=35, FillMax=65;
    int32 Seed=19019;
    ECargo Cargo=ECargo::IronOre;
    bool Validate(FString& Error) const;
};
struct MA0T10_DT_API FSurface
{
    FSettings Settings;
    int32 NX=0, NY=0, Peaks=0;
    double DX=0, DY=0, Volume=0, Fill=0, Repose=0, Density=0;
    TArray<float> Heights;
    float At(int32 X,int32 Y) const { return Heights[Y*(NX+1)+X]; }
    double Height(double X,double Y) const;
    uint32 Hash() const;
};
struct FObservation { FVector PositionMeters; FName Label; };
struct FCell
{
    int32 Count=0;
    double Sum=0, Highest=0;
    double Mean() const { return Count ? Sum/Count : 0; }
};
struct MA0T10_DT_API FAnalysis
{
    int32 NX=0,NY=0,Observed=0;
    double DX=0,DY=0,Min=0,Max=0,Mean=0,Volume=0,Coverage=0,FillLowerBound=0;
    TArray<FCell> Cells;
};
MA0T10_DT_API FSurface Generate(const FSettings& Settings);
MA0T10_DT_API void GenerateInto(const FSettings& Settings,FSurface& Surface,TArray<double>& ShapeScratch);
MA0T10_DT_API FAnalysis Analyze(const FSettings& Settings,double CellSize,const TArray<FObservation>& Points);
MA0T10_DT_API FColor Jet(double Normalized);
MA0T10_DT_API double Reflectivity(ECargo Cargo);
}
