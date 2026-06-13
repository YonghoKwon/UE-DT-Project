#if WITH_DEV_AUTOMATION_TESTS

#include "Json.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarCsvReplayLoadTest, "M7AT10.SensorReplay.CsvLoadSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarJsonLinesReplayLoadTest, "M7AT10.SensorReplay.JsonLinesLoadSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayInjectFrameTest, "M7AT10.SensorReplay.InjectFrameUpdatesStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayPayloadPolicyJsonTest, "M7AT10.SensorReplay.PayloadPolicyJson", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

bool FLidarReplayPayloadPolicyJsonTest::RunTest(const FString& Parameters)
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
    TestTrue(TEXT("CSV frame loads before payload policy test"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-PAYLOAD-POLICY");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->SlabSemanticLabel = TEXT("Slab");
    LidarComp->MinSlabPointsForAnalysis = 3;
    LidarComp->SetServerPayloadPolicy(3, 5, false);
    LidarComp->SetPreviewPolicy(2, 7, true);
    LidarComp->InjectPointCloudFrame(Points, false);

    const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
    TestEqual(TEXT("Runtime server payload count honors server policy"), Status.ServerPayloadPointCount, 5);
    TestEqual(TEXT("Runtime preview count honors preview policy"), Status.PreviewPointCount, 7);

    const FString& PayloadJson = LidarComp->GetLastJsonPayload();
    TestFalse(TEXT("Last JSON payload is cached"), PayloadJson.IsEmpty());

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
    TestTrue(TEXT("Payload JSON parses"), FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid());
    if (!RootObject.IsValid())
    {
        return false;
    }

    TestEqual(TEXT("schema version"), RootObject->GetStringField(TEXT("schemaVersion")), FString(TEXT("virtual-lidar.v1")));
    TestEqual(TEXT("payloadPointCount root field"), static_cast<int32>(RootObject->GetIntegerField(TEXT("payloadPointCount"))), 5);
    TestEqual(TEXT("previewPointStride root field"), static_cast<int32>(RootObject->GetIntegerField(TEXT("previewPointStride"))), 2);
    TestEqual(TEXT("maxPreviewPoints root field"), static_cast<int32>(RootObject->GetIntegerField(TEXT("maxPreviewPoints"))), 7);

    const TSharedPtr<FJsonObject>* PayloadPolicy = nullptr;
    TestTrue(TEXT("payloadPolicy object exists"), RootObject->TryGetObjectField(TEXT("payloadPolicy"), PayloadPolicy) && PayloadPolicy && PayloadPolicy->IsValid());
    if (PayloadPolicy && PayloadPolicy->IsValid())
    {
        TestEqual(TEXT("payloadPolicy stride"), static_cast<int32>((*PayloadPolicy)->GetIntegerField(TEXT("stride"))), 3);
        TestEqual(TEXT("payloadPolicy maxPoints"), static_cast<int32>((*PayloadPolicy)->GetIntegerField(TEXT("maxPoints"))), 5);
        TestFalse(TEXT("payloadPolicy includeMissPoints"), (*PayloadPolicy)->GetBoolField(TEXT("includeMissPoints")));
        TestEqual(TEXT("payloadPolicy pointSelection"), (*PayloadPolicy)->GetStringField(TEXT("pointSelection")), FString(TEXT("hit_only")));
    }

    const TSharedPtr<FJsonObject>* PreviewPolicy = nullptr;
    TestTrue(TEXT("previewPolicy object exists"), RootObject->TryGetObjectField(TEXT("previewPolicy"), PreviewPolicy) && PreviewPolicy && PreviewPolicy->IsValid());
    if (PreviewPolicy && PreviewPolicy->IsValid())
    {
        TestEqual(TEXT("previewPolicy stride"), static_cast<int32>((*PreviewPolicy)->GetIntegerField(TEXT("stride"))), 2);
        TestEqual(TEXT("previewPolicy maxPoints"), static_cast<int32>((*PreviewPolicy)->GetIntegerField(TEXT("maxPoints"))), 7);
        TestTrue(TEXT("previewPolicy hitOnly"), (*PreviewPolicy)->GetBoolField(TEXT("hitOnly")));
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonPoints = nullptr;
    TestTrue(TEXT("points array exists"), RootObject->TryGetArrayField(TEXT("points"), JsonPoints) && JsonPoints);
    if (JsonPoints)
    {
        TestEqual(TEXT("points array count follows server payload policy"), JsonPoints->Num(), 5);
    }

    const TSharedPtr<FJsonObject>* SlabAnalysis = nullptr;
    TestTrue(TEXT("slabAnalysis object exists"), RootObject->TryGetObjectField(TEXT("slabAnalysis"), SlabAnalysis) && SlabAnalysis && SlabAnalysis->IsValid());
    if (SlabAnalysis && SlabAnalysis->IsValid())
    {
        TestTrue(TEXT("slabAnalysis valid"), (*SlabAnalysis)->GetBoolField(TEXT("valid")));
        TestEqual(TEXT("slabAnalysis slab point count remains full hit set"), static_cast<int32>((*SlabAnalysis)->GetIntegerField(TEXT("slabHitPointCount"))), 24);
    }

    return true;
}

#endif
