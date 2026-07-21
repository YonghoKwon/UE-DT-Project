#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Base64.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualCameraStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualLidarStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualLidarTopicReceiverParserTest,
	"MA0T10.SensorReceiver.LidarPayloadValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualCameraTopicReceiverParserTest,
	"MA0T10.SensorReceiver.CameraPayloadValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualPointCloudTopicReceiverParserTest,
	"MA0T10.SensorReceiver.PointCloudFormatValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualLidarTopicReceiverParserTest::RunTest(const FString& Parameters)
{
	UVirtualLidarStreamReceiverTC* Handler = NewObject<UVirtualLidarStreamReceiverTC>();
	const FString Payload = TEXT("{\"schemaVersion\":\"virtual-lidar.v1\",\"sensorId\":\"LIDAR-RX-01\",\"frameId\":10,\"horizontalSamples\":2,\"verticalChannels\":1,\"rayCount\":2,\"totalPointCount\":2,\"hitPointCount\":1,\"payloadPointCount\":1,\"points\":[{\"hit\":true,\"distance\":120.0,\"row\":0,\"col\":0}]}");
	const TSharedPtr<FVirtualLidarStreamReceiverData> Data = StaticCastSharedPtr<FVirtualLidarStreamReceiverData>(Handler->ParseToStruct(Payload));
	TestTrue(TEXT("LiDAR payload without MESSAGE_ID is accepted by direct Topic handler"), Data.IsValid() && Data->bValid);
	TestTrue(TEXT("LiDAR frame 10 receives deep point validation"), Data.IsValid() && Data->bDeepValidated);
	TestEqual(TEXT("LiDAR payload array count is preserved"), Data.IsValid() ? Data->PayloadPointCount : -1, 1);

	const FString BadCounts = Payload.Replace(TEXT("\"payloadPointCount\":1"), TEXT("\"payloadPointCount\":2"));
	const TSharedPtr<FVirtualLidarStreamReceiverData> Invalid = StaticCastSharedPtr<FVirtualLidarStreamReceiverData>(Handler->ParseToStruct(BadCounts));
	TestFalse(TEXT("LiDAR payload count mismatch is rejected"), Invalid.IsValid() && Invalid->bValid);
	TestTrue(TEXT("LiDAR mismatch explains counters"), Invalid.IsValid() && Invalid->Message.Contains(TEXT("불일치")));
	return true;
}

bool FVirtualCameraTopicReceiverParserTest::RunTest(const FString& Parameters)
{
	UVirtualCameraStreamReceiverTC* Handler = NewObject<UVirtualCameraStreamReceiverTC>();
	const TArray<uint8> JpegBytes = { 0xff, 0xd8, 0xff, 0xd9 };
	const FString Payload = FString::Printf(
		TEXT("{\"schemaVersion\":\"virtual-camera.v1\",\"sensorId\":\"CAM-RX-01\",\"frameId\":10,\"width\":640,\"height\":360,\"byteSize\":4,\"encoding\":\"jpeg/base64\",\"image\":\"%s\"}"),
		*FBase64::Encode(JpegBytes));
	const TSharedPtr<FVirtualCameraStreamReceiverData> Data = StaticCastSharedPtr<FVirtualCameraStreamReceiverData>(Handler->ParseToStruct(Payload));
	TestTrue(TEXT("Camera JPEG envelope validates"), Data.IsValid() && Data->bValid);
	TestTrue(TEXT("Camera frame 10 decodes Base64"), Data.IsValid() && Data->bDeepValidated);
	TestEqual(TEXT("Camera decoded byte count matches"), Data.IsValid() ? Data->DecodedByteSize : -1, 4);

	const FString BadBase64 = Payload.Replace(*FBase64::Encode(JpegBytes), TEXT("not-base64***"));
	const TSharedPtr<FVirtualCameraStreamReceiverData> Invalid = StaticCastSharedPtr<FVirtualCameraStreamReceiverData>(Handler->ParseToStruct(BadBase64));
	TestFalse(TEXT("invalid Camera Base64 is rejected on sampled frame"), Invalid.IsValid() && Invalid->bValid);
	return true;
}

bool FVirtualPointCloudTopicReceiverParserTest::RunTest(const FString& Parameters)
{
	UVirtualPointCloudStreamReceiverTC* Handler = NewObject<UVirtualPointCloudStreamReceiverTC>();
	struct FFormatFixture
	{
		const TCHAR* Format;
		TArray<uint8> Bytes;
	};
	const TArray<FFormatFixture> Fixtures = {
		{ TEXT("CSV"), TArray<uint8>({ 'x', ',', 'y', ',', 'z', ',', 'd', 'i', 's', 't', 'a', 'n', 'c', 'e', ',', 'h', 'i', 't', '\n' }) },
		{ TEXT("JSONL"), TArray<uint8>({ '{', '"', 'x', '"', ':', '1', '}', '\n' }) },
		{ TEXT("PCD"), TArray<uint8>({ '#', ' ', '.', 'P', 'C', 'D', ' ', 'v', '0', '.', '7', '\n' }) },
		{ TEXT("LAS"), TArray<uint8>({ 'L', 'A', 'S', 'F', 0, 0, 0, 0 }) },
		{ TEXT("LAZ"), TArray<uint8>({ 'L', 'A', 'S', 'F', 1, 2, 3, 4 }) }
	};
	for (const FFormatFixture& Fixture : Fixtures)
	{
		const FString Payload = FString::Printf(
			TEXT("{\"schema\":\"virtual-pointcloud.v1\",\"sensorId\":\"LIDAR-RX-01\",\"frameId\":10,\"format\":\"%s\",\"encoding\":\"base64\",\"pointCount\":1,\"byteCount\":%d,\"data\":\"%s\"}"),
			Fixture.Format, Fixture.Bytes.Num(), *FBase64::Encode(Fixture.Bytes));
		const TSharedPtr<FVirtualPointCloudStreamReceiverData> Data = StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Handler->ParseToStruct(Payload));
		TestTrue(FString::Printf(TEXT("%s point-cloud signature validates"), Fixture.Format), Data.IsValid() && Data->bValid);
		TestTrue(FString::Printf(TEXT("%s frame 10 is deeply validated"), Fixture.Format), Data.IsValid() && Data->bDeepValidated);
	}

	const FString Invalid = TEXT("{\"schema\":\"virtual-pointcloud.v1\",\"sensorId\":\"LIDAR-RX-01\",\"frameId\":10,\"format\":\"CSV\",\"encoding\":\"base64\",\"pointCount\":1,\"byteCount\":4,\"data\":\"AQIDBA==\"}");
	const TSharedPtr<FVirtualPointCloudStreamReceiverData> InvalidData = StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Handler->ParseToStruct(Invalid));
	TestFalse(TEXT("wrong CSV signature is rejected"), InvalidData.IsValid() && InvalidData->bValid);
	return true;
}

#endif
