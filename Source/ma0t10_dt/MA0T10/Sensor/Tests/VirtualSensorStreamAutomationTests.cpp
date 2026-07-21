#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "HAL/PlatformMisc.h"

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
	Hit.Row = 2;
	Hit.Col = 3;
	Hit.SemanticLabel = TEXT("TestObject");
	FVirtualLidarPoint& Miss = Points.AddDefaulted_GetRef();
	Miss.bHit = false;
	Frame.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));

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
		Config.PointCloudFormat = EVirtualPointCloudStreamFormat::CSV;
		State->Publisher->ConfigureStream(Config);
	}
	State->StartedSeconds = FPlatformTime::Seconds();
	State->Transport->TestConnection();
	ADD_LATENT_AUTOMATION_COMMAND(FVirtualSensorArtemisSmokeCommand(State, this));
	return true;
}

#endif
