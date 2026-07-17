#include "ma0t10_dt/MA0T10/Camera/VirtualCameraPayloadCodec.h"

#include "Json.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> VectorArray(const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Add(MakeShared<FJsonValueNumber>(Value.X));
    Result.Add(MakeShared<FJsonValueNumber>(Value.Y));
    Result.Add(MakeShared<FJsonValueNumber>(Value.Z));
    return Result;
}
}

FString FVirtualCameraPayloadCodec::EncodeJpegBase64(
    const FVirtualCameraPayloadSnapshot& Snapshot,
    const FString& Base64Image,
    int64 ByteSize)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schemaVersion"), TEXT("virtual-camera.v1"));
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_camera"));
    Root->SetStringField(TEXT("sensorId"), Snapshot.SensorId);
    Root->SetStringField(TEXT("manufacturer"), Snapshot.Manufacturer);
    Root->SetStringField(TEXT("model"), Snapshot.Model);
    Root->SetNumberField(TEXT("frameId"), static_cast<double>(Snapshot.FrameId));
    Root->SetStringField(TEXT("timestampUtc"),
        (Snapshot.TimestampUtc == FDateTime()) ? FDateTime::UtcNow().ToIso8601() : Snapshot.TimestampUtc.ToIso8601());
    Root->SetNumberField(TEXT("width"), Snapshot.Width);
    Root->SetNumberField(TEXT("height"), Snapshot.Height);
    Root->SetNumberField(TEXT("byteSize"), static_cast<double>(ByteSize));
    Root->SetNumberField(TEXT("horizontalFov"), Snapshot.HorizontalFov);
    Root->SetNumberField(TEXT("verticalFov"), Snapshot.VerticalFov);
    Root->SetStringField(TEXT("simulationQuality"), Snapshot.SimulationQuality);
    Root->SetStringField(TEXT("encoding"), TEXT("jpeg/base64"));

    TSharedRef<FJsonObject> TransformObject = MakeShared<FJsonObject>();
    const TArray<TSharedPtr<FJsonValue>> Location = VectorArray(Snapshot.Location);
    TransformObject->SetArrayField(TEXT("location"), Location);
    TArray<TSharedPtr<FJsonValue>> Rotation;
    Rotation.Add(MakeShared<FJsonValueNumber>(Snapshot.Rotation.Pitch));
    Rotation.Add(MakeShared<FJsonValueNumber>(Snapshot.Rotation.Yaw));
    Rotation.Add(MakeShared<FJsonValueNumber>(Snapshot.Rotation.Roll));
    TransformObject->SetArrayField(TEXT("rotation"), Rotation);
    TransformObject->SetArrayField(TEXT("forward"), VectorArray(Snapshot.Forward));
    TransformObject->SetArrayField(TEXT("up"), VectorArray(Snapshot.Up));
    Root->SetObjectField(TEXT("sensorTransform"), TransformObject);
    Root->SetArrayField(TEXT("sensorWorldLocation"), Location);
    Root->SetStringField(TEXT("image"), Base64Image);

    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}
