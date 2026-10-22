#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchDimensions,"MA0T10.Hatch.DimensionsAndValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchDimensions::RunTest(const FString&)
{
    ShipHatch::FSettings S;FString Error;TestTrue(TEXT("default settings valid"),S.Validate(Error));
    TestEqual(TEXT("breadth meters"),S.Width,42.3);TestEqual(TEXT("length meters"),S.Length,25.8);TestEqual(TEXT("depth meters"),S.Depth,22.5);
    TestEqual(TEXT("opening breadth"),S.OpeningWidth,23.);TestEqual(TEXT("opening length"),S.OpeningLength,20.);TestEqual(TEXT("coaming"),S.Coaming,2.);
    TestTrue(TEXT("underdeck blind area is real"),S.OpeningWidth<S.Width&&S.OpeningLength<S.Length);
    S.FillMin=70;S.FillMax=20;TestFalse(TEXT("inverted fill rejected"),S.Validate(Error));
    S=ShipHatch::FSettings();S.OpeningWidth=50;TestFalse(TEXT("opening outside hold rejected"),S.Validate(Error));
    S=ShipHatch::FSettings();S.SensorAboveDeck=1;TestFalse(TEXT("sensor inside coaming rejected"),S.Validate(Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchGeneration,"MA0T10.Hatch.DeterministicCargoVolumeAndSlope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchGeneration::RunTest(const FString&)
{
    TSet<uint32> Hashes;
    for(auto Cargo:{ShipHatch::ECargo::IronOre,ShipHatch::ECargo::Coal})for(int32 Seed=0;Seed<50;++Seed)
    {
        ShipHatch::FSettings S;S.Cargo=Cargo;S.Seed=Seed;auto A=ShipHatch::Generate(S),B=ShipHatch::Generate(S);
        TestEqual(TEXT("same seed, settings and material reproduce exact float field"),A.Hash(),B.Hash());Hashes.Add(A.Hash());
        TestTrue(TEXT("seed selects fill inside configured range"),A.Fill>=S.FillMin&&A.Fill<=S.FillMax);
        const double Expected=S.Length*S.Width*S.Depth*A.Fill*.01;
        TestTrue(TEXT("triangle-integral generated volume error < 0.001 percent"),FMath::Abs(A.Volume-Expected)/Expected<.00001);
        const double Limit=FMath::Tan(FMath::DegreesToRadians(A.Repose));
        for(int32 Y=0;Y<A.NY;++Y)for(int32 X=0;X<A.NX;++X)
        {
            TestTrue(TEXT("heights remain inside hold"),A.At(X,Y)>=0&&A.At(X,Y)<=S.Depth);
            TestTrue(TEXT("neighbor X slope bounded by selected repose"),FMath::Abs(A.At(X+1,Y)-A.At(X,Y))/A.DX<=Limit+1.e-5);
            TestTrue(TEXT("neighbor Y slope bounded by selected repose"),FMath::Abs(A.At(X,Y+1)-A.At(X,Y))/A.DY<=Limit+1.e-5);
            const double GX=(A.At(X+1,Y)-A.At(X,Y))/A.DX,GY=(A.At(X+1,Y+1)-A.At(X+1,Y))/A.DY;
            TestTrue(TEXT("actual triangular face gradient stays within repose cap"),FMath::Sqrt(GX*GX+GY*GY)<=Limit+1.e-5);
        }
    }
    TestEqual(TEXT("all material/seed fields distinct"),Hashes.Num(),100);
    TestTrue(TEXT("coal is less reflective"),ShipHatch::Reflectivity(ShipHatch::ECargo::Coal)<ShipHatch::Reflectivity(ShipHatch::ECargo::IronOre));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchFullLoadRelief,"MA0T10.Hatch.FullLoadKeepsVolumeAndRelief",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchFullLoadRelief::RunTest(const FString&)
{
    for(auto Cargo:{ShipHatch::ECargo::IronOre,ShipHatch::ECargo::Coal})for(int32 Seed=0;Seed<30;++Seed)
    {
        ShipHatch::FSettings S;S.Cargo=Cargo;S.Seed=Seed;S.FillMin=S.FillMax=95;
        const auto F=ShipHatch::Generate(S);const double Target=S.Length*S.Width*S.Depth*.95;
        TestTrue(TEXT("full load keeps original95percent triangle-volume target within0.001percent"),FMath::Abs(F.Volume-Target)/Target<.00001);
        double Low=DBL_MAX,High=-DBL_MAX;int32 Clipped=0;
        for(float H:F.Heights){Low=FMath::Min(Low,double(H));High=FMath::Max(High,double(H));Clipped+=H>=S.Depth-.001;}
        TestTrue(TEXT("full load has at least1m actual mound/wall relief"),High-Low>=1);
        TestTrue(TEXT("deck clipping covers less than1percent of the full-load vertices"),Clipped<F.Heights.Num()*.01);
        TestTrue(TEXT("configured material repose retains user ranges"),Cargo==ShipHatch::ECargo::Coal?(F.Repose>=33&&F.Repose<=35):(F.Repose>=35&&F.Repose<=40));
        const double Limit=FMath::Tan(FMath::DegreesToRadians(F.Repose));
        for(int Y=0;Y<F.NY;++Y)for(int X=0;X<F.NX;++X)
        {
            const double GX1=(F.At(X+1,Y)-F.At(X,Y))/F.DX,GY1=(F.At(X+1,Y+1)-F.At(X+1,Y))/F.DY;
            const double GX2=(F.At(X+1,Y+1)-F.At(X,Y+1))/F.DX,GY2=(F.At(X,Y+1)-F.At(X,Y))/F.DY;
            if(FMath::Max(GX1*GX1+GY1*GY1,GX2*GX2+GY2*GY2)>Limit*Limit+1.e-5){AddError(TEXT("full-load actual triangle exceeds repose"));break;}
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchAnalysis,"MA0T10.Hatch.AnalysisRejectsStructureAndMissingCells",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchAnalysis::RunTest(const FString&)
{
    ShipHatch::FSettings S;S.FillMin=S.FillMax=50;auto Surface=ShipHatch::Generate(S);
    TArray<ShipHatch::FObservation> Points;
    const double Cell=1;const int32 NX=FMath::CeilToInt(S.Length/Cell),NY=FMath::CeilToInt(S.Width/Cell);
    double Reference=0;
    for(int32 Y=0;Y<NY;++Y)for(int32 X=0;X<NX;++X)
    {
        const double PX=(X+.5)*S.Length/NX-S.Length*.5,PY=(Y+.5)*S.Width/NY-S.Width*.5,H=Surface.Height(PX,PY);
        Points.Add({FVector(PX,PY,H),TEXT("HatchCargo")});Reference+=H*S.Length/NX*S.Width/NY;
    }
    auto A=ShipHatch::Analyze(S,Cell,Points);TestEqual(TEXT("known complete observations cover all cells"),A.Coverage,1.);
    TestTrue(TEXT("analysis integral matches independent midpoint reference"),FMath::Abs(A.Volume-Reference)<1.e-6);
    TestTrue(TEXT("midpoint vs triangle volume error < 0.2 percent"),FMath::Abs(A.Volume-Surface.Volume)/Surface.Volume<.002);
    for(int32 I=0;I<Points.Num();++I)TestTrue(TEXT("per-cell mean uses measured height"),FMath::Abs(A.Cells[I].Mean()-Points[I].PositionMeters.Z)<1.e-6);
    Points.SetNum(10);Points.Add({FVector(0,0,22),TEXT("HatchCoaming")});Points.Add({FVector(0,0,21),TEXT("HatchHold")});
    A=ShipHatch::Analyze(S,Cell,Points);TestEqual(TEXT("structure never becomes cargo"),A.Observed,10);
    TestTrue(TEXT("missing cells stay explicitly unobserved"),A.Coverage<.01&&A.Cells.Last().Count==0);
    TestTrue(TEXT("partial volume never extrapolated to whole hold"),A.Volume<Surface.Volume*.1);
    const auto* Cells=A.Cells.GetData();const auto Bytes=A.Cells.GetAllocatedSize();
    const TArray<ShipHatch::FObservation> Single{Points[0]};
    for(int I=0;I<50;++I){ShipHatch::AnalyzeInto(S,Cell,Single,A);TestEqual(TEXT("reused analysis resets counts rather than accumulating revisions"),A.Observed,1);TestEqual(TEXT("single known cell keeps exactly one measured sample"),A.Cells[0].Count,1);TestTrue(TEXT("reused cell allocation remains bounded"),A.Cells.GetData()==Cells&&A.Cells.GetAllocatedSize()==Bytes);}
    TestTrue(TEXT("reused partial integral matches the one independent cell"),FMath::Abs(A.Volume-Points[0].PositionMeters.Z*S.Length/NX*S.Width/NY)<1.e-6);
    ShipHatch::AnalyzeInto(S,.1,Single,A);TestTrue(TEXT("invalid request clears all prior metrics and cells"),A.Cells.IsEmpty()&&A.Observed==0&&A.Volume==0&&A.Coverage==0);
    ShipHatch::AnalyzeInto(S,Cell,Points,A);TestEqual(TEXT("valid request recovers after invalid input without stale cells"),A.Observed,10);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchExcavation,"MA0T10.Hatch.GrabHeightfield",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchExcavation::RunTest(const FString&)
{
    for(auto Cargo:{ShipHatch::ECargo::IronOre,ShipHatch::ECargo::Coal})
    {
        ShipHatch::FSettings S;S.Cargo=Cargo;auto A=ShipHatch::Generate(S);const auto Original=A;
        const int32 X=A.NX/2,Y=A.NY/2;const FVector2D Center(X*A.DX-S.Length*.5,Y*A.DY-S.Width*.5);
        FVector Hit;TestTrue(TEXT("SI ray hits exact current triangle"),ShipHatch::Raycast(A,FVector(Center,40),FVector(0,0,-1),Hit));
        TestTrue(TEXT("ray center accuracy < 1 micrometer"),(FVector2D(Hit.X,Hit.Y)-Center).Size()<1.e-6&&FMath::Abs(Hit.Z-A.Height(Center.X,Center.Y))<1.e-6);
        TestFalse(TEXT("ray outside hold misses rather than clamps"),ShipHatch::Raycast(A,FVector(100,100,40),FVector(0,0,-1),Hit));
        TestFalse(TEXT("ray pointing away misses"),ShipHatch::Raycast(A,FVector(0,0,40),FVector(0,0,1),Hit));
        auto Integral=[](const ShipHatch::FSurface& F)
        {
            double V=0;for(int32 J=0;J<F.NY;++J)for(int32 I=0;I<F.NX;++I)
            {const double A=F.At(I,J),B=F.At(I+1,J),C=F.At(I,J+1),D=F.At(I+1,J+1);V+=((A+B+D)/3.+(A+C+D)/3.)*(F.DX*F.DY*.5);}return V;
        };
        auto Grab=Original;
        for(int32 J=0;J<=Grab.NY;++J)for(int32 I=0;I<=Grab.NX;++I)
        {const double R2=(FVector2D(I*Grab.DX-S.Length*.5,J*Grab.DY-S.Width*.5)-Center).SizeSquared()/4.;if(R2<1)Grab.Heights[J*(Grab.NX+1)+I]=FMath::Max(0.,Grab.At(I,J)-1.5*(1-R2));}
        const double Removed=ShipHatch::Excavate(A,Center,2,1.5);
        TestTrue(TEXT("default grab removes positive volume"),Removed>0);
        TestTrue(TEXT("removed volume equals independent triangle-difference integral within 1e-6 m3"),FMath::Abs(Removed-(Integral(Original)-Integral(A)))<1.e-6);
        TestTrue(TEXT("settled center is depressed but never deeper than the 1.5m grab"),Original.At(X,Y)-A.At(X,Y)>.05&&Original.At(X,Y)-A.At(X,Y)<=1.5+2.e-6);
        TestTrue(TEXT("settling preserves post-grab triangle volume within 0.005m3"),FMath::Abs(Integral(A)-Integral(Grab))<.005);
        TestTrue(TEXT("remaining fill follows exact remaining volume"),FMath::Abs(A.Fill-100*A.Volume/(S.Length*S.Width*S.Depth))<1.e-10);
        TestTrue(TEXT("repeat grab accumulates"),ShipHatch::Excavate(A,Center,2,1.5)>0&&A.Volume<Original.Volume-Removed);
        for(int32 I=0;I<30;++I)ShipHatch::Excavate(A,Center,2,1.5);
        const double Limit=FMath::Tan(FMath::DegreesToRadians(A.Repose));double MaxSlope=0;
        for(int32 J=0;J<A.NY;++J)for(int32 I=0;I<A.NX;++I)
        {
            const double GX1=(A.At(I+1,J)-A.At(I,J))/A.DX,GY1=(A.At(I+1,J+1)-A.At(I+1,J))/A.DY;
            const double GX2=(A.At(I+1,J+1)-A.At(I,J+1))/A.DX,GY2=(A.At(I,J+1)-A.At(I,J))/A.DY;
            MaxSlope=FMath::Max(MaxSlope,FMath::Max(FMath::Sqrt(GX1*GX1+GY1*GY1),FMath::Sqrt(GX2*GX2+GY2*GY2)));
        }
        TestTrue(TEXT("both triangle gradients obey selected material repose with 1e-5 tolerance"),MaxSlope<=Limit+1.e-5);
        for(float H:A.Heights)if(H<0||H>S.Depth){AddError(TEXT("settling crossed floor or hold height"));break;}
        // A shallow stamp does not need shoulder relaxation: radius is independently observable.
        auto Flat=Original;for(auto& H:Flat.Heights)H=10;Flat.Volume=Integral(Flat);
        ShipHatch::Excavate(Flat,Center,2,.1);bool RadiusExact=true;
        for(int32 J=0;J<=Flat.NY;++J)for(int32 I=0;I<=Flat.NX;++I)
            if(FVector2D::Distance(FVector2D(I*Flat.DX-S.Length*.5,J*Flat.DY-S.Width*.5),Center)>=2&&Flat.At(I,J)!=10)RadiusExact=false;
        TestTrue(TEXT("unrelaxed footprint stays inside requested 2m radius"),RadiusExact);
        const uint32 Hash=A.Hash();TestEqual(TEXT("invalid radius ignored"),ShipHatch::Excavate(A,Center,-2,1.5),0.);TestEqual(TEXT("invalid stamp does not change field"),A.Hash(),Hash);
        for(auto& H:Flat.Heights)H=.05f;ShipHatch::Excavate(Flat,Center,2,3);TestEqual(TEXT("grab stops exactly at floor"),Flat.At(X,Y),0.f);
        bool Floor=true;for(float H:Flat.Heights)Floor&=H>=0;TestTrue(TEXT("all settled heights obey floor lower bound"),Floor);
        for(auto& H:Flat.Heights)H=0;TestFalse(TEXT("empty tank top is not cargo"),ShipHatch::Raycast(Flat,FVector(0,0,40),FVector(0,0,-1),Hit));
    }
    for(int32 Seed=0;Seed<20;++Seed)
    {
        ShipHatch::FSettings S;S.Seed=Seed;S.MeshCell=Seed%3==0?2:.4;S.Cargo=Seed%2?ShipHatch::ECargo::Coal:ShipHatch::ECargo::IronOre;
        auto A=ShipHatch::Generate(S);const FVector2D Center((A.NX/2)*A.DX-S.Length*.5,(A.NY/2)*A.DY-S.Width*.5);
        ShipHatch::Excavate(A,Center,Seed%2?.5:5,Seed%2?.1:3);
        const double Limit=FMath::Tan(FMath::DegreesToRadians(A.Repose));double MaxSlope=0;
        for(int32 Y=0;Y<A.NY;++Y)for(int32 X=0;X<A.NX;++X)
        {
            const double GX1=(A.At(X+1,Y)-A.At(X,Y))/A.DX,GY1=(A.At(X+1,Y+1)-A.At(X+1,Y))/A.DY;
            const double GX2=(A.At(X+1,Y+1)-A.At(X,Y+1))/A.DX,GY2=(A.At(X,Y+1)-A.At(X,Y))/A.DY;
            MaxSlope=FMath::Max(MaxSlope,FMath::Max(FMath::Sqrt(GX1*GX1+GY1*GY1),FMath::Sqrt(GX2*GX2+GY2*GY2)));
        }
        TestTrue(TEXT("configured radius/depth and coarse/fine grids obey actual face repose"),MaxSlope<=Limit+1.e-5);
    }
    ShipHatch::FSettings Maximum;Maximum.Length=100;Maximum.Width=32;Maximum.MeshCell=.4;
    FString Error;TestTrue(TEXT("maximum 20000-cell history fixture valid"),Maximum.Validate(Error));
    const auto MaximumSurface=ShipHatch::Generate(Maximum);TArray<float> Retained[18];uint64 Bytes=0;
    for(auto& Field:Retained){Field=MaximumSurface.Heights;Bytes+=Field.GetAllocatedSize();}
    TestTrue(TEXT("original + 16 undo states + scratch at maximum mesh allocate <=2MiB"),Bytes<=2*1024*1024);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchAperture,"MA0T10.Hatch.ApertureVisibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchAperture::RunTest(const FString&)
{
    ShipHatch::FSettings S;
    TestTrue(TEXT("central open floor ray passes deck and coaming"),ShipHatch::ThroughOpening(S,FVector(0,0,75),FVector(0,0,0)));
    TestFalse(TEXT("deck occludes a point below its outer wing"),ShipHatch::ThroughOpening(S,FVector(0,0,75),FVector(0,21,15)));
    TestFalse(TEXT("coaming can occlude a ray which passes deck opening"),ShipHatch::ThroughOpening(S,FVector(0,14.5,30.5),FVector(0,0,0)));
    TestTrue(TEXT("oblique ray can see under-deck floor through aperture"),ShipHatch::ThroughOpening(S,FVector(0,0,30.5),FVector(0,20,0)));
    TestFalse(TEXT("observer below deck cannot see through its top opening"),ShipHatch::ThroughOpening(S,FVector(0,0,20),FVector(0,0,0)));
    return true;
}
#endif
