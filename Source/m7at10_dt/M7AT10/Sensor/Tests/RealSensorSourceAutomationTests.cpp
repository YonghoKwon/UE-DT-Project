#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLiveSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorAdapterStubs.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceBaseStateTest, "M7AT10.RealSensorSource.BaseState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePlaceholderStateTest, "M7AT10.RealSensorSource.PlaceholderState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePushFrameToTargetTest, "M7AT10.RealSensorSource.PushFrameToTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceJsonLiveBridgeTest, "M7AT10.RealSensorSource.JsonLiveBridgePushFrame", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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
    TestTrue(TEXT("first point preserves grid coord"), Points[0].bHasGridCoord);
    TestEqual(TEXT("first point row"), Points[0].Row, 2);
    TestEqual(TEXT("first point col"), Points[0].Col, 5);
    TestEqual(TEXT("first point return index"), Points[0].ReturnIndex, 1);
    TestFalse(TEXT("second point keeps fallback marker invalid"), Points[1].bHasGridCoord);

    TestTrue(TEXT("JSON live pushes frame through base source helper"), JsonLive->PushFrameOnce(false));
    TestEqual(TEXT("JSON live source frame id increments"), JsonLive->LastSourceFrameId, static_cast<int64>(1));
    TestEqual(TEXT("JSON live source point count"), JsonLive->LastSourcePointCount, 2);
    TestEqual(TEXT("JSON live source state running"), JsonLive->GetConnectionState(), ERealSensorSourceConnectionState::Running);
    TestEqual(TEXT("JSON live target received points"), TargetLidar->GetLastPoints().Num(), 2);
    TestEqual(TEXT("JSON live buffer clears after push"), JsonLive->PendingLineCount, 0);
    TestFalse(TEXT("JSON live target has cached payload"), TargetLidar->GetLastJsonPayload().IsEmpty());
    TestTrue(TEXT("payload includes preserved grid coord source"), TargetLidar->GetLastJsonPayload().Contains(TEXT("\"gridCoordSource\":\"point_metadata\"")));
    TestTrue(TEXT("payload includes fallback grid coord source"), TargetLidar->GetLastJsonPayload().Contains(TEXT("\"gridCoordSource\":\"derived_from_point_index\"")));

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

#endif
