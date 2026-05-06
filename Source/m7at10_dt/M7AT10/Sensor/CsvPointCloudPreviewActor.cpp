#include "CsvPointCloudPreviewActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ACsvPointCloudPreviewActor::ACsvPointCloudPreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;

    PointCloudComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CsvPointCloudPreview"));
    SetRootComponent(PointCloudComponent);

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

bool ACsvPointCloudPreviewActor::LoadCsvPointCloud()
{
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
        return false;
    }

    UStaticMesh* MeshToUse = PointMesh;
    if (!MeshToUse)
    {
        MeshToUse = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    }
    if (!MeshToUse)
    {
        MeshToUse = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    }
    if (!MeshToUse)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CsvPointCloudPreview] Could not load default point mesh."));
        return false;
    }
    PointCloudComponent->SetStaticMesh(MeshToUse);

    const int32 SafeStride = FMath::Max(1, PointStride);
    const int32 SafeMaxPoints = FMath::Max(0, MaxPointsToLoad);
    const float SafePointScale = FMath::Max(0.001f, PointScale);
    const float SafeCoordinateScale = FMath::Max(0.001f, CoordinateScale);

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

        if (SafeMaxPoints > 0 && LoadedPointCount >= SafeMaxPoints)
        {
            break;
        }

        Point *= SafeCoordinateScale;
        if (bSkipZeroVectorPoints && Point.IsNearlyZero())
        {
            ++ParsedPointCount;
            continue;
        }

        FVector InstanceLocation = Point;
        if (bTreatCsvCoordinatesAsWorldSpace)
        {
            InstanceLocation = GetActorTransform().InverseTransformPosition(Point);
        }

        FTransform InstanceTransform;
        InstanceTransform.SetLocation(InstanceLocation);
        InstanceTransform.SetRotation(FQuat::Identity);
        InstanceTransform.SetScale3D(FVector(SafePointScale));
        PointCloudComponent->AddInstance(InstanceTransform, false);

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

        ++LoadedPointCount;
        ++ParsedPointCount;
    }

    PointCloudComponent->MarkRenderStateDirty();
    LastLoadedPath = ResolvedPath;

    UE_LOG(LogTemp, Log, TEXT("[CsvPointCloudPreview] Loaded %d points from %s row=%d~%d col=%d~%d bounds min=%s max=%s"),
        LoadedPointCount,
        *ResolvedPath,
        RowRange.X,
        RowRange.Y,
        ColRange.X,
        ColRange.Y,
        *MinBounds.ToString(),
        *MaxBounds.ToString());

    return LoadedPointCount > 0;
}

void ACsvPointCloudPreviewActor::ClearPointCloudPreview()
{
    if (PointCloudComponent)
    {
        PointCloudComponent->ClearInstances();
        PointCloudComponent->MarkRenderStateDirty();
    }
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
    if (!PointCloudComponent)
    {
        return;
    }

    UStaticMesh* MeshToUse = PointMesh;
    if (!MeshToUse)
    {
        MeshToUse = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    }
    if (MeshToUse)
    {
        PointCloudComponent->SetStaticMesh(MeshToUse);
    }

    if (PointMaterial)
    {
        DynamicPointMaterial = UMaterialInstanceDynamic::Create(PointMaterial, this);
        if (DynamicPointMaterial)
        {
            DynamicPointMaterial->SetVectorParameterValue(TEXT("BaseColor"), PointColor);
            DynamicPointMaterial->SetVectorParameterValue(TEXT("Color"), PointColor);
            DynamicPointMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), PointColor);
            PointCloudComponent->SetMaterial(0, DynamicPointMaterial);
        }
        else
        {
            PointCloudComponent->SetMaterial(0, PointMaterial);
        }
    }
}

void ACsvPointCloudPreviewActor::ResetStatus()
{
    LoadedPointCount = 0;
    RowRange = FIntPoint::ZeroValue;
    ColRange = FIntPoint::ZeroValue;
    MinBounds = FVector::ZeroVector;
    MaxBounds = FVector::ZeroVector;
    LastLoadedPath.Reset();
}
