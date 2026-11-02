#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "ma0t10_dt/MA0T10/Sensor/DeviceSensorStreamCodec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceStatelessReceiverBoundary,"MA0T10.DeviceBoundary.StatelessReceiverBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeviceStatelessReceiverBoundary::RunTest(const FString&)
{
    FString Json;
    const FString Path=FPaths::ProjectDir()/TEXT("Source/ma0t10_dt/MA0T10/Sensor/Tests/Fixtures/ArchitectureWire/with-context.lidar.json");
    if(!TestTrue(TEXT("committed golden fixture loaded"),FFileHelper::LoadFileToString(Json,*Path)))return false;
    auto Work=Async(EAsyncExecution::ThreadPool,[Json](){return DeviceSensorStreamCodec::ParseLidarJson(Json);});
    const auto Data=StaticCastSharedPtr<FVirtualLidarStreamReceiverData>(Work.Get());
    if(!TestTrue(TEXT("plain runtime receiver data returned"),Data.IsValid()))return false;
    TestTrue(TEXT("golden telemetry accepted on worker without TC UObject"),Data->bValid);
    TestEqual(TEXT("wire sensor identity"),Data->SensorId,FString(TEXT("sensor-fixed-007")));
    TestEqual(TEXT("golden rays"),Data->RayCount,2);
    TestEqual(TEXT("golden total points"),Data->TotalPointCount,2);
    TestEqual(TEXT("golden hit points"),Data->HitPointCount,2);
    const auto Invalid=StaticCastSharedPtr<FVirtualCameraStreamReceiverData>(DeviceSensorStreamCodec::ParseCameraJson(TEXT("{}")));
    TestTrue(TEXT("invalid schema is rejected by plain codec"),Invalid.IsValid()&&!Invalid->bValid);
    return true;
}
#endif
