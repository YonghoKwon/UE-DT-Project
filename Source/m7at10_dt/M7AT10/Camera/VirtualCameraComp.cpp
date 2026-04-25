#include "VirtualCameraComp.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Json.h"
#include "Modules/ModuleManager.h"
#include "Misc/Base64.h"
#include "m7at10_dt/M7AT10/Sensor/SensorDataRelayComp.h"
#include "TimerManager.h"
#include "TextureResource.h"

UVirtualCameraComp::UVirtualCameraComp()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 타이머 기반 캡처를 사용하기 위해 자동 캡처를 비활성화
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
}

void UVirtualCameraComp::BeginPlay()
{
	Super::BeginPlay();
	ResolveRelayComponent();

	if (!CameraRenderTarget)
	{
		CameraRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		CameraRenderTarget->InitCustomFormat(CaptureResolution.X, CaptureResolution.Y, PF_B8G8R8A8, true);
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
	if (!RTResource->ReadPixels(RawPixels) || RawPixels.Num() == 0)
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

	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(80);
	FString Base64Image = FBase64::Encode(CompressedData.GetData(), static_cast<uint32>(CompressedData.Num()));

	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetStringField(TEXT("sensor_id"), SensorId);
	RootObj->SetStringField(TEXT("sensor_type"), TEXT("camera"));
	RootObj->SetNumberField(TEXT("timestamp_unix_ms"), static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp()) * 1000.0);
	RootObj->SetNumberField(TEXT("width"), CameraRenderTarget->SizeX);
	RootObj->SetNumberField(TEXT("height"), CameraRenderTarget->SizeY);
	RootObj->SetStringField(TEXT("encoding"), TEXT("jpeg_base64"));
	RootObj->SetStringField(TEXT("image"), Base64Image);

	FString PayloadJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(RootObj, Writer);

	if (!RelayComp)
	{
		ResolveRelayComponent();
	}

	if (RelayComp)
	{
		RelayComp->PublishPayload(SensorId, TEXT("camera"), PayloadJson);
	}
}

void UVirtualCameraComp::ResolveRelayComponent()
{
	RelayComp = GetOwner() ? GetOwner()->FindComponentByClass<USensorDataRelayComp>() : nullptr;
}
