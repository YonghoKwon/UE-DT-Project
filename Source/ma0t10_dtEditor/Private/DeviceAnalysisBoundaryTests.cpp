#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Async/Async.h"
#include "Device/Sensor/DevicePointCloudAnalysis.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceAnalysisGeometryOracle,"MA0T10.DeviceBoundary.AnalysisGeometryOracle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeviceAnalysisGeometryOracle::RunTest(const FString&)
{
    const auto Provider=DevicePointCloudAnalysis::Snapshot();
    if(!TestTrue(TEXT("host analysis policy registered before any scan"),Provider.IsValid()))return false;
    TArray<FVirtualLidarPoint> Points;
    for(const FVector& Local:{FVector(-100,-10,20),FVector(-100,10,20),FVector(100,-10,20),FVector(100,10,20)})
    {
        FVirtualLidarPoint P;P.bHit=true;P.SemanticLabel=TEXT("Slab");P.WorldLocation=FRotator(0,30,0).RotateVector(Local);Points.Add(P);
    }
    FVirtualLidarPoint Other;Other.bHit=true;Other.SemanticLabel=TEXT("Floor");Other.WorldLocation=FVector(9000,8000,7000);Points.Add(Other);
    FVirtualLidarPoint Miss=Other;Miss.bHit=false;Miss.SemanticLabel=TEXT("Slab");Points.Add(Miss);
    auto Result=Provider->Analyze(Points,TEXT("Slab"),5,3);
    TestTrue(TEXT("rectangle valid"),Result.bValid);
    TestEqual(TEXT("host filters label and misses"),Result.SlabHitPointCount,4);
    TestTrue(TEXT("independent analytic centroid"),Result.Center.Equals(FVector(0,0,20),1e-4));
    TestTrue(TEXT("independent analytic yaw"),FMath::IsNearlyEqual(Result.EstimatedYawDegrees,30.f,.001f));
    TestTrue(TEXT("host reference yaw preserved"),FMath::IsNearlyEqual(Result.AngleDeviationDegrees,25.f,.001f));
    TestTrue(TEXT("analytic covariance ratio"),FMath::IsNearlyEqual(Result.Confidence,.99f,.0001f));
    TestEqual(TEXT("legacy status bytes"),Result.StatusMessage,FString(TEXT("ok")));
    const auto Small=Provider->Analyze(Points,TEXT("Slab"),5,12);
    TestFalse(TEXT("host minimum preserved"),Small.bValid);
    TestEqual(TEXT("legacy insufficient status"),Small.StatusMessage,FString(TEXT("insufficient slab points: 4/12")));
    auto Worker=Async(EAsyncExecution::ThreadPool,[Provider,Points](){return Provider->Analyze(Points,TEXT("Slab"),5,3);});
    const auto AsyncResult=Worker.Get();
    TestEqual(TEXT("immutable worker snapshot yaw"),AsyncResult.EstimatedYawDegrees,Result.EstimatedYawDegrees);
    DevicePointCloudAnalysis::Unregister();
    TestFalse(TEXT("explicit unregistration"),DevicePointCloudAnalysis::Snapshot().IsValid());
    TestTrue(TEXT("in-flight immutable policy retains lifetime"),Provider->Analyze(Points,TEXT("Slab"),5,3).bValid);
    DevicePointCloudAnalysis::Register(Provider.ToSharedRef());
    return true;
}
#endif
