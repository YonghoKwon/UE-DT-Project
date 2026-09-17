#include "VirtualLidarGeometry.h"

ELidarGeometryClass FVirtualLidarGeometryResult::Classify(const FVector& P) const
{
    if (!bReliable || P.ContainsNaN()) return ELidarGeometryClass::Unknown;
    const double Height = FVector::DotProduct(Normal, P) - OffsetCm;
    if (FMath::Abs(Height) <= 2.0) return ELidarGeometryClass::Plane;
    if (Height >= 5.0 || (Height >= 3.0 && ProtrudingCells.Contains(Cell(P)))) return ELidarGeometryClass::Protrusion;
    return ELidarGeometryClass::Unknown;
}

FVirtualLidarGeometryResult VirtualLidarGeometry::Analyze(TConstArrayView<FVector> PointsCm, const FVirtualLidarGeometryResult* Previous)
{
    const double Started = FPlatformTime::Seconds();
    FVirtualLidarGeometryResult R;
    auto Fail = [&](const TCHAR* Reason) { R.bReliable = false; R.Reason = Reason; R.UnknownPoints = PointsCm.Num(); R.ProcessingMs = (FPlatformTime::Seconds()-Started)*1000; return R; };
    if (PointsCm.Num() < 64) return Fail(TEXT("검출점 부족: 높이 색상으로 표시"));
    TArray<FVector> Sample;
    const int32 Count = FMath::Min(4096, PointsCm.Num());
    Sample.Reserve(Count);
    for (int32 I=0; I<Count; ++I)
    {
        const FVector P = PointsCm[int64(I)*PointsCm.Num()/Count];
        if (!P.ContainsNaN()) Sample.Add(P);
    }
    if (Sample.Num() < 64) return Fail(TEXT("유효점 부족: 높이 색상으로 표시"));
    struct FCandidate { FVector N; double D; int32 Support; };
    TArray<FCandidate> Candidates;
    FRandomStream Random(170923);
    const double MinUp = FMath::Cos(FMath::DegreesToRadians(20.0));
    for (int32 Trial=0; Trial<96; ++Trial)
    {
        const FVector A=Sample[Random.RandRange(0,Sample.Num()-1)];
        const FVector B=Sample[Random.RandRange(0,Sample.Num()-1)];
        const FVector C=Sample[Random.RandRange(0,Sample.Num()-1)];
        FVector N=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
        if (N.Z<0) N=-N;
        if (N.Z<MinUp) continue;
        const double D=FVector::DotProduct(N,A);
        int32 Support=0;
        for (const FVector& P:Sample) Support += FMath::Abs(FVector::DotProduct(N,P)-D)<=2.0;
        Candidates.Add({N,D,Support});
    }
    if (Candidates.IsEmpty()) return Fail(TEXT("수평 기준면 없음: 높이 색상으로 표시"));
    Candidates.Sort([](const FCandidate& A,const FCandidate& B){return A.Support>B.Support;});
    const auto Best=Candidates[0];
    if (Best.Support < FMath::Max(64,Sample.Num()/4)) return Fail(TEXT("기준면 지지점 부족: 높이 색상으로 표시"));
    FVector Mean=FVector::ZeroVector;
    for (const FVector& P:Sample) if(FMath::Abs(FVector::DotProduct(Best.N,P)-Best.D)<=2) Mean+=P;
    Mean/=Best.Support;
    for (const auto& Candidate:Candidates)
    {
        if (Candidate.Support < Best.Support*.9) break;
        if (FMath::Abs(FVector::DotProduct(Candidate.N,Mean)-Candidate.D)>4)
            return Fail(TEXT("기준면 후보 모호: 높이 색상으로 표시"));
    }
    double XX=0,YY=0,XY=0,XZ=0,YZ=0;
    FBox Bounds(ForceInit);
    for (const FVector& P:Sample) if(FMath::Abs(FVector::DotProduct(Best.N,P)-Best.D)<=2)
    {
        const FVector V=P-Mean; XX+=V.X*V.X; YY+=V.Y*V.Y; XY+=V.X*V.Y; XZ+=V.X*V.Z; YZ+=V.Y*V.Z; Bounds+=P;
    }
    const double Det=XX*YY-XY*XY;
    if (Det<=1e-6*FMath::Max(1.0,XX*YY) || Bounds.GetSize().X<50 || Bounds.GetSize().Y<50)
        return Fail(TEXT("기준면 면적 부족: 높이 색상으로 표시"));
    R.Normal=FVector(-(XZ*YY-YZ*XY)/Det,-(YZ*XX-XZ*XY)/Det,1).GetSafeNormal();
    if(R.Normal.Z<MinUp) return Fail(TEXT("판 기울기 범위 초과: 높이 색상으로 표시"));
    R.OffsetCm=FVector::DotProduct(R.Normal,Mean); R.bReliable=true;
    const bool bKeepHistory=Previous && Previous->bReliable && FVector::DotProduct(Previous->Normal,R.Normal)>.999 && FMath::Abs(FVector::DotProduct(Previous->Normal,Mean)-Previous->OffsetCm)<2;
    for(const FVector& P:PointsCm)
    {
        if(P.ContainsNaN()) {++R.UnknownPoints; continue;}
        const double H=FVector::DotProduct(R.Normal,P)-R.OffsetCm;
        if(FMath::Abs(H)<=2) ++R.PlanePoints;
        else if(H>=5 || (H>=3 && bKeepHistory && Previous->ProtrudingCells.Contains(FVirtualLidarGeometryResult::Cell(P))))
        { ++R.ProtrusionPoints; R.ProtrudingCells.Add(FVirtualLidarGeometryResult::Cell(P)); }
        else ++R.UnknownPoints;
    }
    R.Reason=TEXT("XYZ 기반 기준면·돌출점 구분");
    R.ProcessingMs=(FPlatformTime::Seconds()-Started)*1000;
    return R;
}
