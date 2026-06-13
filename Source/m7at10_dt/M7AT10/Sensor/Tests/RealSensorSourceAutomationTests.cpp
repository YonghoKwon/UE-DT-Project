#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorAdapterStubs.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorSourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceBaseStateTest, "M7AT10.RealSensorSource.BaseState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePlaceholderStateTest, "M7AT10.RealSensorSource.PlaceholderState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePushFrameToTargetTest, "M7AT10.RealSensorSource.PushFrameToTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRealSensorSourceBaseStateTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* CsvReplay = NewObject<ULidarCsvReplaySourceComp>();
    ULidarJsonLinesReplaySourceComp* JsonReplay = NewObject<ULidarJsonLinesReplaySourceComp>();
    TestNotNull(TEXT("CSV replay source"), CsvReplay);
    TestNotNull(TEXT("JSONL replay source"), JsonReplay);

    URealSensorSourceComp* CsvAsSource = Cast<URealSensorSourceComp>(CsvReplay);
    URealSensorSourceComp* JsonAsSource = Cast<URealSensorSourceComp>(JsonReplay);
    TestNotNull(TEXT("CSV replay inherits real sensor source"), CsvAsSource);
    TestNotNull(TEXT("JSONL replay inherits real sensor source"), JsonAsSource);
    TestEqual(TEXT("CSV replay source kind"), CsvAsSource->SourceKind, ERealSensorSourceKind::FileReplay);
    TestEqual(TEXT("JSONL replay source kind"), JsonAsSource->SourceKind, ERealSensorSourceKind::FileReplay);
    TestEqual(TEXT("CSV replay starts stopped"), CsvAsSource->GetConnectionState(), ERealSensorSourceConnectionState::Stopped);
    TestEqual(TEXT("JSONL replay starts stopped"), JsonAsSource->GetConnectionState(), ERealSensorSourceConnectionState::Stopped);
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

#endif
