// VirtualCameraComp.cpp
#include "VirtualCameraComp.h"

#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Json.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TextureResource.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"

UVirtualCameraComp::UVirtualCameraComp()
{
    PrimaryComponentTick.bCanEverTick = false;
    bCaptureEveryFrame = false;
    bCaptureOnMovement = false;
    CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
}

void UVirtualCameraComp::BeginPlay()
{
    Super::BeginPlay();

    if (bApplyDeviceProfileOnBeginPlay)
    {
        ApplyDeviceProfile(DeviceProfile);
    }

    EnsureRenderTarget();
    TryAutoRegisterToManager();

    if (bAutoStartCapture)
    {
        StartCapture();
    }
}

void UVirtualCameraComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCapture();
    Super::EndPlay(EndPlayReason);
}

void UVirtualCameraComp::StartCapture()
{
    EnsureRenderTarget();

    if (!GetWorld() || CaptureInterval <= 0.0f)
    {
        return;
    }

    GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(CaptureTimerHandle, this, &UVirtualCameraComp::CaptureAndSendImage, CaptureInterval, true, 0.0f);
}

void UVirtualCameraComp::StopCapture()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
    }
}

void UVirtualCameraComp::SetTransportComponent(UVirtualSensorDataTransportComp* InTransportComponent)
{
    TransportComponent = InTransportComponent;
}

void UVirtualCameraComp::ApplyDeviceProfile(EVirtualCameraDeviceProfile NewProfile)
{
    DeviceProfile = NewProfile;

    if (DeviceProfile == EVirtualCameraDeviceProfile::IntelRealSenseD455)
    {
        DeviceSpec.Manufacturer = TEXT("Intel");
        DeviceSpec.Model = TEXT("RealSense D455");
        DeviceSpec.HorizontalFovDegrees = 87.0f;
        DeviceSpec.VerticalFovDegrees = 58.0f;
        DeviceSpec.MinRangeCm = 60.0f;
        DeviceSpec.TypicalRangeCm = 600.0f;
        DeviceSpec.MaxRangeCm = 600.0f;
        DeviceSpec.FrameRateHz = 30.0f;
        DeviceSpec.Width = 1280;
        DeviceSpec.Height = 720;
        DeviceSpec.Notes = TEXT("Depth profile reference for Intel RealSense D455. RGB sensor may use a wider FOV.");
        CaptureResolution = FIntPoint(DeviceSpec.Width, DeviceSpec.Height);
        CaptureInterval = 1.0f / FMath::Max(1.0f, DeviceSpec.FrameRateHz);
        FOVAngle = DeviceSpec.HorizontalFovDegrees;
    }
    else
    {
        DeviceSpec.Manufacturer = TEXT("Generic");
        DeviceSpec.Model = TEXT("Generic Camera");
    }
}

void UVirtualCameraComp::EnsureRenderTarget()
{
    if (!CameraRenderTarget)
    {
        CameraRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("VirtualCameraRenderTarget"));
    }

    if (CameraRenderTarget->SizeX != CaptureResolution.X || CameraRenderTarget->SizeY != CaptureResolution.Y)
    {
        CameraRenderTarget->InitCustomFormat(CaptureResolution.X, CaptureResolution.Y, PF_B8G8R8A8, true);
        CameraRenderTarget->ClearColor = FLinearColor::Black;
        CameraRenderTarget->UpdateResourceImmediate(true);
    }

    TextureTarget = CameraRenderTarget;
}

void UVirtualCameraComp::CaptureAndSendImage()
{
    EnsureRenderTarget();
    CaptureScene();
    ++FrameId;

    if (CaptureMode == EVirtualCameraCaptureMode::PreviewOnly)
    {
        UpdateRuntimeStatus(0, TEXT("Preview only"));
        OnFrameCaptured.Broadcast(TEXT(""), CameraRenderTarget);
        return;
    }

    TArray64<uint8> JpegBytes;
    if (!ReadRenderTargetAsJpeg(JpegBytes))
    {
        UpdateRuntimeStatus(0, TEXT("Read render target failed"));
        return;
    }

    const FString Base64Image = FBase64::Encode(JpegBytes.GetData(), static_cast<uint32>(JpegBytes.Num()));
    const FString JsonPayload = BuildJsonPayload(Base64Image, JpegBytes.Num());

    FString StatusMessage = TEXT("Captured");
    if (CaptureMode == EVirtualCameraCaptureMode::PayloadAndOutput)
    {
        DispatchPayload(JsonPayload, JpegBytes);
        if (TransportComponent)
        {
            StatusMessage = TransportComponent->LastResult.Message;
        }
    }

    UpdateRuntimeStatus(JsonPayload.Len(), StatusMessage);
    OnFrameCaptured.Broadcast(JsonPayload, CameraRenderTarget);
}

bool UVirtualCameraComp::ReadRenderTargetAsJpeg(TArray64<uint8>& OutJpegBytes) const
{
    if (!CameraRenderTarget)
    {
        return false;
    }

    FTextureRenderTargetResource* Resource = CameraRenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return false;
    }

    TArray<FColor> RawPixels;
    if (!Resource->ReadPixels(RawPixels) || RawPixels.Num() == 0)
    {
        return false;
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), CameraRenderTarget->SizeX, CameraRenderTarget->SizeY, ERGBFormat::BGRA, 8);
    OutJpegBytes = ImageWrapper->GetCompressed(FMath::Clamp(JpegQuality, 1, 100));
    return OutJpegBytes.Num() > 0;
}

FString UVirtualCameraComp::BuildJsonPayload(const FString& Base64Image, int64 ByteSize) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_camera"));
    Root->SetStringField(TEXT("sensorId"), SensorId);
    Root->SetStringField(TEXT("manufacturer"), DeviceSpec.Manufacturer);
    Root->SetStringField(TEXT("model"), DeviceSpec.Model);
    Root->SetNumberField(TEXT("frameId"), static_cast<double>(FrameId));
    Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("width"), CaptureResolution.X);
    Root->SetNumberField(TEXT("height"), CaptureResolution.Y);
    Root->SetNumberField(TEXT("byteSize"), static_cast<double>(ByteSize));
    Root->SetNumberField(TEXT("horizontalFov"), DeviceSpec.HorizontalFovDegrees);
    Root->SetNumberField(TEXT("verticalFov"), DeviceSpec.VerticalFovDegrees);
    Root->SetStringField(TEXT("encoding"), TEXT("jpeg/base64"));

    TSharedRef<FJsonObject> TransformObject = MakeShared<FJsonObject>();
    const FVector ComponentLocation = GetComponentLocation();
    const FRotator ComponentRotation = GetComponentRotation();
    const FVector Forward = GetForwardVector();
    const FVector Up = GetUpVector();

    TArray<TSharedPtr<FJsonValue>> Location;
    Location.Add(MakeShared<FJsonValueNumber>(ComponentLocation.X));
    Location.Add(MakeShared<FJsonValueNumber>(ComponentLocation.Y));
    Location.Add(MakeShared<FJsonValueNumber>(ComponentLocation.Z));
    TransformObject->SetArrayField(TEXT("location"), Location);

    TArray<TSharedPtr<FJsonValue>> Rotation;
    Rotation.Add(MakeShared<FJsonValueNumber>(ComponentRotation.Pitch));
    Rotation.Add(MakeShared<FJsonValueNumber>(ComponentRotation.Yaw));
    Rotation.Add(MakeShared<FJsonValueNumber>(ComponentRotation.Roll));
    TransformObject->SetArrayField(TEXT("rotation"), Rotation);

    TArray<TSharedPtr<FJsonValue>> ForwardJson;
    ForwardJson.Add(MakeShared<FJsonValueNumber>(Forward.X));
    ForwardJson.Add(MakeShared<FJsonValueNumber>(Forward.Y));
    ForwardJson.Add(MakeShared<FJsonValueNumber>(Forward.Z));
    TransformObject->SetArrayField(TEXT("forward"), ForwardJson);

    TArray<TSharedPtr<FJsonValue>> UpJson;
    UpJson.Add(MakeShared<FJsonValueNumber>(Up.X));
    UpJson.Add(MakeShared<FJsonValueNumber>(Up.Y));
    UpJson.Add(MakeShared<FJsonValueNumber>(Up.Z));
    TransformObject->SetArrayField(TEXT("up"), UpJson);

    Root->SetObjectField(TEXT("sensorTransform"), TransformObject);
    Root->SetArrayField(TEXT("sensorWorldLocation"), Location);
    Root->SetStringField(TEXT("image"), Base64Image);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

void UVirtualCameraComp::UpdateRuntimeStatus(int32 PayloadLength, const FString& Message)
{
    RuntimeStatus.SensorId = SensorId;
    RuntimeStatus.SensorType = TEXT("virtual_camera");
    RuntimeStatus.FrameId = FrameId;
    RuntimeStatus.LastUpdateUtc = FDateTime::UtcNow();
    RuntimeStatus.LastPayloadLength = PayloadLength;
    RuntimeStatus.LastMessage = Message;
}

void UVirtualCameraComp::DispatchPayload(const FString& JsonPayload, const TArray64<uint8>& JpegBytes) const
{
    if (TransportComponent)
    {
        TransportComponent->SendJson(SensorId, TEXT("virtual_camera"), JsonPayload);
        TArray<uint8> Bytes32;
        Bytes32.Append(JpegBytes.GetData(), static_cast<int32>(JpegBytes.Num()));
        TransportComponent->SendBinary(SensorId, TEXT("virtual_camera_frame"), TEXT("jpg"), Bytes32);
        return;
    }

    switch (OutputMode)
    {
    case EVirtualCameraOutputMode::None:
        break;
    case EVirtualCameraOutputMode::LogOnly:
        UE_LOG(LogTemp, Log, TEXT("[VirtualCamera:%s] payloadLength=%d"), *SensorId, JsonPayload.Len());
        break;
    case EVirtualCameraOutputMode::SaveJpeg:
        SaveJpegToDisk(JpegBytes);
        break;
    case EVirtualCameraOutputMode::HttpPost:
        PostJson(JsonPayload);
        break;
    default:
        break;
    }
}

void UVirtualCameraComp::PostJson(const FString& JsonPayload) const
{
    if (HttpEndpoint.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualCamera:%s] HttpEndpoint is empty."), *SensorId);
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(HttpEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonPayload);
    Request->ProcessRequest();
}

void UVirtualCameraComp::SaveJpegToDisk(const TArray64<uint8>& JpegBytes) const
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId);
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FString Timestamp = FString::Printf(TEXT("%s_%lld"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::UtcNow().GetTicks());
    const FString FileName = FString::Printf(TEXT("%s_%s.jpg"), *SensorId, *Timestamp);
    const FString Path = FPaths::Combine(Directory, FileName);

    TArray<uint8> Bytes32;
    Bytes32.Append(JpegBytes.GetData(), static_cast<int32>(JpegBytes.Num()));
    FFileHelper::SaveArrayToFile(Bytes32, *Path);
}

void UVirtualCameraComp::TryAutoRegisterToManager()
{
    if (!bAutoRegisterToManager || !GetWorld())
    {
        return;
    }

    for (TActorIterator<AVirtualSensorManager> It(GetWorld()); It; ++It)
    {
        It->RegisterCamera(this);
        break;
    }
}
