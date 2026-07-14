#if WITH_DEV_AUTOMATION_TESTS

#include "Json.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarCsvReplaySourceComp.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDataTransportComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarCsvReplayLoadTest, "MA0T10.SensorReplay.CsvLoadSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarJsonLinesReplayLoadTest, "MA0T10.SensorReplay.JsonLinesLoadSample", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayInjectFrameTest, "MA0T10.SensorReplay.InjectFrameUpdatesStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayPayloadPolicyJsonTest, "MA0T10.SensorReplay.PayloadPolicyJson", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarPayloadPreviewPolicyBoundaryTest, "MA0T10.SensorReplay.PayloadPreviewPolicyBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarJsonByteFingerprintTest, "MA0T10.SensorReplay.JsonByteFingerprint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayPayloadGridCoordTest, "MA0T10.SensorReplay.PayloadPreservesGridCoord", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayTransportSaveToFileTest, "MA0T10.SensorReplay.TransportSaveToFilePayload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReplayPerformanceWarningTest, "MA0T10.SensorReplay.PerformanceWarningStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarLazPlaceholderExportTest, "MA0T10.SensorReplay.LazPlaceholderWritesLasSource", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarLazExternalCompressorMissingTest, "MA0T10.SensorReplay.LazExternalCompressorMissingFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarLazExternalCompressorFakeWritesOutputTest, "MA0T10.SensorReplay.LazExternalCompressorFakeWritesOutput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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
    TestTrue(TEXT("CSV points preserve grid coord"), Points.Num() > 0 && Points[0].bHasGridCoord);
    TestEqual(TEXT("CSV first row"), Points.Num() > 0 ? Points[0].Row : INDEX_NONE, 0);
    TestEqual(TEXT("CSV first col"), Points.Num() > 0 ? Points[0].Col : INDEX_NONE, 0);
    TestEqual(TEXT("CSV last row"), Points.Num() > 0 ? Points.Last().Row : INDEX_NONE, 3);
    TestEqual(TEXT("CSV last col"), Points.Num() > 0 ? Points.Last().Col : INDEX_NONE, 5);
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
    TestTrue(TEXT("JSONL points preserve grid coord"), Points.Num() > 0 && Points[0].bHasGridCoord);
    TestEqual(TEXT("JSONL first row"), Points.Num() > 0 ? Points[0].Row : INDEX_NONE, 0);
    TestEqual(TEXT("JSONL first col"), Points.Num() > 0 ? Points[0].Col : INDEX_NONE, 0);
    TestEqual(TEXT("JSONL last row"), Points.Num() > 0 ? Points.Last().Row : INDEX_NONE, 2);
    TestEqual(TEXT("JSONL last col"), Points.Num() > 0 ? Points.Last().Col : INDEX_NONE, 5);
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
        const TSharedPtr<FJsonObject> FirstPointObject = JsonPoints->Num() > 0 ? (*JsonPoints)[0]->AsObject() : nullptr;
        TestTrue(TEXT("payload point object exists"), FirstPointObject.IsValid());
        if (FirstPointObject.IsValid())
        {
            TestEqual(TEXT("payload point row from preserved coord"), static_cast<int32>(FirstPointObject->GetIntegerField(TEXT("row"))), 0);
            TestEqual(TEXT("payload point col from preserved coord"), static_cast<int32>(FirstPointObject->GetIntegerField(TEXT("col"))), 0);
            TestEqual(TEXT("payload point returnIndex"), static_cast<int32>(FirstPointObject->GetIntegerField(TEXT("returnIndex"))), 0);
            TestTrue(TEXT("payload point gridCoordValid"), FirstPointObject->GetBoolField(TEXT("gridCoordValid")));
            TestEqual(TEXT("payload point gridCoordSource"), FirstPointObject->GetStringField(TEXT("gridCoordSource")), FString(TEXT("point_metadata")));
        }
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

bool FLidarPayloadPreviewPolicyBoundaryTest::RunTest(const FString& Parameters)
{
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    TestNotNull(TEXT("LiDAR component"), LidarComp);
    if (!LidarComp)
    {
        return false;
    }

    TArray<FVirtualLidarPoint> Points;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        FVirtualLidarPoint Point;
        Point.WorldLocation = FVector(static_cast<float>(Index * 10), 0.0f, 0.0f);
        Point.LocalDirection = FVector::ForwardVector;
        Point.Distance = static_cast<float>(Index * 10);
        Point.bHit = (Index % 2) == 0;
        Point.Row = 0;
        Point.Col = Index;
        Point.bHasGridCoord = true;
        Points.Add(Point);
    }

    LidarComp->HorizontalSamples = Points.Num();
    LidarComp->VerticalChannels = 1;
    LidarComp->SetServerPayloadPolicy(0, -1, false);
    LidarComp->SetPreviewPolicy(0, -1, true);

    TestEqual(TEXT("server stride clamps to one"), LidarComp->ServerPayloadStride, 1);
    TestEqual(TEXT("server max clamps to unlimited zero"), LidarComp->MaxServerPayloadPoints, 0);
    TestEqual(TEXT("preview stride clamps to one"), LidarComp->PreviewPointStride, 1);
    TestEqual(TEXT("preview max clamps to unlimited zero"), LidarComp->MaxPreviewPoints, 0);

    LidarComp->InjectPointCloudFrame(Points, false);
    TestEqual(TEXT("uncapped hit-only server count"), LidarComp->GetLastServerPayloadPointCount(), 4);
    TestEqual(TEXT("uncapped hit-only preview count"), LidarComp->GetLastPreviewPointCount(), 4);

    LidarComp->SetServerPayloadPolicy(2, 3, false);
    TestEqual(TEXT("server policy change preserves preview stride"), LidarComp->PreviewPointStride, 1);
    TestEqual(TEXT("server policy change preserves preview max"), LidarComp->MaxPreviewPoints, 0);
    LidarComp->InjectPointCloudFrame(Points, false);
    TestEqual(TEXT("server stride samples even hit indices and cap stops at three"), LidarComp->GetLastServerPayloadPointCount(), 3);
    TestEqual(TEXT("server policy does not change preview count"), LidarComp->GetLastPreviewPointCount(), 4);

    LidarComp->SetPreviewPolicy(3, 1, true);
    TestEqual(TEXT("preview policy change preserves server stride"), LidarComp->ServerPayloadStride, 2);
    TestEqual(TEXT("preview policy change preserves server max"), LidarComp->MaxServerPayloadPoints, 3);
    LidarComp->InjectPointCloudFrame(Points, false);
    TestEqual(TEXT("preview stride and finite cap apply independently"), LidarComp->GetLastPreviewPointCount(), 1);
    TestEqual(TEXT("preview policy does not change server count"), LidarComp->GetLastServerPayloadPointCount(), 3);

    LidarComp->SetServerPayloadPolicy(3, 0, true);
    LidarComp->InjectPointCloudFrame(Points, false);
    TestEqual(TEXT("include-miss server policy counts all sampled indices"), LidarComp->GetLastServerPayloadPointCount(), 3);
    TestEqual(TEXT("include-miss server policy preserves preview count"), LidarComp->GetLastPreviewPointCount(), 1);
    return true;
}

bool FLidarJsonByteFingerprintTest::RunTest(const FString& Parameters)
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
    TestTrue(TEXT("CSV frame loads before byte fingerprint test"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-JSON-BYTES");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->bEnableSemanticClassification = true;
    LidarComp->bIncludeSlabAnalysisInPayload = false;
    LidarComp->SetServerPayloadPolicy(3, 4, false);
    LidarComp->SetPreviewPolicy(2, 7, true);
    LidarComp->InjectPointCloudFrame(Points, false);

    FString NormalizedPayload = LidarComp->GetLastJsonPayload();
    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(NormalizedPayload);
    TestTrue(TEXT("fingerprint payload parses"), FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid());
    if (!RootObject.IsValid())
    {
        return false;
    }

    const FString Timestamp = RootObject->GetStringField(TEXT("timestampUtc"));
    TestFalse(TEXT("fingerprint payload timestamp exists"), Timestamp.IsEmpty());
    NormalizedPayload.ReplaceInline(*Timestamp, TEXT("<TIMESTAMP>"), ESearchCase::CaseSensitive);

    FTCHARToUTF8 Utf8Payload(*NormalizedPayload);
    uint8 Hash[FSHA1::DigestSize];
    FSHA1::HashBuffer(Utf8Payload.Get(), Utf8Payload.Length(), Hash);
    const FString Fingerprint = BytesToHex(Hash, FSHA1::DigestSize);
    const FString ExpectedFingerprint = TEXT("92A0C0EE3421707720C03E98EBFE4D60ED5E1938");
    AddInfo(FString::Printf(TEXT("Normalized LiDAR JSON SHA1=%s bytes=%d"), *Fingerprint, Utf8Payload.Length()));
    TestEqual(TEXT("normalized raw JSON byte fingerprint"), Fingerprint, ExpectedFingerprint);
    return true;
}

bool FLidarReplayPayloadGridCoordTest::RunTest(const FString& Parameters)
{
    UVirtualLidarSensorComp* LidarComp = NewObject<UVirtualLidarSensorComp>();
    TestNotNull(TEXT("LiDAR component"), LidarComp);
    if (!LidarComp)
    {
        return false;
    }

    TArray<FVirtualLidarPoint> Points;

    FVirtualLidarPoint PreservedPoint;
    PreservedPoint.WorldLocation = FVector(100.0f, 200.0f, 0.0f);
    PreservedPoint.LocalDirection = FVector::ForwardVector;
    PreservedPoint.Distance = 223.6f;
    PreservedPoint.bHit = true;
    PreservedPoint.Row = 7;
    PreservedPoint.Col = 11;
    PreservedPoint.ReturnIndex = 2;
    PreservedPoint.bHasGridCoord = true;
    PreservedPoint.SemanticLabel = FName(TEXT("Slab"));
    Points.Add(PreservedPoint);

    FVirtualLidarPoint DerivedPoint;
    DerivedPoint.WorldLocation = FVector(120.0f, 210.0f, 0.0f);
    DerivedPoint.LocalDirection = FVector::ForwardVector;
    DerivedPoint.Distance = 241.9f;
    DerivedPoint.bHit = true;
    DerivedPoint.bHasGridCoord = false;
    DerivedPoint.SemanticLabel = FName(TEXT("Slab"));
    Points.Add(DerivedPoint);

    LidarComp->SensorId = TEXT("TEST-LIDAR-GRID-COORD");
    LidarComp->HorizontalSamples = 2;
    LidarComp->VerticalChannels = 1;
    LidarComp->SetServerPayloadPolicy(1, 0, false);
    LidarComp->InjectPointCloudFrame(Points, false);

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LidarComp->GetLastJsonPayload());
    TestTrue(TEXT("Payload JSON parses"), FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid());
    if (!RootObject.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonPoints = nullptr;
    TestTrue(TEXT("points array exists"), RootObject->TryGetArrayField(TEXT("points"), JsonPoints) && JsonPoints);
    if (!JsonPoints || JsonPoints->Num() < 2)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> PreservedJson = (*JsonPoints)[0]->AsObject();
    const TSharedPtr<FJsonObject> DerivedJson = (*JsonPoints)[1]->AsObject();
    TestTrue(TEXT("preserved point json exists"), PreservedJson.IsValid());
    TestTrue(TEXT("derived point json exists"), DerivedJson.IsValid());
    if (PreservedJson.IsValid())
    {
        TestEqual(TEXT("preserved row"), static_cast<int32>(PreservedJson->GetIntegerField(TEXT("row"))), 7);
        TestEqual(TEXT("preserved col"), static_cast<int32>(PreservedJson->GetIntegerField(TEXT("col"))), 11);
        TestEqual(TEXT("preserved returnIndex"), static_cast<int32>(PreservedJson->GetIntegerField(TEXT("returnIndex"))), 2);
        TestTrue(TEXT("preserved grid coord valid"), PreservedJson->GetBoolField(TEXT("gridCoordValid")));
        TestEqual(TEXT("preserved grid coord source"), PreservedJson->GetStringField(TEXT("gridCoordSource")), FString(TEXT("point_metadata")));
    }
    if (DerivedJson.IsValid())
    {
        TestEqual(TEXT("derived row"), static_cast<int32>(DerivedJson->GetIntegerField(TEXT("row"))), 0);
        TestEqual(TEXT("derived col"), static_cast<int32>(DerivedJson->GetIntegerField(TEXT("col"))), 1);
        TestFalse(TEXT("derived grid coord invalid"), DerivedJson->GetBoolField(TEXT("gridCoordValid")));
        TestEqual(TEXT("derived grid coord source"), DerivedJson->GetStringField(TEXT("gridCoordSource")), FString(TEXT("derived_from_point_index")));
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
    LidarComp->SetServerPayloadPolicy(1, 0, false);
    LidarComp->SetPreviewPolicy(1, 0, true);
    LidarComp->InjectPointCloudFrame(Points, false);

    const FString Warning = LidarComp->GetPerformanceWarning();
    const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
    TestFalse(TEXT("performance warning is populated"), Warning.IsEmpty());
    TestEqual(TEXT("runtime status carries performance warning"), Status.PerformanceWarning, Warning);
    TestTrue(TEXT("warning includes fullspec multihit"), Warning.Contains(TEXT("FullSpec+MultiHit")));
    TestTrue(TEXT("warning includes export-on-scan"), Warning.Contains(TEXT("FullSpec export-on-scan")));
    TestTrue(TEXT("warning includes uncapped preview"), Warning.Contains(TEXT("Preview is uncapped")));
    TestTrue(TEXT("warning includes uncapped server payload"), Warning.Contains(TEXT("Server payload is uncapped")));
    TestTrue(TEXT("runtime status message carries warning"), Status.LastMessage.Contains(TEXT("Warning=")) && Status.LastMessage.Contains(TEXT("FullSpec+MultiHit")));

    LidarComp->SetServerPayloadPolicy(2, 8, false);
    LidarComp->InjectPointCloudFrame(Points, false);
    const FString CappedWarning = LidarComp->GetPerformanceWarning();
    TestFalse(TEXT("finite server payload cap removes uncapped warning"), CappedWarning.Contains(TEXT("Server payload is uncapped")));
    TestTrue(TEXT("server cap change preserves uncapped preview warning"), CappedWarning.Contains(TEXT("Preview is uncapped")));
    TestEqual(TEXT("server cap change preserves preview stride"), LidarComp->PreviewPointStride, 1);
    TestEqual(TEXT("server cap change preserves preview max"), LidarComp->MaxPreviewPoints, 0);
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
    TestTrue(TEXT("LAZ placeholder telemetry records export attempt"), LidarComp->WasLastLazExportAttempted());
    TestTrue(TEXT("LAZ placeholder telemetry records export success"), LidarComp->DidLastLazExportSucceed());
    TestTrue(TEXT("LAZ placeholder telemetry marks placeholder-only"), LidarComp->WasLastLazExportPlaceholderOnly());
    TestFalse(TEXT("LAZ placeholder telemetry does not request compressor"), LidarComp->WasLastLazExternalCompressorRequested());
    TestFalse(TEXT("LAZ placeholder telemetry does not attempt compressor"), LidarComp->WasLastLazExternalCompressorAttempted());
    TestFalse(TEXT("LAZ placeholder telemetry compressor did not succeed"), LidarComp->DidLastLazExternalCompressorSucceed());
    TestFalse(TEXT("LAZ placeholder telemetry produces no laz output"), LidarComp->DidLastLazProduceOutputFile());
    TestFalse(TEXT("LAZ placeholder telemetry is not true LAZ validated"), LidarComp->WasLastLazTrueCompressionValidated());
    TestEqual(TEXT("LAZ placeholder telemetry records point count"), LidarComp->GetLastLazExportedPointCount(), Rows * Cols);
    TestEqual(TEXT("LAZ placeholder telemetry return code is unset"), LidarComp->GetLastLazExternalCompressorReturnCode(), INDEX_NONE);
    TestEqual(TEXT("LAZ placeholder telemetry output size is zero"), LidarComp->GetLastLazOutputSizeBytes(), static_cast<int64>(0));
    TestTrue(TEXT("LAZ placeholder telemetry records warning"), LidarComp->GetLastLazExportWarningText().Contains(TEXT("LAS-compatible source")));
    TestTrue(TEXT("LAZ placeholder telemetry records LAS source path"), LidarComp->GetLastLazLasSourcePath().Contains(TEXT("_laz_source_")) && LidarComp->GetLastLazLasSourcePath().EndsWith(TEXT(".las")));
    TestTrue(TEXT("LAZ placeholder telemetry records status"), LidarComp->GetLastLazExportStatusText().Contains(TEXT("PlaceholderOnlyLasSource")));

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

bool FLidarLazExternalCompressorMissingTest::RunTest(const FString& Parameters)
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
    TestTrue(TEXT("CSV frame loads before missing external compressor export"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-LAZ-MISSING-COMPRESSOR");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->bUseExternalLazCompressor = true;
    LidarComp->ExternalLazCompressorPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("missing_laz_compressor.exe"));
    LidarComp->InjectPointCloudFrame(Points, false);

    const FString Prefix = TEXT("automation_laz_missing_external");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), LidarComp->SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    IFileManager::Get().MakeDirectory(*Directory, true);

    TestFalse(TEXT("missing external compressor makes LAZ export fail"), LidarComp->ExportLastPointCloudLaz(Prefix));
    TestTrue(TEXT("missing compressor telemetry records export attempt"), LidarComp->WasLastLazExportAttempted());
    TestFalse(TEXT("missing compressor telemetry records export failure"), LidarComp->DidLastLazExportSucceed());
    TestFalse(TEXT("missing compressor telemetry is not placeholder-only"), LidarComp->WasLastLazExportPlaceholderOnly());
    TestTrue(TEXT("missing compressor telemetry records request"), LidarComp->WasLastLazExternalCompressorRequested());
    TestTrue(TEXT("missing compressor telemetry records attempt"), LidarComp->WasLastLazExternalCompressorAttempted());
    TestFalse(TEXT("missing compressor telemetry records failure"), LidarComp->DidLastLazExternalCompressorSucceed());
    TestFalse(TEXT("missing compressor telemetry produces no laz output"), LidarComp->DidLastLazProduceOutputFile());
    TestFalse(TEXT("missing compressor telemetry is not true LAZ validated"), LidarComp->WasLastLazTrueCompressionValidated());
    TestTrue(TEXT("missing compressor telemetry records point count"), LidarComp->GetLastLazExportedPointCount() > 0);
    TestEqual(TEXT("missing compressor telemetry return code is unset"), LidarComp->GetLastLazExternalCompressorReturnCode(), INDEX_NONE);
    TestEqual(TEXT("missing compressor telemetry output size is zero"), LidarComp->GetLastLazOutputSizeBytes(), static_cast<int64>(0));
    TestTrue(TEXT("missing compressor telemetry records warning"), LidarComp->GetLastLazExportWarningText().Contains(TEXT("missing")));
    TestTrue(TEXT("missing compressor telemetry records LAS source path"), LidarComp->GetLastLazLasSourcePath().Contains(TEXT("_laz_source_")) && LidarComp->GetLastLazLasSourcePath().EndsWith(TEXT(".las")));
    TestTrue(TEXT("missing compressor telemetry records output path"), LidarComp->GetLastLazOutputPath().EndsWith(TEXT(".laz")));
    TestTrue(TEXT("missing compressor telemetry records status"), LidarComp->GetLastLazExportStatusText().Contains(TEXT("ExternalCompressorMissing")));

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

    TestEqual(TEXT("missing external compressor still leaves one LAS source"), MatchingLasSourceCount, 1);
    TestEqual(TEXT("missing external compressor does not create .laz files"), LazFiles.Num(), 0);
    return true;
}

bool FLidarLazExternalCompressorFakeWritesOutputTest::RunTest(const FString& Parameters)
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
    TestTrue(TEXT("CSV frame loads before external compressor success export"), ReplayComp->LoadCsvFrame(Points, Rows, Cols));

    LidarComp->SensorId = TEXT("TEST-LIDAR-LAZ-EXTERNAL-SUCCESS");
    LidarComp->HorizontalSamples = Cols;
    LidarComp->VerticalChannels = Rows;
    LidarComp->bUseExternalLazCompressor = true;

    FString CompressorPath = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
    if (CompressorPath.IsEmpty())
    {
        CompressorPath = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")), TEXT("System32"), TEXT("cmd.exe"));
    }
    TestTrue(TEXT("external compressor surrogate exists"), FPaths::FileExists(CompressorPath));
    if (!FPaths::FileExists(CompressorPath))
    {
        return false;
    }

    // This is a process-contract surrogate. It proves {input}/{output} handling,
    // not true LAZ compression.
    LidarComp->ExternalLazCompressorPath = CompressorPath;
    LidarComp->ExternalLazCompressorArguments = TEXT("/C copy /Y {input} {output}");
    LidarComp->InjectPointCloudFrame(Points, false);

    const FString Prefix = TEXT("automation_laz_external_success");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), LidarComp->SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    IFileManager::Get().MakeDirectory(*Directory, true);

    TestTrue(TEXT("external compressor surrogate makes LAZ export succeed"), LidarComp->ExportLastPointCloudLaz(Prefix));
    TestTrue(TEXT("external success telemetry records export attempt"), LidarComp->WasLastLazExportAttempted());
    TestTrue(TEXT("external success telemetry records export success"), LidarComp->DidLastLazExportSucceed());
    TestFalse(TEXT("external success telemetry is not placeholder-only"), LidarComp->WasLastLazExportPlaceholderOnly());
    TestTrue(TEXT("external success telemetry records request"), LidarComp->WasLastLazExternalCompressorRequested());
    TestTrue(TEXT("external success telemetry records attempt"), LidarComp->WasLastLazExternalCompressorAttempted());
    TestTrue(TEXT("external success telemetry records success"), LidarComp->DidLastLazExternalCompressorSucceed());
    TestTrue(TEXT("external success telemetry produces laz output"), LidarComp->DidLastLazProduceOutputFile());
    TestFalse(TEXT("external success telemetry is not true LAZ validated"), LidarComp->WasLastLazTrueCompressionValidated());
    TestTrue(TEXT("external success telemetry records point count"), LidarComp->GetLastLazExportedPointCount() > 0);
    TestEqual(TEXT("external success telemetry records return code"), LidarComp->GetLastLazExternalCompressorReturnCode(), 0);
    TestTrue(TEXT("external success telemetry records output size"), LidarComp->GetLastLazOutputSizeBytes() > 0);
    TestTrue(TEXT("external success telemetry records source path"), LidarComp->GetLastLazLasSourcePath().Contains(TEXT("_laz_source_")) && LidarComp->GetLastLazLasSourcePath().EndsWith(TEXT(".las")));
    TestTrue(TEXT("external success telemetry records output path"), LidarComp->GetLastLazOutputPath().EndsWith(TEXT(".laz")));
    TestTrue(TEXT("external success telemetry records status"), LidarComp->GetLastLazExportStatusText().Contains(TEXT("ExternalCompressorSucceeded")));

    TArray<FString> LasSourceFiles;
    IFileManager::Get().FindFiles(LasSourceFiles, *Directory, TEXT("las"));
    int32 MatchingLasSourceCount = 0;
    FString LasSourcePath;
    for (const FString& FileName : LasSourceFiles)
    {
        if (FileName.StartsWith(Prefix + TEXT("_laz_source_")) && FileName.EndsWith(TEXT(".las")))
        {
            ++MatchingLasSourceCount;
            LasSourcePath = FPaths::Combine(Directory, FileName);
        }
    }

    TArray<FString> LazFiles;
    IFileManager::Get().FindFiles(LazFiles, *Directory, TEXT("laz"));
    int32 MatchingLazCount = 0;
    FString LazOutputPath;
    for (const FString& FileName : LazFiles)
    {
        if (FileName.StartsWith(Prefix + TEXT("_")) && FileName.EndsWith(TEXT(".laz")))
        {
            ++MatchingLazCount;
            LazOutputPath = FPaths::Combine(Directory, FileName);
        }
    }

    TestEqual(TEXT("external compressor success leaves one LAS source"), MatchingLasSourceCount, 1);
    TestEqual(TEXT("external compressor success creates one .laz output"), MatchingLazCount, 1);
    TestTrue(TEXT("LAS source path exists"), !LasSourcePath.IsEmpty() && IFileManager::Get().FileExists(*LasSourcePath));
    TestTrue(TEXT("LAZ output path exists"), !LazOutputPath.IsEmpty() && IFileManager::Get().FileExists(*LazOutputPath));
    if (!LasSourcePath.IsEmpty() && !LazOutputPath.IsEmpty())
    {
        TestFalse(TEXT("LAZ output path differs from LAS source path"), FPaths::ConvertRelativePathToFull(LazOutputPath) == FPaths::ConvertRelativePathToFull(LasSourcePath));
        const int64 LasSize = IFileManager::Get().FileSize(*LasSourcePath);
        const int64 LazSize = IFileManager::Get().FileSize(*LazOutputPath);
        TestTrue(TEXT("LAS source is non-empty"), LasSize > 0);
        TestTrue(TEXT("LAZ surrogate output is non-empty"), LazSize > 0);
        TestEqual(TEXT("copy surrogate output matches LAS source size"), LazSize, LasSize);
    }
    return true;
}

#endif
