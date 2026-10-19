#include "ShipHatchModel.h"
#include "Misc/Crc.h"

namespace ShipHatch
{
bool FSettings::Validate(FString& Error) const
{
    for(double V:{Length,Width,Depth,OpeningLength,OpeningWidth,Coaming,SensorAboveDeck,MeshCell,FillMin,FillMax})
        if(!FMath::IsFinite(V)){Error=TEXT("설정값은 유한한 수여야 합니다.");return false;}
    if(Length<5||Length>100||Width<5||Width>100||Depth<5||Depth>50||OpeningLength<2||OpeningWidth<2||
       OpeningLength>=Length||OpeningWidth>=Width||Coaming<.1||Coaming>5||SensorAboveDeck<=Coaming||SensorAboveDeck>30||
       MeshCell<.4||MeshCell>2||FillMin<0||FillMax>95||FillMin>FillMax||
       FMath::CeilToInt(Length/MeshCell)*FMath::CeilToInt(Width/MeshCell)>20000)
    {Error=TEXT("치수·개구부·높이·적재율 범위를 확인하세요. 메시 격자는 최대 20,000칸입니다.");return false;}
    Error.Reset();return true;
}
double Reflectivity(ECargo Cargo){return Cargo==ECargo::Coal?.05:.25;}
FColor Jet(double T)
{
    auto C=[T](double Offset){return uint8(FMath::Clamp(1.5-FMath::Abs(4*FMath::Clamp(T,0.,1.)-Offset),0.,1.)*255);};
    return FColor(C(3),C(2),C(1));
}
FSurface Generate(const FSettings& S)
{
    FSurface R;R.Settings=S;FString Error;if(!S.Validate(Error))return R;
    FRandomStream Random(S.Seed);
    R.NX=FMath::CeilToInt(S.Length/S.MeshCell);R.NY=FMath::CeilToInt(S.Width/S.MeshCell);
    R.DX=S.Length/R.NX;R.DY=S.Width/R.NY;
    R.Fill=Random.FRandRange(S.FillMin,S.FillMax);
    R.Repose=Random.FRandRange(S.Cargo==ECargo::Coal?28.f:32.f,S.Cargo==ECargo::Coal?35.f:40.f);
    R.Density=S.Cargo==ECargo::Coal?900:2400;
    R.Peaks=Random.RandRange(1,5);
    // A triangular interpolation can amplify sampled directional slopes by sqrt(2).
    const double Slope=.7*FMath::Tan(FMath::DegreesToRadians(R.Repose));
    const FVector2D Bias(Random.FRandRange(-.25f,.25f)*S.Length,Random.FRandRange(-.25f,.25f)*S.Width);
    TArray<FVector> Peaks;
    for(int32 I=0;I<R.Peaks;++I)Peaks.Add(FVector(Bias.X+Random.FRandRange(-.3f,.3f)*S.Length,
        Bias.Y+Random.FRandRange(-.3f,.3f)*S.Width,Random.FRandRange(2.f,8.f)));
    TArray<double> Shape;Shape.SetNumUninitialized((R.NX+1)*(R.NY+1));
    for(int32 Y=0;Y<=R.NY;++Y)for(int32 X=0;X<=R.NX;++X)
    {
        const FVector2D P(X*R.DX-S.Length*.5,Y*R.DY-S.Width*.5);
        double H=-DBL_MAX;
        for(const auto& Peak:Peaks)H=FMath::Max(H,Peak.Z-Slope*FVector2D::Distance(P,FVector2D(Peak.X,Peak.Y)));
        Shape[Y*(R.NX+1)+X]=H;
    }
    R.Heights.SetNumUninitialized(Shape.Num());
    auto Integrate=[&](double Offset)
    {
        for(int32 I=0;I<Shape.Num();++I)R.Heights[I]=FMath::Clamp(Shape[I]+Offset,0.,S.Depth);
        double Sum=0;
        // Exact integral of the same two planar triangles used by the collision mesh.
        for(int32 Y=0;Y<R.NY;++Y)for(int32 X=0;X<R.NX;++X)
            Sum+=(2.*R.At(X,Y)+R.At(X+1,Y)+R.At(X,Y+1)+2.*R.At(X+1,Y+1))/6.*R.DX*R.DY;
        return Sum;
    };
    double Low=-1000,High=1000;const double Target=S.Length*S.Width*S.Depth*R.Fill*.01;
    for(int32 I=0;I<48;++I){double Mid=(Low+High)*.5;if(Integrate(Mid)<Target)Low=Mid;else High=Mid;}
    R.Volume=Integrate((Low+High)*.5);return R;
}
double FSurface::Height(double X,double Y) const
{
    if(!Heights.Num())return 0;
    double U=FMath::Clamp((X+Settings.Length*.5)/DX,0.,double(NX));
    double V=FMath::Clamp((Y+Settings.Width*.5)/DY,0.,double(NY));
    int32 IX=FMath::Min(FMath::FloorToInt(U),NX-1),IY=FMath::Min(FMath::FloorToInt(V),NY-1);U-=IX;V-=IY;
    const double A=At(IX,IY),D=At(IX+1,IY+1);
    return U>=V?A*(1-U)+At(IX+1,IY)*(U-V)+D*V:A*(1-V)+At(IX,IY+1)*(V-U)+D*U;
}
uint32 FSurface::Hash() const{return FCrc::MemCrc32(Heights.GetData(),Heights.Num()*sizeof(float));}
FAnalysis Analyze(const FSettings& S,double CellSize,const TArray<FObservation>& Points)
{
    FAnalysis R;if(!FMath::IsFinite(CellSize)||CellSize<.25||CellSize>5)return R;
    R.NX=FMath::CeilToInt(S.Length/CellSize);R.NY=FMath::CeilToInt(S.Width/CellSize);
    R.DX=S.Length/R.NX;R.DY=S.Width/R.NY;R.Cells.SetNum(R.NX*R.NY);
    for(const auto& P:Points)
    {
        if(P.PositionMeters.ContainsNaN()||P.Label!=TEXT("HatchCargo"))continue;
        double X=P.PositionMeters.X+S.Length*.5,Y=P.PositionMeters.Y+S.Width*.5,Z=P.PositionMeters.Z;
        if(X<0||X>S.Length||Y<0||Y>S.Width||Z<0||Z>S.Depth)continue;
        auto& C=R.Cells[FMath::Min(FMath::FloorToInt(Y/R.DY),R.NY-1)*R.NX+FMath::Min(FMath::FloorToInt(X/R.DX),R.NX-1)];
        ++C.Count;C.Sum+=Z;C.Highest=FMath::Max(C.Highest,Z);
    }
    R.Min=DBL_MAX;
    for(const auto& C:R.Cells)if(C.Count){++R.Observed;R.Min=FMath::Min(R.Min,C.Mean());R.Max=FMath::Max(R.Max,C.Mean());R.Mean+=C.Mean();R.Volume+=C.Mean()*R.DX*R.DY;}
    if(R.Observed)R.Mean/=R.Observed;else R.Min=0;
    R.Coverage=double(R.Observed)/R.Cells.Num();R.FillLowerBound=100*R.Volume/(S.Length*S.Width*S.Depth);
    return R;
}
}
