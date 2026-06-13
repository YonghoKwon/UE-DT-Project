#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

namespace
{
bool ReadVectorArray(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FVector& OutVector)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values || Values->Num() < 3)
    {
        return false;
    }

    OutVector.X = static_cast<float>((*Values)[0]->AsNumber());
    OutVector.Y = static_cast<float>((*Values)[1]->AsNumber());
    OutVector.Z = static_cast<float>((*Values)[2]->AsNumber());
    return true;
}

void ReadTagString(const FString& TagsText, TArray<FName>& OutTags)
{
    TArray<FString> Parts;
    TagsText.ParseIntoArray(Parts, TEXT("|"), true);
    for (FString& Part : Parts)
    {
        Part.TrimStartAndEndInline();
        if (!Part.IsEmpty())
        {
            OutTags.Add(FName(*Part));
        }
    }
}
}

ULidarJsonLinesReplaySourceComp::ULidarJsonLinesReplaySourceComp()
{
    PrimaryComponentTick.bCanEverTick = false;
    SourceKind = ERealSensorSourceKind::FileReplay;
    SourceId = TEXT("JsonLinesLidarReplay");
}

void ULidarJsonLinesReplaySourceComp::BeginPlay()
{
    Super::BeginPlay();
}

void ULidarJsonLinesReplaySourceComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopReplay();
    Super::EndPlay(EndPlayReason);
}

bool ULidarJsonLinesReplaySourceComp::LoadJsonLinesFrame(TArray<FVirtualLidarPoint>& OutPoints) const
{
    OutPoints.Reset();
    const FString ResolvedPath = ResolveJsonLinesFilePath();
    const_cast<ULidarJsonLinesReplaySourceComp*>(this)->LastResolvedJsonLinesPath = ResolvedPath;
    if (ResolvedPath.IsEmpty() || !FPaths::FileExists(ResolvedPath))
    {
        const_cast<ULidarJsonLinesReplaySourceComp*>(this)->LastReplayMessage = FString::Printf(TEXT("JSONL file not found: %s"), *ResolvedPath);
        return false;
    }

    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *ResolvedPath))
    {
        const_cast<ULidarJsonLinesReplaySourceComp*>(this)->LastReplayMessage = FString::Printf(TEXT("Failed to read JSONL: %s"), *ResolvedPath);
        return false;
    }

    const int32 SafeStride = FMath::Max(1, PointStride);
    int32 ParsedPointCount = 0;
    for (const FString& Line : Lines)
    {
        FVirtualLidarPoint Point;
        if (!ParseJsonPointLine(Line, Point))
        {
            continue;
        }
        if (bSkipMissPoints && !Point.bHit)
        {
            ++ParsedPointCount;
            continue;
        }
        if ((ParsedPointCount % SafeStride) != 0)
        {
            ++ParsedPointCount;
            continue;
        }

        OutPoints.Add(Point);
        ++ParsedPointCount;
        if (MaxPointsToLoad > 0 && OutPoints.Num() >= MaxPointsToLoad)
        {
            break;
        }
    }

    const_cast<ULidarJsonLinesReplaySourceComp*>(this)->LastLoadedPointCount = OutPoints.Num();
    const_cast<ULidarJsonLinesReplaySourceComp*>(this)->LastReplayMessage = FString::Printf(TEXT("Loaded %d points from %s"), OutPoints.Num(), *ResolvedPath);
    return OutPoints.Num() > 0;
}

bool ULidarJsonLinesReplaySourceComp::PushFrameOnce(bool bSendTransport)
{
    UVirtualLidarSensorComp* LidarComp = ResolveTargetLidar();
    if (!LidarComp)
    {
        LastReplayMessage = TEXT("Target LiDAR is not set.");
        SetSourceState(ERealSensorSourceConnectionState::Error, LastReplayMessage);
        return false;
    }

    TArray<FVirtualLidarPoint> Points;
    if (!LoadJsonLinesFrame(Points))
    {
        return false;
    }

    LastReplayMessage = FString::Printf(TEXT("Pushed JSONL replay frame. points=%d sendTransport=%s"),
        Points.Num(),
        bSendTransport ? TEXT("true") : TEXT("false"));
    return PushPointFrameToTarget(Points, bSendTransport, LastReplayMessage);
}

void ULidarJsonLinesReplaySourceComp::PushFrameOnceInEditor()
{
    PushFrameOnce(bSendTransportByDefault);
}

void ULidarJsonLinesReplaySourceComp::PushFrameOnceNoTransportInEditor()
{
    PushFrameOnce(false);
}

void ULidarJsonLinesReplaySourceComp::StartReplay()
{
    StartSource();
}

bool ULidarJsonLinesReplaySourceComp::StartSource()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("World is not available."));
        return false;
    }

    SetSourceState(ERealSensorSourceConnectionState::Starting, TEXT("JSONL replay starting."));
    bReplayActive = true;
    PushFrameOnce(bSendTransportByDefault);
    World->GetTimerManager().SetTimer(ReplayTimerHandle, this, &ULidarJsonLinesReplaySourceComp::PushFrameFromTimer, FMath::Max(0.033f, ReplayInterval), true);
    SetSourceState(ERealSensorSourceConnectionState::Running, TEXT("JSONL replay running."));
    return true;
}

void ULidarJsonLinesReplaySourceComp::StartReplayInEditor()
{
    StartReplay();
}

void ULidarJsonLinesReplaySourceComp::StopReplay()
{
    StopSource();
}

void ULidarJsonLinesReplaySourceComp::StopSource()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReplayTimerHandle);
    }
    bReplayActive = false;
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("JSONL replay stopped."));
}

void ULidarJsonLinesReplaySourceComp::StopReplayInEditor()
{
    StopReplay();
}

FString ULidarJsonLinesReplaySourceComp::ResolveJsonLinesFilePath() const
{
    FString Path = JsonLinesFilePath;
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

bool ULidarJsonLinesReplaySourceComp::ParseJsonPointLine(const FString& Line, FVirtualLidarPoint& OutPoint) const
{
    FString Trimmed = Line;
    Trimmed.TrimStartAndEndInline();
    if (Trimmed.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> Object;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
    if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
    {
        return false;
    }

    FVector WorldLocation = FVector::ZeroVector;
    if (!ReadVectorArray(Object, TEXT("worldLocation"), WorldLocation))
    {
        WorldLocation.X = static_cast<float>(Object->GetNumberField(TEXT("x")));
        WorldLocation.Y = static_cast<float>(Object->GetNumberField(TEXT("y")));
        WorldLocation.Z = static_cast<float>(Object->GetNumberField(TEXT("z")));
    }

    const FVector Origin = ResolveTargetLidar() ? ResolveTargetLidar()->GetComponentLocation() : GetComponentLocation();
    const FTransform DirectionTransform = ResolveTargetLidar() ? ResolveTargetLidar()->GetComponentTransform() : GetComponentTransform();

    OutPoint.WorldLocation = WorldLocation;
    OutPoint.Distance = Object->HasField(TEXT("distance"))
        ? static_cast<float>(Object->GetNumberField(TEXT("distance")))
        : FVector::Distance(Origin, WorldLocation);
    OutPoint.bHit = Object->HasField(TEXT("hit")) ? Object->GetBoolField(TEXT("hit")) : true;
    OutPoint.LocalDirection = DirectionTransform.InverseTransformVectorNoScale(WorldLocation - Origin).GetSafeNormal();
    ReadVectorArray(Object, TEXT("localDirection"), OutPoint.LocalDirection);
    FString ActorName;
    if (Object->TryGetStringField(TEXT("actor"), ActorName))
    {
        OutPoint.HitActorName = FName(*ActorName);
    }
    if (Object->TryGetStringField(TEXT("hitActor"), ActorName))
    {
        OutPoint.HitActorName = FName(*ActorName);
    }

    FString ActorClassName;
    if (Object->TryGetStringField(TEXT("actorClass"), ActorClassName))
    {
        OutPoint.HitActorClassName = FName(*ActorClassName);
    }
    if (Object->TryGetStringField(TEXT("hitActorClass"), ActorClassName))
    {
        OutPoint.HitActorClassName = FName(*ActorClassName);
    }

    FString SemanticLabel;
    OutPoint.SemanticLabel = Object->TryGetStringField(TEXT("semanticLabel"), SemanticLabel)
        ? FName(*SemanticLabel)
        : DefaultSemanticLabel;

    FString TagsText;
    if (Object->TryGetStringField(TEXT("tags"), TagsText))
    {
        ReadTagString(TagsText, OutPoint.HitActorTags);
    }
    return true;
}

void ULidarJsonLinesReplaySourceComp::PushFrameFromTimer()
{
    PushFrameOnce(bSendTransportByDefault);
}
