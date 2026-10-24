#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorIdentity.h"

#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"

namespace
{
FString ReadPayloadIdentity(const FString& Json)
{
    if(!Json.Contains(VirtualSensorIdentity::LegacyCoilQuay,ESearchCase::CaseSensitive))return Json;
    TSharedPtr<FJsonObject> Object;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object)||!Object.IsValid())return Json;
    bool Changed=false;
    for(const TCHAR* Key:{TEXT("sensorId"),TEXT("sensor_id")})
    {
        FString Id;if(Object->TryGetStringField(Key,Id)&&Id==VirtualSensorIdentity::LegacyCoilQuay)
        {Object->SetStringField(Key,VirtualSensorIdentity::CoilQuay);Changed=true;}
    }
    if(!Changed)return Json;
    FString Result;FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Result));return Result;
}
}

UVirtualSensorRecorderComponent::UVirtualSensorRecorderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualSensorRecorderComponent::StartRecording()
{
    if(GetWorld())for(TActorIterator<AVirtualSensorActorBase> It(GetWorld());It;++It)
        if(It->OutputComponent&&It->OutputComponent->RecorderComponent==this)
        {FString Error;if(!It->PrepareForDataCollection(Error))return;}
    bRecording = true;
}

void UVirtualSensorRecorderComponent::StopRecording()
{
    bRecording = false;
}

void UVirtualSensorRecorderComponent::ClearRecording()
{
    ImuSamples.Reset();ImuOverflow.Reset();
    RecordedFrames.Reset();
    ReplayFrameIndex = 0;
}

void UVirtualSensorRecorderComponent::RecordJsonFrame(const FString& SensorId, const FString& SensorType, int64 FrameId, const FString& PayloadJson)
{
    if (!IsRecordingSensor(SensorId))
    {
        return;
    }

    FVirtualSensorRecordedFrame Frame;
    Frame.SensorId = SensorId;
    Frame.SensorType = SensorType;
    Frame.FrameId = FrameId;
    Frame.TimestampUtc = FDateTime::UtcNow();
    Frame.PayloadJson = PayloadJson;
    RecordedFrames.Add(Frame);
}

bool UVirtualSensorRecorderComponent::SaveSession(const FString& FileNamePrefix) const
{
    TArray<TSharedPtr<FJsonValue>> FramesJson;
    FramesJson.Reserve(RecordedFrames.Num());

    for (const FVirtualSensorRecordedFrame& Frame : RecordedFrames)
    {
        TSharedRef<FJsonObject> FrameObject = MakeShared<FJsonObject>();
        FrameObject->SetStringField(TEXT("sensorId"), Frame.SensorId);
        FrameObject->SetStringField(TEXT("sensorType"), Frame.SensorType);
        FrameObject->SetNumberField(TEXT("frameId"), static_cast<double>(Frame.FrameId));
        FrameObject->SetStringField(TEXT("timestampUtc"), Frame.TimestampUtc.ToIso8601());
        FrameObject->SetStringField(TEXT("payloadJson"), Frame.PayloadJson);
        FramesJson.Add(MakeShared<FJsonValueObject>(FrameObject));
    }

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("sessionType"), TEXT("virtual_sensor_recording"));
    Root->SetStringField(TEXT("createdUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("frameCount"), RecordedFrames.Num());
    Root->SetArrayField(TEXT("frames"), FramesJson);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);

    LastSavedSessionPath = BuildSessionPath(FileNamePrefix);
    return FFileHelper::SaveStringToFile(Output, *LastSavedSessionPath);
}

bool UVirtualSensorRecorderComponent::LoadSessionFromFile(const FString& AbsoluteFilePath)
{
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *AbsoluteFilePath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* FramesJson = nullptr;
    if (!Root->TryGetArrayField(TEXT("frames"), FramesJson) || !FramesJson)
    {
        return false;
    }

    RecordedFrames.Reset();
    for (const TSharedPtr<FJsonValue>& Value : *FramesJson)
    {
        const TSharedPtr<FJsonObject>* FrameObject = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(FrameObject) || !FrameObject || !FrameObject->IsValid())
        {
            continue;
        }

        FVirtualSensorRecordedFrame Frame;
        Frame.SensorId = VirtualSensorIdentity::Canonical((*FrameObject)->GetStringField(TEXT("sensorId")));
        Frame.SensorType = (*FrameObject)->GetStringField(TEXT("sensorType"));
        Frame.FrameId = static_cast<int64>((*FrameObject)->GetNumberField(TEXT("frameId")));
        FDateTime::ParseIso8601(*(*FrameObject)->GetStringField(TEXT("timestampUtc")), Frame.TimestampUtc);
        Frame.PayloadJson = ReadPayloadIdentity((*FrameObject)->GetStringField(TEXT("payloadJson")));
        RecordedFrames.Add(Frame);
    }

    ReplayFrameIndex = 0;
    return true;
}

bool UVirtualSensorRecorderComponent::GetRecordedFrame(int32 FrameIndex, FVirtualSensorRecordedFrame& OutFrame) const
{
    if (!RecordedFrames.IsValidIndex(FrameIndex))
    {
        return false;
    }

    OutFrame = RecordedFrames[FrameIndex];
    return true;
}

void UVirtualSensorRecorderComponent::StartReplay(float FramesPerSecond)
{
    if (!GetWorld() || RecordedFrames.Num() <= 0)
    {
        return;
    }

    ReplayFrameIndex = 0;
    const float Interval = 1.0f / FMath::Max(1.0f, FramesPerSecond);
    GetWorld()->GetTimerManager().ClearTimer(ReplayTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(ReplayTimerHandle, this, &UVirtualSensorRecorderComponent::ReplayNextFrame, Interval, true, 0.0f);
}

void UVirtualSensorRecorderComponent::StopReplay()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReplayTimerHandle);
    }
}

void UVirtualSensorRecorderComponent::ReplayNextFrame()
{
    if (!RecordedFrames.IsValidIndex(ReplayFrameIndex))
    {
        StopReplay();
        return;
    }

    OnReplayFrame.Broadcast(RecordedFrames[ReplayFrameIndex]);
    ++ReplayFrameIndex;
}

FString UVirtualSensorRecorderComponent::BuildSessionPath(const FString& FileNamePrefix) const
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), SaveSubDirectory);
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FString Prefix = FileNamePrefix.IsEmpty() ? TEXT("VirtualSensorSession") : FileNamePrefix;
    const FString Timestamp = FString::Printf(TEXT("%s_%lld"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::UtcNow().GetTicks());
    return FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s.json"), *Prefix, *Timestamp));
}

void UVirtualSensorRecorderComponent::RecordImuSamples(const FString& Id,const TArray<FVirtualImuSample>& Samples)
{
    if(!ImuSamples.Contains(Id)&&ImuSamples.Num()>=16)return;
    auto& Buffer=ImuSamples.FindOrAdd(Id);const int32 Keep=FMath::Min(Samples.Num(),24000-Buffer.Num());
    Buffer.Append(Samples.GetData(),Keep);ImuOverflow.FindOrAdd(Id)+=Samples.Num()-Keep;
}
TArray<FVirtualImuSample> UVirtualSensorRecorderComponent::GetImuSamples(const FString& Id) const
{const auto* P=ImuSamples.Find(Id);return P?*P:TArray<FVirtualImuSample>();}
