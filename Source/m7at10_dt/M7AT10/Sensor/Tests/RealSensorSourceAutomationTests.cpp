#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Core/DTCoreSettings.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLiveSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorAdapterStubs.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/WebSocket/TC/LidarJsonLiveFrameTC.h"
#include "WebSocket/TransactionCodeStruct.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceBaseStateTest, "M7AT10.RealSensorSource.BaseState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePlaceholderStateTest, "M7AT10.RealSensorSource.PlaceholderState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePushFrameToTargetTest, "M7AT10.RealSensorSource.PushFrameToTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveBridgeTest, "M7AT10.RealSensorSource.JsonLiveBridgePushFrame", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveTransactionParseTest, "M7AT10.RealSensorSource.JsonLiveTransactionParse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveTransactionRoutingTest, "M7AT10.RealSensorSource.JsonLiveTransactionRouting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveTransactionDataTableRegistrationTest, "M7AT10.Evidence.WebSocketTransactionRegistration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

#endif
