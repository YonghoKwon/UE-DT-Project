#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGeometry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarGeometryTest,"MA0T10.LidarGeometry.GeometryOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarGeometryTest::RunTest(const FString&)
{
    for(double Tilt:{0.0,0.15})
    {
        TArray<FVector> Points;
        for(int X=-40;X<=40;++X) for(int Y=-40;Y<=40;++Y) Points.Add(FVector(X*5,Y*5,Tilt*X*5));
        for(int X=-10;X<=10;++X) for(int Y=-10;Y<=10;++Y) Points.Add(FVector(X*5,Y*5,25+Tilt*X*5));
        const auto R=VirtualLidarGeometry::Analyze(Points);
        TestTrue(TEXT("tag-free plane reliable"),R.bReliable);
        TestEqual(TEXT("plate blue"),R.Classify(FVector(0,0,0)),ELidarGeometryClass::Plane);
        TestEqual(TEXT("25cm object orange"),R.Classify(FVector(0,0,25)),ELidarGeometryClass::Protrusion);
        TestNotEqual(TEXT("actual colors differ"),VirtualLidarGeometry::Color(ELidarGeometryClass::Plane),VirtualLidarGeometry::Color(ELidarGeometryClass::Protrusion));
        TestTrue(TEXT("bounded runtime reported"),R.ProcessingMs>=0);
    }
    TestFalse(TEXT("empty scene falls back"),VirtualLidarGeometry::Analyze({}).bReliable);
    TArray<FVector> Vertical;
    for(int Y=0;Y<30;++Y) for(int Z=0;Z<30;++Z) Vertical.Add(FVector(0,Y*5,Z*5));
    TestFalse(TEXT("wall is not a support plane"),VirtualLidarGeometry::Analyze(Vertical).bReliable);
    TArray<FVector> Flat;
    for(int X=0;X<40;++X) for(int Y=0;Y<40;++Y) Flat.Add(FVector(X*5,Y*5,0));
    auto Raised=Flat; Raised.Add(FVector(50,50,5.1));
    const auto Previous=VirtualLidarGeometry::Analyze(Raised);
    Raised.Last().Z=4.0;
    const auto Next=VirtualLidarGeometry::Analyze(Raised,&Previous);
    TestEqual(TEXT("hysteresis retains 4cm protrusion"),Next.Classify(Raised.Last()),ELidarGeometryClass::Protrusion);
    const auto Cleared=VirtualLidarGeometry::Analyze(Flat,&Next);
    TestEqual(TEXT("no ghost after object leaves"),Cleared.ProtrusionPoints,0);
    auto Ambiguous=Flat;
    for(const auto& P:Flat) Ambiguous.Add(P+FVector(300,0,25));
    TestFalse(TEXT("equally supported parallel planes are ambiguous"),VirtualLidarGeometry::Analyze(Ambiguous).bReliable);
    return true;
}
#endif
