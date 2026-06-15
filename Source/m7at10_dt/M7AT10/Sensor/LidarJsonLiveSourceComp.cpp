#include "m7at10_dt/M7AT10/Sensor/LidarJsonLiveSourceComp.h"

#include "Async/Async.h"
#include "Json.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

namespace
{
bool ReadLiveVectorArray(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FVector& OutVector)
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

void ReadLiveTagString(const FString& TagsText, TArray<FName>& OutTags)
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

ULidarJsonLiveSourceComp::ULidarJsonLiveSourceComp()
{
    PrimaryComponentTick.bCanEverTick = false;
    SourceKind = ERealSensorSourceKind::JsonLiveBridge;
    SourceId = TEXT("JsonLiveLidarBridge");
    LastSourceMessage = TEXT("JSON live LiDAR bridge is ready for buffered JSON lines.");
}

bool ULidarJsonLiveSourceComp::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Running, TEXT("JSON live LiDAR bridge ready."));
    return true;
}

void ULidarJsonLiveSourceComp::StopSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("JSON live LiDAR bridge stopped."));
}

void ULidarJsonLiveSourceComp::AppendJsonLine(const FString& JsonLine)
{
    if (!IsInGameThread())
    {
        TWeakObjectPtr<ULidarJsonLiveSourceComp> WeakThis(this);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, JsonLine]()
        {
            if (WeakThis.IsValid())
            {
                WeakThis->AppendJsonLine(JsonLine);
            }
        });
        return;
    }

    AppendNormalizedLine(JsonLine);
}

void ULidarJsonLiveSourceComp::AppendJsonLines(const FString& JsonLines)
{
    if (!IsInGameThread())
    {
        TWeakObjectPtr<ULidarJsonLiveSourceComp> WeakThis(this);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, JsonLines]()
        {
            if (WeakThis.IsValid())
            {
                WeakThis->AppendJsonLines(JsonLines);
            }
        });
        return;
    }

    TArray<FString> Lines;
    JsonLines.ParseIntoArrayLines(Lines, false);
    for (const FString& Line : Lines)
    {
        AppendNormalizedLine(Line);
    }
}

void ULidarJsonLiveSourceComp::AppendNormalizedLine(const FString& JsonLine)
{
    FString Trimmed = JsonLine;
    Trimmed.TrimStartAndEndInline();
    if (Trimmed.IsEmpty())
    {
        return;
    }

    if (!BufferedJsonLines.IsEmpty())
    {
        BufferedJsonLines += TEXT("\n");
    }
    BufferedJsonLines += Trimmed;
    ++PendingLineCount;
    LastSourceMessage = FString::Printf(TEXT("Buffered JSON live line. pendingLines=%d"), PendingLineCount);
}

void ULidarJsonLiveSourceComp::ClearBufferedFrame()
{
    BufferedJsonLines.Reset();
    PendingLineCount = 0;
    LastSourceMessage = TEXT("JSON live LiDAR buffer cleared.");
}

bool ULidarJsonLiveSourceComp::BuildFrameFromBufferedLines(TArray<FVirtualLidarPoint>& OutPoints) const
{
    OutPoints.Reset();

    TArray<FString> Lines;
    BufferedJsonLines.ParseIntoArrayLines(Lines, false);

    int32 ParsedLineCount = 0;
    int32 DroppedLineCount = 0;
    for (const FString& Line : Lines)
    {
        FVirtualLidarPoint Point;
        if (!ParseJsonPointLine(Line, Point))
        {
            ++DroppedLineCount;
            continue;
        }
        ++ParsedLineCount;

        if (bSkipMissPoints && !Point.bHit)
        {
            ++DroppedLineCount;
            continue;
        }
        if (MaxPointsPerFrame > 0 && OutPoints.Num() >= MaxPointsPerFrame)
        {
            ++DroppedLineCount;
            continue;
        }
        OutPoints.Add(Point);
    }

    const_cast<ULidarJsonLiveSourceComp*>(this)->LastParsedLineCount = ParsedLineCount;
    const_cast<ULidarJsonLiveSourceComp*>(this)->LastDroppedLineCount = DroppedLineCount;
    return OutPoints.Num() > 0;
}

bool ULidarJsonLiveSourceComp::PushFrameOnce(bool bSendTransport)
{
    if (!IsInGameThread())
    {
        QueuePushFrameOnce(bSendTransport);
        return true;
    }

    if (PendingLineCount <= 0)
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("JSON live LiDAR buffer is empty."));
        return false;
    }

    TArray<FVirtualLidarPoint> Points;
    if (!BuildFrameFromBufferedLines(Points))
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, FString::Printf(TEXT("JSON live LiDAR frame contained no usable points. dropped=%d"), LastDroppedLineCount));
        if (bClearBufferAfterPush)
        {
            BufferedJsonLines.Reset();
            PendingLineCount = 0;
        }
        return false;
    }

    const FString SuccessMessage = FString::Printf(TEXT("Pushed JSON live LiDAR frame. points=%d parsed=%d dropped=%d sendTransport=%s"),
        Points.Num(),
        LastParsedLineCount,
        LastDroppedLineCount,
        bSendTransport ? TEXT("true") : TEXT("false"));
    const bool bPushed = PushPointFrameToTarget(Points, bSendTransport, SuccessMessage);
    if (bPushed && bClearBufferAfterPush)
    {
        BufferedJsonLines.Reset();
        PendingLineCount = 0;
    }
    return bPushed;
}

void ULidarJsonLiveSourceComp::QueuePushFrameOnce(bool bSendTransport)
{
    TWeakObjectPtr<ULidarJsonLiveSourceComp> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, bSendTransport]()
    {
        if (WeakThis.IsValid())
        {
            WeakThis->PushFrameOnce(bSendTransport);
        }
    });
}

bool ULidarJsonLiveSourceComp::ParseJsonPointLine(const FString& Line, FVirtualLidarPoint& OutPoint) const
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
    if (!ReadLiveVectorArray(Object, TEXT("worldLocation"), WorldLocation))
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!Object->TryGetNumberField(TEXT("x"), X) || !Object->TryGetNumberField(TEXT("y"), Y) || !Object->TryGetNumberField(TEXT("z"), Z))
        {
            return false;
        }
        WorldLocation.X = static_cast<float>(X);
        WorldLocation.Y = static_cast<float>(Y);
        WorldLocation.Z = static_cast<float>(Z);
    }

    const FVector Origin = ResolveTargetLidar() ? ResolveTargetLidar()->GetComponentLocation() : GetComponentLocation();
    const FTransform DirectionTransform = ResolveTargetLidar() ? ResolveTargetLidar()->GetComponentTransform() : GetComponentTransform();

    OutPoint.WorldLocation = WorldLocation;
    OutPoint.Distance = Object->HasField(TEXT("distance"))
        ? static_cast<float>(Object->GetNumberField(TEXT("distance")))
        : FVector::Distance(Origin, WorldLocation);
    OutPoint.bHit = Object->HasField(TEXT("hit")) ? Object->GetBoolField(TEXT("hit")) : true;
    OutPoint.LocalDirection = DirectionTransform.InverseTransformVectorNoScale(WorldLocation - Origin).GetSafeNormal();
    ReadLiveVectorArray(Object, TEXT("localDirection"), OutPoint.LocalDirection);

    double Row = 0.0;
    double Col = 0.0;
    const bool bHasRowCol = Object->TryGetNumberField(TEXT("row"), Row) && Object->TryGetNumberField(TEXT("col"), Col);
    if (bHasRowCol)
    {
        OutPoint.Row = FMath::RoundToInt(Row);
        OutPoint.Col = FMath::RoundToInt(Col);
    }

    double ReturnIndex = 0.0;
    if (Object->TryGetNumberField(TEXT("returnIndex"), ReturnIndex))
    {
        OutPoint.ReturnIndex = FMath::RoundToInt(ReturnIndex);
    }

    bool bGridCoordValid = bHasRowCol;
    Object->TryGetBoolField(TEXT("gridCoordValid"), bGridCoordValid);
    FString GridCoordSource;
    if (Object->TryGetStringField(TEXT("gridCoordSource"), GridCoordSource))
    {
        bGridCoordValid = bGridCoordValid && GridCoordSource.Equals(TEXT("point_metadata"), ESearchCase::IgnoreCase);
    }
    OutPoint.bHasGridCoord = bGridCoordValid;

    FString ActorName;
    if (Object->TryGetStringField(TEXT("actor"), ActorName) || Object->TryGetStringField(TEXT("hitActor"), ActorName))
    {
        OutPoint.HitActorName = FName(*ActorName);
    }

    FString ActorClassName;
    if (Object->TryGetStringField(TEXT("actorClass"), ActorClassName) || Object->TryGetStringField(TEXT("hitActorClass"), ActorClassName))
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
        ReadLiveTagString(TagsText, OutPoint.HitActorTags);
    }

    return true;
}
