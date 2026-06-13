#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

ULidarCsvReplaySourceComp::ULidarCsvReplaySourceComp()
{
    PrimaryComponentTick.bCanEverTick = false;
    SourceKind = ERealSensorSourceKind::FileReplay;
    SourceId = TEXT("CsvLidarReplay");
}

void ULidarCsvReplaySourceComp::BeginPlay()
{
    Super::BeginPlay();
}

void ULidarCsvReplaySourceComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopReplay();
    Super::EndPlay(EndPlayReason);
}

bool ULidarCsvReplaySourceComp::LoadCsvFrame(TArray<FVirtualLidarPoint>& OutPoints, int32& OutRows, int32& OutCols) const
{
    OutPoints.Reset();
    OutRows = 0;
    OutCols = 0;

    const FString ResolvedPath = ResolveCsvFilePath();
    const_cast<ULidarCsvReplaySourceComp*>(this)->LastResolvedCsvPath = ResolvedPath;
    if (ResolvedPath.IsEmpty() || !FPaths::FileExists(ResolvedPath))
    {
        const_cast<ULidarCsvReplaySourceComp*>(this)->LastReplayMessage = FString::Printf(TEXT("CSV file not found: %s"), *ResolvedPath);
        return false;
    }

    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *ResolvedPath))
    {
        const_cast<ULidarCsvReplaySourceComp*>(this)->LastReplayMessage = FString::Printf(TEXT("Failed to read CSV: %s"), *ResolvedPath);
        return false;
    }

    const FVector Origin = ResolveTargetLidar() ? ResolveTargetLidar()->GetComponentLocation() : GetComponentLocation();
    const FTransform DirectionTransform = ResolveTargetLidar() ? ResolveTargetLidar()->GetComponentTransform() : GetComponentTransform();
    const int32 SafeStride = FMath::Max(1, PointStride);
    int32 ParsedPointCount = 0;
    int32 MaxRow = INDEX_NONE;
    int32 MaxCol = INDEX_NONE;

    for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
    {
        FVector ParsedPoint = FVector::ZeroVector;
        int32 Row = 0;
        int32 Col = ParsedPointCount;
        if (!ParseCsvPointLine(Lines[LineIndex], LineIndex, Row, Col, ParsedPoint))
        {
            continue;
        }

        if ((ParsedPointCount % SafeStride) != 0)
        {
            ++ParsedPointCount;
            continue;
        }

        FVector WorldPoint = ParsedPoint * CoordinateScale;
        if (!bTreatCsvCoordinatesAsWorldSpace)
        {
            WorldPoint = GetComponentTransform().TransformPosition(WorldPoint);
        }
        if (bSkipZeroVectorPoints && WorldPoint.IsNearlyZero())
        {
            ++ParsedPointCount;
            continue;
        }

        FVirtualLidarPoint Point;
        Point.WorldLocation = WorldPoint;
        Point.Distance = FVector::Distance(Origin, WorldPoint);
        Point.LocalDirection = DirectionTransform.InverseTransformVectorNoScale(WorldPoint - Origin).GetSafeNormal();
        Point.bHit = true;
        Point.HitActorName = FName(TEXT("CsvReplay"));
        Point.HitActorClassName = FName(TEXT("ReplaySource"));
        Point.SemanticLabel = ReplaySemanticLabel;
        OutPoints.Add(Point);

        MaxRow = FMath::Max(MaxRow, Row);
        MaxCol = FMath::Max(MaxCol, Col);
        ++ParsedPointCount;

        if (MaxPointsToLoad > 0 && OutPoints.Num() >= MaxPointsToLoad)
        {
            break;
        }
    }

    OutRows = MaxRow >= 0 ? MaxRow + 1 : 0;
    OutCols = MaxCol >= 0 ? MaxCol + 1 : OutPoints.Num();
    const_cast<ULidarCsvReplaySourceComp*>(this)->LastLoadedPointCount = OutPoints.Num();
    const_cast<ULidarCsvReplaySourceComp*>(this)->LastReplayMessage = FString::Printf(TEXT("Loaded %d points from %s"), OutPoints.Num(), *ResolvedPath);
    return OutPoints.Num() > 0;
}

bool ULidarCsvReplaySourceComp::PushFrameOnce(bool bSendTransport)
{
    UVirtualLidarSensorComp* LidarComp = ResolveTargetLidar();
    if (!LidarComp)
    {
        LastReplayMessage = TEXT("Target LiDAR is not set.");
        SetSourceState(ERealSensorSourceConnectionState::Error, LastReplayMessage);
        return false;
    }

    TArray<FVirtualLidarPoint> Points;
    int32 Rows = 0;
    int32 Cols = 0;
    if (!LoadCsvFrame(Points, Rows, Cols))
    {
        return false;
    }

    if (bUpdateLidarDimensionsFromCsv && Rows > 0 && Cols > 0)
    {
        LidarComp->VerticalChannels = FMath::Clamp(Rows, 1, 256);
        LidarComp->HorizontalSamples = FMath::Clamp(Cols, 1, 1440);
    }

    LidarComp->InjectPointCloudFrame(Points, bSendTransport);
    LastReplayMessage = FString::Printf(TEXT("Pushed replay frame. points=%d rows=%d cols=%d sendTransport=%s"),
        Points.Num(),
        Rows,
        Cols,
        bSendTransport ? TEXT("true") : TEXT("false"));
    MarkFramePushed(Points.Num(), LastReplayMessage);
    return true;
}

void ULidarCsvReplaySourceComp::PushFrameOnceInEditor()
{
    PushFrameOnce(bSendTransportByDefault);
}

void ULidarCsvReplaySourceComp::PushFrameOnceNoTransportInEditor()
{
    PushFrameOnce(false);
}

void ULidarCsvReplaySourceComp::StartReplay()
{
    StartSource();
}

bool ULidarCsvReplaySourceComp::StartSource()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("World is not available."));
        return false;
    }

    SetSourceState(ERealSensorSourceConnectionState::Starting, TEXT("CSV replay starting."));
    bReplayActive = true;
    PushFrameOnce(bSendTransportByDefault);
    World->GetTimerManager().SetTimer(ReplayTimerHandle, this, &ULidarCsvReplaySourceComp::PushFrameFromTimer, FMath::Max(0.033f, ReplayInterval), true);
    SetSourceState(ERealSensorSourceConnectionState::Running, TEXT("CSV replay running."));
    return true;
}

void ULidarCsvReplaySourceComp::StartReplayInEditor()
{
    StartReplay();
}

void ULidarCsvReplaySourceComp::StopReplay()
{
    StopSource();
}

void ULidarCsvReplaySourceComp::StopSource()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReplayTimerHandle);
    }
    bReplayActive = false;
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("CSV replay stopped."));
}

void ULidarCsvReplaySourceComp::StopReplayInEditor()
{
    StopReplay();
}

FString ULidarCsvReplaySourceComp::ResolveCsvFilePath() const
{
    FString Path = CsvFilePath;
    Path.TrimStartAndEndInline();
    if (Path.IsEmpty())
    {
        return FString();
    }
    if (FPaths::FileExists(Path))
    {
        return FPaths::ConvertRelativePathToFull(Path);
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

bool ULidarCsvReplaySourceComp::ParseCsvPointLine(const FString& Line, int32 LineIndex, int32& OutRow, int32& OutCol, FVector& OutPoint) const
{
    FString Trimmed = Line;
    Trimmed.TrimStartAndEndInline();
    if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("row"), ESearchCase::IgnoreCase) || Trimmed.StartsWith(TEXT("x"), ESearchCase::IgnoreCase))
    {
        return false;
    }

    Trimmed.ReplaceInline(TEXT("\t"), TEXT(","));
    TArray<FString> Cells;
    Trimmed.ParseIntoArray(Cells, TEXT(","), true);
    for (FString& Cell : Cells)
    {
        Cell.TrimStartAndEndInline();
    }

    if (Cells.Num() >= 5)
    {
        OutRow = FCString::Atoi(*Cells[0]);
        OutCol = FCString::Atoi(*Cells[1]);
        OutPoint.X = FCString::Atof(*Cells[2]);
        OutPoint.Y = FCString::Atof(*Cells[3]);
        OutPoint.Z = FCString::Atof(*Cells[4]);
        return true;
    }

    if (Cells.Num() >= 3)
    {
        OutRow = 0;
        OutCol = LineIndex;
        OutPoint.X = FCString::Atof(*Cells[0]);
        OutPoint.Y = FCString::Atof(*Cells[1]);
        OutPoint.Z = FCString::Atof(*Cells[2]);
        return true;
    }

    return false;
}

void ULidarCsvReplaySourceComp::PushFrameFromTimer()
{
    PushFrameOnce(bSendTransportByDefault);
}
