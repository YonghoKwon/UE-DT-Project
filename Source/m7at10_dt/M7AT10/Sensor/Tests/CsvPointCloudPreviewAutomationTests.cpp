#if WITH_DEV_AUTOMATION_TESTS

#include "EngineGlobals.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Sensor/CsvPointCloudPreviewActor.h"

namespace
{
FString WriteGeneratedCsvPointCloud(const FString& FileName, int32 PointCount, int32 Columns)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CsvPointCloudPreview"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    TArray<FString> Lines;
    Lines.Reserve(PointCount + 1);
    Lines.Add(TEXT("row,col,x,y,z"));
    for (int32 Index = 0; Index < PointCount; ++Index)
    {
        const int32 Row = Columns > 0 ? Index / Columns : 0;
        const int32 Col = Columns > 0 ? Index % Columns : Index;
        Lines.Add(FString::Printf(TEXT("%d,%d,%d,%d,%d"), Row, Col, Index, Row, Col));
    }

    const FString Path = FPaths::Combine(Directory, FileName);
    FFileHelper::SaveStringArrayToFile(Lines, *Path);
    return Path;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCsvPointCloudPreviewProceduralHighDensityLoadTest, "M7AT10.Sensor.CsvPointCloudPreview.ProceduralHighDensityLoad", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCsvPointCloudPreviewInstancedBatchLoadTest, "M7AT10.Sensor.CsvPointCloudPreview.InstancedBatchLoad", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCsvPointCloudPreviewProceduralPerformanceBudgetTest, "M7AT10.Sensor.CsvPointCloudPreview.ProceduralPerformanceBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCsvPointCloudPreviewAutoPromoteLargeInstancedTest, "M7AT10.Sensor.CsvPointCloudPreview.AutoPromoteLargeInstanced", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCsvPointCloudPreviewProceduralHighDensityLoadTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    constexpr int32 PointCount = 120000;
    constexpr int32 Columns = 400;
    constexpr int32 BatchSize = 50000;
    const FString CsvPath = WriteGeneratedCsvPointCloud(TEXT("procedural_high_density.csv"), PointCount, Columns);

    ACsvPointCloudPreviewActor* PreviewActor = World->SpawnActor<ACsvPointCloudPreviewActor>();
    TestNotNull(TEXT("CSV preview actor"), PreviewActor);
    if (!PreviewActor)
    {
        return false;
    }

    PreviewActor->CsvFilePath = CsvPath;
    PreviewActor->RenderMode = ECsvPointCloudPreviewRenderMode::ProceduralMesh;
    PreviewActor->ProceduralBatchSize = BatchSize;
    PreviewActor->PointScale = 0.01f;
    PreviewActor->bTreatCsvCoordinatesAsWorldSpace = false;

    const bool bLoaded = PreviewActor->LoadCsvPointCloud();
    TestTrue(TEXT("procedural CSV point cloud loads"), bLoaded);
    TestEqual(TEXT("loaded point count"), PreviewActor->GetLoadedPointCount(), PointCount);
    TestEqual(TEXT("row range min"), PreviewActor->RowRange.X, 0);
    TestEqual(TEXT("row range max"), PreviewActor->RowRange.Y, (PointCount - 1) / Columns);
    TestEqual(TEXT("col range min"), PreviewActor->ColRange.X, 0);
    TestEqual(TEXT("col range max"), PreviewActor->ColRange.Y, Columns - 1);
    TestEqual(TEXT("procedural section count"), PreviewActor->GetProceduralPreviewSectionCount(), 3);
    TestEqual(TEXT("instanced renderer remains inactive"), PreviewActor->GetInstancedPreviewInstanceCount(), 0);
    TestEqual(TEXT("telemetry accepted point count"), PreviewActor->LastAcceptedPointCount, PointCount);
    TestEqual(TEXT("telemetry section count"), PreviewActor->LastPreviewSectionCount, 3);
    TestEqual(TEXT("telemetry instance count"), PreviewActor->LastPreviewInstanceCount, 0);
    TestEqual(TEXT("telemetry active render mode"), PreviewActor->LastRenderModeName, FString(TEXT("ProceduralMesh")));
    TestEqual(TEXT("telemetry status"), PreviewActor->LastPreviewStatus, FString(TEXT("Loaded")));
    TestTrue(TEXT("telemetry parse duration observed"), PreviewActor->LastParseDurationMs >= 0.0f);
    TestTrue(TEXT("telemetry build duration observed"), PreviewActor->LastBuildDurationMs >= 0.0f);
    TestTrue(TEXT("telemetry total duration covers parse"), PreviewActor->LastLoadDurationMs >= PreviewActor->LastParseDurationMs);
    TestTrue(TEXT("telemetry total duration covers build"), PreviewActor->LastLoadDurationMs >= PreviewActor->LastBuildDurationMs);
    const FString ProceduralTelemetry = PreviewActor->GetLastPreviewTelemetryText();
    TestTrue(TEXT("telemetry text includes mode"), ProceduralTelemetry.Contains(TEXT("mode=ProceduralMesh")));
    TestTrue(TEXT("telemetry text includes accepted count"), ProceduralTelemetry.Contains(TEXT("accepted=120000")));
    TestTrue(TEXT("telemetry text includes section count"), ProceduralTelemetry.Contains(TEXT("sections=3")));
    TestTrue(TEXT("telemetry text includes loaded status"), ProceduralTelemetry.Contains(TEXT("status=Loaded")));
    TestEqual(TEXT("min bounds"), PreviewActor->MinBounds, FVector(0.0, 0.0, 0.0));
    TestEqual(TEXT("max bounds"), PreviewActor->MaxBounds, FVector(PointCount - 1, (PointCount - 1) / Columns, Columns - 1));

    PreviewActor->Destroy();
    IFileManager::Get().Delete(*CsvPath);
    return true;
}

bool FCsvPointCloudPreviewInstancedBatchLoadTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    constexpr int32 PointCount = 128;
    constexpr int32 Columns = 16;
    const FString CsvPath = WriteGeneratedCsvPointCloud(TEXT("instanced_batch.csv"), PointCount, Columns);

    ACsvPointCloudPreviewActor* PreviewActor = World->SpawnActor<ACsvPointCloudPreviewActor>();
    TestNotNull(TEXT("CSV preview actor"), PreviewActor);
    if (!PreviewActor)
    {
        return false;
    }

    PreviewActor->CsvFilePath = CsvPath;
    PreviewActor->RenderMode = ECsvPointCloudPreviewRenderMode::InstancedMesh;
    PreviewActor->AutoPromoteInstancedToProceduralPointThreshold = 0;
    PreviewActor->PointScale = 0.01f;
    PreviewActor->bTreatCsvCoordinatesAsWorldSpace = false;

    const bool bLoaded = PreviewActor->LoadCsvPointCloud();
    TestTrue(TEXT("instanced CSV point cloud loads"), bLoaded);
    TestEqual(TEXT("loaded point count"), PreviewActor->GetLoadedPointCount(), PointCount);
    TestEqual(TEXT("instanced preview instance count"), PreviewActor->GetInstancedPreviewInstanceCount(), PointCount);
    TestEqual(TEXT("procedural renderer remains inactive"), PreviewActor->GetProceduralPreviewSectionCount(), 0);
    TestEqual(TEXT("telemetry accepted point count"), PreviewActor->LastAcceptedPointCount, PointCount);
    TestEqual(TEXT("telemetry instance count"), PreviewActor->LastPreviewInstanceCount, PointCount);
    TestEqual(TEXT("telemetry section count"), PreviewActor->LastPreviewSectionCount, 0);
    TestEqual(TEXT("telemetry active render mode"), PreviewActor->LastRenderModeName, FString(TEXT("InstancedMesh")));
    TestEqual(TEXT("telemetry requested render mode"), PreviewActor->LastRequestedRenderModeName, FString(TEXT("InstancedMesh")));
    TestFalse(TEXT("instanced smoke is not auto-promoted"), PreviewActor->WasLastRenderModeAutoPromoted());
    TestEqual(TEXT("telemetry status"), PreviewActor->LastPreviewStatus, FString(TEXT("Loaded")));
    TestTrue(TEXT("telemetry text includes mode"), PreviewActor->GetLastPreviewTelemetryText().Contains(TEXT("mode=InstancedMesh")));

    PreviewActor->Destroy();
    IFileManager::Get().Delete(*CsvPath);
    return true;
}

bool FCsvPointCloudPreviewProceduralPerformanceBudgetTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    constexpr int32 PointCount = 250000;
    constexpr int32 Columns = 500;
    constexpr int32 BatchSize = 50000;
    constexpr float MaxTotalLoadMs = 60000.0f;
    const FString CsvPath = WriteGeneratedCsvPointCloud(TEXT("procedural_performance_budget.csv"), PointCount, Columns);

    ACsvPointCloudPreviewActor* PreviewActor = World->SpawnActor<ACsvPointCloudPreviewActor>();
    TestNotNull(TEXT("CSV preview actor"), PreviewActor);
    if (!PreviewActor)
    {
        return false;
    }

    PreviewActor->CsvFilePath = CsvPath;
    PreviewActor->RenderMode = ECsvPointCloudPreviewRenderMode::ProceduralMesh;
    PreviewActor->ProceduralBatchSize = BatchSize;
    PreviewActor->PointScale = 0.01f;
    PreviewActor->bTreatCsvCoordinatesAsWorldSpace = false;

    const bool bLoaded = PreviewActor->LoadCsvPointCloud();
    TestTrue(TEXT("procedural performance CSV point cloud loads"), bLoaded);
    TestEqual(TEXT("performance loaded point count"), PreviewActor->GetLoadedPointCount(), PointCount);
    TestEqual(TEXT("performance input line count includes header"), PreviewActor->LastInputLineCount, PointCount + 1);
    TestEqual(TEXT("performance accepted point count"), PreviewActor->LastAcceptedPointCount, PointCount);
    TestEqual(TEXT("performance section count"), PreviewActor->LastPreviewSectionCount, 5);
    TestEqual(TEXT("performance instanced renderer remains inactive"), PreviewActor->LastPreviewInstanceCount, 0);
    TestEqual(TEXT("performance active render mode"), PreviewActor->LastRenderModeName, FString(TEXT("ProceduralMesh")));
    TestEqual(TEXT("performance requested render mode"), PreviewActor->LastRequestedRenderModeName, FString(TEXT("ProceduralMesh")));
    TestFalse(TEXT("explicit procedural performance load is not auto-promoted"), PreviewActor->WasLastRenderModeAutoPromoted());
    TestEqual(TEXT("performance status"), PreviewActor->LastPreviewStatus, FString(TEXT("Loaded")));
    TestTrue(TEXT("parse duration observed"), PreviewActor->LastParseDurationMs >= 0.0f);
    TestTrue(TEXT("build duration observed"), PreviewActor->LastBuildDurationMs >= 0.0f);
    TestTrue(TEXT("total duration covers parse"), PreviewActor->LastLoadDurationMs >= PreviewActor->LastParseDurationMs);
    TestTrue(TEXT("total duration covers build"), PreviewActor->LastLoadDurationMs >= PreviewActor->LastBuildDurationMs);
    TestTrue(TEXT("total duration within generous budget"), PreviewActor->LastLoadDurationMs < MaxTotalLoadMs);
    const FString PerformanceTelemetry = PreviewActor->GetLastPreviewTelemetryText();
    TestTrue(TEXT("telemetry text includes mode"), PerformanceTelemetry.Contains(TEXT("mode=ProceduralMesh")));
    TestTrue(TEXT("telemetry text includes accepted count"), PerformanceTelemetry.Contains(TEXT("accepted=250000")));
    TestTrue(TEXT("telemetry text includes section count"), PerformanceTelemetry.Contains(TEXT("sections=5")));
    TestTrue(TEXT("telemetry text includes loaded status"), PerformanceTelemetry.Contains(TEXT("status=Loaded")));

    PreviewActor->Destroy();
    IFileManager::Get().Delete(*CsvPath);
    return true;
}

bool FCsvPointCloudPreviewAutoPromoteLargeInstancedTest::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    TestNotNull(TEXT("editor world"), World);
    if (!World)
    {
        return false;
    }

    constexpr int32 PointCount = 4096;
    constexpr int32 Columns = 64;
    constexpr int32 AutoPromoteThreshold = 1024;
    const FString CsvPath = WriteGeneratedCsvPointCloud(TEXT("auto_promote_instanced.csv"), PointCount, Columns);

    ACsvPointCloudPreviewActor* PreviewActor = World->SpawnActor<ACsvPointCloudPreviewActor>();
    TestNotNull(TEXT("CSV preview actor"), PreviewActor);
    if (!PreviewActor)
    {
        return false;
    }

    PreviewActor->CsvFilePath = CsvPath;
    PreviewActor->RenderMode = ECsvPointCloudPreviewRenderMode::InstancedMesh;
    PreviewActor->bAutoPromoteLargeInstancedPreviewToProcedural = true;
    PreviewActor->AutoPromoteInstancedToProceduralPointThreshold = AutoPromoteThreshold;
    PreviewActor->ProceduralBatchSize = 2048;
    PreviewActor->PointScale = 0.01f;
    PreviewActor->bTreatCsvCoordinatesAsWorldSpace = false;

    const bool bLoaded = PreviewActor->LoadCsvPointCloud();
    TestTrue(TEXT("auto-promoted CSV point cloud loads"), bLoaded);
    TestEqual(TEXT("auto-promoted loaded point count"), PreviewActor->GetLoadedPointCount(), PointCount);
    TestEqual(TEXT("requested render mode remains instanced"), PreviewActor->LastRequestedRenderModeName, FString(TEXT("InstancedMesh")));
    TestEqual(TEXT("effective render mode is procedural"), PreviewActor->LastRenderModeName, FString(TEXT("ProceduralMesh")));
    TestTrue(TEXT("auto-promote flag recorded"), PreviewActor->WasLastRenderModeAutoPromoted());
    TestTrue(TEXT("auto-promote reason records threshold"), PreviewActor->LastRenderModeAutoPromotionReason.Contains(TEXT("threshold=1024")));
    TestEqual(TEXT("auto-promoted procedural section count"), PreviewActor->GetProceduralPreviewSectionCount(), 2);
    TestEqual(TEXT("auto-promoted instanced renderer remains inactive"), PreviewActor->GetInstancedPreviewInstanceCount(), 0);
    TestEqual(TEXT("auto-promoted status"), PreviewActor->LastPreviewStatus, FString(TEXT("LoadedAutoPromotedToProcedural")));
    const FString Telemetry = PreviewActor->GetLastPreviewTelemetryText();
    TestTrue(TEXT("telemetry includes effective mode"), Telemetry.Contains(TEXT("mode=ProceduralMesh")));
    TestTrue(TEXT("telemetry includes requested mode"), Telemetry.Contains(TEXT("requestedMode=InstancedMesh")));
    TestTrue(TEXT("telemetry includes auto promote flag"), Telemetry.Contains(TEXT("autoPromoted=true")));

    PreviewActor->Destroy();
    IFileManager::Get().Delete(*CsvPath);
    return true;
}

#endif
