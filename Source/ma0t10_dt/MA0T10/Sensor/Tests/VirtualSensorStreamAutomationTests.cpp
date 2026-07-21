#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

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

#endif
