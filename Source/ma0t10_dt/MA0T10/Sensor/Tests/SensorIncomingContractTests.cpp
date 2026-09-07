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
	for(const int32 InputCount:{0,32256,64512})
	{
	FVirtualSensorFrameEnvelope Source;
	TArray<FVirtualLidarPoint> Points; Points.SetNum(InputCount);
	for (auto& P:Points) { P.bHit=true; P.Validity=EVirtualLidarPointValidity::Valid; P.SensorLocalPositionMeters=FVector(1,2,3); }
	Source.PointSnapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>(MoveTemp(Points));
	auto Config=UVirtualSensorStreamPublisherComponent::NormalizeLiveConfig(FVirtualSensorStreamConfig());
	Config.StreamKind=EVirtualSensorStreamKind::PointCloud; Config=UVirtualSensorStreamPublisherComponent::NormalizeLiveConfig(Config);
	TArray<uint8> Body; FString Extension,Error; int32 Count=0;
	TestTrue(TEXT("production serializer"),UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Source,Config,Extension,Body,Count,Error));
	FVirtualPointCloudBinaryMetadata Meta;
	Meta.SensorId=TEXT("LIDAR-TEST-001"); Meta.FrameId=2657; Meta.TimestampUtc=FDateTime::UtcNow().ToIso8601();
	Meta.ProfileKey=TEXT("MLX80_Native"); Meta.PointCount=Count; Meta.SourcePointCount=InputCount; Meta.ByteCount=Body.Num();
	Meta.SlabContext.RunId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Meta.SlabContext.MtlNo=TEXT("SQ83521 047"); Meta.SlabContext.SlabFrameNo=580; Meta.SlabContext.ElapsedSec=29.0; Meta.SlabContext.bEligible=true;
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
	TestTrue(TEXT("Raw production sender is accepted by UI receiver"),Raw->bValid);
	TestTrue(TEXT("same body already passes Engine sender headers"),Engine->bValid);
	TestEqual(TEXT("material number preserved"),Raw->MtlNo,Meta.SlabContext.MtlNo);
	TestEqual(TEXT("Slab frame preserved independently"),Raw->SlabFrameNo,static_cast<int64>(580));
	TestEqual(TEXT("sensor frame remains independent"),Raw->FrameId,static_cast<int64>(2657));
	TestEqual(TEXT("engine UTC normalized"),Engine->SourceTimestampUtc,Raw->SourceTimestampUtc);
	RawHeaders.Remove(TEXT("x-checksum-sha1")); RawHeaders.Remove(TEXT("x-acquisition-profile")); RawHeaders.Remove(TEXT("x-utc"));
	TestTrue(TEXT("already deployed Raw header aliases accepted"),StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Receiver->ParseBinaryPcdToStruct(Body,RawHeaders))->bValid);
	RawHeaders.Add(TEXT("x-checksum-sha1"),FString::ChrN(40,TEXT('0')));
	TestFalse(TEXT("conflicting aliases rejected"),StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Receiver->ParseBinaryPcdToStruct(Body,RawHeaders))->bValid);
	RawHeaders.Remove(TEXT("x-checksum-sha1")); RawHeaders.Remove(TEXT("checksum"));
	const auto Missing=StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Receiver->ParseBinaryPcdToStruct(Body,RawHeaders));
	TestTrue(TEXT("missing header identifies required name"),Missing->Message.Contains(TEXT("checksum")));
	}
	FVirtualSensorReceiveSelection Selection; Selection.bAllTopics=false; Selection.StreamKeys.Add(TEXT("2|LIDAR-TEST-001"));
	TestTrue(TEXT("active PCD allowed"),Selection.Accepts(2,TEXT("LIDAR-TEST-001")));
	TestFalse(TEXT("other publisher ignored"),Selection.Accepts(2,TEXT("OTHER")));
	TestFalse(TEXT("camera not subscribed for PCD only"),Selection.WantsKind(1));
	TestFalse(TEXT("telemetry not subscribed for PCD only"),Selection.WantsKind(0));
	Selection.bEnabled=false; TestFalse(TEXT("receiver stop disables subscriptions"),Selection.WantsKind(2));
	return true;
}
#endif
