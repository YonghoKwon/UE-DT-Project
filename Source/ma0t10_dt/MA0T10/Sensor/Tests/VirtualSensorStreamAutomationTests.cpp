#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"
#include "HAL/PlatformMisc.h"
#include "Misc/SecureHash.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorStreamLatestFrameTest,
	"MA0T10.SensorStream.LatestFrameBackpressure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorStreamLatestFrameTest::RunTest(const FString& Parameters)
{
	UVirtualSensorTransportComponent* Transport = NewObject<UVirtualSensorTransportComponent>();
	Transport->TransportMode = EVirtualSensorTransportMode::LogOnly;
	UVirtualSensorStreamPublisherComponent* Publisher = NewObject<UVirtualSensorStreamPublisherComponent>();
	Publisher->SetTransportComponent(Transport);

	FVirtualSensorStreamConfig Config;
	Config.StreamKind = EVirtualSensorStreamKind::LidarPayload;
	Config.bEnabled = true;
	Config.FrameStride = 1;
	Publisher->ConfigureStream(Config);

	for (int64 FrameId = 1; FrameId <= 3; ++FrameId)
	{
		FVirtualSensorFrameEnvelope Frame;
		Frame.SensorId = TEXT("LIDAR-A");
		Frame.SensorKind = EVirtualSensorKind::Lidar;
		Frame.FrameId = FrameId;
		Frame.TimestampUtc = FDateTime::UtcNow();
		Frame.SchemaVersion = TEXT("virtual-lidar.v1");
		Frame.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(FString::Printf(TEXT("{\"frameId\":%lld}"), FrameId));
		Publisher->SubmitFrame(Frame);
	}

	TArray<FVirtualSensorStreamStatus> Before = Publisher->GetStreamStatuses();
	TestEqual(TEXT("one global LiDAR stream exists"), Before.Num(), 1);
	TestEqual(TEXT("two older prepared frames were replaced"), Before[0].ReplacedPendingFrameCount, static_cast<int64>(2));
	Publisher->PumpPublisherOnce(FPlatformTime::Seconds());
	const TArray<FVirtualSensorStreamStatus> After = Publisher->GetStreamStatuses();
	TestEqual(TEXT("only one latest frame was submitted"), After[0].SubmittedFrameCount, static_cast<int64>(1));
	TestEqual(TEXT("latest frame wins"), After[0].LastSubmittedFrameId, static_cast<int64>(3));
	TestFalse(TEXT("queue is empty after submission"), After[0].bPendingLatestFrame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorStreamFormatRevisionTest,
	"MA0T10.SensorStream.PointCloudFormatRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorStreamFormatRevisionTest::RunTest(const FString& Parameters)
{
	UVirtualSensorStreamPublisherComponent* Publisher = NewObject<UVirtualSensorStreamPublisherComponent>();
	FVirtualSensorStreamConfig Config;
	Config.StreamKind = EVirtualSensorStreamKind::PointCloud;
	Config.SensorId = TEXT("LIDAR-REVISION");
	Config.bEnabled = true;
	Config.FrameStride = 1;
	Config.PointCloudFormat = EVirtualPointCloudStreamFormat::CSV;
	Publisher->ConfigureStream(Config);

	const TArray<FVirtualSensorStreamStatus> Initial = Publisher->GetStreamStatuses();
	TestEqual(TEXT("one Point Cloud stream is configured"), Initial.Num(), 1);
	if (Initial.Num() != 1) return false;
	const int32 InitialRevision = Initial[0].ConfigRevision;

	Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
	Publisher->ConfigureStream(Config);
	const TArray<FVirtualSensorStreamStatus> Changed = Publisher->GetStreamStatuses();
	TestEqual(TEXT("live PCD normalization prevents accidental ASCII/CSV revision changes"), Changed[0].ConfigRevision, InitialRevision);
	TestFalse(TEXT("format change clears a pending old-format result"), Changed[0].bPendingLatestFrame);

	Publisher->ConfigureStream(Config);
	const TArray<FVirtualSensorStreamStatus> Unchanged = Publisher->GetStreamStatuses();
	TestEqual(TEXT("reapplying the same format keeps the revision"), Unchanged[0].ConfigRevision, Changed[0].ConfigRevision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorConnectedNoLossQueueTest,
	"MA0T10.SensorStream.ConnectedNoLossQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorConnectedNoLossQueueTest::RunTest(const FString& Parameters)
{
	UVirtualSensorStreamPublisherComponent* Publisher = NewObject<UVirtualSensorStreamPublisherComponent>();
	FVirtualSensorStreamConfig Config;
	Config.StreamKind = EVirtualSensorStreamKind::PointCloud;
	Config.SensorId = TEXT("LIDAR-NO-LOSS");
	Config.bEnabled = true;
	Config.FrameStride = 7;
	Config.ReceiptSampleInterval = 10;
	Config.PointCloudFormat = EVirtualPointCloudStreamFormat::CSV;
	Config.PcdDataMode = EVirtualPcdDataMode::Ascii;
	Config.DeliveryMode = EVirtualPointCloudDeliveryMode::ConnectedNoLoss;
	Config.MaxBufferedFrames = 20;
	Publisher->ConfigureStream(Config);

	TArray<FVirtualLidarPoint> Points;
	FVirtualLidarPoint& Point = Points.AddDefaulted_GetRef();
	Point.bHit = true;
	Point.SensorLocalPositionMeters = FVector(1.0, 0.0, 0.0);
	Point.Validity = EVirtualLidarPointValidity::Valid;
	const TSharedPtr<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe> Snapshot =
		MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));

	for (int64 FrameId = 1; FrameId <= 22; ++FrameId)
	{
		FVirtualSensorFrameEnvelope Frame;
		Frame.SensorId = Config.SensorId;
		Frame.SensorKind = EVirtualSensorKind::Lidar;
		Frame.FrameId = FrameId;
		Frame.TimestampUtc = FDateTime::UtcNow();
		Frame.PointSnapshot = Snapshot;
		Publisher->SubmitFrame(Frame);
	}

	const TArray<FVirtualSensorStreamStatus> Statuses = Publisher->GetStreamStatuses();
	TestEqual(TEXT("one no-loss stream exists"), Statuses.Num(), 1);
	if (Statuses.Num() != 1) return false;
	TestTrue(TEXT("queue overflow stops the stream explicitly"), Statuses[0].bOverloaded);
	TestFalse(TEXT("overloaded stream is no longer enabled"), Statuses[0].bEnabled);
	TestEqual(TEXT("no latest-frame replacement occurs"), Statuses[0].ReplacedPendingFrameCount, static_cast<int64>(0));
	TestEqual(TEXT("overload is counted"), Statuses[0].OverloadCount, static_cast<int64>(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorStreamTopicRoutingTest,
	"MA0T10.SensorStream.TopicRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorStreamGlobalControlTest,
	"MA0T10.SensorStream.GlobalAndPerSensorControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorStreamTopicRoutingTest::RunTest(const FString& Parameters)
{
	UVirtualSensorTransportComponent* Transport = NewObject<UVirtualSensorTransportComponent>();
	FVirtualSensorTransportProfile Profile;
	Profile.CameraTopic = TEXT("camera-topic");
	Profile.LidarTopic = TEXT("lidar-topic");
	Profile.ExportTopic = TEXT("pointcloud-topic");
	Transport->ConfigureTransportProfile(Profile);
	TestEqual(TEXT("camera stream topic"), Transport->ResolveDestination(TEXT("camera"), TEXT("camera-stream")), Profile.CameraTopic);
	TestEqual(TEXT("LiDAR stream topic"), Transport->ResolveDestination(TEXT("lidar"), TEXT("lidar-stream")), Profile.LidarTopic);
	TestEqual(TEXT("point-cloud stream topic"), Transport->ResolveDestination(TEXT("lidar"), TEXT("pointcloud-stream")), Profile.ExportTopic);
	TestEqual(TEXT("legacy export remains compatible"), Transport->ResolveDestination(TEXT("lidar"), TEXT("manual-export")), Profile.ExportTopic);
	return true;
}

bool FVirtualSensorStreamGlobalControlTest::RunTest(const FString& Parameters)
{
	UVirtualSensorTransportComponent* Transport = NewObject<UVirtualSensorTransportComponent>();
	Transport->TransportMode = EVirtualSensorTransportMode::LogOnly;
	UVirtualSensorStreamPublisherComponent* Publisher = NewObject<UVirtualSensorStreamPublisherComponent>();
	Publisher->SetTransportComponent(Transport);
	FVirtualSensorStreamConfig Global;
	Global.StreamKind = EVirtualSensorStreamKind::LidarPayload;
	Global.bEnabled = true;
	Publisher->ConfigureStream(Global);
	FVirtualSensorStreamConfig Exact = Global;
	Exact.SensorId = TEXT("LIDAR-A");
	Publisher->ConfigureStream(Exact);

	FVirtualSensorFrameEnvelope Frame;
	Frame.SensorId = TEXT("LIDAR-A");
	Frame.SensorKind = EVirtualSensorKind::Lidar;
	Frame.FrameId = 7;
	Frame.TimestampUtc = FDateTime::UtcNow();
	Frame.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(TEXT("{\"frameId\":7}"));
	Publisher->SubmitFrame(Frame);
	Publisher->PumpPublisherOnce(FPlatformTime::Seconds());
	int64 TotalSubmitted = 0;
	for (const FVirtualSensorStreamStatus& Status : Publisher->GetStreamStatuses()) TotalSubmitted += Status.SubmittedFrameCount;
	TestEqual(TEXT("exact stream suppresses duplicate global delivery"), TotalSubmitted, static_cast<int64>(1));
	Publisher->StopAllStreams(FString());
	for (const FVirtualSensorStreamStatus& Status : Publisher->GetStreamStatuses()) TestFalse(TEXT("global stop disables every configured stream"), Status.bEnabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorPointCloudSerializationTest,
	"MA0T10.SensorStream.PointCloudFormats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorPointCloudSerializationTest::RunTest(const FString& Parameters)
{
	FVirtualSensorFrameEnvelope Frame;
	Frame.SensorId = TEXT("LIDAR-FORMAT-TEST");
	Frame.SensorKind = EVirtualSensorKind::Lidar;
	Frame.FrameId = 42;
	Frame.TimestampUtc = FDateTime(2026, 1, 2, 3, 4, 5);
	TArray<FVirtualLidarPoint> Points;
	FVirtualLidarPoint& Hit = Points.AddDefaulted_GetRef();
	Hit.bHit = true;
	Hit.WorldLocation = FVector(100.0, 200.0, 300.0);
	Hit.Distance = 374.1657f;
	Hit.SensorLocalPositionMeters = FVector(1.0, 2.0, 3.0);
	Hit.RangeMillimeters = 3742;
	Hit.RawIntensity = 1234;
	Hit.Ring = 2;
	Hit.HorizontalIndex = 3;
	Hit.EchoIndex = 0;
	Hit.EchoCount = 1;
	Hit.PointTimeOffsetNanoseconds = 5000;
	Hit.Validity = EVirtualLidarPointValidity::Valid;
	Hit.Confidence = 0.75f;
	Hit.Row = 2;
	Hit.Col = 3;
	Hit.SemanticLabel = TEXT("TestObject");
	FVirtualLidarPoint& Miss = Points.AddDefaulted_GetRef();
	Miss.bHit = false;
	Frame.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));
	TSharedPtr<FVirtualLidarFrameSnapshot, ESPMode::ThreadSafe> PhysicalFrame =
		MakeShared<FVirtualLidarFrameSnapshot, ESPMode::ThreadSafe>();
	PhysicalFrame->FrameId = Frame.FrameId;
	PhysicalFrame->ProfileKey = TEXT("iyobot-mlx80-native");
	PhysicalFrame->RequestedRayCount = 2;
	PhysicalFrame->ValidPointCount = 1;
	PhysicalFrame->Points = Frame.PointSnapshot;
	Frame.LidarFrameSnapshot = StaticCastSharedPtr<const FVirtualLidarFrameSnapshot>(PhysicalFrame);

	for (const TPair<EVirtualPointCloudStreamFormat, FString>& Case : {
		TPair<EVirtualPointCloudStreamFormat, FString>(EVirtualPointCloudStreamFormat::CSV, TEXT("csv")),
		TPair<EVirtualPointCloudStreamFormat, FString>(EVirtualPointCloudStreamFormat::JSONL, TEXT("jsonl")),
		TPair<EVirtualPointCloudStreamFormat, FString>(EVirtualPointCloudStreamFormat::PCD, TEXT("pcd")),
		TPair<EVirtualPointCloudStreamFormat, FString>(EVirtualPointCloudStreamFormat::LAS, TEXT("las")) })
	{
		FVirtualSensorStreamConfig Config;
		Config.PointCloudFormat = Case.Key;
		FString Extension, Error;
		TArray<uint8> Bytes;
		int32 PointCount = 0;
		TestTrue(FString::Printf(TEXT("%s serialization succeeds"), *Case.Value),
			UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Frame, Config, Extension, Bytes, PointCount, Error));
		TestEqual(FString::Printf(TEXT("%s extension"), *Case.Value), Extension, Case.Value);
		TestEqual(FString::Printf(TEXT("%s includes hit only"), *Case.Value), PointCount, 1);
		TestTrue(FString::Printf(TEXT("%s has bytes"), *Case.Value), !Bytes.IsEmpty());
	}

	{
		FVirtualSensorStreamConfig Config;
		Config.PointCloudFormat = EVirtualPointCloudStreamFormat::CompactBinary;
		FString Extension, Error;
		TArray<uint8> Bytes;
		int32 PointCount = 0;
		TestTrue(TEXT("compact binary serialization succeeds"),
			UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Frame, Config, Extension, Bytes, PointCount, Error));
		TestEqual(TEXT("compact binary extension"), Extension, FString(TEXT("vlb2")));
		TestEqual(TEXT("compact binary includes the valid point"), PointCount, 1);
		TestTrue(TEXT("compact binary includes a header and point record"), Bytes.Num() > 32);
	}

	FVirtualSensorStreamConfig LazConfig;
	LazConfig.PointCloudFormat = EVirtualPointCloudStreamFormat::LAZ;
	FString LazExtension, LazError;
	TArray<uint8> LazBytes;
	int32 LazPointCount = 0;
	TestFalse(TEXT("LAZ without a real compressor is rejected"),
		UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Frame, LazConfig, LazExtension, LazBytes, LazPointCount, LazError));
	TestTrue(TEXT("LAZ rejection explains the missing compressor"), LazError.Contains(TEXT("LAZ")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorBinaryPcdContractTest,
	"MA0T10.SensorStream.BinaryPcdContractAndFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorBinaryPcdContractTest::RunTest(const FString& Parameters)
{
	auto MakePoint = [](const FVector& LocalMeters, FName Semantic, const TArray<FName>& Tags, int32 HorizontalIndex)
	{
		FVirtualLidarPoint Point;
		Point.bHit = true;
		Point.SensorLocalPositionMeters = LocalMeters;
		Point.Distance = static_cast<float>(LocalMeters.Size() * 100.0);
		Point.RangeMillimeters = FMath::RoundToInt(LocalMeters.Size() * 1000.0);
		Point.RawIntensity = 1000 + HorizontalIndex;
		Point.Ring = 4;
		Point.HorizontalIndex = HorizontalIndex;
		Point.EchoIndex = 1;
		Point.EchoCount = 2;
		Point.PointTimeOffsetNanoseconds = 25000 + HorizontalIndex;
		Point.Validity = EVirtualLidarPointValidity::Valid;
		Point.Confidence = 0.8f;
		Point.SemanticLabel = Semantic;
		Point.HitActorTags = Tags;
		return Point;
	};

	TArray<FVirtualLidarPoint> Points;
	Points.Add(MakePoint(FVector(1.0, 0.0, 0.0), TEXT("Crate"), {TEXT("PointCloudTarget")}, 10));
	Points.Add(MakePoint(FVector(2.0, 0.0, 0.0), TEXT("Wall"), {TEXT("PointCloudTarget")}, 11));
	Points.Add(MakePoint(FVector(1.0, 5.0, 0.0), TEXT("Crate"), {TEXT("Other")}, 12));
	FVirtualLidarPoint& Miss = Points.AddDefaulted_GetRef();
	Miss.bHit = false;

	FVirtualSensorFrameEnvelope Frame;
	Frame.SensorId = TEXT("LIDAR-PCD-BINARY");
	Frame.SensorKind = EVirtualSensorKind::Lidar;
	Frame.FrameId = 100;
	Frame.TimestampUtc = FDateTime(2026, 8, 4, 10, 0, 0);
	Frame.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));

	FVirtualSensorStreamConfig Config;
	Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
	Config.PcdDataMode = EVirtualPcdDataMode::Binary;
	Config.PointCloudFilter.IncludeActorTags = {TEXT("PointCloudTarget")};
	Config.PointCloudFilter.IncludeSemanticLabels = {TEXT("Crate")};
	Config.PointCloudFilter.bEnableSensorLocalRoi = true;
	Config.PointCloudFilter.SensorLocalRoiMinCm = FVector(0.0, -100.0, -100.0);
	Config.PointCloudFilter.SensorLocalRoiMaxCm = FVector(150.0, 100.0, 100.0);
	Config.PointCloudFilter.MinRangeCm = 50.0f;
	Config.PointCloudFilter.MaxRangeCm = 150.0f;

	FString Extension, Error;
	TArray<uint8> Bytes;
	int32 PointCount = 0;
	TestTrue(TEXT("filtered binary PCD serializes"),
		UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Frame, Config, Extension, Bytes, PointCount, Error));
	TestEqual(TEXT("binary PCD extension"), Extension, FString(TEXT("pcd")));
	TestEqual(TEXT("Tag, semantic and ROI groups are ANDed"), PointCount, 1);

	const ANSICHAR Marker[] = "DATA binary\n";
	int32 PayloadOffset = INDEX_NONE;
	for (int32 Index = 0; Index + static_cast<int32>(sizeof(Marker) - 1) <= Bytes.Num(); ++Index)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Index, Marker, sizeof(Marker) - 1) == 0)
		{
			PayloadOffset = Index + static_cast<int32>(sizeof(Marker) - 1);
			break;
		}
	}
	TestTrue(TEXT("PCD declares DATA binary"), PayloadOffset != INDEX_NONE);
	if (PayloadOffset != INDEX_NONE)
	{
		TestEqual(TEXT("binary record is explicitly packed to 33 bytes"), Bytes.Num() - PayloadOffset, 33);
		float X = 0.0f;
		FMemory::Memcpy(&X, Bytes.GetData() + PayloadOffset, sizeof(float));
		TestEqual(TEXT("sensor-local X is written in metres"), X, 1.0f);
	}

	Config.PointCloudFilter.ExcludeSemanticLabels = {TEXT("Crate")};
	Bytes.Reset();
	PointCount = -1;
	TestTrue(TEXT("zero-point binary PCD remains a valid frame"),
		UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Frame, Config, Extension, Bytes, PointCount, Error));
	TestEqual(TEXT("exclude rules take precedence"), PointCount, 0);
	TestTrue(TEXT("zero-point PCD still carries its header"), Bytes.Num() > 100);

	for (const int32 ScalePointCount : {32256, 64512})
	{
		TArray<FVirtualLidarPoint> ScalePoints;
		ScalePoints.Reserve(ScalePointCount);
		for (int32 Index = 0; Index < ScalePointCount; ++Index)
		{
			ScalePoints.Add(MakePoint(
				FVector(1.0 + (Index % 576) * 0.01, (Index % 56) * 0.01, 0.5),
				TEXT("ScaleTarget"), {TEXT("PointCloudTarget")}, Index % 576));
		}
		FVirtualSensorFrameEnvelope ScaleFrame;
		ScaleFrame.SensorId = TEXT("LIDAR-PCD-SCALE");
		ScaleFrame.SensorKind = EVirtualSensorKind::Lidar;
		ScaleFrame.FrameId = ScalePointCount;
		ScaleFrame.TimestampUtc = Frame.TimestampUtc;
		ScaleFrame.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(ScalePoints));
		FVirtualSensorStreamConfig ScaleConfig;
		ScaleConfig.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
		ScaleConfig.PcdDataMode = EVirtualPcdDataMode::Binary;
		Bytes.Reset();
		PointCount = 0;
		TestTrue(FString::Printf(TEXT("%d-point binary PCD serializes"), ScalePointCount),
			UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(
				ScaleFrame, ScaleConfig, Extension, Bytes, PointCount, Error));
		TestEqual(FString::Printf(TEXT("%d-point PCD preserves every point"), ScalePointCount), PointCount, ScalePointCount);
		TestTrue(FString::Printf(TEXT("%d-point PCD body stays below 16 MiB"), ScalePointCount), Bytes.Num() < 16 * 1024 * 1024);
		TestTrue(FString::Printf(TEXT("%d-point PCD contains exactly 33 bytes per record plus header"), ScalePointCount),
			Bytes.Num() > ScalePointCount * 33 && Bytes.Num() < ScalePointCount * 33 + 1024);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorBinaryPcdReceiverTest,
	"MA0T10.SensorStream.BinaryPcdReceiverValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorBinaryPcdReceiverTest::RunTest(const FString& Parameters)
{
	FVirtualLidarPoint Point;
	Point.bHit = true;
	Point.SensorLocalPositionMeters = FVector(1.25, -2.5, 0.75);
	Point.RangeMillimeters = 2880;
	Point.RawIntensity = 4321;
	Point.Ring = 8;
	Point.HorizontalIndex = 17;
	Point.EchoCount = 1;
	Point.Validity = EVirtualLidarPointValidity::Valid;
	Point.Confidence = 0.95f;
	TArray<FVirtualLidarPoint> Points;
	Points.Add(Point);
	FVirtualSensorFrameEnvelope Frame;
	Frame.SensorId = TEXT("LIDAR-PCD-RECEIVER");
	Frame.SensorKind = EVirtualSensorKind::Lidar;
	Frame.FrameId = 77;
	Frame.TimestampUtc = FDateTime(2026, 8, 4, 12, 0, 0);
	Frame.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));
	FVirtualSensorStreamConfig Config;
	Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
	Config.PcdDataMode = EVirtualPcdDataMode::Binary;
	FString Extension, Error;
	TArray<uint8> Bytes;
	int32 PointCount = 0;
	TestTrue(TEXT("receiver fixture serializes"), UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(
		Frame, Config, Extension, Bytes, PointCount, Error));

	uint8 Hash[FSHA1::DigestSize];
	FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash);
	TMap<FName, FString> Headers;
	Headers.Add(TEXT("schema"), TEXT("virtual-pointcloud.pcd.v1"));
	Headers.Add(TEXT("content-type"), TEXT("application/vnd.pcd"));
	Headers.Add(TEXT("x-sensor-id"), Frame.SensorId);
	Headers.Add(TEXT("x-frame-id"), LexToString(Frame.FrameId));
	Headers.Add(TEXT("x-point-count"), LexToString(PointCount));
	Headers.Add(TEXT("x-source-point-count"), TEXT("1"));
	Headers.Add(TEXT("x-filter-revision"), TEXT("3"));
	Headers.Add(TEXT("x-acquisition-profile"), TEXT("iyobot-mlx80-native"));
	Headers.Add(TEXT("x-utc"), Frame.TimestampUtc.ToIso8601());
	Headers.Add(TEXT("x-checksum-sha1"), BytesToHex(Hash, FSHA1::DigestSize).ToLower());

	UVirtualPointCloudStreamReceiverTC* Receiver = NewObject<UVirtualPointCloudStreamReceiverTC>();
	TSharedPtr<FVirtualPointCloudStreamReceiverData> Parsed = StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(
		Receiver->ParseBinaryPcdToStruct(Bytes, Headers));
	TestTrue(TEXT("raw PCD receiver accepts the binary contract"), Parsed.IsValid() && Parsed->bValid);
	if (Parsed.IsValid())
	{
		TestEqual(TEXT("receiver keeps frame id"), Parsed->FrameId, Frame.FrameId);
		TestEqual(TEXT("receiver validates point count"), Parsed->PointCount, 1);
		TestTrue(TEXT("receiver performs checksum validation every frame"), Parsed->bDeepValidated);
	}

	Headers[TEXT("x-checksum-sha1")] = TEXT("0000000000000000000000000000000000000000");
	Parsed = StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Receiver->ParseBinaryPcdToStruct(Bytes, Headers));
	TestTrue(TEXT("checksum mismatch is rejected"), Parsed.IsValid() && !Parsed->bValid);
	TestTrue(TEXT("checksum failure is explained"), Parsed.IsValid() && Parsed->Message.Contains(TEXT("checksum")));
	return true;
}

struct FVirtualSensorArtemisSmokeState
{
	TObjectPtr<UVirtualSensorTransportComponent> Transport;
	TObjectPtr<UVirtualSensorStreamPublisherComponent> Publisher;
	double StartedSeconds = 0.0;
	bool bFramesSubmitted = false;
};

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FVirtualSensorArtemisSmokeCommand,
	TSharedPtr<FVirtualSensorArtemisSmokeState>, State,
	FAutomationTestBase*, Test);

bool FVirtualSensorArtemisSmokeCommand::Update()
{
	if (!State.IsValid() || !State->Transport || !State->Publisher) return true;
	const double Now = FPlatformTime::Seconds();
	if (Now - State->StartedSeconds > 20.0)
	{
		Test->AddError(TEXT("Artemis smoke timed out before all three streams were submitted."));
		State->Transport->RemoveFromRoot();
		State->Publisher->RemoveFromRoot();
		return true;
	}
	if (!State->Transport->IsStompConnected())
	{
		State->Transport->TestConnection();
		return false;
	}
	if (!State->bFramesSubmitted)
	{
		FVirtualSensorFrameEnvelope Lidar;
		Lidar.SensorId = TEXT("LIDAR-ARTEMIS-SMOKE");
		Lidar.SensorKind = EVirtualSensorKind::Lidar;
		Lidar.FrameId = 101;
		Lidar.TimestampUtc = FDateTime::UtcNow();
		Lidar.SchemaVersion = TEXT("virtual-lidar.v1");
		Lidar.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(TEXT("{\"schema\":\"virtual-lidar.v1\",\"frameId\":101,\"smoke\":true}"));
		TArray<FVirtualLidarPoint> Points;
		FVirtualLidarPoint& Point = Points.AddDefaulted_GetRef();
		Point.bHit = true;
		Point.WorldLocation = FVector(100.0, 200.0, 300.0);
		Point.Distance = 374.1657f;
		Point.Row = 0;
		Point.Col = 0;
		Lidar.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));
		State->Publisher->SubmitFrame(Lidar);

		FVirtualSensorFrameEnvelope Camera;
		Camera.SensorId = TEXT("CAMERA-ARTEMIS-SMOKE");
		Camera.SensorKind = EVirtualSensorKind::Camera;
		Camera.FrameId = 202;
		Camera.TimestampUtc = FDateTime::UtcNow();
		Camera.SchemaVersion = TEXT("virtual-camera.v1");
		Camera.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(TEXT("{\"schema\":\"virtual-camera.v1\",\"frameId\":202,\"imageBase64\":\"/9j/2Q==\",\"smoke\":true}"));
		State->Publisher->SubmitFrame(Camera);
		State->bFramesSubmitted = true;
	}
	State->Publisher->PumpPublisherOnce(Now);
	int32 SubmittedStreams = 0;
	int32 ReceiptStreams = 0;
	for (const FVirtualSensorStreamStatus& Status : State->Publisher->GetStreamStatuses())
	{
		if (Status.SubmittedFrameCount > 0) ++SubmittedStreams;
		if (Status.ReceiptReceivedCount > 0) ++ReceiptStreams;
	}
	if (SubmittedStreams < 3 || ReceiptStreams < 3) return false;
	Test->TestEqual(TEXT("Artemis smoke submitted LiDAR, Camera, and Point Cloud streams"), SubmittedStreams, 3);
	Test->TestEqual(TEXT("Artemis smoke received a broker receipt for every stream"), ReceiptStreams, 3);
	State->Transport->RemoveFromRoot();
	State->Publisher->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorArtemisIntegrationSmokeTest,
	"MA0T10.SensorStream.ArtemisIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorArtemisIntegrationSmokeTest::RunTest(const FString& Parameters)
{
	if (!FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RUN_ARTEMIS_SMOKE")).Equals(TEXT("1")))
	{
		AddInfo(TEXT("Artemis integration smoke skipped. Set MA0T10_RUN_ARTEMIS_SMOKE=1 to enable it."));
		return true;
	}
	TSharedPtr<FVirtualSensorArtemisSmokeState> State = MakeShared<FVirtualSensorArtemisSmokeState>();
	State->Transport = NewObject<UVirtualSensorTransportComponent>();
	State->Publisher = NewObject<UVirtualSensorStreamPublisherComponent>();
	State->Transport->AddToRoot();
	State->Publisher->AddToRoot();
	FVirtualSensorTransportProfile Profile;
	const FString Url = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_URL"));
	const FString User = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_USER"));
	const FString Password = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_PASSWORD"));
	if (!Url.IsEmpty()) Profile.BrokerUrl = Url;
	if (!User.IsEmpty()) Profile.UserName = User;
	Profile.MaxMessageBytes = 8 * 1024 * 1024;
	State->Transport->ConfigureTransportProfile(Profile);
	State->Transport->SetSessionCredentials(Password, FString());
	State->Transport->TransportMode = EVirtualSensorTransportMode::StompWebSocket;
	State->Publisher->SetTransportComponent(State->Transport);
	for (EVirtualSensorStreamKind Kind : {EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::CameraImage, EVirtualSensorStreamKind::PointCloud})
	{
		FVirtualSensorStreamConfig Config;
		Config.StreamKind = Kind;
		Config.bEnabled = true;
		Config.ReceiptSampleInterval = 1;
		Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
		Config.PcdDataMode = EVirtualPcdDataMode::Binary;
		Config.DeliveryMode = Kind == EVirtualSensorStreamKind::PointCloud
			? EVirtualPointCloudDeliveryMode::ConnectedNoLoss
			: EVirtualPointCloudDeliveryMode::LatestFrame;
		Config.MaxBufferedFrames = 20;
		State->Publisher->ConfigureStream(Config);
	}
	State->StartedSeconds = FPlatformTime::Seconds();
	State->Transport->TestConnection();
	ADD_LATENT_AUTOMATION_COMMAND(FVirtualSensorArtemisSmokeCommand(State, this));
	return true;
}

#endif
