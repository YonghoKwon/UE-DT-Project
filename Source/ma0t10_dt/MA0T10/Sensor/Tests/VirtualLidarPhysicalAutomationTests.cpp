#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarPayloadCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualLidarNativeProfileContractTest,
	"MA0T10.SensorV2.Physical.NativeProfileContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualLidarV2CodecContractTest,
	"MA0T10.SensorV2.Physical.V2CodecContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualLidarSurfaceResponseDefaultsTest,
	"MA0T10.SensorV2.Physical.SurfaceResponseDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualLidarNativeProfileContractTest::RunTest(const FString& Parameters)
{
	const FVirtualLidarProfilePreset Integration = UVirtualLidarScanComponent::ResolveProfilePreset(
		EVirtualLidarDeviceProfile::IYOBOT_MLX80,
		EVirtualSensorSimulationQuality::FullSpec);
	const FVirtualLidarProfilePreset Native = UVirtualLidarScanComponent::ResolveProfilePreset(
		EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE,
		EVirtualSensorSimulationQuality::FullSpec);

	TestEqual(TEXT("integration contract remains 200 columns"), Integration.HorizontalSamples, 200);
	TestEqual(TEXT("integration contract remains 56 rows"), Integration.VerticalChannels, 56);
	TestEqual(TEXT("integration contract remains 224k points/s"), Integration.DeviceSpec.PointRate, 224000);
	TestEqual(TEXT("integration contract is explicitly downsampled"), Integration.ProfileClass, EVirtualLidarProfileClass::IntegrationDownsampled);
	TestEqual(TEXT("native contract has 576 columns"), Native.HorizontalSamples, 576);
	TestEqual(TEXT("native contract has 56 rows"), Native.VerticalChannels, 56);
	TestEqual(TEXT("native contract has 645120 points/s"), Native.DeviceSpec.PointRate, 645120);
	TestTrue(TEXT("native contract is 20Hz"), FMath::IsNearlyEqual(Native.ScanIntervalSeconds, 0.05f));
	TestTrue(TEXT("native horizontal angular resolution is public spec"), FMath::IsNearlyEqual(Native.DeviceSpec.HorizontalAngularResolutionDegrees, 0.139f));
	TestTrue(TEXT("native vertical angular resolution is public spec"), FMath::IsNearlyEqual(Native.DeviceSpec.VerticalAngularResolutionDegrees, 0.417f));
	TestEqual(TEXT("native contract supports two echoes"), Native.DeviceSpec.MaxEchoesPerPixel, 2);
	TestEqual(TEXT("native contract never claims hardware calibration"), Native.FidelityMode, EVirtualSensorFidelityMode::PublicSpecBased);
	TestFalse(TEXT("native public contract is not protocol verified"), Native.DeviceSpec.bProtocolVerifiedAgainstHardware);
	return true;
}

bool FVirtualLidarV2CodecContractTest::RunTest(const FString& Parameters)
{
	TArray<FVirtualLidarPoint> Points;
	FVirtualLidarPoint& Point = Points.AddDefaulted_GetRef();
	Point.bHit = true;
	Point.SensorLocalPositionMeters = FVector(1.25, -0.5, 0.2);
	Point.RangeMillimeters = 1361;
	Point.RawIntensity = 32000;
	Point.NormalizedIntensity = 32000.0f / 65535.0f;
	Point.Ring = 12;
	Point.HorizontalIndex = 34;
	Point.EchoIndex = 0;
	Point.EchoCount = 2;
	Point.EchoType = EVirtualLidarEchoType::First;
	Point.PointTimeOffsetNanoseconds = 25000;
	Point.Validity = EVirtualLidarPointValidity::Valid;
	Point.Confidence = 0.95f;
	Point.WorldLocation = FVector(125.0, 50.0, 20.0);
	Point.SemanticLabel = TEXT("Fixture");

	FVirtualLidarFrameSnapshot Frame;
	Frame.Points = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));
	Frame.FrameId = 77;
	Frame.ProfileKey = TEXT("iyobot-mlx80-native");
	Frame.CalibrationId = TEXT("public-spec-derived-2026");
	Frame.HorizontalSamples = 576;
	Frame.VerticalChannels = 56;
	Frame.RequestedRayCount = 32256;
	Frame.ValidPointCount = 1;
	Frame.FirstEchoCount = 1;
	Frame.AcquisitionStartUnixNanoseconds = 1000000000;
	Frame.AcquisitionEndUnixNanoseconds = 1050000000;
	Frame.FidelityMode = EVirtualSensorFidelityMode::PublicSpecBased;

	FVirtualLidarPayloadDescriptor Descriptor;
	Descriptor.SensorId = TEXT("MLX80-TEST");
	Descriptor.Manufacturer = TEXT("IYOBOT");
	Descriptor.Model = TEXT("ML-X(80) Native");
	Descriptor.HorizontalFovDegrees = 80.0f;
	Descriptor.VerticalFovDegrees = 23.3f;
	Descriptor.MaxRangeMeters = 150.0f;

	FVirtualLidarV2EncodeOptions Options;
	Options.bIncludeDigitalTwinExtensions = true;
	const FString Json = FVirtualLidarPayloadCodec::EncodeV2Json(Frame, Descriptor, Options);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("v2 JSON parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (Root.IsValid())
	{
		TestEqual(TEXT("v2 schema is explicit"), Root->GetStringField(TEXT("schemaVersion")), FString(TEXT("virtual-lidar.v2")));
		TestEqual(TEXT("vendor packet disclaimer is explicit"), Root->GetStringField(TEXT("wireCompatibility")), FString(TEXT("project_contract_not_vendor_packet")));
		TestFalse(TEXT("public spec does not claim protocol verification"), Root->GetBoolField(TEXT("protocolVerifiedAgainstHardware")));
		const TArray<TSharedPtr<FJsonValue>>& JsonPoints = Root->GetArrayField(TEXT("points"));
		TestEqual(TEXT("one physical point encoded"), JsonPoints.Num(), 1);
		if (!JsonPoints.IsEmpty())
		{
			const TSharedPtr<FJsonObject> JsonPoint = JsonPoints[0]->AsObject();
			TestEqual(TEXT("intensity preserved"), static_cast<int32>(JsonPoint->GetNumberField(TEXT("intensity"))), 32000);
			TestEqual(TEXT("ring preserved"), static_cast<int32>(JsonPoint->GetNumberField(TEXT("ring"))), 12);
			TestFalse(TEXT("semantic metadata is not mixed into physical point"), JsonPoint->HasField(TEXT("semanticLabel")));
		}
		TestTrue(TEXT("digital twin metadata lives in extension object"), Root->HasTypedField<EJson::Object>(TEXT("digitalTwinExtensions")));
	}

	TArray64<uint8> Binary;
	int32 EncodedPointCount = 0;
	TestTrue(TEXT("compact binary encodes"), FVirtualLidarPayloadCodec::EncodeCompactBinary(Frame, Options, Binary, EncodedPointCount));
	TestEqual(TEXT("compact binary point count"), EncodedPointCount, 1);
	TestEqual(TEXT("compact binary header plus one record"), Binary.Num(), static_cast<int64>(66));
	TestTrue(TEXT("compact binary magic"), Binary.Num() >= 5 && Binary[0] == 'V' && Binary[1] == 'L' && Binary[2] == 'D' && Binary[3] == 'R' && Binary[4] == '2');
	return true;
}

bool FVirtualLidarSurfaceResponseDefaultsTest::RunTest(const FString& Parameters)
{
	const UVirtualLidarSurfaceResponseComponent* Response = NewObject<UVirtualLidarSurfaceResponseComponent>();
	TestTrue(TEXT("default reflectivity is 20 percent"), FMath::IsNearlyEqual(Response->Reflectivity940Nm, 0.2f));
	TestTrue(TEXT("default intensity gain is neutral"), FMath::IsNearlyEqual(Response->IntensityGain, 1.0f));
	TestTrue(TEXT("default detection multiplier is neutral"), FMath::IsNearlyEqual(Response->DetectionProbabilityScale, 1.0f));
	TestFalse(TEXT("default surface is one-sided"), Response->bTwoSided);
	return true;
}

#endif

