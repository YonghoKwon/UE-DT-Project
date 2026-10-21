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
    FSurface R;TArray<double> Shape;GenerateInto(S,R,Shape);return R;
}
void GenerateInto(const FSettings& S,FSurface& R,TArray<double>& Shape)
{
    R.Settings=S;FString Error;if(!S.Validate(Error)){R=FSurface();R.Settings=S;return;}
    FRandomStream Random(S.Seed);
    R.NX=FMath::CeilToInt(S.Length/S.MeshCell);R.NY=FMath::CeilToInt(S.Width/S.MeshCell);
    R.DX=S.Length/R.NX;R.DY=S.Width/R.NY;
    R.Fill=Random.FRandRange(S.FillMin,S.FillMax);
    R.Repose=Random.FRandRange(S.Cargo==ECargo::Coal?28.f:32.f,S.Cargo==ECargo::Coal?35.f:40.f);
    R.Density=S.Cargo==ECargo::Coal?900:2400;
    R.Peaks=Random.RandRange(1,5);
    // A triangular interpolation can amplify sampled directional slopes by sqrt(2).
    // Reserve face-gradient budget for sub-meter lumps and wall-bank detail.
    // 0.5m collision grid resolves >=1m shapes; smaller grain lives in the shader.
    const double Slope=.6*FMath::Tan(FMath::DegreesToRadians(R.Repose));
    const FVector2D Bias(Random.FRandRange(-.25f,.25f)*S.Length,Random.FRandRange(-.25f,.25f)*S.Width);
    TArray<FVector,TInlineAllocator<5>> Peaks;
    for(int32 I=0;I<R.Peaks;++I)Peaks.Add(FVector(Bias.X+Random.FRandRange(-.3f,.3f)*S.Length,
        Bias.Y+Random.FRandRange(-.3f,.3f)*S.Width,Random.FRandRange(2.f,8.f)));
    Shape.SetNumUninitialized((R.NX+1)*(R.NY+1),false);
    for(int32 Y=0;Y<=R.NY;++Y)for(int32 X=0;X<=R.NX;++X)
    {
        const FVector2D P(X*R.DX-S.Length*.5,Y*R.DY-S.Width*.5);
        double H=-DBL_MAX;
        for(const auto& Peak:Peaks)H=FMath::Max(H,Peak.Z-Slope*FVector2D::Distance(P,FVector2D(Peak.X,Peak.Y)));
        const double Phase=S.Seed*.017;
        // A0.5m grid resolves metre-scale aggregates. Domain-warped noise
        // avoids the regular sinusoidal stripes of the original surface;
        // centimetre chunks remain normal detail with no height displacement.
        const FVector2D Warp(FMath::PerlinNoise2D(P/6.+FVector2D(Phase,3.1)),FMath::PerlinNoise2D(P/6.+FVector2D(7.4,-Phase)));
        const double Lump=.10*FMath::PerlinNoise2D(P/1.8+Warp*.7+FVector2D(Phase,-Phase))+.035*FMath::PerlinNoise2D(P/4.2+FVector2D(2.7,Phase));
        const double Rill=.012*FMath::Sin(P.X*2.5+Warp.Y*3+P.Y*.4+Phase);
        const double Wall=FMath::Min(S.Length*.5-FMath::Abs(P.X),S.Width*.5-FMath::Abs(P.Y));
        Shape[Y*(R.NX+1)+X]=H+Lump+Rill+.08*FMath::Exp(-Wall/2.);
    }
    R.Heights.SetNumUninitialized(Shape.Num(),false);
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
    R.Volume=Integrate((Low+High)*.5);
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
void Section(const FSurface& S,bool AlongLength,double Offset,TArray<FSectionSample>& Out)
{
    Out.Reset();if(S.Heights.IsEmpty())return;
    const double Span=AlongLength?S.Settings.Length:S.Settings.Width,Step=AlongLength?S.DX:S.DY;
    const double Across=AlongLength?S.Settings.Width:S.Settings.Length,CrossStep=AlongLength?S.DY:S.DX;
    Offset=FMath::Clamp(Offset,-Across*.5,Across*.5);
    const double Cell=(Offset+Across*.5)/CrossStep,Fraction=Cell-FMath::FloorToDouble(Cell);
    const int32 N=AlongLength?S.NX:S.NY;
    auto Add=[&](double P){Out.Add({P,AlongLength?S.Height(P,Offset):S.Height(Offset,P)});};
    for(int32 I=0;I<N;++I){Add(I*Step-Span*.5);if(Fraction>1.e-8&&Fraction<1-1.e-8)Add((I+Fraction)*Step-Span*.5);}
    Add(Span*.5);
}
double Integrate(const FSurface& S)
{
    double Sum=0;
    for(int32 Y=0;Y<S.NY;++Y)for(int32 X=0;X<S.NX;++X)
        Sum+=(2.*S.At(X,Y)+S.At(X+1,Y)+S.At(X,Y+1)+2.*S.At(X+1,Y+1))/6.*S.DX*S.DY;
    return Sum;
}
bool Raycast(const FSurface& S,FVector O,FVector D,FVector& Hit)
{
    if(O.ContainsNaN()||D.ContainsNaN()||!D.Normalize()||S.Heights.IsEmpty())return false;
    double Nearest=DBL_MAX;
    auto Triangle=[&](FVector A,FVector B,FVector C)
    {
        const FVector E1=B-A,E2=C-A,P=FVector::CrossProduct(D,E2);
        const double Det=FVector::DotProduct(E1,P);if(FMath::Abs(Det)<1.e-12)return;
        const FVector T=O-A;const double U=FVector::DotProduct(T,P)/Det;if(U<0||U>1)return;
        const FVector Q=FVector::CrossProduct(T,E1);const double V=FVector::DotProduct(D,Q)/Det;if(V<0||U+V>1)return;
        const double Distance=FVector::DotProduct(E2,Q)/Det;
        if(Distance>=0&&Distance<Nearest&&(O+D*Distance).Z>1.e-4)Nearest=Distance;
    };
    for(int32 Y=0;Y<S.NY;++Y)for(int32 X=0;X<S.NX;++X)
    {
        const double PX=X*S.DX-S.Settings.Length*.5,PY=Y*S.DY-S.Settings.Width*.5;
        const FVector A(PX,PY,S.At(X,Y)),B(PX+S.DX,PY,S.At(X+1,Y));
        const FVector C(PX,PY+S.DY,S.At(X,Y+1)),E(PX+S.DX,PY+S.DY,S.At(X+1,Y+1));
        Triangle(A,E,B);Triangle(A,C,E);
    }
    if(Nearest==DBL_MAX)return false;Hit=O+D*Nearest;return true;
}
double Excavate(FSurface& S,FVector2D Center,double Radius,double Depth,double Aspect)
{
    if(S.Heights.IsEmpty()||Center.ContainsNaN()||!FMath::IsFinite(Radius)||!FMath::IsFinite(Depth)||
       !FMath::IsFinite(Aspect)||Aspect<.5||Aspect>2||Radius<.5||Radius>5||Depth<.1||Depth>3||FMath::Abs(Center.X)>S.Settings.Length*.5||
       FMath::Abs(Center.Y)>S.Settings.Width*.5||S.Height(Center.X,Center.Y)<=1.e-4)return 0;
    const double Before=Integrate(S);
    for(int32 Y=0;Y<=S.NY;++Y)for(int32 X=0;X<=S.NX;++X)
    {
        const FVector2D Delta=FVector2D(X*S.DX-S.Settings.Length*.5,Y*S.DY-S.Settings.Width*.5)-Center;
        const double R2=FMath::Square(Delta.X/(Radius*Aspect))+FMath::Square(Delta.Y/Radius);
        if(R2<1){auto& H=S.Heights[Y*(S.NX+1)+X];H=FMath::Max(0.,H-Depth*(1-R2));}
    }
    // Relax actual face gradients by moving remaining material into the cavity.
    // The exact mesh integral gives each vertex a global triangle-incidence
    // weight. Contraction about that weighted mean conserves remaining volume
    // (up to float rounding), stays above the floor, and avoids counting slump
    // material as another grab. Requested depth is before this settling pass.
    const double Limit=FMath::Tan(FMath::DegreesToRadians(S.Repose));const int32 Row=S.NX+1;
    auto Weight=[&](int32 I){const int32 X=I%Row,Y=I/Row;return double((X<S.NX&&Y<S.NY?2:0)+(X>0&&Y>0?2:0)+(X>0&&Y<S.NY?1:0)+(X<S.NX&&Y>0?1:0));};
    auto Relax=[&](int32 A,int32 B,int32 C,bool First)
    {
        const double HA=S.Heights[A],HB=S.Heights[B],HC=S.Heights[C];
        const double GX=(First?HB-HA:HC-HB)/S.DX,GY=(First?HC-HB:HB-HA)/S.DY;
        const double Gradient=FMath::Sqrt(GX*GX+GY*GY);if(Gradient<=Limit+1.e-6)return false;
        const double WA=Weight(A),WB=Weight(B),WC=Weight(C),Mean=(HA*WA+HB*WB+HC*WC)/(WA+WB+WC),Scale=Limit*(1-2.e-6)/Gradient;bool Changed=false;
        for(int32 I:{A,B,C}){const float H=Mean+(S.Heights[I]-Mean)*Scale;Changed|=H!=S.Heights[I];S.Heights[I]=H;}return Changed;
    };
    bool Unstable=false;
    for(int32 Pass=0;Pass<2*(S.NX+S.NY);++Pass)
    {
        bool Changed=false;
        for(int32 J=0;J<S.NY;++J)for(int32 I=0;I<S.NX;++I)
        {
            const int32 X=Pass%2?S.NX-1-I:I,Y=Pass%2?S.NY-1-J:J,A=Y*Row+X,B=A+1,C=A+Row,D=C+1;
            Changed|=Relax(A,B,D,true);Changed|=Relax(A,C,D,false);
        }
        Unstable=Changed;if(!Changed)break;
    }
    // Bound work for pathological custom fields as well. A final contraction
    // about the whole-field weighted mean preserves volume and bounds every
    // triangle if the local iteration budget was exhausted.
    if(Unstable)
    {
        double Maximum=0;
        for(int32 Y=0;Y<S.NY;++Y)for(int32 X=0;X<S.NX;++X)
        {const double A=S.At(X,Y),B=S.At(X+1,Y),C=S.At(X,Y+1),D=S.At(X+1,Y+1);Maximum=FMath::Max(Maximum,FMath::Max(FMath::Sqrt(FMath::Square((B-A)/S.DX)+FMath::Square((D-B)/S.DY)),FMath::Sqrt(FMath::Square((D-C)/S.DX)+FMath::Square((C-A)/S.DY))));}
        if(Maximum>Limit+1.e-6){const double Mean=Integrate(S)/(S.Settings.Length*S.Settings.Width),Scale=Limit*(1-2.e-6)/Maximum;for(auto& H:S.Heights)H=Mean+(H-Mean)*Scale;}
    }
    S.Volume=Integrate(S);S.Fill=100*S.Volume/(S.Settings.Length*S.Settings.Width*S.Settings.Depth);
    return FMath::Max(0.,Before-S.Volume);
}
bool ThroughOpening(const FSettings& S,FVector Origin,FVector Target)
{
    for(double Z:{S.Depth,S.Depth+S.Coaming})
    {
        if(Target.Z>=Z)continue;
        if(Origin.Z<=Z||Origin.Z<=Target.Z)return false;
        const FVector P=Origin+(Target-Origin)*((Origin.Z-Z)/(Origin.Z-Target.Z));
        if(FMath::Abs(P.X)>S.OpeningLength*.5+1.e-6||FMath::Abs(P.Y)>S.OpeningWidth*.5+1.e-6)return false;
    }
    return true;
}
FAnalysis Analyze(const FSettings& S,double CellSize,const TArray<FObservation>& Points)
{
    FAnalysis R;AnalyzeInto(S,CellSize,Points,R);return R;
}
void AnalyzeInto(const FSettings& S,double CellSize,const TArray<FObservation>& Points,FAnalysis& R)
{
    auto Cells=MoveTemp(R.Cells);R=FAnalysis();R.Cells=MoveTemp(Cells);R.Cells.Reset();
    if(!FMath::IsFinite(CellSize)||CellSize<.25||CellSize>5)return;
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
}
}
