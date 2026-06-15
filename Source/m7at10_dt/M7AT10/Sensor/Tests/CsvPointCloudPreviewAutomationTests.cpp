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
    PreviewActor->PointScale = 0.01f;
    PreviewActor->bTreatCsvCoordinatesAsWorldSpace = false;

    const bool bLoaded = PreviewActor->LoadCsvPointCloud();
    TestTrue(TEXT("instanced CSV point cloud loads"), bLoaded);
    TestEqual(TEXT("loaded point count"), PreviewActor->GetLoadedPointCount(), PointCount);
    TestEqual(TEXT("instanced preview instance count"), PreviewActor->GetInstancedPreviewInstanceCount(), PointCount);
    TestEqual(TEXT("procedural renderer remains inactive"), PreviewActor->GetProceduralPreviewSectionCount(), 0);

    PreviewActor->Destroy();
    IFileManager::Get().Delete(*CsvPath);
    return true;
}

#endif
