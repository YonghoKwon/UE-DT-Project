// VirtualCameraComp.cpp
#include "VirtualCameraComp.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Base64.h"
#include "TextureResource.h"
#include "ImageCore.h" // 추가

UVirtualCameraComp::UVirtualCameraComp()
{
	PrimaryComponentTick.bCanEverTick = true;

    // 매 프레임 캡처하거나 움직일 때 캡처하는 기본 동작을 끕니다.
    // (성능 최적화를 위해 우리가 원하는 타이밍에 수동으로 캡처하기 위함)
	bCaptureEveryFrame = true;
	bCaptureOnMovement = true;
}

void UVirtualCameraComp::BeginPlay()
{
	Super::BeginPlay();

	// 1. 렌더 타겟 동적 생성 및 설정
	if (!CameraRenderTarget)
	{
		CameraRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		CameraRenderTarget->InitCustomFormat(CaptureResolution.X, CaptureResolution.Y, PF_B8G8R8A8, true);
		CameraRenderTarget->UpdateResourceImmediate(true);
	}

	// 2. 캡처 컴포넌트에 렌더 타겟 할당
	TextureTarget = CameraRenderTarget;

	bCaptureEveryFrame = true;
	bCaptureOnMovement = true;

	// 3. 주기적으로 캡처를 실행하는 타이머 설정
	if (CaptureInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(CaptureTimerHandle, this, &UVirtualCameraComp::CaptureAndSendImage, CaptureInterval, true);
	}
}

void UVirtualCameraComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
	Super::EndPlay(EndPlayReason);
}
#pragma optimize("", off) // 이 아래부터 최적화를 강제로 꺼서 디버거에서 변수가 보이게 합니다.
void UVirtualCameraComp::CaptureAndSendImage()
{
	if (!CameraRenderTarget) return;

    // 1. 현재 시점을 렌더 타겟에 수동으로 캡처합니다.
    // CaptureScene();

    FTextureRenderTargetResource* RTResource = CameraRenderTarget->GameThread_GetRenderTargetResource();
    if (!RTResource) return;

    // 2. 렌더 타겟에서 픽셀 데이터를 읽어옵니다.
    TArray<FColor> RawPixels;
    if (RTResource->ReadPixels(RawPixels))
    {
		// 3. ImageWrapper 모듈을 사용하여 JPEG 형식으로 압축합니다.
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

		if (ImageWrapper.IsValid())
		{
			// 픽셀 데이터 세팅 (BGRA 8비트)
			ImageWrapper->SetRaw(RawPixels.GetData(), (int64)(RawPixels.Num() * sizeof(FColor)), CameraRenderTarget->SizeX, CameraRenderTarget->SizeY, ERGBFormat::BGRA, 8);

			// [변경 코드 - TArray64 사용 및 Encode 파라미터 수정]
			const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(80);

			// FBase64::Encode는 주로 32비트 길이를 요구하므로, GetData() 포인터와 사이즈를 형변환하여 넘겨줍니다.
			FString Base64Image = FBase64::Encode(CompressedData.GetData(), (uint32)CompressedData.Num());

			// =====================================================================
			// [데이터 전송 로직 작성 구간]
			// 추측입니다만, 추출된 Base64Image 문자열 데이터는 기존 DxApiServiceTest와 유사한
			// HTTP POST 통신을 거치거나, KP1D0012와 같은 WebSocket 전문(Json)을 통해 백엔드로 전달될 것으로 보입니다.
			// 이곳에서 해당 API 호출 시스템(예: ApiSubsystem->SendCameraData(Base64Image))을 연결해주시면 됩니다.
			// =====================================================================

            // 테스트용 로그 (문자열이 너무 길어질 수 있으므로 실제 서비스에서는 제거 권장)
            // UE_LOG(LogTemp, Log, TEXT("Captured Image Base64 Length: %d"), Base64Image.Len());
		}
	}
}
#pragma optimize("", on) // 최적화를 다시 켭니다.