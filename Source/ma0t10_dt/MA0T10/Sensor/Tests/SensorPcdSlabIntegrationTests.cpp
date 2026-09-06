#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdNativeLimitTest, "MA0T10.SensorStream.NativePcdLimits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPcdNativeLimitTest::RunTest(const FString& Parameters)
{
	for (const int32 Count : {0, 32256, 64512})
	{
		TArray<FVirtualLidarPoint> Points;
		Points.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			Points[I].bHit = true;
			Points[I].SensorLocalPositionMeters = FVector(1.0, I % 56, 0.5);
			Points[I].Validity = EVirtualLidarPointValidity::Valid;
		}
		FVirtualSensorFrameEnvelope Frame;
		Frame.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));
		FVirtualSensorStreamConfig Config;
		Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
		Config.PcdDataMode = EVirtualPcdDataMode::Binary;
		FString Ext, Error;
		TArray<uint8> Body;
		int32 EncodedCount = -1;
		TestTrue(TEXT("Native PCD serializes"), UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Frame, Config, Ext, Body, EncodedCount, Error));
		TestEqual(TEXT("all points retained"), EncodedCount, Count);
		TestTrue(TEXT("binary record size with bounded header"), Body.Num() >= Count * 33 && Body.Num() < Count * 33 + 1024);
		TestTrue(TEXT("Native worst case fits default 8MiB body limit"), Body.Num() < 8 * 1024 * 1024);
		if (Count > 0)
		{
			auto* Transport = NewObject<UVirtualSensorTransportComponent>();
			Transport->TransportProfile.MaxMessageBytes = 1024;
			FVirtualPointCloudBinaryMetadata Metadata;
			Metadata.ByteCount = Body.Num();
			const auto Result = Transport->SendStompBinaryStreamRequest(Body, Metadata);
			TestFalse(TEXT("small user limit rejects before connection"), Result.bSubmitted);
			TestTrue(TEXT("rejection carries byte counts"), Result.Message.Contains(LexToString(Body.Num())));
		}
	}
	return true;
}
#endif
