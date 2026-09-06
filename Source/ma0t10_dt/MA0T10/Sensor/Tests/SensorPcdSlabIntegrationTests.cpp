#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStompAuthContractTest, "MA0T10.SensorStream.StompAuthContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStompAuthContractTest::RunTest(const FString& Parameters)
{
	TMap<FName,FString> Headers; FString Error;
	TestTrue(TEXT("empty password is a broker decision, never an engine assertion"),
		UVirtualSensorTransportComponent::BuildStompConnectHeaders(TEXT("user"),TEXT(""),TEXT(""),Headers,Error));
	TestTrue(TEXT("login and passcode keys travel together"),Headers.Contains(TEXT("login")) && Headers.Contains(TEXT("passcode")));
	TestFalse(TEXT("mixed upgrade and CONNECT auth rejected before engine call"),
		UVirtualSensorTransportComponent::BuildStompConnectHeaders(TEXT("user"),TEXT("secret"),TEXT("token"),Headers,Error));
	TestTrue(TEXT("token-only auth has no CONNECT login"),
		UVirtualSensorTransportComponent::BuildStompConnectHeaders(TEXT(""),TEXT(""),TEXT("token"),Headers,Error) && Headers.IsEmpty());
	return true;
}

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdLiveEntryTest, "MA0T10.SensorStream.LivePcdEntryPoints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPcdLiveEntryTest::RunTest(const FString& Parameters)
{
	auto* Publisher = NewObject<UVirtualSensorStreamPublisherComponent>();
	Publisher->StartStream(EVirtualSensorStreamKind::PointCloud, TEXT("DIRECT-BP"));
	auto Config = Publisher->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud, TEXT("DIRECT-BP"));
	TestEqual(TEXT("direct Blueprint start uses PCD"), Config.PointCloudFormat, EVirtualPointCloudStreamFormat::PCD);
	TestEqual(TEXT("direct start never uses ASCII/base64"), Config.PcdDataMode, EVirtualPcdDataMode::Binary);
	Config.PointCloudFormat = EVirtualPointCloudStreamFormat::CSV;
	Config.PcdDataMode = EVirtualPcdDataMode::Ascii;
	Config.FrameStride = 10;
	Publisher->ConfigureStream(Config);
	Config = Publisher->GetEffectiveStreamConfig(Config.StreamKind, Config.SensorId);
	TestEqual(TEXT("configured stream preserves every acquisition"), Config.FrameStride, 1);
	FString Error;
	TestTrue(TEXT("2-echo binary fits 8MiB"), Publisher->ValidateBinaryBodySize(64512LL * 33 + 1024, 8 * 1024 * 1024, Error));
	TestFalse(TEXT("lower limit remains enforced"), Publisher->ValidateBinaryBodySize(64512LL * 33 + 1024, 1024, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabContextSnapshotTest, "MA0T10.SensorStream.SlabContextSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlabContextSnapshotTest::RunTest(const FString& Parameters)
{
	FVirtualSlabFrameContext Current;
	Current.RunId=FGuid::NewGuid().ToString(); Current.MtlNo=TEXT("SQ83521 047"); Current.SlabFrameNo=100; Current.ElapsedSec=5; Current.Generation=1; Current.bEligible=true;
	FVirtualSensorFrameEnvelope SensorFrame;
	SensorFrame.FrameId=9001; SensorFrame.SlabContext=Current;
	Current.SlabFrameNo=101; Current.MtlNo=TEXT("NEXT-SLAB");
	const auto Headers=SensorFrame.SlabContext.ToHeaders();
	TestEqual(TEXT("delayed result keeps acquisition slab number"), Headers.FindRef(TEXT("x-slab-frame-no")), FString(TEXT("100")));
	TestEqual(TEXT("material id includes original whitespace"), Headers.FindRef(TEXT("x-mtl-no")), FString(TEXT("SQ83521 047")));
	TestEqual(TEXT("sensor frame independent of slab frame"), SensorFrame.FrameId, static_cast<int64>(9001));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabLifecycleTest, "MA0T10.SensorStream.SlabSessionLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlabLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
	auto* Manager=World->SpawnActor<AVirtualSensorCoordinator>();
	auto* Sensor=World->SpawnActor<AVirtualLidarSensorActor>();
	Manager->RegisterSensorActor(Sensor);
	auto* Slab=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	TestNotNull(TEXT("world adapter exists"),Slab);
	if (!Slab) return false;
	TestTrue(TEXT("log-only transport cannot claim a Topic session"),Slab->BeginSlabSensorSession(FString(),{}).IsEmpty());
	Manager->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
	const FString Run=Slab->BeginSlabSensorSession(FString(),{});
	TestFalse(TEXT("begin creates UUID"),Run.IsEmpty());
	TestFalse(TEXT("ready state does not admit sensor frames"),Slab->CaptureContext(Sensor->GetSensorId(),9001).bEligible);
	TestTrue(TEXT("first applied slab activates session"),Slab->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),100,5.0));
	const auto Captured=Slab->CaptureContext(Sensor->GetSensorId(),9001);
	TestTrue(TEXT("duplicate applied data is idempotent"),Slab->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),100,5.0));
	TestFalse(TEXT("reversed slab frame rejected"),Slab->NotifySlabFrameApplied(Run,TEXT("SQ83521 047"),99,4.95));
	TestFalse(TEXT("wrong run rejected"),Slab->EndSlabSensorSession(TEXT("wrong"),false));
	Slab->NotifySlabFrameApplied(Run,TEXT("NEXT"),101,5.05);
	TestEqual(TEXT("captured context stays at earlier applied slab frame"),Captured.SlabFrameNo,static_cast<int64>(100));
	Slab->SetSlabSensorSessionPaused(Run,true);
	TestFalse(TEXT("paused acquisition not admitted"),Slab->CaptureContext(Sensor->GetSensorId(),9002).bEligible);
	TestTrue(TEXT("pre-pause acquisition can drain"),Slab->AllowsFrame(Sensor->GetSensorId(),Captured));
	Slab->SetSlabSensorSessionPaused(Run,false);
	Slab->EndSlabSensorSession(Run,false);
	TestFalse(TEXT("new acquisition after movement end rejected"),Slab->CaptureContext(Sensor->GetSensorId(),9003).bEligible);
	Slab->Tick(0.01f);
	TestEqual(TEXT("in-flight acquisition holds draining state"),Slab->GetSlabSensorSessionStatus().State,EVirtualSlabSessionState::Draining);
	Slab->CompleteAcquisition(Sensor->GetSensorId(),9001); Slab->Tick(0.01f);
	TestEqual(TEXT("all pending work completed"),Slab->GetSlabSensorSessionStatus().State,EVirtualSlabSessionState::Completed);
	TestTrue(TEXT("used UUID cannot be reused"),Slab->BeginSlabSensorSession(Run,{}).IsEmpty());
	const FString Next=Slab->BeginSlabSensorSession(FString(),{});
	TestTrue(TEXT("new run succeeds"),!Next.IsEmpty() && Next!=Run);
	TestFalse(TEXT("previous run result never leaks"),Slab->AllowsFrame(Sensor->GetSensorId(),Captured));
	Slab->EndSlabSensorSession(Next,true); Slab->Tick(0.01f);
	Sensor->StopSensor(); Sensor->Destroy(); Manager->Destroy();
	return true;
}
#endif
