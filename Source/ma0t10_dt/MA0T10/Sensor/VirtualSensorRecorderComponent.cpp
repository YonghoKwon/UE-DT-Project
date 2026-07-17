#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"

#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UVirtualSensorRecorderComponent::UVirtualSensorRecorderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualSensorRecorderComponent::StartRecording()
{
    bRecording = true;
}

void UVirtualSensorRecorderComponent::StopRecording()
{
    bRecording = false;
}

void UVirtualSensorRecorderComponent::ClearRecording()
{
    RecordedFrames.Reset();
    ReplayFrameIndex = 0;
}

void UVirtualSensorRecorderComponent::RecordJsonFrame(const FString& SensorId, const FString& SensorType, int64 FrameId, const FString& PayloadJson)
{
    if (!bRecording)
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
        Frame.SensorId = (*FrameObject)->GetStringField(TEXT("sensorId"));
        Frame.SensorType = (*FrameObject)->GetStringField(TEXT("sensorType"));
        Frame.FrameId = static_cast<int64>((*FrameObject)->GetNumberField(TEXT("frameId")));
        FDateTime::ParseIso8601(*(*FrameObject)->GetStringField(TEXT("timestampUtc")), Frame.TimestampUtc);
        Frame.PayloadJson = (*FrameObject)->GetStringField(TEXT("payloadJson"));
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