#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

// f7ec4ab4 contract: sensor local metres, X forward/Y left/Z up.
// Binary transport changes representation, not the frame of reference.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarPrePr16CoordinateContract,
    "MA0T10.LidarRegression.PrePr16CoordinateContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarPrePr16CoordinateContract::RunTest(const FString&)
{
    for (const FRotator Rotation : { FRotator::ZeroRotator, FRotator(-45, 30, 0), FRotator(-90, 0, 0) })
    {
        const FTransform Pose(Rotation, FVector(100, -200, 1000));
        auto Points = MakeShared<TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>();
        for (double Z : { 0.0, 25.0 })
        {
            FVirtualLidarPoint P;
            P.bHit = true;
            P.WorldLocation = FVector(300, 50, Z);
            const FVector Local = Pose.InverseTransformPosition(P.WorldLocation);
            P.SensorLocalPositionMeters = FVector(Local.X, -Local.Y, Local.Z) * 0.01;
            Points->Add(P);
        }
        FVirtualSensorFrameEnvelope Frame;
        Frame.SensorKind = EVirtualSensorKind::Lidar;
        Frame.SensorId = TEXT("coordinate-fixture");
        Frame.PointSnapshot = Points;
        FVirtualSensorStreamConfig Config;
        Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
        Config.PcdDataMode = EVirtualPcdDataMode::Binary;
        TArray<uint8> Bytes;
        FString Extension, Error;
        int32 Count = 0;
        if (!TestTrue(TEXT("production serializer"), UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Frame, Config, Extension, Bytes, Count, Error))) return false;
        const ANSICHAR* Marker = "DATA binary\n";
        int32 Start = INDEX_NONE;
        for (int32 I = 0; I + 12 <= Bytes.Num(); ++I)
            if (FMemory::Memcmp(Bytes.GetData() + I, Marker, 12) == 0) { Start = I + 12; break; }
        if (!TestTrue(TEXT("binary header boundary"), Start != INDEX_NONE)) return false;
        TestEqual(TEXT("packed record, no C++ padding"), Bytes.Num() - Start, Count * 33);
        FVector Restored[2];
        for (int32 I = 0; I < 2; ++I)
        {
            float XYZ[3];
            for (int32 Axis = 0; Axis < 3; ++Axis)
            {
                const uint8* B = Bytes.GetData() + Start + I * 33 + Axis * 4;
                const uint32 Bits = uint32(B[0]) | uint32(B[1]) << 8 | uint32(B[2]) << 16 | uint32(B[3]) << 24;
                FMemory::Memcpy(&XYZ[Axis], &Bits, 4);
            }
            Restored[I] = Pose.TransformPosition(FVector(XYZ[0], -XYZ[1], XYZ[2]) * 100.0);
            TestTrue(TEXT("local binary XYZ reconstructs original world position"), Restored[I].Equals((*Points)[I].WorldLocation, 0.01));
        }
        TestTrue(TEXT("25cm object remains 25cm above plate at every pose"), FMath::IsNearlyEqual(Restored[1].Z - Restored[0].Z, 25.0, 0.01));
    }
    return true;
}
#endif
