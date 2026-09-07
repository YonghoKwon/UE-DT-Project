#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorWireHeaders.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorIncomingProductionContractTest,"MA0T10.SensorReceiver.ProductionPcdHeaders",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorIncomingProductionContractTest::RunTest(const FString& Parameters)
{
	FVirtualSensorFrameEnvelope Source;
	TArray<FVirtualLidarPoint> Points; Points.SetNum(32256);
	for (auto& P:Points) { P.bHit=true; P.Validity=EVirtualLidarPointValidity::Valid; P.SensorLocalPositionMeters=FVector(1,2,3); }
	Source.PointSnapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>(MoveTemp(Points));
	auto Config=UVirtualSensorStreamPublisherComponent::NormalizeLiveConfig(FVirtualSensorStreamConfig());
	Config.StreamKind=EVirtualSensorStreamKind::PointCloud; Config=UVirtualSensorStreamPublisherComponent::NormalizeLiveConfig(Config);
	TArray<uint8> Body; FString Extension,Error; int32 Count=0;
	TestTrue(TEXT("production serializer"),UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Source,Config,Extension,Body,Count,Error));
	FVirtualPointCloudBinaryMetadata Meta;
	Meta.SensorId=TEXT("LIDAR-TEST-001"); Meta.FrameId=2657; Meta.TimestampUtc=FDateTime::UtcNow().ToIso8601();
	Meta.ProfileKey=TEXT("MLX80_Native"); Meta.PointCount=Count; Meta.SourcePointCount=32256; Meta.ByteCount=Body.Num();
	uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Body.GetData(),Body.Num(),Hash); Meta.ChecksumSha1=BytesToHex(Hash,FSHA1::DigestSize).ToLower();
	FVirtualSensorBinaryFrame Wire;
	Wire.SensorId=Meta.SensorId; Wire.FrameId=Meta.FrameId; FDateTime::ParseIso8601(*Meta.TimestampUtc,Wire.TimestampUtc);
	Wire.StreamKind=EVirtualSensorStreamKind::PointCloud; Wire.Schema=Meta.Schema; Wire.ContentType=TEXT("application/vnd.pcd");
	Wire.Destination=TEXT("topic.virtual.sensor.export.0"); Wire.Body32=MakeShared<const TArray<uint8>,ESPMode::ThreadSafe>(Body);
	Wire.Headers=FVirtualSensorWireHeaders::RawPcd(Meta);
	TMap<FName,FString> RawHeaders;
	for (const auto& Pair:FVirtualSensorWireHeaders::RawFrame(Wire)) RawHeaders.Add(FName(*Pair.Key),Pair.Value);
	auto* Receiver=NewObject<UVirtualPointCloudStreamReceiverTC>();
	const auto Raw=StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Receiver->ParseBinaryPcdToStruct(Body,RawHeaders));
	const auto Engine=StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Receiver->ParseBinaryPcdToStruct(Body,FVirtualSensorWireHeaders::EnginePcd(Meta,TEXT("engine-test"))));
	// Characterization of the reported defect; replaced by acceptance after normalization is added.
	TestFalse(TEXT("reproduces Raw sender versus UI receiver mismatch"),Raw->bValid);
	TestTrue(TEXT("same body already passes Engine sender headers"),Engine->bValid);
	return true;
}
#endif
