// VirtualCameraComp.h
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "VirtualCameraComp.generated.h"

class UTextureRenderTarget2D;
class USensorDataRelayComp;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualCameraComp : public USceneCaptureComponent2D
{
	GENERATED_BODY()

public:
	UVirtualCameraComp();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 타이머에 의해 주기적으로 호출되어 화면을 캡처하고 데이터를 전송하는 함수
	UFUNCTION()
	void CaptureAndSendImage();

	// 캡처 주기 (단위: 초) - 에디터에서 수정 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	float CaptureInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	FString SensorId = TEXT("camera.main");

	// 캡처 해상도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	FIntPoint CaptureResolution = FIntPoint(1280, 720);

	// 이 컴포넌트가 사용할 전용 렌더 타겟
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	TObjectPtr<UTextureRenderTarget2D> CameraRenderTarget;

private:
	void ResolveRelayComponent();

private:
	UPROPERTY(Transient)
	TObjectPtr<USensorDataRelayComp> RelayComp;

	FTimerHandle CaptureTimerHandle;
};
