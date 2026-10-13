#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorWireHeaders.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStompProtocol.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

namespace
{
TArray<uint8> Utf8Bytes(const FString& Text)
{
    FTCHARToUTF8 Utf8(*Text);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
    return Bytes;
}
FString Checksum(const TArray<uint8>& Bytes)
{
    uint8 Hash[FSHA1::DigestSize];
    FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash);
    return BytesToHex(Hash, FSHA1::DigestSize).ToLower();
}
// Use the worker's exact header traversal and escape routine, including order.
TArray<uint8> WireHeader(const TMap<FString,FString>& Headers)
{
    FString Text = TEXT("SEND\n");
    for (const auto& Pair : Headers)
        Text += FVirtualSensorStompParser::EscapeHeader(Pair.Key) + TEXT(":") + FVirtualSensorStompParser::EscapeHeader(Pair.Value) + TEXT("\n");
    return Utf8Bytes(Text + TEXT("\n"));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArchitectureWireGoldenTest, "MA0T10.Architecture.WireGolden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArchitectureWireGoldenTest::RunTest(const FString&)
{
    const FString Directory = FPaths::ProjectDir() / TEXT("Source/ma0t10_dt/MA0T10/Sensor/Tests/Fixtures/ArchitectureWire");
    auto Compare = [this, &Directory](const FString& Name, const TArray<uint8>& Actual)
    {
        const FString Path = Directory / Name;
        TArray<uint8> Expected;
        if (!TestTrue(*(Name + TEXT(" fixture exists")), FFileHelper::LoadFileToArray(Expected, *Path))) return false;
        return TestTrue(*(Name + TEXT(" byte identical")), Actual == Expected);
    };
    auto* Publisher = NewObject<UVirtualSensorStreamPublisherComponent>();
    Publisher->TransportComponent = NewObject<UVirtualSensorTransportComponent>(Publisher);
    Publisher->TransportComponent->TransportMode = EVirtualSensorTransportMode::StompWebSocket;
    Publisher->TransportComponent->TransportProfile.BrokerUrl = TEXT("tcp://127.0.0.1:61616");
    auto* Scan = NewObject<UVirtualLidarScanComponent>();
    Scan->SensorId = TEXT("sensor-fixed-007");
    Scan->bIncludeSlabAnalysisInPayload = false;
    for (const bool WithContext : {false, true})
    {
        const FString Prefix = WithContext ? TEXT("with-context") : TEXT("without-context");
        FVirtualSensorFrameEnvelope Frame;
        Frame.SensorId = Scan->SensorId;
        Frame.SensorKind = EVirtualSensorKind::Lidar;
        Frame.FrameId = 9007199254740993LL;
        Frame.TimestampUtc = FDateTime(2026, 10, 12, 1, 2, 3, 456);
        if (WithContext)
        {
            auto& C = Frame.SlabContext;
            C.RunId = TEXT("12345678-1234-4321-9876-123456789abc");
            C.ScenarioUUID = TEXT("abcdef01-2345-6789-abcd-ef0123456789");
            C.MtlNo = TEXT("0000123456789012345");
            C.SlabFrameNo = 9007199254740995LL;
            C.ElapsedSec = 12.3456789;
            C.Generation = 19; C.Segment = 3; C.bEligible = true;
            C.AcquisitionUtcTicks = Frame.TimestampUtc.GetTicks();
        }
        TArray<FVirtualLidarPoint> Points;
        for (int32 I = 0; I < 2; ++I)
        {
            auto& P = Points.AddDefaulted_GetRef();
            P.bHit = true; P.SensorLocalPositionMeters = FVector(1.25 + I, -2.5, 0.75);
            P.Distance = 375;
            P.RawIntensity = 12345 + I; P.Ring = 7; P.HorizontalIndex = 11 + I;
            P.EchoIndex = I; P.EchoCount = 2; P.PointTimeOffsetNanoseconds = 1234567890123LL + I;
            P.Validity = EVirtualLidarPointValidity::Valid; P.Confidence = .875f;
        }
        Frame.PointSnapshot = MakeShared<const TArray<FVirtualLidarPoint>, ESPMode::ThreadSafe>(MoveTemp(Points));
        auto Snapshot = MakeShared<FVirtualLidarFrameSnapshot, ESPMode::ThreadSafe>();
        Snapshot->Points = Frame.PointSnapshot; Snapshot->SlabContext = Frame.SlabContext;
        Snapshot->FrameId = Frame.FrameId;
        Snapshot->AcquisitionTransform = FTransform(FQuat::Identity, FVector(123, -456, 789));
        Snapshot->AcquisitionStartUnixNanoseconds = Frame.TimestampUtc.ToUnixTimestamp() * 1000000000LL + 456000000;
        Snapshot->AcquisitionEndUnixNanoseconds = Snapshot->AcquisitionStartUnixNanoseconds + 123456;
        Snapshot->ProfileKey = TEXT("fixed-profile"); Snapshot->HorizontalSamples = 2; Snapshot->VerticalChannels = 1;
        Snapshot->RequestedRayCount = 2; Snapshot->ValidPointCount = 2;
        Snapshot->FirstEchoCount = 1; Snapshot->SecondEchoCount = 1;
        Snapshot->MinRangeMeters = 1.25f; Snapshot->MaxRangeMeters = 3.75f; Snapshot->MeanRangeMeters = 2.5f;
        Snapshot->MinIntensity = 12; Snapshot->MaxIntensity = 34; Snapshot->MeanIntensity = 23;
        Frame.LidarFrameSnapshot = Snapshot;
        FVirtualSensorStreamConfig Config;
        Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD; Config.PcdDataMode = EVirtualPcdDataMode::Binary;
        TArray<uint8> Pcd; FString Extension, Error; int32 Count = 0;
        if (!TestTrue(TEXT("PCD serialized"), Publisher->SerializePointCloudFrame(Frame, Config, Extension, Pcd, Count, Error))) return false;
        TestEqual(TEXT("two 33-byte points"), Count, 2);
        Compare(Prefix + TEXT(".pcd"), Pcd);
        Compare(Prefix + TEXT(".lidar.json"), Utf8Bytes(Scan->CreateLocalFilePayloadEncoder(Snapshot)()));

        FVirtualPointCloudBinaryMetadata Metadata;
        Metadata.SlabContext = Frame.SlabContext; Metadata.SensorId = Frame.SensorId; Metadata.FrameId = Frame.FrameId;
        Metadata.TimestampUtc = Frame.TimestampUtc.ToIso8601(); Metadata.ProfileKey = Snapshot->ProfileKey;
        Metadata.SourcePointCount = 2; Metadata.PointCount = Count; Metadata.ByteCount = Pcd.Num();
        Metadata.FilterRevision = 17; Metadata.ChecksumSha1 = Checksum(Pcd);
        FVirtualSensorBinaryFrame Binary;
        Binary.SensorId = Frame.SensorId; Binary.FrameId = Frame.FrameId; Binary.TimestampUtc = Frame.TimestampUtc;
        Binary.RequestId = TEXT("fixed-request"); Binary.Destination = TEXT("topic.fixed");
        Binary.StreamKind = EVirtualSensorStreamKind::PointCloud; Binary.Schema = Metadata.Schema;
        Binary.ContentType = TEXT("application/vnd.pcd"); Binary.Body32 = MakeShared<const TArray<uint8>, ESPMode::ThreadSafe>(Pcd);
        Binary.Headers = FVirtualSensorWireHeaders::RawPcd(Metadata);
        Compare(Prefix + TEXT(".pcd.headers"), WireHeader(FVirtualSensorWireHeaders::RawFrame(Binary)));
        TMap<FString,FString> EngineHeaders;
        for (const auto& P : FVirtualSensorWireHeaders::EnginePcd(Metadata, TEXT("fixed-request"))) EngineHeaders.Add(P.Key.ToString(), P.Value);
        Compare(Prefix + TEXT(".pcd.engine-headers"), WireHeader(EngineHeaders));

        for (const auto Kind : {EVirtualSensorStreamKind::LidarPayload, EVirtualSensorStreamKind::CameraImage})
        {
            UVirtualSensorStreamPublisherComponent::FStreamRuntime Runtime;
            Runtime.Config.StreamKind = Kind;
            Runtime.Config.TransportBackend = EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;
            Frame.SensorKind = Kind == EVirtualSensorStreamKind::CameraImage ? EVirtualSensorKind::Camera : EVirtualSensorKind::Lidar;
            TArray64<uint8> Jpeg = {0xff, 0xd8, 0x00, 0x12, 0xff, 0xd9}; // opaque fixed encoded input; no RHI/codec dependency
            Frame.BinaryPayload = MakeShared<const TArray64<uint8>, ESPMode::ThreadSafe>(MoveTemp(Jpeg));
            Frame.CameraJpegMetadata.Width = 1280; Frame.CameraJpegMetadata.Height = 720;
            Frame.CameraJpegMetadata.JpegQuality = 80; Frame.CameraJpegMetadata.ByteCount = 6;
            Frame.CameraJpegMetadata.ChecksumSha1 = TEXT("2b7d4870f4d397618fa51eff4e4a90027962b1a8");
            Publisher->QueueFrameForRuntime(TEXT("fixed"), Runtime, Frame);
            if (!TestEqual(TEXT("one prepared binary frame"), Runtime.PreparedMessageQueue.Num(), 1)) return false;
            const auto& Prepared = Runtime.PreparedMessageQueue[0];
            Binary.StreamKind = Kind; Binary.Schema = Prepared.BinarySchema; Binary.ContentType = Prepared.BinaryContentType;
            Binary.Headers = Prepared.BinaryHeaders; Binary.Body32 = Prepared.BinaryBody; Binary.Body64 = Prepared.BinaryBody64;
            const FString Suffix = Kind == EVirtualSensorStreamKind::CameraImage ? TEXT(".camera") : TEXT(".telemetry");
            Compare(Prefix + Suffix + TEXT(".headers"), WireHeader(FVirtualSensorWireHeaders::RawFrame(Binary)));
            if (Kind == EVirtualSensorStreamKind::LidarPayload) Compare(Prefix + TEXT(".telemetry.json"), *Prepared.BinaryBody);
        }
    }
    return true;
}
#endif
