#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLiveSourceComp.h"

#include "Async/Async.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"

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

bool TryGetLiveObjectFieldAny(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, const TSharedPtr<FJsonObject>*& OutObject)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const FString& FieldName : FieldNames)
    {
        if (Object->TryGetObjectField(FieldName, OutObject) && OutObject && OutObject->IsValid())
        {
            return true;
        }
    }
    return false;
}

bool TryGetLiveArrayFieldAny(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, const TArray<TSharedPtr<FJsonValue>>*& OutArray)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const FString& FieldName : FieldNames)
    {
        if (Object->TryGetArrayField(FieldName, OutArray) && OutArray)
        {
            return true;
        }
    }
    return false;
}

bool TryGetLiveStringFieldAny(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, FString& OutValue)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const FString& FieldName : FieldNames)
    {
        if (Object->TryGetStringField(FieldName, OutValue))
        {
            return true;
        }
    }
    return false;
}

FString SerializeLiveJsonObjectLine(const TSharedPtr<FJsonObject>& Object)
{
    if (!Object.IsValid())
    {
        return FString();
    }

    FString Line;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
    FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
    return Line;
}

FString BuildLiveJsonLinesFromPayloadObject(const TSharedPtr<FJsonObject>& PayloadObject)
{
    FString JsonLinesText;
    if (TryGetLiveStringFieldAny(PayloadObject, { TEXT("JSON_LINES"), TEXT("jsonLines"), TEXT("json_lines") }, JsonLinesText))
    {
        JsonLinesText.TrimStartAndEndInline();
        return JsonLinesText;
    }

    const TArray<TSharedPtr<FJsonValue>>* PointValues = nullptr;
    if (!TryGetLiveArrayFieldAny(PayloadObject, { TEXT("POINTS"), TEXT("points"), TEXT("LIDAR_POINTS"), TEXT("lidarPoints") }, PointValues))
    {
        return FString();
    }

    FString JsonLines;
    for (const TSharedPtr<FJsonValue>& PointValue : *PointValues)
    {
        const TSharedPtr<FJsonObject> PointObject = PointValue.IsValid() ? PointValue->AsObject() : nullptr;
        const FString Line = SerializeLiveJsonObjectLine(PointObject);
        if (Line.IsEmpty())
        {
            continue;
        }
        if (!JsonLines.IsEmpty())
        {
            JsonLines += TEXT("\n");
        }
        JsonLines += Line;
    }
    return JsonLines;
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

bool ULidarJsonLiveSourceComp::AppendWebSocketPayload(const FString& PayloadJson)
{
    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Failed to parse WebSocket live LiDAR payload JSON."));
        return false;
    }

    const TSharedPtr<FJsonObject>* DataMapObject = nullptr;
    const TSharedPtr<FJsonObject>* PayloadObject = &RootObject;
    if (TryGetLiveObjectFieldAny(RootObject, { TEXT("DATA_MAP"), TEXT("dataMap"), TEXT("payload"), TEXT("Payload") }, DataMapObject))
    {
        PayloadObject = DataMapObject;
    }

    FString TimestampKey;
    if (TryGetLiveStringFieldAny(RootObject, { TEXT("CREATE_TIMESTAMP"), TEXT("createTimestamp") }, TimestampKey))
    {
        const TSharedPtr<FJsonObject>* TimestampPayload = nullptr;
        if ((*PayloadObject)->TryGetObjectField(TimestampKey, TimestampPayload) && TimestampPayload && TimestampPayload->IsValid())
        {
            PayloadObject = TimestampPayload;
        }
    }

    const FString JsonLines = BuildLiveJsonLinesFromPayloadObject(*PayloadObject);
    if (JsonLines.IsEmpty())
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("WebSocket live LiDAR payload contains no usable POINTS or JSON_LINES."));
        return false;
    }

    AppendJsonLines(JsonLines);
    LastSourceMessage = FString::Printf(TEXT("Buffered WebSocket live LiDAR payload. pendingLines=%d"), PendingLineCount);
    return true;
}

bool ULidarJsonLiveSourceComp::AppendLivePayloadJson(const FString& PayloadJson)
{
    return AppendWebSocketPayload(PayloadJson);
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

void ULidarJsonLiveSourceComp::AppendSampleWebSocketFrameInEditor()
{
    const FString ResolvedPath = ResolveSampleWebSocketPayloadPath();
    FString PayloadJson;
    if (ResolvedPath.IsEmpty() || !FPaths::FileExists(ResolvedPath))
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, FString::Printf(TEXT("WebSocket sample payload not found: %s"), *ResolvedPath));
        return;
    }
    if (!FFileHelper::LoadFileToString(PayloadJson, *ResolvedPath))
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, FString::Printf(TEXT("Failed to read WebSocket sample payload: %s"), *ResolvedPath));
        return;
    }
    ClearBufferedFrame();
    AppendWebSocketPayload(PayloadJson);
}

void ULidarJsonLiveSourceComp::PushBufferedFrameInEditor()
{
    PushFrameOnce(bSendTransportByDefault);
}

void ULidarJsonLiveSourceComp::PushBufferedFrameNoTransportInEditor()
{
    PushFrameOnce(false);
}

void ULidarJsonLiveSourceComp::ClearBufferedFrameInEditor()
{
    ClearBufferedFrame();
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

FString ULidarJsonLiveSourceComp::ResolveSampleWebSocketPayloadPath() const
{
    FString Path = SampleWebSocketPayloadPath;
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
