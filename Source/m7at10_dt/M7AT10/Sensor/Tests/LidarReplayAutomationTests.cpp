#if WITH_DEV_AUTOMATION_TESTS

#include "Json.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarCsvReplayLoadTest, "M7AT10.SensorReplay.CsvLoadSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarJsonLinesReplayLoadTest, "M7AT10.SensorReplay.JsonLinesLoadSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayInjectFrameTest, "M7AT10.SensorReplay.InjectFrameUpdatesStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayPayloadPolicyJsonTest, "M7AT10.SensorReplay.PayloadPolicyJson", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayTransportSaveToFileTest, "M7AT10.SensorReplay.TransportSaveToFilePayload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayPerformanceWarningTest, "M7AT10.SensorReplay.PerformanceWarningStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarLazPlaceholderExportTest, "M7AT10.SensorReplay.LazPlaceholderWritesLasSource", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

bool FLidarReplayTransportSaveToFileTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* ReplayComp = NewObject<ULidarCsvReplaySourceComp>();
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    UVirtualSensorDataTransportComp* TransportComp = NewObject<UVirtualSensorDataTransportComp>();
    TestNotNull(TEXT("CSV replay component"), ReplayComp);
    TestNotNull(TEXT("LiDAR component"), LidarComp);
    TestNotNull(TEXT("transport component"), TransportComp);
    if (!ReplayComp || !LidarComp || !TransportComp)
    {
        return false;
    }

    ReplayComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    ReplayComp->ReplaySemanticLabel = TEXT("Slab");

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    TestTrue(TEXT("CSV frame loads before transport test"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    const FString SensorId = TEXT("TEST-LIDAR-TRANSPORT-SAVE");
    const FString SaveSubDirectory = TEXT("Automation/TransportSaveToFile");
    const FString SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), SaveSubDirectory, SensorId);
    IFileManager::Get().DeleteDirectory(*SaveDirectory, false, true);

    TransportComp->TransportMode = EVirtualSensorTransportMode::SaveToFile;
    TransportComp->SaveSubDirectory = SaveSubDirectory;

    LidarComp->SensorId = SensorId;
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->SlabSemanticLabel = TEXT("Slab");
    LidarComp->MinSlabPointsForAnalysis = 3;
    LidarComp->SetServerPayloadPolicy(1, 0, false);
    LidarComp->SetPreviewPolicy(2, 5000, true);
    LidarComp->SetTransportComponent(TransportComp);
    LidarComp->InjectPointCloudFrame(Points, true);

    const FVirtualSensorTransportResult& Result = TransportComp->LastResult;
    TestTrue(TEXT("transport submitted"), Result.bSubmitted);
    TestTrue(TEXT("transport accepted"), Result.bAccepted);
    TestTrue(TEXT("transport saved file path is set"), !Result.SavedFilePath.IsEmpty());
    TestTrue(TEXT("transport saved file exists"), IFileManager::Get().FileExists(*Result.SavedFilePath));
    TestTrue(TEXT("transport saved file is under requested directory"), Result.SavedFilePath.Contains(SaveSubDirectory));
    TestEqual(TEXT("transport data length matches cached payload"), Result.DataLength, LidarComp->GetLastJsonPayload().Len());

    FString SavedJson;
    TestTrue(TEXT("saved JSON can be loaded"), FFileHelper::LoadFileToString(SavedJson, *Result.SavedFilePath));
    TestEqual(TEXT("saved JSON matches last payload"), SavedJson, LidarComp->GetLastJsonPayload());

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SavedJson);
    TestTrue(TEXT("saved payload JSON parses"), FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid());
    if (RootObject.IsValid())
    {
        TestEqual(TEXT("saved payload sensor id"), RootObject->GetStringField(TEXT("sensorId")), SensorId);
        TestEqual(TEXT("saved payload point count"), static_cast<int32>(RootObject->GetIntegerField(TEXT("payloadPointCount"))), 24);
    }
    return true;
}

bool FLidarReplayPerformanceWarningTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* ReplayComp = NewObject<ULidarCsvReplaySourceComp>();
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    TestNotNull(TEXT("CSV replay component"), ReplayComp);
    TestNotNull(TEXT("LiDAR component"), LidarComp);
    if (!ReplayComp || !LidarComp)
    {
        return false;
    }

    ReplayComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    ReplayComp->ReplaySemanticLabel = TEXT("Slab");

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    TestTrue(TEXT("CSV frame loads before performance warning test"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-PERF-WARNING");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->SimulationQuality = EVirtualSensorSimulationQuality::FullSpec;
    LidarComp->bUseMultiHit = true;
    LidarComp->bExportCsvOnScan = true;
    LidarComp->bExportJsonLinesOnScan = true;
    LidarComp->bPointCloudPreviewEnabled = true;
    LidarComp->SetPreviewPolicy(1, 0, true);
    LidarComp->InjectPointCloudFrame(Points, false);

    const FString Warning = LidarComp->GetPerformanceWarning();
    const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
    TestFalse(TEXT("performance warning is populated"), Warning.IsEmpty());
    TestEqual(TEXT("runtime status carries performance warning"), Status.PerformanceWarning, Warning);
    TestTrue(TEXT("warning includes fullspec multihit"), Warning.Contains(TEXT("FullSpec+MultiHit")));
    TestTrue(TEXT("warning includes export-on-scan"), Warning.Contains(TEXT("FullSpec export-on-scan")));
    TestTrue(TEXT("warning includes uncapped preview"), Warning.Contains(TEXT("Preview is uncapped")));
    TestTrue(TEXT("runtime status message carries warning"), Status.LastMessage.Contains(TEXT("Warning=")) && Status.LastMessage.Contains(TEXT("FullSpec+MultiHit")));
    return true;
}

bool FLidarLazPlaceholderExportTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* ReplayComp = NewObject<ULidarCsvReplaySourceComp>();
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    TestNotNull(TEXT("CSV replay component"), ReplayComp);
    TestNotNull(TEXT("LiDAR component"), LidarComp);
    if (!ReplayComp || !LidarComp)
    {
        return false;
    }

    ReplayComp->CsvFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Samples/slab_replay_sample.csv"));
    ReplayComp->ReplaySemanticLabel = TEXT("Slab");

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    TestTrue(TEXT("CSV frame loads before LAZ placeholder export"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-LAZ-PLACEHOLDER");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->InjectPointCloudFrame(Points, false);

    const FString Prefix = TEXT("automation_laz_placeholder");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), LidarComp->SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    IFileManager::Get().MakeDirectory(*Directory, true);

    TestTrue(TEXT("LAZ placeholder export succeeds by writing LAS source"), LidarComp->ExportLastPointCloudLaz(Prefix));

    TArray<FString> LasSourceFiles;
    IFileManager::Get().FindFiles(LasSourceFiles, *Directory, TEXT("las"));
    int32 MatchingLasSourceCount = 0;
    for (const FString& FileName : LasSourceFiles)
    {
        if (FileName.StartsWith(Prefix + TEXT("_laz_source_")) && FileName.EndsWith(TEXT(".las")))
        {
            ++MatchingLasSourceCount;
        }
    }

    TArray<FString> LazFiles;
    IFileManager::Get().FindFiles(LazFiles, *Directory, TEXT("laz"));

    TestEqual(TEXT("LAZ placeholder creates one LAS source file"), MatchingLasSourceCount, 1);
    TestEqual(TEXT("LAZ placeholder does not create compressed .laz files"), LazFiles.Num(), 0);
    return true;
}

#endif
