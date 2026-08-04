#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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

#endif
