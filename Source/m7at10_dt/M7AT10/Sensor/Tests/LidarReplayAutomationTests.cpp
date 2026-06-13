#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarCsvReplayLoadTest, "M7AT10.SensorReplay.CsvLoadSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarJsonLinesReplayLoadTest, "M7AT10.SensorReplay.JsonLinesLoadSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayInjectFrameTest, "M7AT10.SensorReplay.InjectFrameUpdatesStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLidarCsvReplayLoadTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* ReplayComp = NewObject<ULidarCsvReplaySourceComp>();
    TestNotNull(TEXT("CSV replay component"), ReplayComp);
    ReplayComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    ReplayComp->ReplaySemanticLabel = TEXT("Slab");

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    TestTrue(TEXT("CSV frame loads"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));
    TestEqual(TEXT("CSV sample point count"), Points.Num(), 24);
    TestEqual(TEXT("CSV row count"), Rows, 4);
    TestEqual(TEXT("CSV column count"), Cols, 6);
    TestTrue(TEXT("CSV points are hit points"), Points.Num() > 0 && Points[0].bHit);
    TestEqual(TEXT("CSV semantic label"), Points.Num() > 0 ? Points[0].SemanticLabel : NAME_None, FName(TEXT("Slab")));
    return true;
}

bool FLidarJsonLinesReplayLoadTest::RunTest(const FString& Parameters)
{
    ULidarJsonLinesReplaySourceComp* ReplayComp = NewObject<ULidarJsonLinesReplaySourceComp>();
    TestNotNull(TEXT("JSONL replay component"), ReplayComp);
    ReplayComp->JsonLinesFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.jsonl"));

    TArray<FVirtualLidarPoint> Points;
    TestTrue(TEXT("JSONL frame loads"), ReplayComp->LoadJsonLinesFrame(Points));
    TestEqual(TEXT("JSONL sample point count"), Points.Num(), 18);
    TestTrue(TEXT("JSONL points are hit points"), Points.Num() > 0 && Points[0].bHit);
    TestEqual(TEXT("JSONL semantic label"), Points.Num() > 0 ? Points[0].SemanticLabel : NAME_None, FName(TEXT("Slab")));
    return true;
}

bool FLidarReplayInjectFrameTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* ReplayComp = NewObject<ULidarCsvReplaySourceComp>();
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    TestNotNull(TEXT("CSV replay component"), ReplayComp);
    TestNotNull(TEXT("LiDAR component"), LidarComp);

    ReplayComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    ReplayComp->ReplaySemanticLabel = TEXT("Slab");

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    TestTrue(TEXT("CSV frame loads before injection"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-REPLAY");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->SlabSemanticLabel = TEXT("Slab");
    LidarComp->MinSlabPointsForAnalysis = 3;
    LidarComp->SetServerPayloadPolicy(1, 0, false);
    LidarComp->SetPreviewPolicy(2, 5000, true);
    LidarComp->InjectPointCloudFrame(Points, false);

    const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
    TestEqual(TEXT("Injected total point count"), Status.TotalPointCount, 24);
    TestEqual(TEXT("Injected hit point count"), Status.HitPointCount, 24);
    TestEqual(TEXT("Server payload count remains full hit set"), Status.ServerPayloadPointCount, 24);
    TestEqual(TEXT("Preview count honors stride"), Status.PreviewPointCount, 12);
    TestTrue(TEXT("Slab analysis is valid"), Status.SlabAnalysis.bValid);
    TestTrue(TEXT("Slab analysis has confidence"), Status.SlabAnalysis.Confidence > 0.0f);
    return true;
}

#endif
