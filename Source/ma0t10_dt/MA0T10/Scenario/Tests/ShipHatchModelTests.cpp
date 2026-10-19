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
    TestTrue(TEXT("partial volume never extrapolated to whole hold"),A.Volume<Surface.Volume*.1);return true;
}
#endif
