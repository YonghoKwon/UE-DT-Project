#include "VirtualCameraComp.h"

#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "M7AT10/Sensor/VirtualSensorPublisherComp.h"
#include "Modules/ModuleManager.h"
#include "Misc/Base64.h"
#include "TextureResource.h"
#include "m7at10_dt/m7at10_dt.h"

UVirtualCameraComp::UVirtualCameraComp()
{
	PrimaryComponentTick.bCanEverTick = false;
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
}

void UVirtualCameraComp::BeginPlay()
{
	Super::BeginPlay();

	if (!CameraRenderTarget)
	{
		CameraRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		CameraRenderTarget->InitCustomFormat(CaptureResolution.X, CaptureResolution.Y, PF_B8G8R8A8, true);
		CameraRenderTarget->UpdateResourceImmediate(true);
	}

	TextureTarget = CameraRenderTarget;

	if (CaptureInterval > 0.0f)
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
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (!ImageWrapper.IsValid())
	{
		return;
	}

	ImageWrapper->SetRaw(
		RawPixels.GetData(),
		static_cast<int64>(RawPixels.Num() * sizeof(FColor)),
		CameraRenderTarget->SizeX,
		CameraRenderTarget->SizeY,
		ERGBFormat::BGRA,
		8);

	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(FMath::Clamp(JpegQuality, 1, 100));
	if (CompressedData.IsEmpty())
	{
		return;
	}

	const FString Base64Image = FBase64::Encode(CompressedData.GetData(), static_cast<uint32>(CompressedData.Num()));
	const FVirtualSensorPacket Packet = BuildCameraPacket(Base64Image);

	OnCameraPacketReady.Broadcast(Packet);

	if (bPublishToHttpAutomatically)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UVirtualSensorPublisherComp* Publisher = OwnerActor->FindComponentByClass<UVirtualSensorPublisherComp>())
			{
				Publisher->PublishSensorPacket(Packet);
			}
		}
	}

	UE_LOG(LogM7AT10, Verbose, TEXT("[VirtualCameraComp] Captured packet. PayloadLength=%d"), Packet.PayloadJson.Len());
}

FVirtualSensorPacket UVirtualCameraComp::BuildCameraPacket(const FString& Base64Image) const
{
	const int64 TimestampMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
	const FString SensorName = GetName();

	const FString PayloadJson = FString::Printf(
		TEXT("{\"sensorType\":\"camera\",\"sensorName\":\"%s\",\"timestampUnixMs\":%lld,\"width\":%d,\"height\":%d,\"encoding\":\"jpeg_base64\",\"imageData\":\"%s\"}"),
		*SensorName,
		TimestampMs,
		CaptureResolution.X,
		CaptureResolution.Y,
		*Base64Image);

	FVirtualSensorPacket Packet;
	Packet.SensorType = TEXT("camera");
	Packet.SensorName = SensorName;
	Packet.PayloadJson = PayloadJson;
	Packet.TimestampUnixMs = TimestampMs;
	return Packet;
}
