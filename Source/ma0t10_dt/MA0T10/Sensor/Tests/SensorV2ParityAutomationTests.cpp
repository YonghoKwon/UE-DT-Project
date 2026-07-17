#if WITH_DEV_AUTOMATION_TESTS

#include "Json.h"
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraPayloadCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSensorV2CameraPayloadParityTest,
    "MA0T10.SensorV2.Parity.CameraPayloadContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSensorV2LidarPayloadParityTest,
    "MA0T10.SensorV2.Parity.LidarPayloadContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
TSharedPtr<FJsonObject> ParseObject(const FString& Json)
{
    TSharedPtr<FJsonObject> Result;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    FJsonSerializer::Deserialize(Reader, Result);
    return Result;
}
}

bool FSensorV2CameraPayloadParityTest::RunTest(const FString& Parameters)
{
    FVirtualCameraPayloadSnapshot Snapshot;
    Snapshot.SensorId = TEXT("CAM-V2-PARITY");
    Snapshot.Manufacturer = TEXT("Intel");
    Snapshot.Model = TEXT("RealSense D455");
    Snapshot.SimulationQuality = TEXT("FullSpec");
    Snapshot.FrameId = 42;
    Snapshot.TimestampUtc = FDateTime(2026, 7, 17, 0, 0, 0);
    Snapshot.Width = 1280;
    Snapshot.Height = 720;
    Snapshot.HorizontalFov = 87.0f;
    Snapshot.VerticalFov = 58.0f;
    Snapshot.Location = FVector(100.0, 200.0, 170.0);

    const FString Payload = FVirtualCameraPayloadCodec::EncodeJpegBase64(Snapshot, TEXT("/9j/AA=="), 4);
    const TSharedPtr<FJsonObject> Root = ParseObject(Payload);
    TestTrue(TEXT("Camera V2 payload parses"), Root.IsValid());
    if (!Root) return false;
    TestEqual(TEXT("Camera schema remains v1"), Root->GetStringField(TEXT("schemaVersion")), FString(TEXT("virtual-camera.v1")));
    TestEqual(TEXT("Camera sensor type remains stable"), Root->GetStringField(TEXT("sensorType")), FString(TEXT("virtual_camera")));
    TestEqual(TEXT("Camera frame id remains stable"), static_cast<int64>(Root->GetNumberField(TEXT("frameId"))), static_cast<int64>(42));
    TestEqual(TEXT("Camera FullSpec width"), static_cast<int32>(Root->GetNumberField(TEXT("width"))), 1280);
    TestEqual(TEXT("Camera FullSpec height"), static_cast<int32>(Root->GetNumberField(TEXT("height"))), 720);
    TestEqual(TEXT("Camera encoding remains JPEG base64"), Root->GetStringField(TEXT("encoding")), FString(TEXT("jpeg/base64")));
    return true;
}

bool FSensorV2LidarPayloadParityTest::RunTest(const FString& Parameters)
{
    UVirtualLidarScanComponent* Lidar = NewObject<UVirtualLidarScanComponent>();
    Lidar->SensorId = TEXT("LIDAR-V2-PARITY");
    Lidar->HorizontalSamples = 2;
    Lidar->VerticalChannels = 1;
    Lidar->SetServerPayloadPolicy(1, 0, false);

    FVirtualLidarPoint Point;
    Point.WorldLocation = FVector(100.0, 0.0, 0.0);
    Point.LocalDirection = FVector::ForwardVector;
    Point.Distance = 100.0f;
    Point.bHit = true;
    Point.bHasGridCoord = true;
    Point.Row = 0;
    Point.Col = 0;
    Point.SemanticLabel = TEXT("Target");
    Lidar->InjectPointCloudFrame({Point}, false);

    const TSharedPtr<FJsonObject> Root = ParseObject(Lidar->GetLastJsonPayload());
    TestTrue(TEXT("LiDAR V2 payload parses"), Root.IsValid());
    if (!Root) return false;
    TestEqual(TEXT("LiDAR schema remains v1"), Root->GetStringField(TEXT("schemaVersion")), FString(TEXT("virtual-lidar.v1")));
    TestEqual(TEXT("LiDAR sensor type remains stable"), Root->GetStringField(TEXT("sensorType")), FString(TEXT("virtual_lidar")));
    const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
    TestTrue(TEXT("LiDAR payload keeps points array"), Root->TryGetArrayField(TEXT("points"), Points));
    TestEqual(TEXT("LiDAR point count remains stable"), Points ? Points->Num() : 0, 1);
    if (Points && Points->Num() == 1)
    {
        TestEqual(TEXT("LiDAR semantic label remains stable"), Points->operator[](0)->AsObject()->GetStringField(TEXT("semanticLabel")), FString(TEXT("Target")));
    }
    return true;
}

#endif
