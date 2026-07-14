#include "CsvPointCloudPreviewActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#endif

namespace
{
UMaterialInterface* LoadDefaultPointMaterial()
{
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        return Material;
    }

    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")))
    {
        return Material;
    }

    return nullptr;
}
}

ACsvPointCloudPreviewActor::ACsvPointCloudPreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    ProceduralPointCloudComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralCsvPointCloudPreview"));
    ProceduralPointCloudComponent->SetupAttachment(SceneRoot);
    ProceduralPointCloudComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProceduralPointCloudComponent->bUseAsyncCooking = true;
    ProceduralPointCloudComponent->SetCastShadow(false);

    PointCloudComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CsvPointCloudPreview"));
    PointCloudComponent->SetupAttachment(SceneRoot);
    PointCloudComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PointCloudComponent->SetGenerateOverlapEvents(false);
    PointCloudComponent->SetCastShadow(false);
    PointCloudComponent->bSelectable = false;
}

#if WITH_EDITOR
void ACsvPointCloudPreviewActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    ApplyPreviewStyle();

    if (bAutoLoadOnConstruction && !CsvFilePath.IsEmpty())
    {
        LoadCsvPointCloud();
    }
}
#endif

int32 ACsvPointCloudPreviewActor::GetProceduralPreviewSectionCount() const
{
    return ProceduralPointCloudComponent ? ProceduralPointCloudComponent->GetNumSections() : 0;
}

int32 ACsvPointCloudPreviewActor::GetInstancedPreviewInstanceCount() const
{
    return PointCloudComponent ? PointCloudComponent->GetInstanceCount() : 0;
}

FString ACsvPointCloudPreviewActor::GetLastPreviewTelemetryText() const
{
    return FString::Printf(TEXT("mode=%s requestedMode=%s autoPromoted=%s autoReason=%s lines=%d accepted=%d sections=%d instances=%d parseMs=%.3f buildMs=%.3f totalMs=%.3f status=%s"),
        *LastRenderModeName,
        *LastRequestedRenderModeName,
        bLastRenderModeAutoPromoted ? TEXT("true") : TEXT("false"),
        *LastRenderModeAutoPromotionReason,
        LastInputLineCount,
        LastAcceptedPointCount,
        LastPreviewSectionCount,
        LastPreviewInstanceCount,
        LastParseDurationMs,
        LastBuildDurationMs,
        LastLoadDurationMs,
        *LastPreviewStatus);
}

bool ACsvPointCloudPreviewActor::LoadCsvPointCloud()
{
    const double LoadStartTime = FPlatformTime::Seconds();
    ClearPointCloudPreview();
    ResetStatus();
    ApplyPreviewStyle();

    const FString ResolvedPath = ResolveCsvFilePath();
    if (ResolvedPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CsvPointCloudPreview] CSV file path is empty."));
        return false;
    }

    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *ResolvedPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CsvPointCloudPreview] Failed to read CSV: %s"), *ResolvedPath);
        LastPreviewStatus = TEXT("FailedToReadCsv");
        LastLoadDurationMs = static_cast<float>((FPlatformTime::Seconds() - LoadStartTime) * 1000.0);
        return false;
    }
    LastInputLineCount = Lines.Num();
    const double ParseStartTime = FPlatformTime::Seconds();

    const int32 SafeStride = FMath::Max(1, PointStride);
    const int32 SafeMaxPoints = FMath::Max(0, MaxPointsToLoad);
    const float SafeCoordinateScale = FMath::Max(0.001f, CoordinateScale);

    TArray<FVector> LoadedPoints;
    LoadedPoints.Reserve(SafeMaxPoints > 0 ? FMath::Min(SafeMaxPoints, Lines.Num()) : Lines.Num());

    bool bBoundsInitialized = false;
    bool bIndexRangeInitialized = false;
    int32 ParsedPointCount = 0;

    for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
    {
        int32 Row = 0;
        int32 Col = 0;
        FVector Point = FVector::ZeroVector;
        if (!ParseCsvPointLine(Lines[LineIndex], Row, Col, Point))
        {
            continue;
        }

        if ((ParsedPointCount % SafeStride) != 0)
        {
            ++ParsedPointCount;
            continue;
        }

        if (SafeMaxPoints > 0 && LoadedPoints.Num() >= SafeMaxPoints)
        {
            break;
        }

        Point *= SafeCoordinateScale;
        if (bSkipZeroVectorPoints && Point.IsNearlyZero())
        {
            ++ParsedPointCount;
            continue;
        }

        LoadedPoints.Add(Point);

        if (!bBoundsInitialized)
        {
            MinBounds = Point;
            MaxBounds = Point;
            bBoundsInitialized = true;
        }
        else
        {
            MinBounds.X = FMath::Min(MinBounds.X, Point.X);
            MinBounds.Y = FMath::Min(MinBounds.Y, Point.Y);
            MinBounds.Z = FMath::Min(MinBounds.Z, Point.Z);
            MaxBounds.X = FMath::Max(MaxBounds.X, Point.X);
            MaxBounds.Y = FMath::Max(MaxBounds.Y, Point.Y);
            MaxBounds.Z = FMath::Max(MaxBounds.Z, Point.Z);
        }

        if (!bIndexRangeInitialized)
        {
            RowRange = FIntPoint(Row, Row);
            ColRange = FIntPoint(Col, Col);
            bIndexRangeInitialized = true;
        }
        else
        {
            RowRange.X = FMath::Min(RowRange.X, Row);
            RowRange.Y = FMath::Max(RowRange.Y, Row);
            ColRange.X = FMath::Min(ColRange.X, Col);
            ColRange.Y = FMath::Max(ColRange.Y, Col);
        }

        ++ParsedPointCount;
    }

    LoadedPointCount = LoadedPoints.Num();
    LastAcceptedPointCount = LoadedPointCount;
    LastParseDurationMs = static_cast<float>((FPlatformTime::Seconds() - ParseStartTime) * 1000.0);
    LastRequestedRenderModeName = GetRenderModeName(RenderMode);
    const ECsvPointCloudPreviewRenderMode EffectiveRenderMode = ResolveEffectiveRenderMode(LoadedPointCount);
    const double BuildStartTime = FPlatformTime::Seconds();
    if (EffectiveRenderMode == ECsvPointCloudPreviewRenderMode::ProceduralMesh)
    {
        BuildProceduralPointCloud(LoadedPoints);
    }
    else
    {
        BuildInstancedPointCloud(LoadedPoints);
    }
    LastBuildDurationMs = static_cast<float>((FPlatformTime::Seconds() - BuildStartTime) * 1000.0);
    LastLoadDurationMs = static_cast<float>((FPlatformTime::Seconds() - LoadStartTime) * 1000.0);
    LastRenderModeName = GetRenderModeName(EffectiveRenderMode);
    LastPreviewStatus = LoadedPointCount > 0 ? TEXT("Loaded") : TEXT("NoPointsLoaded");
    if (bLastRenderModeAutoPromoted && LoadedPointCount > 0)
    {
        LastPreviewStatus = TEXT("LoadedAutoPromotedToProcedural");
    }

    LastLoadedPath = ResolvedPath;

    UE_LOG(LogTemp, Log, TEXT("[CsvPointCloudPreview] Loaded %d points from %s mode=%s row=%d~%d col=%d~%d bounds min=%s max=%s telemetry=%s"),
        LoadedPointCount,
        *ResolvedPath,
        *LastRenderModeName,
        RowRange.X,
        RowRange.Y,
        ColRange.X,
        ColRange.Y,
        *MinBounds.ToString(),
        *MaxBounds.ToString(),
        *GetLastPreviewTelemetryText());

    return LoadedPointCount > 0;
}

void ACsvPointCloudPreviewActor::LoadCsvPointCloudInEditor()
{
    LoadCsvPointCloud();
}

void ACsvPointCloudPreviewActor::SelectCsvFileAndLoadInEditor()
{
#if WITH_EDITOR
    FString SelectedPath;
    if (OpenCsvFileDialog(SelectedPath))
    {
        CsvFilePath = SelectedPath;
        LoadCsvPointCloud();
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("[CsvPointCloudPreview] File dialog is only available in editor builds."));
#endif
}

void ACsvPointCloudPreviewActor::ClearPointCloudPreview()
{
    if (PointCloudComponent)
    {
        PointCloudComponent->ClearInstances();
        PointCloudComponent->SetHiddenInGame(true);
        PointCloudComponent->SetVisibility(false, true);
        PointCloudComponent->MarkRenderStateDirty();
    }

    if (ProceduralPointCloudComponent)
    {
        ProceduralPointCloudComponent->ClearAllMeshSections();
        ProceduralPointCloudComponent->SetHiddenInGame(true);
        ProceduralPointCloudComponent->SetVisibility(false, true);
        ProceduralPointCloudComponent->MarkRenderStateDirty();
    }
}

void ACsvPointCloudPreviewActor::ClearPointCloudPreviewInEditor()
{
    ClearPointCloudPreview();
    ResetStatus();
}

FString ACsvPointCloudPreviewActor::ResolveCsvFilePath() const
{
    FString Path = CsvFilePath;
    if (Path.IsEmpty())
    {
        return FString();
    }

    FPaths::NormalizeFilename(Path);

    if (FPaths::FileExists(Path))
    {
        return Path;
    }

    const FString ProjectRelative = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Path);
    if (FPaths::FileExists(ProjectRelative))
    {
        return ProjectRelative;
    }

    const FString SavedRelative = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir(), Path);
    if (FPaths::FileExists(SavedRelative))
    {
        return SavedRelative;
    }

    return Path;
}

bool ACsvPointCloudPreviewActor::ParseCsvPointLine(const FString& Line, int32& OutRow, int32& OutCol, FVector& OutPoint) const
{
    FString Trimmed = Line;
    Trimmed.TrimStartAndEndInline();
    if (Trimmed.IsEmpty())
    {
        return false;
    }

    if (Trimmed.StartsWith(TEXT("row"), ESearchCase::IgnoreCase))
    {
        return false;
    }

    Trimmed.ReplaceInline(TEXT("\t"), TEXT(","));
    TArray<FString> Cells;
    Trimmed.ParseIntoArray(Cells, TEXT(","), true);

    if (Cells.Num() < 5)
    {
        return false;
    }

    for (FString& Cell : Cells)
    {
        Cell.TrimStartAndEndInline();
    }

    if (!Cells[0].IsNumeric() || !Cells[1].IsNumeric())
    {
        return false;
    }

    OutRow = FCString::Atoi(*Cells[0]);
    OutCol = FCString::Atoi(*Cells[1]);
    OutPoint.X = FCString::Atof(*Cells[2]);
    OutPoint.Y = FCString::Atof(*Cells[3]);
    OutPoint.Z = FCString::Atof(*Cells[4]);
    return true;
}

void ACsvPointCloudPreviewActor::ApplyPreviewStyle()
{
    UMaterialInterface* MaterialToUse = PointMaterial ? PointMaterial.Get() : LoadDefaultPointMaterial();
    if (!MaterialToUse)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CsvPointCloudPreview] No point material available. PointColor cannot be applied."));
        return;
    }

    DynamicPointMaterial = UMaterialInstanceDynamic::Create(MaterialToUse, this);
    if (!DynamicPointMaterial)
    {
        if (PointCloudComponent)
        {
            PointCloudComponent->SetMaterial(0, MaterialToUse);
        }
        if (ProceduralPointCloudComponent)
        {
            ProceduralPointCloudComponent->SetMaterial(0, MaterialToUse);
        }
        return;
    }

    DynamicPointMaterial->SetVectorParameterValue(TEXT("Color"), PointColor);
    DynamicPointMaterial->SetVectorParameterValue(TEXT("BaseColor"), PointColor);
    DynamicPointMaterial->SetVectorParameterValue(TEXT("Base Color"), PointColor);
    DynamicPointMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), PointColor);
    DynamicPointMaterial->SetVectorParameterValue(TEXT("Emissive"), PointColor);
    DynamicPointMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.2f);

    if (PointCloudComponent)
    {
        if (UStaticMesh* MeshToUse = ResolvePointMesh())
        {
            PointCloudComponent->SetStaticMesh(MeshToUse);
        }
        PointCloudComponent->SetMaterial(0, DynamicPointMaterial);
    }

    if (ProceduralPointCloudComponent)
    {
        ProceduralPointCloudComponent->SetMaterial(0, DynamicPointMaterial);
    }
}

UStaticMesh* ACsvPointCloudPreviewActor::ResolvePointMesh() const
{
    if (PointMesh)
    {
        return PointMesh;
    }

    if (bUseLowCostCubeWhenPointMeshIsEmpty)
    {
        if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
        {
            return CubeMesh;
        }
    }

    if (UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
    {
        return SphereMesh;
    }

    return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
}

void ACsvPointCloudPreviewActor::BuildProceduralPointCloud(const TArray<FVector>& Points)
{
    if (!ProceduralPointCloudComponent)
    {
        return;
    }

    if (PointCloudComponent)
    {
        PointCloudComponent->ClearInstances();
        PointCloudComponent->SetHiddenInGame(true);
        PointCloudComponent->SetVisibility(false, true);
        LastPreviewInstanceCount = 0;
    }

    ProceduralPointCloudComponent->SetHiddenInGame(false);
    ProceduralPointCloudComponent->SetVisibility(true, true);
    ProceduralPointCloudComponent->ClearAllMeshSections();
    ApplyPreviewStyle();

    const int32 SafeBatchSize = FMath::Clamp(ProceduralBatchSize, 1, 100000);
    const float HalfSize = FMath::Max(0.001f, PointScale) * 50.0f;
    const FVector CameraFacingAxisA(0.0f, HalfSize, 0.0f);
    const FVector CameraFacingAxisB(0.0f, 0.0f, HalfSize);

    int32 SectionIndex = 0;
    for (int32 StartIndex = 0; StartIndex < Points.Num(); StartIndex += SafeBatchSize)
    {
        const int32 Count = FMath::Min(SafeBatchSize, Points.Num() - StartIndex);
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UV0;
        TArray<FProcMeshTangent> Tangents;
        TArray<FLinearColor> VertexColors;

        Vertices.Reserve(Count * 4);
        Triangles.Reserve(Count * 6);
        Normals.Reserve(Count * 4);
        UV0.Reserve(Count * 4);
        Tangents.Reserve(Count * 4);
        VertexColors.Reserve(Count * 4);

        for (int32 LocalIndex = 0; LocalIndex < Count; ++LocalIndex)
        {
            FVector Point = Points[StartIndex + LocalIndex];
            if (bTreatCsvCoordinatesAsWorldSpace)
            {
                Point = GetActorTransform().InverseTransformPosition(Point);
            }

            const int32 BaseVertex = Vertices.Num();
            Vertices.Add(Point - CameraFacingAxisA - CameraFacingAxisB);
            Vertices.Add(Point + CameraFacingAxisA - CameraFacingAxisB);
            Vertices.Add(Point + CameraFacingAxisA + CameraFacingAxisB);
            Vertices.Add(Point - CameraFacingAxisA + CameraFacingAxisB);

            Triangles.Add(BaseVertex + 0);
            Triangles.Add(BaseVertex + 1);
            Triangles.Add(BaseVertex + 2);
            Triangles.Add(BaseVertex + 0);
            Triangles.Add(BaseVertex + 2);
            Triangles.Add(BaseVertex + 3);

            for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
            {
                Normals.Add(FVector::ForwardVector);
                Tangents.Add(FProcMeshTangent(0.0f, 1.0f, 0.0f));
                VertexColors.Add(PointColor);
            }
            UV0.Add(FVector2D(0.0f, 0.0f));
            UV0.Add(FVector2D(1.0f, 0.0f));
            UV0.Add(FVector2D(1.0f, 1.0f));
            UV0.Add(FVector2D(0.0f, 1.0f));
        }

        ProceduralPointCloudComponent->CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);
        if (DynamicPointMaterial)
        {
            ProceduralPointCloudComponent->SetMaterial(SectionIndex, DynamicPointMaterial);
        }
        ++SectionIndex;
    }

    ProceduralPointCloudComponent->MarkRenderStateDirty();
    LastPreviewSectionCount = SectionIndex;
    LastPreviewInstanceCount = 0;
}

ECsvPointCloudPreviewRenderMode ACsvPointCloudPreviewActor::ResolveEffectiveRenderMode(int32 AcceptedPointCount)
{
    bLastRenderModeAutoPromoted = false;
    LastRenderModeAutoPromotionReason.Reset();

    const int32 SafeThreshold = FMath::Max(0, AutoPromoteInstancedToProceduralPointThreshold);
    if (RenderMode == ECsvPointCloudPreviewRenderMode::InstancedMesh &&
        bAutoPromoteLargeInstancedPreviewToProcedural &&
        SafeThreshold > 0 &&
        AcceptedPointCount >= SafeThreshold)
    {
        bLastRenderModeAutoPromoted = true;
        LastRenderModeAutoPromotionReason = FString::Printf(TEXT("Instanced preview accepted %d points; threshold=%d"), AcceptedPointCount, SafeThreshold);
        return ECsvPointCloudPreviewRenderMode::ProceduralMesh;
    }

    return RenderMode;
}

FString ACsvPointCloudPreviewActor::GetRenderModeName(ECsvPointCloudPreviewRenderMode InRenderMode) const
{
    return InRenderMode == ECsvPointCloudPreviewRenderMode::ProceduralMesh ? TEXT("ProceduralMesh") : TEXT("InstancedMesh");
}

void ACsvPointCloudPreviewActor::BuildInstancedPointCloud(const TArray<FVector>& Points)
{
    if (!PointCloudComponent)
    {
        return;
    }

    if (ProceduralPointCloudComponent)
    {
        ProceduralPointCloudComponent->ClearAllMeshSections();
        ProceduralPointCloudComponent->SetHiddenInGame(true);
        ProceduralPointCloudComponent->SetVisibility(false, true);
        LastPreviewSectionCount = 0;
    }

    PointCloudComponent->SetHiddenInGame(false);
    PointCloudComponent->SetVisibility(true, true);
    PointCloudComponent->ClearInstances();

    if (UStaticMesh* MeshToUse = ResolvePointMesh())
    {
        PointCloudComponent->SetStaticMesh(MeshToUse);
    }
    ApplyPreviewStyle();

    const float SafePointScale = FMath::Max(0.001f, PointScale);
    TArray<FTransform> InstanceTransforms;
    InstanceTransforms.Reserve(Points.Num());
    for (FVector Point : Points)
    {
        if (bTreatCsvCoordinatesAsWorldSpace)
        {
            Point = GetActorTransform().InverseTransformPosition(Point);
        }

        FTransform InstanceTransform;
        InstanceTransform.SetLocation(Point);
        InstanceTransform.SetRotation(FQuat::Identity);
        InstanceTransform.SetScale3D(FVector(SafePointScale));
        InstanceTransforms.Add(InstanceTransform);
    }

    if (InstanceTransforms.Num() > 0)
    {
        PointCloudComponent->AddInstances(InstanceTransforms, false, true);
    }
    PointCloudComponent->MarkRenderStateDirty();
    LastPreviewInstanceCount = PointCloudComponent->GetInstanceCount();
    LastPreviewSectionCount = 0;
}

void ACsvPointCloudPreviewActor::ResetStatus()
{
    LoadedPointCount = 0;
    RowRange = FIntPoint::ZeroValue;
    ColRange = FIntPoint::ZeroValue;
    MinBounds = FVector::ZeroVector;
    MaxBounds = FVector::ZeroVector;
    LastLoadedPath.Reset();
    LastInputLineCount = 0;
    LastAcceptedPointCount = 0;
    LastPreviewSectionCount = 0;
    LastPreviewInstanceCount = 0;
    LastParseDurationMs = 0.0f;
    LastBuildDurationMs = 0.0f;
    LastLoadDurationMs = 0.0f;
    LastRenderModeName.Reset();
    LastRequestedRenderModeName.Reset();
    LastPreviewStatus.Reset();
    bLastRenderModeAutoPromoted = false;
    LastRenderModeAutoPromotionReason.Reset();
}

#if WITH_EDITOR
bool ACsvPointCloudPreviewActor::OpenCsvFileDialog(FString& OutFilePath) const
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CsvPointCloudPreview] DesktopPlatform is not available."));
        return false;
    }

    const void* ParentWindowHandle = FSlateApplication::IsInitialized() && FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
        ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
        : nullptr;

    TArray<FString> OutFiles;
    const FString DefaultPath = !CsvFilePath.IsEmpty()
        ? FPaths::GetPath(ResolveCsvFilePath())
        : FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"));

    const bool bOpened = DesktopPlatform->OpenFileDialog(
        ParentWindowHandle,
        TEXT("Select LiDAR Point Cloud CSV"),
        DefaultPath,
        TEXT(""),
        TEXT("CSV files (*.csv)|*.csv|All files (*.*)|*.*"),
        EFileDialogFlags::None,
        OutFiles);

    if (bOpened && OutFiles.Num() > 0)
    {
        OutFilePath = OutFiles[0];
        FPaths::NormalizeFilename(OutFilePath);
        return true;
    }

    return false;
}
#endif
