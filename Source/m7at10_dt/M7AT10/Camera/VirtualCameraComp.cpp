#include "VirtualCameraComp.h"

#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "Modules/ModuleManager.h"
#include "Sensor/SensorDataPublisherComp.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TextureResource.h"
#include "TimerManager.h"

UVirtualCameraComp::UVirtualCameraComp()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 수동 타이머 캡처 방식 사용
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
}

void UVirtualCameraComp::BeginPlay()
{
	Super::BeginPlay();

	if (!CameraRenderTarget)
	{
		CameraRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("VirtualCameraRT"));
		CameraRenderTarget->InitCustomFormat(CaptureResolution.X, CaptureResolution.Y, PF_B8G8R8A8, false);
		CameraRenderTarget->UpdateResourceImmediate(true);
	}

	TextureTarget = CameraRenderTarget;

	if (CaptureInterval > 0.0f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(CaptureTimerHandle, this, &UVirtualCameraComp::CaptureAndSendImage, CaptureInterval, true);
	}
}

void UVirtualCameraComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UVirtualCameraComp::CaptureAndSendImage()
{
	if (!CameraRenderTarget)
	{
		return;
	}

	CaptureScene();

	FTextureRenderTargetResource* RTResource = CameraRenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		return;
	}

	TArray<FColor> RawPixels;
	if (!RTResource->ReadPixels(RawPixels) || RawPixels.IsEmpty())
	{
		return;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
	if (!ImageWrapper.IsValid())
	{
		return;
	}

	ImageWrapper->SetRaw(
		RawPixels.GetData(),
		RawPixels.Num() * sizeof(FColor),
		CameraRenderTarget->SizeX,
		CameraRenderTarget->SizeY,
		ERGBFormat::BGRA,
		8);

	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(FMath::Clamp(JpegQuality, 10, 100));
	if (CompressedData.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> PayloadObj = MakeShared<FJsonObject>();
	PayloadObj->SetStringField(TEXT("sensor_type"), TEXT("camera"));
	PayloadObj->SetStringField(TEXT("sensor_name"), SensorName);
	PayloadObj->SetStringField(TEXT("timestamp_utc"), FDateTime::UtcNow().ToIso8601());
	PayloadObj->SetNumberField(TEXT("width"), CameraRenderTarget->SizeX);
	PayloadObj->SetNumberField(TEXT("height"), CameraRenderTarget->SizeY);
	PayloadObj->SetNumberField(TEXT("jpeg_quality"), JpegQuality);

	if (bIncludeImageBase64InPayload)
	{
		const FString Base64Image = FBase64::Encode(CompressedData.GetData(), static_cast<uint32>(CompressedData.Num()));
		PayloadObj->SetStringField(TEXT("image_base64"), Base64Image);
	}

	FString JsonPayload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
	FJsonSerializer::Serialize(PayloadObj.ToSharedRef(), Writer);

	OnFrameReady.Broadcast(JsonPayload);

	if (USensorDataPublisherComp* Publisher = ResolvePublisher())
	{
		Publisher->PublishPacket(TEXT("camera"), SensorName, JsonPayload);
	}
}

USensorDataPublisherComp* UVirtualCameraComp::ResolvePublisher() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<USensorDataPublisherComp>() : nullptr;
}
