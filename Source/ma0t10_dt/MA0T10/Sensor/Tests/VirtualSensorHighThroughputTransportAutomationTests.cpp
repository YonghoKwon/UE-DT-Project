#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStompProtocol.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorIncrementalStompParserTest,
	"MA0T10.SensorStream.HighThroughput.IncrementalStompParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorIncrementalStompParserTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Wire;
	auto AppendText = [&Wire](const FString& Text)
	{
		FTCHARToUTF8 Utf8(*Text);
		Wire.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	};
	AppendText(TEXT("MESSAGE\nschema:virtual-camera.jpeg.v1\ncontent-length:4\n\n"));
	Wire.Add(0xff);
	Wire.Add(0xd8);
	Wire.Add(0xff);
	Wire.Add(0xd9);
	Wire.Add(0);
	AppendText(TEXT("\nRECEIPT\nreceipt-id:CAM-1\n\n"));
	Wire.Add(0);

	FVirtualSensorStompParser Parser;
	TArray<FVirtualSensorStompFrame> Frames;
	FString Error;
	for (int32 Offset = 0; Offset < Wire.Num(); Offset += 3)
	{
		const int32 Count = FMath::Min(3, Wire.Num() - Offset);
		TestTrue(TEXT("fragment parses without error"), Parser.Append(Wire.GetData() + Offset, Count, Frames, Error));
		if (!Error.IsEmpty()) AddError(Error);
	}
	TestEqual(TEXT("fragmented/coalesced frame count"), Frames.Num(), 2);
	if (Frames.Num() == 2)
	{
		TestEqual(TEXT("binary command"), Frames[0].Command, FString(TEXT("MESSAGE")));
		TestEqual(TEXT("binary body size"), Frames[0].Body.Num(), 4);
		TestEqual(TEXT("binary NUL-safe byte"), Frames[0].Body[1], static_cast<uint8>(0xd8));
		TestEqual(TEXT("receipt command"), Frames[1].Command, FString(TEXT("RECEIPT")));
		TestEqual(TEXT("receipt id"), Frames[1].Headers.FindRef(TEXT("receipt-id")), FString(TEXT("CAM-1")));
	}

	const FString Original = TEXT("topic:test\\line\nnext");
	TestEqual(TEXT("header escaping round trip"), FVirtualSensorStompParser::UnescapeHeader(FVirtualSensorStompParser::EscapeHeader(Original)), Original);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorRawTcpEndpointPolicyTest,
	"MA0T10.SensorStream.HighThroughput.EndpointPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorRawTcpEndpointPolicyTest::RunTest(const FString& Parameters)
{
	FString Reason;
	TestTrue(TEXT("tcp endpoint supported"), UVirtualSensorHighThroughputTransportSubsystem::CanUseRawTcp(TEXT("tcp://127.0.0.1:61616"), &Reason));
	TestTrue(TEXT("existing ws endpoint can reuse raw Artemis acceptor"), UVirtualSensorHighThroughputTransportSubsystem::CanUseRawTcp(TEXT("ws://127.0.0.1:61616"), &Reason));
	TestFalse(TEXT("wss remains compatibility fallback"), UVirtualSensorHighThroughputTransportSubsystem::CanUseRawTcp(TEXT("wss://broker.example:61617"), &Reason));
	TestTrue(TEXT("fallback reason exposed"), !Reason.IsEmpty());
	return true;
}

namespace
{
FString TestSha1(const TArray64<uint8>& Body)
{
	uint8 Hash[FSHA1::DigestSize];
	FSHA1::HashBuffer(Body.GetData(), static_cast<uint32>(Body.Num()), Hash);
	return BytesToHex(Hash, FSHA1::DigestSize).ToLower();
}

struct FVirtualSensorRawTcpLoopbackState
{
	TObjectPtr<UVirtualSensorHighThroughputTransportSubsystem> Subsystem;
	double StartedSeconds = 0.0;
};

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FVirtualSensorRawTcpLoopbackCommand,
	TSharedPtr<FVirtualSensorRawTcpLoopbackState>, State,
	FAutomationTestBase*, Test);

bool FVirtualSensorRawTcpLoopbackCommand::Update()
{
	if (!State.IsValid() || !State->Subsystem) return true;
	State->Subsystem->Tick(0.016f);
	const TArray<FVirtualSensorStreamTelemetry> Telemetry = State->Subsystem->GetStreamTelemetry();
	int32 CompletedKinds = 0;
	for (const FVirtualSensorStreamTelemetry& Item : Telemetry)
	{
		if (Item.SubmittedCount >= 1 && Item.ReceiptCount >= 1 && Item.ConsumerReceivedCount >= 1 && Item.ValidationFailureCount == 0)
		{
			++CompletedKinds;
		}
	}
	if (CompletedKinds >= 3)
	{
		Test->TestEqual(TEXT("all three raw streams were submitted, receipted, and consumed"), CompletedKinds, 3);
		State->Subsystem->StopHighThroughputTransport();
		State->Subsystem->RemoveFromRoot();
		return true;
	}
	if (FPlatformTime::Seconds() - State->StartedSeconds > 15.0)
	{
		for (const FVirtualSensorStreamTelemetry& Item : Telemetry)
		{
			Test->AddError(FString::Printf(TEXT("Raw loopback timeout kind=%d submitted=%lld receipt=%lld consumed=%lld invalid=%lld state=%s message=%s"),
				static_cast<int32>(Item.StreamKind), Item.SubmittedCount, Item.ReceiptCount, Item.ConsumerReceivedCount,
				Item.ValidationFailureCount, *Item.State, *Item.Message));
		}
		State->Subsystem->StopHighThroughputTransport();
		State->Subsystem->RemoveFromRoot();
		return true;
	}
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVirtualSensorRawTcpArtemisLoopbackTest,
	"MA0T10.SensorStream.HighThroughput.RawTcpArtemisLoopback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorRawTcpArtemisLoopbackTest::RunTest(const FString& Parameters)
{
	if (!FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_RUN_ARTEMIS_SMOKE")).Equals(TEXT("1")))
	{
		AddInfo(TEXT("Raw TCP Artemis loopback skipped. Set MA0T10_RUN_ARTEMIS_SMOKE=1 to enable it."));
		return true;
	}
	TSharedPtr<FVirtualSensorRawTcpLoopbackState> State = MakeShared<FVirtualSensorRawTcpLoopbackState>();
	State->Subsystem = NewObject<UVirtualSensorHighThroughputTransportSubsystem>();
	State->Subsystem->AddToRoot();
	FVirtualSensorHighThroughputProfile Profile;
	Profile.BrokerUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_RAW_URL"));
	if (Profile.BrokerUrl.IsEmpty()) Profile.BrokerUrl = TEXT("tcp://127.0.0.1:61616");
	const FString User = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_USER"));
	if (!User.IsEmpty()) Profile.UserName = User;
	const FString Password = FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ARTEMIS_PASSWORD"));
	TestTrue(TEXT("raw transport starts"), State->Subsystem->StartHighThroughputTransport(Profile, Password));

	auto Enqueue = [this, &State](EVirtualSensorStreamKind Kind, const FString& SensorId, int64 FrameId, const FString& Schema, const FString& ContentType, const FString& Destination, TArray64<uint8>&& Body)
	{
		FVirtualSensorBinaryFrame Frame;
		Frame.StreamKind = Kind;
		Frame.SensorId = SensorId;
		Frame.FrameId = FrameId;
		Frame.TimestampUtc = FDateTime::UtcNow();
		Frame.Schema = Schema;
		Frame.ContentType = ContentType;
		Frame.Destination = Destination;
		Frame.Headers.Add(TEXT("checksum"), TestSha1(Body));
		Frame.Body64 = MakeShared<const TArray64<uint8>, ESPMode::ThreadSafe>(MoveTemp(Body));
		FString Error;
		TestTrue(FString::Printf(TEXT("enqueue %s"), *Schema), State->Subsystem->EnqueueBinaryFrame(Frame, Error));
		if (!Error.IsEmpty()) AddError(Error);
	};

	TArray64<uint8> CameraBody;
	CameraBody.Add(0xff); CameraBody.Add(0xd8); CameraBody.Add(0xff); CameraBody.Add(0xd9);
	Enqueue(EVirtualSensorStreamKind::CameraImage, TEXT("CAM-RAW-SMOKE"), 1, TEXT("virtual-camera.jpeg.v1"), TEXT("image/jpeg"), Profile.CameraTopic, MoveTemp(CameraBody));

	const FString LidarJson = TEXT("{\"schema\":\"virtual-lidar.telemetry.v1\",\"sensorId\":\"LIDAR-RAW-SMOKE\",\"frameId\":2}");
	FTCHARToUTF8 LidarUtf8(*LidarJson);
	TArray64<uint8> LidarBody;
	LidarBody.Append(reinterpret_cast<const uint8*>(LidarUtf8.Get()), LidarUtf8.Length());
	Enqueue(EVirtualSensorStreamKind::LidarPayload, TEXT("LIDAR-RAW-SMOKE"), 2, TEXT("virtual-lidar.telemetry.v1"), TEXT("application/json"), Profile.LidarTopic, MoveTemp(LidarBody));

	const FString PcdText = TEXT("# .PCD v0.7\nVERSION 0.7\nFIELDS x y z\nSIZE 4 4 4\nTYPE F F F\nCOUNT 1 1 1\nWIDTH 0\nHEIGHT 1\nPOINTS 0\nDATA binary\n");
	FTCHARToUTF8 PcdUtf8(*PcdText);
	TArray64<uint8> PcdBody;
	PcdBody.Append(reinterpret_cast<const uint8*>(PcdUtf8.Get()), PcdUtf8.Length());
	Enqueue(EVirtualSensorStreamKind::PointCloud, TEXT("LIDAR-RAW-SMOKE"), 2, TEXT("virtual-pointcloud.pcd.v1"), TEXT("application/vnd.pcd"), Profile.PointCloudTopic, MoveTemp(PcdBody));

	State->StartedSeconds = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FVirtualSensorRawTcpLoopbackCommand(State, this));
	return true;
}

#endif
