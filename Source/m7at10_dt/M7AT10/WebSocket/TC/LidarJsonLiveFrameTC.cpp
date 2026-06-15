#include "LidarJsonLiveFrameTC.h"

#include "Async/Async.h"
#include "Json.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLiveSourceComp.h"

namespace
{
bool TryGetObjectFieldAny(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, const TSharedPtr<FJsonObject>*& OutObject)
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

bool TryGetArrayFieldAny(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, const TArray<TSharedPtr<FJsonValue>>*& OutArray)
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

bool TryGetStringFieldAny(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, FString& OutValue)
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

bool TryGetBoolFieldAny(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, bool& OutValue)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const FString& FieldName : FieldNames)
    {
        if (Object->TryGetBoolField(FieldName, OutValue))
        {
            return true;
        }
    }
    return false;
}

FString SerializeJsonObjectLine(const TSharedPtr<FJsonObject>& Object)
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

FString BuildJsonLinesFromPayloadObject(const TSharedPtr<FJsonObject>& PayloadObject)
{
    FString JsonLines;

    FString JsonLinesText;
    if (TryGetStringFieldAny(PayloadObject, { TEXT("JSON_LINES"), TEXT("jsonLines"), TEXT("json_lines") }, JsonLinesText))
    {
        JsonLinesText.TrimStartAndEndInline();
        return JsonLinesText;
    }

    const TArray<TSharedPtr<FJsonValue>>* PointValues = nullptr;
    if (!TryGetArrayFieldAny(PayloadObject, { TEXT("POINTS"), TEXT("points"), TEXT("LIDAR_POINTS"), TEXT("lidarPoints") }, PointValues))
    {
        return FString();
    }

    for (const TSharedPtr<FJsonValue>& PointValue : *PointValues)
    {
        const TSharedPtr<FJsonObject> PointObject = PointValue.IsValid() ? PointValue->AsObject() : nullptr;
        const FString Line = SerializeJsonObjectLine(PointObject);
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

ULidarJsonLiveSourceComp* FindJsonLiveSource(UWorld* World, const FString& SourceId)
{
    if (!World)
    {
        return nullptr;
    }

    ULidarJsonLiveSourceComp* SingleFallbackSource = nullptr;
    int32 FallbackSourceCount = 0;

    for (TObjectIterator<ULidarJsonLiveSourceComp> It; It; ++It)
    {
        ULidarJsonLiveSourceComp* Source = *It;
        if (!IsValid(Source) || Source->GetWorld() != World)
        {
            continue;
        }
        if (SourceId.IsEmpty())
        {
            ++FallbackSourceCount;
            SingleFallbackSource = Source;
            continue;
        }
        if (!Source->SourceId.Equals(SourceId, ESearchCase::IgnoreCase))
        {
            continue;
        }
        return Source;
    }

    if (SourceId.IsEmpty())
    {
        if (FallbackSourceCount == 1)
        {
            return SingleFallbackSource;
        }
        if (FallbackSourceCount > 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LidarJsonLiveFrameTC] SOURCE_ID is required when multiple ULidarJsonLiveSourceComp instances exist. count=%d"), FallbackSourceCount);
        }
    }

    return nullptr;
}
}

ULidarJsonLiveFrameTC::ULidarJsonLiveFrameTC()
{
    TransactionCode = TEXT("LIDAR_JSON_LIVE_FRAME");
}

TSharedPtr<FTransactionCodeDataBase> ULidarJsonLiveFrameTC::ParseToStruct(const FString& JsonString) const
{
    TSharedPtr<FLidarJsonLiveFrameTCData> ParsedData = MakeShared<FLidarJsonLiveFrameTCData>();

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        return nullptr;
    }

    const TSharedPtr<FJsonObject>* DataMapObject = nullptr;
    const TSharedPtr<FJsonObject>* PayloadObject = &RootObject;
    if (TryGetObjectFieldAny(RootObject, { TEXT("DATA_MAP"), TEXT("dataMap"), TEXT("payload"), TEXT("Payload") }, DataMapObject))
    {
        PayloadObject = DataMapObject;
    }

    FString TimestampKey;
    if (TryGetStringFieldAny(RootObject, { TEXT("CREATE_TIMESTAMP"), TEXT("createTimestamp") }, TimestampKey))
    {
        const TSharedPtr<FJsonObject>* TimestampPayload = nullptr;
        if ((*PayloadObject)->TryGetObjectField(TimestampKey, TimestampPayload) && TimestampPayload && TimestampPayload->IsValid())
        {
            PayloadObject = TimestampPayload;
        }
    }

    TryGetStringFieldAny(*PayloadObject, { TEXT("SOURCE_ID"), TEXT("sourceId"), TEXT("sensorId"), TEXT("SENSOR_ID") }, ParsedData->SourceId);
    TryGetBoolFieldAny(*PayloadObject, { TEXT("SEND_TRANSPORT"), TEXT("sendTransport") }, ParsedData->bSendTransport);
    TryGetBoolFieldAny(*PayloadObject, { TEXT("PUSH_FRAME"), TEXT("pushFrame") }, ParsedData->bPushFrame);
    ParsedData->JsonLines = BuildJsonLinesFromPayloadObject(*PayloadObject);

    return ParsedData->JsonLines.IsEmpty() ? nullptr : ParsedData;
}

void ULidarJsonLiveFrameTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<ULidarJsonLiveFrameTC>(this), Data]()
        {
            if (WeakThis.IsValid())
            {
                WeakThis->ProcessStructData(Data);
            }
        });
        return;
    }

    const TSharedPtr<FLidarJsonLiveFrameTCData> ParsedData = StaticCastSharedPtr<FLidarJsonLiveFrameTCData>(Data);
    if (!ParsedData.IsValid())
    {
        return;
    }

    ULidarJsonLiveSourceComp* LiveSource = FindJsonLiveSource(GetWorld(), ParsedData->SourceId);
    if (!LiveSource)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LidarJsonLiveFrameTC] ULidarJsonLiveSourceComp not found. sourceId=%s"), *ParsedData->SourceId);
        return;
    }

    LiveSource->AppendJsonLines(ParsedData->JsonLines);
    if (ParsedData->bPushFrame)
    {
        LiveSource->PushFrameOnce(ParsedData->bSendTransport);
    }
}
