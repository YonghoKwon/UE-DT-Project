#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Core/DTCoreSettings.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/CameraJsonLiveSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarHttpJsonLiveSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLiveSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarUdpJsonLiveSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorAdapterStubs.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorRecorderComp.h"
#include "m7at10_dt/M7AT10/WebSocket/TC/LidarJsonLiveFrameTC.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "WebSocket/TransactionCodeStruct.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Core/DxDataSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceBaseStateTest, "M7AT10.RealSensorSource.BaseState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePlaceholderStateTest, "M7AT10.RealSensorSource.PlaceholderState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePushFrameToTargetTest, "M7AT10.RealSensorSource.PushFrameToTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveBridgeTest, "M7AT10.RealSensorSource.JsonLiveBridgePushFrame", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceCameraJsonLiveBridgeTest, "M7AT10.RealSensorSource.CameraJsonLiveBridgePushFrame", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceHttpJsonLiveBridgeTest, "M7AT10.RealSensorSource.HttpJsonLiveBridgePayload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceHttpJsonLiveLoopbackPostTest, "M7AT10.RealSensorSource.HttpJsonLiveBridgeLoopbackPost", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceUdpJsonLiveBridgeTest, "M7AT10.RealSensorSource.UdpJsonLiveBridgePayload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceUdpJsonLiveDatagramTest, "M7AT10.RealSensorSource.UdpJsonLiveBridgeDatagram", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveTransactionParseTest, "M7AT10.RealSensorSource.JsonLiveTransactionParse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveTransactionRoutingTest, "M7AT10.RealSensorSource.JsonLiveTransactionRouting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveTransactionDataTableRegistrationTest, "M7AT10.Evidence.WebSocketTransactionRegistration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveDTCoreDispatchTest, "M7AT10.RealSensorSource.JsonLiveDTCoreDispatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FBrokerlessWebSocketDispatchCommand : public IAutomationLatentCommand
{
public:
    explicit FBrokerlessWebSocketDispatchCommand(FAutomationTestBase* InTest)
        : Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!Test)
        {
            return true;
        }

        FHttpModule::Get().GetHttpManager().Tick(0.0f);

        if (StartTimeSeconds <= 0.0)
        {
            StartTimeSeconds = FPlatformTime::Seconds();
        }

        UWorld* PieWorld = nullptr;
        if (GEditor)
        {
            if (FWorldContext* PieContext = GEditor->GetPIEWorldContext())
            {
                PieWorld = PieContext->World();
            }
        }

        if (!PieWorld)
        {
            return WaitOrFail(TEXT("PIE world was not created."));
        }

        UGameInstance* GameInstance = PieWorld->GetGameInstance();
        if (!GameInstance)
        {
            return WaitOrFail(TEXT("PIE GameInstance was not created."));
        }

        if (!bMessageInjected)
        {
            return InjectMessage(PieWorld, GameInstance);
        }

        if (DataSubsystem.IsValid())
        {
            DataSubsystem->Tick(0.0f);
        }

        if (TargetLidar.IsValid() && JsonLiveSource.IsValid())
        {
            const int32 TargetPointCount = TargetLidar->GetLastPoints().Num();
            if (TargetPointCount == ExpectedPointCount && !TargetLidar->GetLastJsonPayload().IsEmpty())
            {
                Test->TestEqual(TEXT("brokerless dispatch source frame id"), JsonLiveSource->LastSourceFrameId, static_cast<int64>(1));
                Test->TestEqual(TEXT("brokerless dispatch source point count"), JsonLiveSource->LastSourcePointCount, ExpectedPointCount);
                Test->TestEqual(TEXT("brokerless dispatch target point count"), TargetPointCount, ExpectedPointCount);
                Test->TestEqual(TEXT("brokerless dispatch buffer clears after push"), JsonLiveSource->PendingLineCount, 0);
                Test->TestFalse(TEXT("brokerless dispatch updates cached server payload"), TargetLidar->GetLastJsonPayload().IsEmpty());
                Cleanup();
                return true;
            }
        }

        return WaitOrFail(TEXT("brokerless dispatch did not reach the target LiDAR before timeout."));
    }

private:
    bool InjectMessage(UWorld* PieWorld, UGameInstance* GameInstance)
    {
        UDxDataSubsystem* DxDataSubsystem = GameInstance->GetSubsystem<UDxDataSubsystem>();
        Test->TestNotNull(TEXT("PIE DTCore data subsystem"), DxDataSubsystem);
        if (!DxDataSubsystem)
        {
            return true;
        }
        DataSubsystem = DxDataSubsystem;

        Owner = PieWorld->SpawnActor<AActor>();
        Test->TestNotNull(TEXT("brokerless dispatch owner"), Owner.Get());
        if (!Owner.IsValid())
        {
            return true;
        }

        UVirtualLidarSensorComp* Target = NewObject<UVirtualLidarSensorComp>(Owner.Get());
        ULidarJsonLiveSourceComp* Source = NewObject<ULidarJsonLiveSourceComp>(Owner.Get());
        Test->TestNotNull(TEXT("brokerless dispatch target LiDAR"), Target);
        Test->TestNotNull(TEXT("brokerless dispatch JSON live source"), Source);
        if (!Target || !Source)
        {
            Cleanup();
            return true;
        }

        Owner->AddInstanceComponent(Target);
        Owner->AddInstanceComponent(Source);
        Target->bAutoStartScan = false;
        Target->bAutoRegisterToManager = false;
        Source->bAutoStartSource = false;
        Source->bSendTransportByDefault = false;
        Target->RegisterComponent();
        Source->RegisterComponent();

        Target->SensorId = TEXT("TEST-LIDAR-BROKERLESS-DISPATCH");
        Source->SourceId = TEXT("JsonLiveLidarBridge");
        Source->TargetLidar = Target;

        TargetLidar = Target;
        JsonLiveSource = Source;

        FString SamplePayload;
        const FString SamplePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/websocket/lidar_json_live_frame_sample.json"));
        Test->TestTrue(TEXT("brokerless dispatch sample payload loads"), FFileHelper::LoadFileToString(SamplePayload, *SamplePath));
        if (SamplePayload.IsEmpty())
        {
            Cleanup();
            return true;
        }

        DxDataSubsystem->EnqueueWebSocketData(SamplePayload);
        DxDataSubsystem->Tick(0.0f);

        bMessageInjected = true;
        return false;
    }

    bool WaitOrFail(const TCHAR* TimeoutMessage)
    {
        if ((FPlatformTime::Seconds() - StartTimeSeconds) <= TimeoutSeconds)
        {
            return false;
        }

        Test->AddError(TimeoutMessage);
        Cleanup();
        return true;
    }

    void Cleanup()
    {
        if (Owner.IsValid())
        {
            Owner->Destroy();
        }
        Owner.Reset();
        TargetLidar.Reset();
        JsonLiveSource.Reset();
        DataSubsystem.Reset();
    }

private:
    FAutomationTestBase* Test = nullptr;
    double StartTimeSeconds = 0.0;
    bool bMessageInjected = false;
    static constexpr double TimeoutSeconds = 8.0;
    static constexpr int32 ExpectedPointCount = 3;
    TWeakObjectPtr<AActor> Owner;
    TWeakObjectPtr<UVirtualLidarSensorComp> TargetLidar;
    TWeakObjectPtr<ULidarJsonLiveSourceComp> JsonLiveSource;
    TWeakObjectPtr<UDxDataSubsystem> DataSubsystem;
};
#endif

class FHttpJsonLiveLoopbackPostCommand : public IAutomationLatentCommand
{
public:
    explicit FHttpJsonLiveLoopbackPostCommand(FAutomationTestBase* InTest)
        : Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!Test)
        {
            return true;
        }

        if (StartTimeSeconds <= 0.0)
        {
            StartTimeSeconds = FPlatformTime::Seconds();
            return Setup();
        }

        if (!bRequestSent && (FPlatformTime::Seconds() - StartTimeSeconds) >= SendDelaySeconds)
        {
            return SendPost();
        }

        if (bRequestComplete && TargetLidar.IsValid() && HttpSource.IsValid())
        {
            const int32 TargetPointCount = TargetLidar->GetLastPoints().Num();
            if (bRequestSucceeded && ResponseCode == 202 && TargetPointCount == ExpectedPointCount)
            {
                Test->TestEqual(TEXT("HTTP loopback POST response code"), ResponseCode, 202);
                Test->TestTrue(TEXT("HTTP loopback POST response body accepted"), ResponseBody.Contains(TEXT("\"accepted\":true")));
                Test->TestTrue(TEXT("HTTP loopback POST response body point count"), ResponseBody.Contains(TEXT("\"pointCount\":2")));
                Test->TestTrue(TEXT("HTTP loopback POST response body source id"), ResponseBody.Contains(TEXT("HttpJsonLiveLidarBridge")));
                Test->TestEqual(TEXT("HTTP loopback source response code"), HttpSource->LastResponseCode, 202);
                Test->TestEqual(TEXT("HTTP loopback source frame id"), HttpSource->LastSourceFrameId, static_cast<int64>(1));
                Test->TestEqual(TEXT("HTTP loopback source point count"), HttpSource->LastSourcePointCount, ExpectedPointCount);
                Test->TestEqual(TEXT("HTTP loopback target point count"), TargetPointCount, ExpectedPointCount);
                Test->TestTrue(TEXT("HTTP loopback request bytes recorded"), HttpSource->LastReceivedRequestBytes > 0);
                Test->TestEqual(TEXT("HTTP loopback request byte count"), HttpSource->LastReceivedRequestBytes, RequestBodyUtf8Bytes);
                Test->TestTrue(TEXT("HTTP loopback processed on game thread"), HttpSource->bLastRequestProcessedOnGameThread);
                Test->TestEqual(TEXT("HTTP loopback buffer clears after push"), HttpSource->PendingLineCount, 0);
                Cleanup();
                return true;
            }

            Test->AddError(FString::Printf(TEXT("HTTP loopback POST completed unexpectedly. succeeded=%s code=%d targetPoints=%d body=%s"),
                bRequestSucceeded ? TEXT("true") : TEXT("false"),
                ResponseCode,
                TargetPointCount,
                *ResponseBody));
            Cleanup();
            return true;
        }

        if ((FPlatformTime::Seconds() - StartTimeSeconds) > TimeoutSeconds)
        {
            Test->AddError(TEXT("HTTP JSON live loopback POST did not complete before timeout."));
            Cleanup();
            return true;
        }

        return false;
    }

private:
    bool Setup()
    {
        UWorld* World = GWorld;
        Test->TestNotNull(TEXT("HTTP loopback editor world"), World);
        if (!World)
        {
            return true;
        }

        Owner = World->SpawnActor<AActor>();
        Test->TestNotNull(TEXT("HTTP loopback source owner"), Owner.Get());
        if (!Owner.IsValid())
        {
            return true;
        }

        UVirtualLidarSensorComp* Target = NewObject<UVirtualLidarSensorComp>(Owner.Get());
        ULidarHttpJsonLiveSourceComp* Source = NewObject<ULidarHttpJsonLiveSourceComp>(Owner.Get());
        Test->TestNotNull(TEXT("HTTP loopback target LiDAR"), Target);
        Test->TestNotNull(TEXT("HTTP loopback source"), Source);
        if (!Target || !Source)
        {
            Cleanup();
            return true;
        }

        Owner->AddInstanceComponent(Target);
        Owner->AddInstanceComponent(Source);
        Target->bAutoStartScan = false;
        Target->bAutoRegisterToManager = false;
        Target->RegisterComponent();
        Source->RegisterComponent();

        const int32 FreePort = FindFreeLoopbackTcpPort();
        Test->TestTrue(TEXT("HTTP loopback found a free TCP port"), FreePort > 0);
        if (FreePort <= 0)
        {
            Cleanup();
            return true;
        }

        const FGuid HttpRouteGuid = FGuid::NewGuid();
        Target->SensorId = TEXT("TEST-LIDAR-HTTP-LOOPBACK");
        Source->TargetLidar = Target;
        Source->ListenPort = FreePort;
        Source->RoutePath = FString::Printf(TEXT("/m7at10/loopback/%s"), *HttpRouteGuid.ToString(EGuidFormats::Digits));
        Source->bAutoPushReceivedFrame = true;
        Source->bSendTransportForReceivedFrames = false;
        Test->TestTrue(TEXT("HTTP loopback source starts"), Source->StartSource());
        Test->TestTrue(TEXT("HTTP loopback route is bound"), Source->IsHttpRouteBound());
        Test->TestTrue(TEXT("HTTP loopback source accepts requests"), Source->IsAcceptingHttpRequests());
        if (!Source->IsHttpRouteBound())
        {
            Cleanup();
            return true;
        }

        TargetLidar = Target;
        HttpSource = Source;
        RequestUrl = FString::Printf(TEXT("http://127.0.0.1:%d%s"), Source->ListenPort, *Source->RoutePath);
        return false;
    }

    int32 FindFreeLoopbackTcpPort() const
    {
        FSocket* ProbeSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("M7AT10_HttpJsonLiveProbe"), false);
        if (!ProbeSocket)
        {
            return 0;
        }

        int32 FreePort = 0;
        TSharedRef<FInternetAddr> Address = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
        Address->SetIp(FIPv4Address(127, 0, 0, 1).Value);
        Address->SetPort(0);

        if (ProbeSocket->Bind(*Address))
        {
            TSharedRef<FInternetAddr> BoundAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
            ProbeSocket->GetAddress(*BoundAddress);
            FreePort = BoundAddress->GetPort();
        }

        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ProbeSocket);
        return FreePort;
    }

    bool SendPost()
    {
        if (!HttpSource.IsValid())
        {
            return true;
        }

        RequestBody = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"HttpJsonLiveLidarBridge\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":true,\"POINTS\":[{\"row\":0,\"col\":0,\"x\":10,\"y\":20,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"},{\"row\":0,\"col\":1,\"x\":30,\"y\":40,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}");
        RequestBodyUtf8Bytes = FTCHARToUTF8(*RequestBody).Length();

        HttpRequest = FHttpModule::Get().CreateRequest();
        HttpRequest->SetURL(RequestUrl);
        HttpRequest->SetVerb(TEXT("POST"));
        HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        HttpRequest->SetTimeout(TimeoutSeconds);
        HttpRequest->SetContentAsString(RequestBody);
        HttpRequest->OnProcessRequestComplete().BindRaw(this, &FHttpJsonLiveLoopbackPostCommand::OnRequestComplete);
        bRequestSent = true;
        Test->TestTrue(TEXT("HTTP loopback POST request starts"), HttpRequest->ProcessRequest());
        return false;
    }

    void OnRequestComplete(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bSucceeded)
    {
        bRequestSucceeded = bSucceeded && Response.IsValid();
        ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        ResponseBody = Response.IsValid() ? Response->GetContentAsString() : FString();
        bRequestComplete = true;
    }

    void Cleanup()
    {
        if (HttpRequest.IsValid())
        {
            HttpRequest->OnProcessRequestComplete().Unbind();
            HttpRequest.Reset();
        }
        if (HttpSource.IsValid())
        {
            HttpSource->StopSource();
        }
        if (Owner.IsValid())
        {
            Owner->Destroy();
        }
        HttpSource.Reset();
        TargetLidar.Reset();
        Owner.Reset();
    }

private:
    FAutomationTestBase* Test = nullptr;
    double StartTimeSeconds = 0.0;
    bool bRequestSent = false;
    bool bRequestComplete = false;
    bool bRequestSucceeded = false;
    int32 ResponseCode = 0;
    int32 RequestBodyUtf8Bytes = 0;
    FString RequestUrl;
    FString RequestBody;
    FString ResponseBody;
    static constexpr double SendDelaySeconds = 0.2;
    static constexpr float TimeoutSeconds = 8.0f;
    static constexpr int32 ExpectedPointCount = 2;
    TWeakObjectPtr<AActor> Owner;
    TWeakObjectPtr<UVirtualLidarSensorComp> TargetLidar;
    TWeakObjectPtr<ULidarHttpJsonLiveSourceComp> HttpSource;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
};

class FUdpJsonLiveDatagramCommand : public IAutomationLatentCommand
{
public:
    explicit FUdpJsonLiveDatagramCommand(FAutomationTestBase* InTest)
        : Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!Test)
        {
            return true;
        }

        if (StartTimeSeconds <= 0.0)
        {
            StartTimeSeconds = FPlatformTime::Seconds();
            return SetupAndSend();
        }

        if (TargetLidar.IsValid() && UdpSource.IsValid() && TargetLidar->GetLastPoints().Num() == 2)
        {
            Test->TestEqual(TEXT("UDP datagram source frame id"), UdpSource->LastSourceFrameId, static_cast<int64>(1));
            Test->TestEqual(TEXT("UDP datagram source point count"), UdpSource->LastSourcePointCount, 2);
            Test->TestTrue(TEXT("UDP datagram byte count recorded"), UdpSource->LastReceivedDatagramBytes > 0);
            Test->TestFalse(TEXT("UDP datagram endpoint recorded"), UdpSource->LastReceivedEndpoint.IsEmpty());
            Cleanup();
            return true;
        }

        if ((FPlatformTime::Seconds() - StartTimeSeconds) > TimeoutSeconds)
        {
            Test->AddError(TEXT("UDP JSON live datagram was not received before timeout."));
            Cleanup();
            return true;
        }

        return false;
    }

private:
    bool SetupAndSend()
    {
        UWorld* World = GWorld;
        Test->TestNotNull(TEXT("editor world"), World);
        if (!World)
        {
            return true;
        }

        Owner = World->SpawnActor<AActor>();
        Test->TestNotNull(TEXT("UDP datagram source owner"), Owner.Get());
        if (!Owner.IsValid())
        {
            return true;
        }

        UVirtualLidarSensorComp* Target = NewObject<UVirtualLidarSensorComp>(Owner.Get());
        ULidarUdpJsonLiveSourceComp* Source = NewObject<ULidarUdpJsonLiveSourceComp>(Owner.Get());
        Test->TestNotNull(TEXT("UDP datagram target LiDAR"), Target);
        Test->TestNotNull(TEXT("UDP datagram source"), Source);
        if (!Target || !Source)
        {
            Cleanup();
            return true;
        }

        Owner->AddInstanceComponent(Target);
        Owner->AddInstanceComponent(Source);
        Target->bAutoStartScan = false;
        Target->bAutoRegisterToManager = false;
        Target->RegisterComponent();
        Source->RegisterComponent();

        Target->SensorId = TEXT("TEST-LIDAR-UDP-DATAGRAM");
        Source->TargetLidar = Target;
        Source->BindAddress = TEXT("127.0.0.1");
        Source->BindPort = 0;
        Source->bAutoPushReceivedFrame = true;
        Source->bSendTransportForReceivedFrames = false;
        Test->TestTrue(TEXT("UDP datagram source starts"), Source->StartSource());
        Test->TestTrue(TEXT("UDP datagram source has bound port"), Source->GetBoundPort() > 0);
        if (Source->GetBoundPort() <= 0)
        {
            Cleanup();
            return true;
        }

        TargetLidar = Target;
        UdpSource = Source;

        FSocket* Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_DGram, TEXT("M7AT10_UdpJsonLiveSmokeSender"), false);
        Test->TestNotNull(TEXT("UDP datagram sender socket"), Sender);
        if (!Sender)
        {
            Cleanup();
            return true;
        }

        TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
        RemoteAddress->SetIp(FIPv4Address(127, 0, 0, 1).Value);
        RemoteAddress->SetPort(Source->GetBoundPort());

        const FString Payload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"UdpJsonLiveLidarBridge\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":true,\"POINTS\":[{\"row\":0,\"col\":0,\"x\":10,\"y\":20,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"},{\"row\":0,\"col\":1,\"x\":30,\"y\":40,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}");
        FTCHARToUTF8 PayloadUtf8(*Payload);
        int32 BytesSent = 0;
        Test->TestTrue(TEXT("UDP datagram payload sends"), Sender->SendTo(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length(), BytesSent, *RemoteAddress));
        Test->TestTrue(TEXT("UDP datagram sends bytes"), BytesSent > 0);
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Sender);
        return false;
    }

    void Cleanup()
    {
        if (UdpSource.IsValid())
        {
            UdpSource->StopSource();
        }
        if (Owner.IsValid())
        {
            Owner->Destroy();
        }
        UdpSource.Reset();
        TargetLidar.Reset();
        Owner.Reset();
    }

private:
    FAutomationTestBase* Test = nullptr;
    double StartTimeSeconds = 0.0;
    static constexpr double TimeoutSeconds = 5.0;
    TWeakObjectPtr<AActor> Owner;
    TWeakObjectPtr<UVirtualLidarSensorComp> TargetLidar;
    TWeakObjectPtr<ULidarUdpJsonLiveSourceComp> UdpSource;
};

bool FRealSensorSourceBaseStateTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* CsvReplay = NewObject<ULidarCsvReplaySourceComp>();
    ULidarJsonLinesReplaySourceComp* JsonReplay = NewObject<ULidarJsonLinesReplaySourceComp>();
    ULidarJsonLiveSourceComp* JsonLive = NewObject<ULidarJsonLiveSourceComp>();
    TestNotNull(TEXT("CSV replay source"), CsvReplay);
    TestNotNull(TEXT("JSONL replay source"), JsonReplay);
    TestNotNull(TEXT("JSON live source"), JsonLive);

    URealSensorSourceComp* CsvAsSource = Cast<URealSensorSourceComp>(CsvReplay);
    URealSensorSourceComp* JsonAsSource = Cast<URealSensorSourceComp>(JsonReplay);
    URealSensorSourceComp* JsonLiveAsSource = Cast<URealSensorSourceComp>(JsonLive);
    TestNotNull(TEXT("CSV replay inherits real sensor source"), CsvAsSource);
    TestNotNull(TEXT("JSONL replay inherits real sensor source"), JsonAsSource);
    TestNotNull(TEXT("JSON live inherits real sensor source"), JsonLiveAsSource);
    TestEqual(TEXT("CSV replay source kind"), CsvAsSource->SourceKind, ERealSensorSourceKind::FileReplay);
    TestEqual(TEXT("JSONL replay source kind"), JsonAsSource->SourceKind, ERealSensorSourceKind::FileReplay);
    TestEqual(TEXT("JSON live source kind"), JsonLiveAsSource->SourceKind, ERealSensorSourceKind::JsonLiveBridge);
    TestEqual(TEXT("CSV replay starts stopped"), CsvAsSource->GetConnectionState(), ERealSensorSourceConnectionState::Stopped);
    TestEqual(TEXT("JSONL replay starts stopped"), JsonAsSource->GetConnectionState(), ERealSensorSourceConnectionState::Stopped);
    TestEqual(TEXT("JSON live starts stopped"), JsonLiveAsSource->GetConnectionState(), ERealSensorSourceConnectionState::Stopped);
    return true;
}

bool FRealSensorSourcePlaceholderStateTest::RunTest(const FString& Parameters)
{
    TArray<URealSensorSourceComp*> Sources;
    Sources.Add(NewObject<URos2SensorBridgeSourceComp>());
    Sources.Add(NewObject<ULivoxLidarSourceComp>());
    Sources.Add(NewObject<URealSenseCameraSourceComp>());

    for (URealSensorSourceComp* Source : Sources)
    {
        TestNotNull(TEXT("placeholder source"), Source);
        TestFalse(FString::Printf(TEXT("%s StartSource fails until implemented"), *Source->SourceId), Source->StartSource());
        TestEqual(FString::Printf(TEXT("%s reports error state after StartSource"), *Source->SourceId), Source->GetConnectionState(), ERealSensorSourceConnectionState::Error);
        TestFalse(FString::Printf(TEXT("%s PushFrameOnce fails until implemented"), *Source->SourceId), Source->PushFrameOnce(false));
        TestEqual(FString::Printf(TEXT("%s remains error state after PushFrameOnce"), *Source->SourceId), Source->GetConnectionState(), ERealSensorSourceConnectionState::Error);
        TestFalse(FString::Printf(TEXT("%s is not running"), *Source->SourceId), Source->IsSourceRunning());
        TestFalse(FString::Printf(TEXT("%s has status message"), *Source->SourceId), Source->GetLastSourceMessage().IsEmpty());
    }

    return true;
}

bool FRealSensorSourcePushFrameToTargetTest::RunTest(const FString& Parameters)
{
    UVirtualLidarSensorComp* CsvTargetLidar = NewObject<UVirtualLidarSensorComp>();
    ULidarCsvReplaySourceComp* CsvReplay = NewObject<ULidarCsvReplaySourceComp>();
    TestNotNull(TEXT("CSV target lidar"), CsvTargetLidar);
    TestNotNull(TEXT("CSV replay source"), CsvReplay);
    if (!CsvTargetLidar || !CsvReplay)
    {
        return false;
    }

    CsvReplay->TargetLidar = CsvTargetLidar;
    CsvReplay->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    CsvReplay->ReplaySemanticLabel = TEXT("Slab");
    CsvReplay->bUpdateLidarDimensionsFromCsv = true;

    TestTrue(TEXT("CSV replay pushes frame through base source helper"), CsvReplay->PushFrameOnce(false));
    TestEqual(TEXT("CSV source frame id increments"), CsvReplay->LastSourceFrameId, static_cast<int64>(1));
    TestEqual(TEXT("CSV source point count"), CsvReplay->LastSourcePointCount, 24);
    TestEqual(TEXT("CSV source state running"), CsvReplay->GetConnectionState(), ERealSensorSourceConnectionState::Running);
    TestEqual(TEXT("CSV target received points"), CsvTargetLidar->GetLastPoints().Num(), 24);
    TestEqual(TEXT("CSV target vertical channels updated"), CsvTargetLidar->VerticalChannels, 4);
    TestEqual(TEXT("CSV target horizontal samples updated"), CsvTargetLidar->HorizontalSamples, 6);
    TestEqual(TEXT("CSV target runtime count"), CsvTargetLidar->GetRuntimeStatus().TotalPointCount, 24);

    UVirtualLidarSensorComp* JsonTargetLidar = NewObject<UVirtualLidarSensorComp>();
    ULidarJsonLinesReplaySourceComp* JsonReplay = NewObject<ULidarJsonLinesReplaySourceComp>();
    TestNotNull(TEXT("JSONL target lidar"), JsonTargetLidar);
    TestNotNull(TEXT("JSONL replay source"), JsonReplay);
    if (!JsonTargetLidar || !JsonReplay)
    {
        return false;
    }

    JsonReplay->TargetLidar = JsonTargetLidar;
    JsonReplay->JsonLinesFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.jsonl"));

    TestTrue(TEXT("JSONL replay pushes frame through base source helper"), JsonReplay->PushFrameOnce(false));
    TestEqual(TEXT("JSONL source frame id increments"), JsonReplay->LastSourceFrameId, static_cast<int64>(1));
    TestEqual(TEXT("JSONL source point count"), JsonReplay->LastSourcePointCount, 18);
    TestEqual(TEXT("JSONL source state running"), JsonReplay->GetConnectionState(), ERealSensorSourceConnectionState::Running);
    TestEqual(TEXT("JSONL target received points"), JsonTargetLidar->GetLastPoints().Num(), 18);
    TestEqual(TEXT("JSONL target runtime count"), JsonTargetLidar->GetRuntimeStatus().TotalPointCount, 18);
    TestFalse(TEXT("JSONL target has cached payload"), JsonTargetLidar->GetLastJsonPayload().IsEmpty());

    return true;
}

bool FRealSensorSourceJsonLiveBridgeTest::RunTest(const FString& Parameters)
{
    UVirtualLidarSensorComp* TargetLidar = NewObject<UVirtualLidarSensorComp>();
    ULidarJsonLiveSourceComp* JsonLive = NewObject<ULidarJsonLiveSourceComp>();
    TestNotNull(TEXT("JSON live target lidar"), TargetLidar);
    TestNotNull(TEXT("JSON live source"), JsonLive);
    if (!TargetLidar || !JsonLive)
    {
        return false;
    }

    TargetLidar->SensorId = TEXT("TEST-LIDAR-JSON-LIVE");
    TargetLidar->HorizontalSamples = 2;
    TargetLidar->VerticalChannels = 1;
    JsonLive->TargetLidar = TargetLidar;
    JsonLive->DefaultSemanticLabel = TEXT("Unknown");

    TestTrue(TEXT("JSON live source starts"), JsonLive->StartSource());
    TestEqual(TEXT("JSON live source running"), JsonLive->GetConnectionState(), ERealSensorSourceConnectionState::Running);

    JsonLive->AppendJsonLine(TEXT("{\"row\":2,\"col\":5,\"returnIndex\":1,\"x\":100,\"y\":10,\"z\":0,\"distance\":100.5,\"hit\":true,\"semanticLabel\":\"Slab\",\"tags\":\"Slab|Live\"}"));
    JsonLive->AppendJsonLine(TEXT("{\"row\":0,\"col\":1,\"gridCoordValid\":false,\"gridCoordSource\":\"derived_from_point_index\",\"x\":120,\"y\":20,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}"));
    TestEqual(TEXT("JSON live pending line count"), JsonLive->PendingLineCount, 2);

    TArray<FVirtualLidarPoint> Points;
    TestTrue(TEXT("JSON live builds buffered frame"), JsonLive->BuildFrameFromBufferedLines(Points));
    TestEqual(TEXT("JSON live built point count"), Points.Num(), 2);
    if (Points.Num() >= 2)
    {
        TestTrue(TEXT("first point preserves grid coord"), Points[0].bHasGridCoord);
        TestEqual(TEXT("first point row"), Points[0].Row, 2);
        TestEqual(TEXT("first point col"), Points[0].Col, 5);
        TestEqual(TEXT("first point return index"), Points[0].ReturnIndex, 1);
        TestFalse(TEXT("second point keeps fallback marker invalid"), Points[1].bHasGridCoord);
    }

    TestTrue(TEXT("JSON live pushes frame through base source helper"), JsonLive->PushFrameOnce(false));
    TestEqual(TEXT("JSON live source frame id increments"), JsonLive->LastSourceFrameId, static_cast<int64>(1));
    TestEqual(TEXT("JSON live source point count"), JsonLive->LastSourcePointCount, 2);
    TestEqual(TEXT("JSON live source state running"), JsonLive->GetConnectionState(), ERealSensorSourceConnectionState::Running);
    TestEqual(TEXT("JSON live target received points"), TargetLidar->GetLastPoints().Num(), 2);
    TestEqual(TEXT("JSON live buffer clears after push"), JsonLive->PendingLineCount, 0);
    TestFalse(TEXT("JSON live target has cached payload"), TargetLidar->GetLastJsonPayload().IsEmpty());
    TestTrue(TEXT("payload includes grid coord source field"), TargetLidar->GetLastJsonPayload().Contains(TEXT("gridCoordSource")));
    TestTrue(TEXT("payload includes preserved grid coord source"), TargetLidar->GetLastJsonPayload().Contains(TEXT("point_metadata")));
    TestTrue(TEXT("payload includes fallback grid coord source"), TargetLidar->GetLastJsonPayload().Contains(TEXT("derived_from_point_index")));

    const FString WebSocketPayload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"JsonLiveLidarBridge\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":false,\"POINTS\":[{\"row\":3,\"col\":4,\"returnIndex\":0,\"x\":140,\"y\":30,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"},{\"row\":3,\"col\":5,\"returnIndex\":0,\"x\":150,\"y\":35,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}");
    TestTrue(TEXT("JSON live appends WebSocket-shaped payload"), JsonLive->AppendWebSocketPayload(WebSocketPayload));
    TestEqual(TEXT("JSON live WebSocket payload pending line count"), JsonLive->PendingLineCount, 2);
    TArray<FVirtualLidarPoint> WebSocketPoints;
    TestTrue(TEXT("JSON live builds WebSocket payload frame"), JsonLive->BuildFrameFromBufferedLines(WebSocketPoints));
    TestEqual(TEXT("JSON live WebSocket payload point count"), WebSocketPoints.Num(), 2);
    if (WebSocketPoints.Num() >= 1)
    {
        TestEqual(TEXT("JSON live WebSocket payload first row"), WebSocketPoints[0].Row, 3);
        TestEqual(TEXT("JSON live WebSocket payload first col"), WebSocketPoints[0].Col, 4);
    }
    TestTrue(TEXT("JSON live pushes WebSocket payload without transport"), JsonLive->PushFrameOnce(false));
    TestEqual(TEXT("JSON live WebSocket payload target count"), TargetLidar->GetLastPoints().Num(), 2);
    TestEqual(TEXT("JSON live WebSocket payload buffer clears after push"), JsonLive->PendingLineCount, 0);
    TestFalse(TEXT("JSON live WebSocket payload caches target payload"), TargetLidar->GetLastJsonPayload().IsEmpty());

    TestTrue(TEXT("JSON live appends generic live payload alias"), JsonLive->AppendLivePayloadJson(WebSocketPayload));
    TestEqual(TEXT("JSON live generic live payload pending line count"), JsonLive->PendingLineCount, 2);
    TestTrue(TEXT("JSON live pushes generic live payload without transport"), JsonLive->PushFrameOnce(false));
    TestEqual(TEXT("JSON live generic live payload target count"), TargetLidar->GetLastPoints().Num(), 2);

    ULidarJsonLiveSourceComp* EmptyLive = NewObject<ULidarJsonLiveSourceComp>();
    TestNotNull(TEXT("empty JSON live source"), EmptyLive);
    TestFalse(TEXT("empty JSON live push fails"), EmptyLive->PushFrameOnce(false));
    TestEqual(TEXT("empty JSON live source reports error"), EmptyLive->GetConnectionState(), ERealSensorSourceConnectionState::Error);
    TestTrue(TEXT("empty JSON live source reports empty buffer"), EmptyLive->GetLastSourceMessage().Contains(TEXT("empty")));

    ULidarJsonLiveSourceComp* InvalidLive = NewObject<ULidarJsonLiveSourceComp>();
    TestNotNull(TEXT("invalid JSON live source"), InvalidLive);
    InvalidLive->AppendJsonLine(TEXT("{\"not\":\"a point\"}"));
    InvalidLive->AppendJsonLine(TEXT("not json"));
    TestFalse(TEXT("invalid JSON live frame fails"), InvalidLive->PushFrameOnce(false));
    TestEqual(TEXT("invalid JSON live dropped line count"), InvalidLive->LastDroppedLineCount, 2);
    TestEqual(TEXT("invalid JSON live buffer clears after unusable frame"), InvalidLive->PendingLineCount, 0);
    InvalidLive->ClearBufferedFrame();
    TestEqual(TEXT("clear keeps last dropped diagnostic"), InvalidLive->LastDroppedLineCount, 2);

    ULidarJsonLiveSourceComp* MissingTargetLive = NewObject<ULidarJsonLiveSourceComp>();
    TestNotNull(TEXT("missing target JSON live source"), MissingTargetLive);
    MissingTargetLive->AppendJsonLine(TEXT("{\"x\":100,\"y\":10,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}"));
    TestFalse(TEXT("missing target push fails"), MissingTargetLive->PushFrameOnce(false));
    TestEqual(TEXT("missing target reports error"), MissingTargetLive->GetConnectionState(), ERealSensorSourceConnectionState::Error);
    TestEqual(TEXT("missing target preserves buffered line"), MissingTargetLive->PendingLineCount, 1);

    return true;
}

bool FRealSensorSourceUdpJsonLiveBridgeTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    AActor* SourceOwner = World->SpawnActor<AActor>();
    TestNotNull(TEXT("UDP JSON live source owner"), SourceOwner);
    if (!SourceOwner)
    {
        return false;
    }

    UVirtualLidarSensorComp* TargetLidar = NewObject<UVirtualLidarSensorComp>(SourceOwner);
    ULidarUdpJsonLiveSourceComp* UdpSource = NewObject<ULidarUdpJsonLiveSourceComp>(SourceOwner);
    TestNotNull(TEXT("UDP JSON live target LiDAR"), TargetLidar);
    TestNotNull(TEXT("UDP JSON live source"), UdpSource);
    if (!TargetLidar || !UdpSource)
    {
        SourceOwner->Destroy();
        return false;
    }

    SourceOwner->AddInstanceComponent(TargetLidar);
    SourceOwner->AddInstanceComponent(UdpSource);
    TargetLidar->bAutoStartScan = false;
    TargetLidar->bAutoRegisterToManager = false;
    TargetLidar->RegisterComponent();
    UdpSource->RegisterComponent();

    TargetLidar->SensorId = TEXT("TEST-LIDAR-UDP-JSON-LIVE");
    UdpSource->TargetLidar = TargetLidar;
    UdpSource->bAutoPushReceivedFrame = true;
    UdpSource->bSendTransportForReceivedFrames = false;

    const FString Payload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"UdpJsonLiveLidarBridge\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":true,\"POINTS\":[{\"row\":0,\"col\":0,\"x\":10,\"y\":20,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"},{\"row\":0,\"col\":1,\"x\":30,\"y\":40,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}");
    TestTrue(TEXT("UDP JSON live source processes shared payload"), UdpSource->ProcessUdpPayloadJson(Payload));
    TestEqual(TEXT("UDP JSON live source frame id"), UdpSource->LastSourceFrameId, static_cast<int64>(1));
    TestEqual(TEXT("UDP JSON live source point count"), UdpSource->LastSourcePointCount, 2);
    TestEqual(TEXT("UDP JSON live target point count"), TargetLidar->GetLastPoints().Num(), 2);
    TestEqual(TEXT("UDP JSON live buffer clears after push"), UdpSource->PendingLineCount, 0);

    TestFalse(TEXT("UDP JSON live source rejects invalid payload"), UdpSource->ProcessUdpPayloadJson(TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"POINTS\":[]}}")));

    SourceOwner->Destroy();
    return true;
}

bool FRealSensorSourceHttpJsonLiveBridgeTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    AActor* SourceOwner = World->SpawnActor<AActor>();
    TestNotNull(TEXT("HTTP JSON live source owner"), SourceOwner);
    if (!SourceOwner)
    {
        return false;
    }

    UVirtualLidarSensorComp* TargetLidar = NewObject<UVirtualLidarSensorComp>(SourceOwner);
    ULidarHttpJsonLiveSourceComp* HttpSource = NewObject<ULidarHttpJsonLiveSourceComp>(SourceOwner);
    TestNotNull(TEXT("HTTP JSON live target LiDAR"), TargetLidar);
    TestNotNull(TEXT("HTTP JSON live source"), HttpSource);
    if (!TargetLidar || !HttpSource)
    {
        SourceOwner->Destroy();
        return false;
    }

    SourceOwner->AddInstanceComponent(TargetLidar);
    SourceOwner->AddInstanceComponent(HttpSource);
    TargetLidar->bAutoStartScan = false;
    TargetLidar->bAutoRegisterToManager = false;
    TargetLidar->RegisterComponent();
    HttpSource->RegisterComponent();

    TargetLidar->SensorId = TEXT("TEST-LIDAR-HTTP-JSON-LIVE");
    HttpSource->TargetLidar = TargetLidar;
    HttpSource->bAutoPushReceivedFrame = true;
    HttpSource->bSendTransportForReceivedFrames = false;

    const FString Payload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"HttpJsonLiveLidarBridge\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":true,\"POINTS\":[{\"row\":0,\"col\":0,\"x\":10,\"y\":20,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"},{\"row\":0,\"col\":1,\"x\":30,\"y\":40,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}");
    TestTrue(TEXT("HTTP JSON live source processes shared payload"), HttpSource->ProcessHttpPayloadJson(Payload));
    TestEqual(TEXT("HTTP JSON live source frame id"), HttpSource->LastSourceFrameId, static_cast<int64>(1));
    TestEqual(TEXT("HTTP JSON live source point count"), HttpSource->LastSourcePointCount, 2);
    TestEqual(TEXT("HTTP JSON live target point count"), TargetLidar->GetLastPoints().Num(), 2);
    TestEqual(TEXT("HTTP JSON live response code accepted"), HttpSource->LastResponseCode, 202);
    TestTrue(TEXT("HTTP JSON live payload processed on game thread"), HttpSource->bLastRequestProcessedOnGameThread);
    TestEqual(TEXT("HTTP JSON live buffer clears after push"), HttpSource->PendingLineCount, 0);

    HttpSource->bAutoPushReceivedFrame = false;
    TestTrue(TEXT("HTTP JSON live source can buffer without pushing"), HttpSource->ProcessHttpPayloadJson(Payload));
    TestEqual(TEXT("HTTP JSON live buffered line count"), HttpSource->PendingLineCount, 2);
    TestEqual(TEXT("HTTP JSON live response code buffered"), HttpSource->LastResponseCode, 202);
    TestTrue(TEXT("HTTP JSON live buffer-only path processed on game thread"), HttpSource->bLastRequestProcessedOnGameThread);
    HttpSource->ClearBufferedFrame();

    TestFalse(TEXT("HTTP JSON live source rejects invalid payload"), HttpSource->ProcessHttpPayloadJson(TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"POINTS\":[]}}")));
    TestEqual(TEXT("HTTP JSON live invalid payload response code"), HttpSource->LastResponseCode, 400);

    const FGuid HttpRouteGuid = FGuid::NewGuid();
    HttpSource->ListenPort = 20000 + static_cast<int32>(GetTypeHash(HttpRouteGuid) % 20000);
    HttpSource->RoutePath = FString::Printf(TEXT("/m7at10/test/%s"), *HttpRouteGuid.ToString(EGuidFormats::Digits));
    TestTrue(TEXT("HTTP JSON live StartSource binds a route"), HttpSource->StartSource());
    TestTrue(TEXT("HTTP JSON live route is bound"), HttpSource->IsHttpRouteBound());
    TestTrue(TEXT("HTTP JSON live accepts requests while running"), HttpSource->IsAcceptingHttpRequests());
    HttpSource->StopSource();
    TestFalse(TEXT("HTTP JSON live route unbinds on stop"), HttpSource->IsHttpRouteBound());
    TestFalse(TEXT("HTTP JSON live stops accepting requests"), HttpSource->IsAcceptingHttpRequests());

    SourceOwner->Destroy();
    return true;
}

bool FRealSensorSourceCameraJsonLiveBridgeTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    AActor* SourceOwner = World->SpawnActor<AActor>();
    TestNotNull(TEXT("camera JSON live source owner"), SourceOwner);
    if (!SourceOwner)
    {
        return false;
    }

    UVirtualCameraComp* TargetCamera = NewObject<UVirtualCameraComp>(SourceOwner);
    UCameraJsonLiveSourceComp* CameraSource = NewObject<UCameraJsonLiveSourceComp>(SourceOwner);
    UVirtualSensorDataTransportComp* TransportComp = NewObject<UVirtualSensorDataTransportComp>(SourceOwner);
    UVirtualSensorRecorderComp* RecorderComp = NewObject<UVirtualSensorRecorderComp>(SourceOwner);
    TestNotNull(TEXT("camera JSON live target camera"), TargetCamera);
    TestNotNull(TEXT("camera JSON live source"), CameraSource);
    TestNotNull(TEXT("camera JSON live transport"), TransportComp);
    TestNotNull(TEXT("camera JSON live recorder"), RecorderComp);
    if (!TargetCamera || !CameraSource || !TransportComp || !RecorderComp)
    {
        SourceOwner->Destroy();
        return false;
    }

    SourceOwner->AddInstanceComponent(TargetCamera);
    SourceOwner->AddInstanceComponent(CameraSource);
    SourceOwner->AddInstanceComponent(TransportComp);
    SourceOwner->AddInstanceComponent(RecorderComp);
    TargetCamera->bAutoStartCapture = false;
    TargetCamera->bAutoRegisterToManager = false;
    TargetCamera->SensorId = TEXT("TEST-CAMERA-JSON-LIVE-TARGET");
    TargetCamera->OutputMode = EVirtualCameraOutputMode::SaveJpeg;
    TargetCamera->RegisterComponent();
    CameraSource->RegisterComponent();
    TransportComp->RegisterComponent();
    RecorderComp->RegisterComponent();

    TransportComp->TransportMode = EVirtualSensorTransportMode::SaveToFile;
    TransportComp->SaveSubDirectory = TEXT("Automation/CameraJsonLiveTransport");
    RecorderComp->StartRecording();
    TargetCamera->SetTransportComponent(TransportComp);
    TargetCamera->SetRecorderComponent(RecorderComp);
    CameraSource->TargetCamera = TargetCamera;
    CameraSource->bSendTransportByDefault = true;

    const FString Payload = TEXT("{\"schemaVersion\":\"virtual-camera.v1\",\"sensorType\":\"virtual_camera\",\"sensorId\":\"CAM-JSON-LIVE-01\",\"manufacturer\":\"Fixture\",\"model\":\"ExternalJpeg\",\"frameId\":7,\"timestampUtc\":\"2026-06-14T12:00:00.000Z\",\"width\":640,\"height\":360,\"byteSize\":13,\"horizontalFov\":90.0,\"verticalFov\":58.7,\"simulationQuality\":\"RealTimePreview\",\"encoding\":\"jpeg/base64\",\"sensorTransform\":{\"location\":[0,0,0],\"rotation\":[0,0,0],\"forward\":[1,0,0],\"up\":[0,0,1]},\"sensorWorldLocation\":[0,0,0],\"image\":\"/9j/4AAQSkZJRgABAQ==\"}");
    TestTrue(TEXT("camera JSON live source starts"), CameraSource->StartSource());
    TestTrue(TEXT("camera JSON live source buffers payload"), CameraSource->AppendLivePayloadJson(Payload));
    TestTrue(TEXT("camera JSON live has buffered payload"), CameraSource->bHasBufferedPayload);
    TestEqual(TEXT("camera JSON live payload length tracked"), CameraSource->LastPayloadLength, Payload.Len());
    TestTrue(TEXT("camera JSON live pushes payload"), CameraSource->PushFrameOnce(true));

    TestEqual(TEXT("camera JSON live source frame id"), CameraSource->LastSourceFrameId, static_cast<int64>(1));
    TestEqual(TEXT("camera JSON live source point count represents frame count"), CameraSource->LastSourcePointCount, 1);
    TestFalse(TEXT("camera JSON live buffer clears after push"), CameraSource->bHasBufferedPayload);
    TestEqual(TEXT("camera target caches external payload"), TargetCamera->GetLastJsonPayload(), Payload);
    TestEqual(TEXT("camera runtime frame id from payload"), TargetCamera->GetRuntimeStatus().FrameId, static_cast<int64>(7));
    TestEqual(TEXT("camera runtime sensor id from payload"), TargetCamera->GetRuntimeStatus().SensorId, FString(TEXT("CAM-JSON-LIVE-01")));
    TestTrue(TEXT("camera runtime message mentions external payload"), TargetCamera->GetRuntimeStatus().LastMessage.Contains(TEXT("Injected external camera payload")));
    TestTrue(TEXT("camera external payload transport submitted"), TransportComp->LastResult.bSubmitted);
    TestTrue(TEXT("camera external payload transport accepted"), TransportComp->LastResult.bAccepted);
    TestEqual(TEXT("camera external payload transport length"), TransportComp->LastResult.DataLength, Payload.Len());
    TestTrue(TEXT("camera external payload transport saved JSON"), TransportComp->LastResult.SavedFilePath.EndsWith(TEXT(".json")));
    TestTrue(TEXT("camera external payload transport path uses payload sensor id"), TransportComp->LastResult.SavedFilePath.Contains(TEXT("CAM-JSON-LIVE-01")));
    TestTrue(TEXT("camera external payload transport file exists"), FPaths::FileExists(TransportComp->LastResult.SavedFilePath));
    TestEqual(TEXT("camera external payload recorder frame count"), RecorderComp->GetRecordedFrameCount(), 1);
    FVirtualSensorRecordedFrame RecordedFrame;
    TestTrue(TEXT("camera external payload recorder frame readable"), RecorderComp->GetRecordedFrame(0, RecordedFrame));
    TestEqual(TEXT("camera external payload recorder sensor id"), RecordedFrame.SensorId, FString(TEXT("CAM-JSON-LIVE-01")));
    TestEqual(TEXT("camera external payload recorder sensor type"), RecordedFrame.SensorType, FString(TEXT("virtual_camera")));
    TestEqual(TEXT("camera external payload recorder frame id"), RecordedFrame.FrameId, static_cast<int64>(7));
    TestEqual(TEXT("camera external payload recorder payload"), RecordedFrame.PayloadJson, Payload);

    TestTrue(TEXT("camera JSON live source rejects empty payload"), !CameraSource->AppendLivePayloadJson(TEXT("   ")));

    UCameraJsonLiveSourceComp* InvalidSource = NewObject<UCameraJsonLiveSourceComp>(SourceOwner);
    TestNotNull(TEXT("invalid camera JSON live source"), InvalidSource);
    if (InvalidSource)
    {
        SourceOwner->AddInstanceComponent(InvalidSource);
        InvalidSource->RegisterComponent();
        InvalidSource->TargetCamera = TargetCamera;
        TestTrue(TEXT("invalid camera JSON live buffers payload"), InvalidSource->AppendLivePayloadJson(TEXT("{\"schemaVersion\":\"virtual-camera.v1\",\"sensorType\":\"virtual_camera\",\"encoding\":\"jpeg/base64\",\"image\":\"\"}")));
        TestFalse(TEXT("invalid camera JSON live push is rejected"), InvalidSource->PushFrameOnce(false));
        TestEqual(TEXT("invalid camera JSON live reports error"), InvalidSource->GetConnectionState(), ERealSensorSourceConnectionState::Error);
        TestEqual(TEXT("invalid camera JSON live keeps previous camera payload"), TargetCamera->GetLastJsonPayload(), Payload);
        TestEqual(TEXT("invalid camera JSON live does not record a frame"), RecorderComp->GetRecordedFrameCount(), 1);
    }

    UCameraJsonLiveSourceComp* InvalidBase64Source = NewObject<UCameraJsonLiveSourceComp>(SourceOwner);
    TestNotNull(TEXT("invalid base64 camera JSON live source"), InvalidBase64Source);
    if (InvalidBase64Source)
    {
        SourceOwner->AddInstanceComponent(InvalidBase64Source);
        InvalidBase64Source->RegisterComponent();
        InvalidBase64Source->TargetCamera = TargetCamera;
        const FString InvalidBase64Payload = Payload.Replace(TEXT("\"/9j/4AAQSkZJRgABAQ==\""), TEXT("\"not-base64\""));
        TestTrue(TEXT("invalid base64 camera JSON live buffers payload"), InvalidBase64Source->AppendLivePayloadJson(InvalidBase64Payload));
        TestFalse(TEXT("invalid base64 camera JSON live push is rejected"), InvalidBase64Source->PushFrameOnce(false));
        TestEqual(TEXT("invalid base64 camera JSON live reports error"), InvalidBase64Source->GetConnectionState(), ERealSensorSourceConnectionState::Error);
        TestEqual(TEXT("invalid base64 camera JSON live keeps previous camera payload"), TargetCamera->GetLastJsonPayload(), Payload);
        TestEqual(TEXT("invalid base64 camera JSON live does not record a frame"), RecorderComp->GetRecordedFrameCount(), 1);
    }

    UCameraJsonLiveSourceComp* InvalidQualitySource = NewObject<UCameraJsonLiveSourceComp>(SourceOwner);
    TestNotNull(TEXT("invalid quality camera JSON live source"), InvalidQualitySource);
    if (InvalidQualitySource)
    {
        SourceOwner->AddInstanceComponent(InvalidQualitySource);
        InvalidQualitySource->RegisterComponent();
        InvalidQualitySource->TargetCamera = TargetCamera;
        const FString InvalidQualityPayload = Payload.Replace(TEXT("\"simulationQuality\":\"RealTimePreview\""), TEXT("\"simulationQuality\":\"FastEnough\""));
        TestTrue(TEXT("invalid quality camera JSON live buffers payload"), InvalidQualitySource->AppendLivePayloadJson(InvalidQualityPayload));
        TestFalse(TEXT("invalid quality camera JSON live push is rejected"), InvalidQualitySource->PushFrameOnce(false));
        TestEqual(TEXT("invalid quality camera JSON live reports error"), InvalidQualitySource->GetConnectionState(), ERealSensorSourceConnectionState::Error);
        TestEqual(TEXT("invalid quality camera JSON live keeps previous camera payload"), TargetCamera->GetLastJsonPayload(), Payload);
        TestEqual(TEXT("invalid quality camera JSON live does not record a frame"), RecorderComp->GetRecordedFrameCount(), 1);
    }

    UCameraJsonLiveSourceComp* ByteMismatchSource = NewObject<UCameraJsonLiveSourceComp>(SourceOwner);
    TestNotNull(TEXT("byte mismatch camera JSON live source"), ByteMismatchSource);
    if (ByteMismatchSource)
    {
        SourceOwner->AddInstanceComponent(ByteMismatchSource);
        ByteMismatchSource->RegisterComponent();
        ByteMismatchSource->TargetCamera = TargetCamera;
        const FString ByteMismatchPayload = Payload.Replace(TEXT("\"byteSize\":13"), TEXT("\"byteSize\":12"));
        TestTrue(TEXT("byte mismatch camera JSON live buffers payload"), ByteMismatchSource->AppendLivePayloadJson(ByteMismatchPayload));
        TestFalse(TEXT("byte mismatch camera JSON live push is rejected"), ByteMismatchSource->PushFrameOnce(false));
        TestEqual(TEXT("byte mismatch camera JSON live reports error"), ByteMismatchSource->GetConnectionState(), ERealSensorSourceConnectionState::Error);
        TestEqual(TEXT("byte mismatch camera JSON live keeps previous camera payload"), TargetCamera->GetLastJsonPayload(), Payload);
        TestEqual(TEXT("byte mismatch camera JSON live does not record a frame"), RecorderComp->GetRecordedFrameCount(), 1);
    }

    SourceOwner->Destroy();
    return true;
}

bool FRealSensorSourceUdpJsonLiveDatagramTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FUdpJsonLiveDatagramCommand(this));
    return true;
}

bool FRealSensorSourceHttpJsonLiveLoopbackPostTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FHttpJsonLiveLoopbackPostCommand(this));
    return true;
}

bool FRealSensorSourceJsonLiveTransactionParseTest::RunTest(const FString& Parameters)
{
    ULidarJsonLiveFrameTC* Handler = NewObject<ULidarJsonLiveFrameTC>();
    TestNotNull(TEXT("JSON live TC handler"), Handler);
    if (!Handler)
    {
        return false;
    }

    TestEqual(TEXT("JSON live TC transaction code"), Handler->TransactionCode, FString(TEXT("LIDAR_JSON_LIVE_FRAME")));

    const FString Payload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"JsonLiveLidarBridge\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":true,\"POINTS\":[{\"row\":1,\"col\":2,\"x\":100,\"y\":20,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"},{\"x\":120,\"y\":25,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}");
    const TSharedPtr<FTransactionCodeDataBase> ParsedBase = Handler->ParseToStruct(Payload);
    TestTrue(TEXT("JSON live TC parses payload"), ParsedBase.IsValid());
    if (!ParsedBase.IsValid())
    {
        return false;
    }

    const TSharedPtr<FLidarJsonLiveFrameTCData> ParsedData = StaticCastSharedPtr<FLidarJsonLiveFrameTCData>(ParsedBase);
    TestTrue(TEXT("JSON live TC parsed data valid"), ParsedData.IsValid());
    if (ParsedData.IsValid())
    {
        TestEqual(TEXT("JSON live TC source id"), ParsedData->SourceId, FString(TEXT("JsonLiveLidarBridge")));
        TestFalse(TEXT("JSON live TC send transport flag"), ParsedData->bSendTransport);
        TestTrue(TEXT("JSON live TC push frame flag"), ParsedData->bPushFrame);
        TestTrue(TEXT("JSON live TC includes first point"), ParsedData->JsonLines.Contains(TEXT("\"row\":1")));
        TestTrue(TEXT("JSON live TC includes second point"), ParsedData->JsonLines.Contains(TEXT("\"x\":120")));
    }

    ULidarJsonLiveSourceComp* HelperParityLive = NewObject<ULidarJsonLiveSourceComp>();
    TestNotNull(TEXT("JSON live helper parity source"), HelperParityLive);
    if (HelperParityLive)
    {
        TestTrue(TEXT("JSON live helper parses same WebSocket payload"), HelperParityLive->AppendWebSocketPayload(Payload));
        TestEqual(TEXT("JSON live helper parity pending lines"), HelperParityLive->PendingLineCount, 2);
        TArray<FVirtualLidarPoint> HelperParityPoints;
        TestTrue(TEXT("JSON live helper builds same WebSocket payload points"), HelperParityLive->BuildFrameFromBufferedLines(HelperParityPoints));
        TestEqual(TEXT("JSON live helper parity point count"), HelperParityPoints.Num(), 2);
        if (HelperParityPoints.Num() >= 1)
        {
            TestEqual(TEXT("JSON live helper parity row"), HelperParityPoints[0].Row, 1);
            TestEqual(TEXT("JSON live helper parity col"), HelperParityPoints[0].Col, 2);
        }
    }

    const FString TimestampPayload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"CREATE_TIMESTAMP\":\"T1\",\"DATA_MAP\":{\"T1\":{\"sourceId\":\"BridgeA\",\"sendTransport\":true,\"pushFrame\":false,\"jsonLines\":\"{\\\"x\\\":1,\\\"y\\\":2,\\\"z\\\":3}\"}}}");
    const TSharedPtr<FTransactionCodeDataBase> TimestampParsedBase = Handler->ParseToStruct(TimestampPayload);
    TestTrue(TEXT("JSON live TC parses timestamp DATA_MAP payload"), TimestampParsedBase.IsValid());
    if (TimestampParsedBase.IsValid())
    {
        const TSharedPtr<FLidarJsonLiveFrameTCData> TimestampData = StaticCastSharedPtr<FLidarJsonLiveFrameTCData>(TimestampParsedBase);
        TestEqual(TEXT("timestamp payload source id"), TimestampData->SourceId, FString(TEXT("BridgeA")));
        TestTrue(TEXT("timestamp payload send transport"), TimestampData->bSendTransport);
        TestFalse(TEXT("timestamp payload push frame"), TimestampData->bPushFrame);
        TestTrue(TEXT("timestamp payload json lines"), TimestampData->JsonLines.Contains(TEXT("\"x\":1")));
    }

    const FString InvalidPayload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"POINTS\":[]}}");
    TestFalse(TEXT("JSON live TC rejects empty payload"), Handler->ParseToStruct(InvalidPayload).IsValid());

    const FString WhitespacePayload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"BridgeA\",\"jsonLines\":\"   \"}}");
    TestFalse(TEXT("JSON live TC rejects whitespace-only jsonLines"), Handler->ParseToStruct(WhitespacePayload).IsValid());
    return true;
}

bool FRealSensorSourceJsonLiveTransactionRoutingTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    AActor* SourceOwnerA = World->SpawnActor<AActor>();
    AActor* SourceOwnerB = World->SpawnActor<AActor>();
    TestNotNull(TEXT("JSON live source owner A"), SourceOwnerA);
    TestNotNull(TEXT("JSON live source owner B"), SourceOwnerB);
    if (!SourceOwnerA || !SourceOwnerB)
    {
        return false;
    }

    UVirtualLidarSensorComp* TargetA = NewObject<UVirtualLidarSensorComp>(SourceOwnerA);
    UVirtualLidarSensorComp* TargetB = NewObject<UVirtualLidarSensorComp>(SourceOwnerB);
    ULidarJsonLiveSourceComp* SourceA = NewObject<ULidarJsonLiveSourceComp>(SourceOwnerA);
    ULidarJsonLiveSourceComp* SourceB = NewObject<ULidarJsonLiveSourceComp>(SourceOwnerB);
    TestNotNull(TEXT("target lidar A"), TargetA);
    TestNotNull(TEXT("target lidar B"), TargetB);
    TestNotNull(TEXT("JSON live source A"), SourceA);
    TestNotNull(TEXT("JSON live source B"), SourceB);
    if (!TargetA || !TargetB || !SourceA || !SourceB)
    {
        SourceOwnerA->Destroy();
        SourceOwnerB->Destroy();
        return false;
    }

    SourceOwnerA->AddInstanceComponent(TargetA);
    SourceOwnerA->AddInstanceComponent(SourceA);
    SourceOwnerB->AddInstanceComponent(TargetB);
    SourceOwnerB->AddInstanceComponent(SourceB);
    TargetA->RegisterComponent();
    SourceA->RegisterComponent();
    TargetB->RegisterComponent();
    SourceB->RegisterComponent();

    const FString RouteSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString BridgeAId = FString::Printf(TEXT("Automation_ProcessRoute_A_%s"), *RouteSuffix);
    const FString BridgeBId = FString::Printf(TEXT("Automation_ProcessRoute_B_%s"), *RouteSuffix);
    SourceA->SourceId = BridgeAId;
    SourceB->SourceId = BridgeBId;
    SourceA->TargetLidar = TargetA;
    SourceB->TargetLidar = TargetB;
    TargetA->SensorId = TEXT("TEST-LIDAR-ROUTE-A");
    TargetB->SensorId = TEXT("TEST-LIDAR-ROUTE-B");

    ULidarJsonLiveFrameTC* Handler = NewObject<ULidarJsonLiveFrameTC>(World);
    TestNotNull(TEXT("JSON live TC world-bound handler"), Handler);
    if (!Handler)
    {
        SourceOwnerA->Destroy();
        SourceOwnerB->Destroy();
        return false;
    }
    TestEqual(TEXT("JSON live TC handler resolves test world"), Handler->GetWorld(), World);

    ULidarJsonLiveFrameTC* NoWorldHandler = NewObject<ULidarJsonLiveFrameTC>();
    TestNotNull(TEXT("JSON live TC no-world handler"), NoWorldHandler);
    if (NoWorldHandler)
    {
        TestNull(TEXT("JSON live TC no-world handler has no world"), NoWorldHandler->GetWorld());
    }

    const FString PayloadBridgeAAppendOnly = FString::Printf(TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"%s\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":false,\"POINTS\":[{\"row\":1,\"col\":0,\"x\":200,\"y\":10,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"},{\"row\":1,\"col\":1,\"x\":220,\"y\":12,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}"), *BridgeAId);
    TSharedPtr<FTransactionCodeDataBase> ParsedBridgeA = Handler->ParseToStruct(PayloadBridgeAAppendOnly);
    TestTrue(TEXT("routing payload parses"), ParsedBridgeA.IsValid());
    if (ParsedBridgeA.IsValid() && NoWorldHandler)
    {
        NoWorldHandler->ProcessStructData(ParsedBridgeA);
    }
    TestEqual(TEXT("no-world handler leaves BridgeA unchanged"), SourceA->PendingLineCount, 0);
    TestEqual(TEXT("no-world handler leaves BridgeB unchanged"), SourceB->PendingLineCount, 0);
    if (ParsedBridgeA.IsValid())
    {
        Handler->ProcessStructData(ParsedBridgeA);
    }

    TestEqual(TEXT("BridgeA receives append-only frame"), SourceA->PendingLineCount, 2);
    TestEqual(TEXT("BridgeB remains untouched"), SourceB->PendingLineCount, 0);
    TestEqual(TEXT("BridgeA target not pushed when PUSH_FRAME=false"), TargetA->GetLastPoints().Num(), 0);
    TestTrue(TEXT("BridgeA push after routing succeeds"), SourceA->PushFrameOnce(false));
    TestEqual(TEXT("BridgeA target receives routed points"), TargetA->GetLastPoints().Num(), 2);
    TestEqual(TEXT("BridgeB target still empty"), TargetB->GetLastPoints().Num(), 0);

    const FString AmbiguousPayload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":false,\"POINTS\":[{\"x\":300,\"y\":10,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}");
    TSharedPtr<FTransactionCodeDataBase> ParsedAmbiguous = Handler->ParseToStruct(AmbiguousPayload);
    TestTrue(TEXT("ambiguous no-source payload parses"), ParsedAmbiguous.IsValid());
    if (ParsedAmbiguous.IsValid())
    {
        Handler->ProcessStructData(ParsedAmbiguous);
    }

    TestEqual(TEXT("BridgeA unchanged after ambiguous no-source route"), SourceA->PendingLineCount, 0);
    TestEqual(TEXT("BridgeB unchanged after ambiguous no-source route"), SourceB->PendingLineCount, 0);

    const FString PayloadBridgeBPush = FString::Printf(TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"%s\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":true,\"POINTS\":[{\"row\":2,\"col\":0,\"x\":240,\"y\":20,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}"), *BridgeBId);
    TSharedPtr<FTransactionCodeDataBase> ParsedBridgeB = Handler->ParseToStruct(PayloadBridgeBPush);
    TestTrue(TEXT("BridgeB push payload parses"), ParsedBridgeB.IsValid());
    if (ParsedBridgeB.IsValid())
    {
        Handler->ProcessStructData(ParsedBridgeB);
    }

    TestEqual(TEXT("BridgeB push clears buffer"), SourceB->PendingLineCount, 0);
    TestEqual(TEXT("BridgeB target receives pushed point"), TargetB->GetLastPoints().Num(), 1);
    TestFalse(TEXT("BridgeB target has cached payload"), TargetB->GetLastJsonPayload().IsEmpty());

    SourceOwnerA->Destroy();
    SourceOwnerB->Destroy();
    return true;
}

bool FRealSensorSourceJsonLiveTransactionDataTableRegistrationTest::RunTest(const FString& Parameters)
{
    const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();
    TestNotNull(TEXT("DTCore settings"), Settings);
    if (!Settings)
    {
        return false;
    }

    const FSoftObjectPath ConfiguredPath = Settings->WebSocketDataTable.ToSoftObjectPath();
    TestTrue(TEXT("WebSocket data table path is configured"), ConfiguredPath.IsValid());
    TestEqual(
        TEXT("WebSocket data table path"),
        ConfiguredPath.ToString(),
        FString(TEXT("/Game/M7AT10/Common/DataTables/DT_TransactionCode.DT_TransactionCode")));
    if (!ConfiguredPath.IsValid())
    {
        return false;
    }

    UDataTable* WebSocketDataTable = Settings->WebSocketDataTable.LoadSynchronous();
    TestNotNull(TEXT("WebSocket data table loads"), WebSocketDataTable);
    if (!WebSocketDataTable)
    {
        AddInfo(TEXT("Run Scripts/export_websocket_transaction_registration_report.ps1 for the non-mutating registration checklist."));
        return false;
    }

    const FName ExpectedRowName(TEXT("LIDAR_JSON_LIVE_FRAME"));
    const FTransactionCodeStruct* Row = WebSocketDataTable->FindRow<FTransactionCodeStruct>(
        ExpectedRowName,
        TEXT("M7AT10.Evidence.WebSocketTransactionRegistration"),
        false);
    TestNotNull(TEXT("LIDAR_JSON_LIVE_FRAME row exists in DT_TransactionCode"), Row);
    if (!Row)
    {
        AddInfo(TEXT("Add a DT_TransactionCode row named LIDAR_JSON_LIVE_FRAME with TransactionCodeMessageClass=/Script/m7at10_dt.LidarJsonLiveFrameTC."));
        return false;
    }

    TestEqual(TEXT("TransactionCodeName"), Row->TransactionCodeName, FString(TEXT("LIDAR_JSON_LIVE_FRAME")));
    TestNotNull(TEXT("TransactionCodeMessageClass"), Row->TransactionCodeMessageClass.Get());
    if (!Row->TransactionCodeMessageClass)
    {
        return false;
    }

    TestTrue(
        TEXT("TransactionCodeMessageClass resolves to ULidarJsonLiveFrameTC"),
        Row->TransactionCodeMessageClass->IsChildOf(ULidarJsonLiveFrameTC::StaticClass()));

    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    UTransactionCodeMessage* Handler = NewObject<UTransactionCodeMessage>(
        World,
        Row->TransactionCodeMessageClass);
    TestNotNull(TEXT("Registered handler can be instantiated"), Handler);
    if (!Handler)
    {
        return false;
    }

    TestEqual(TEXT("Registered handler transaction code"), Handler->TransactionCode, FString(TEXT("LIDAR_JSON_LIVE_FRAME")));

    AActor* SourceOwner = World->SpawnActor<AActor>();
    TestNotNull(TEXT("JSON live source owner"), SourceOwner);
    if (!SourceOwner)
    {
        return false;
    }

    UVirtualLidarSensorComp* TargetLidar = NewObject<UVirtualLidarSensorComp>(SourceOwner);
    ULidarJsonLiveSourceComp* JsonLiveSource = NewObject<ULidarJsonLiveSourceComp>(SourceOwner);
    TestNotNull(TEXT("registered route target lidar"), TargetLidar);
    TestNotNull(TEXT("registered route JSON live source"), JsonLiveSource);
    if (!TargetLidar || !JsonLiveSource)
    {
        SourceOwner->Destroy();
        return false;
    }

    SourceOwner->AddInstanceComponent(TargetLidar);
    SourceOwner->AddInstanceComponent(JsonLiveSource);
    TargetLidar->RegisterComponent();
    JsonLiveSource->RegisterComponent();

    JsonLiveSource->SourceId = TEXT("JsonLiveLidarBridge");
    JsonLiveSource->TargetLidar = TargetLidar;
    TargetLidar->SensorId = TEXT("TEST-LIDAR-DATATABLE-DISPATCH");

    const FString Payload = TEXT("{\"MESSAGE_ID\":\"LIDAR_JSON_LIVE_FRAME\",\"DATA_MAP\":{\"SOURCE_ID\":\"JsonLiveLidarBridge\",\"SEND_TRANSPORT\":false,\"PUSH_FRAME\":true,\"POINTS\":[{\"row\":0,\"col\":0,\"returnIndex\":0,\"x\":900,\"y\":-260,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"},{\"row\":0,\"col\":1,\"returnIndex\":0,\"x\":940,\"y\":-120,\"z\":0,\"hit\":true,\"semanticLabel\":\"Slab\"}]}}");
    const TSharedPtr<FTransactionCodeDataBase> ParsedData = Handler->ParseToStruct(Payload);
    TestTrue(TEXT("registered handler parses sample payload through base class"), ParsedData.IsValid());
    if (ParsedData.IsValid())
    {
        Handler->ProcessStructData(ParsedData);
    }

    TestEqual(TEXT("registered data-table handler pushes target LiDAR points"), TargetLidar->GetLastPoints().Num(), 2);
    TestFalse(TEXT("registered data-table handler updates cached server payload"), TargetLidar->GetLastJsonPayload().IsEmpty());

    SourceOwner->Destroy();
    return true;
}

#if WITH_EDITOR
bool FRealSensorSourceJsonLiveDTCoreDispatchTest::RunTest(const FString& Parameters)
{
    FAutomationEditorCommonUtils::CreateNewMap();
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FBrokerlessWebSocketDispatchCommand(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif

#endif
