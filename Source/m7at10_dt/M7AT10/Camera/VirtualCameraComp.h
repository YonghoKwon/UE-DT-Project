#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "VirtualCameraComp.generated.h"

class UTextureRenderTarget2D;
class USensorDataPublisherComp;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualCameraFrameReady, const FString&, JsonPayload);

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
	UFUNCTION(BlueprintCallable, Category = "Sensor|Camera")
	void CaptureAndSendImage();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Camera")
	FString SensorName = TEXT("Camera_01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Camera", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float CaptureInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Camera")
	FIntPoint CaptureResolution = FIntPoint(1280, 720);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Camera", meta = (ClampMin = "10", ClampMax = "100", UIMin = "10", UIMax = "100"))
	int32 JpegQuality = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Camera")
	bool bIncludeImageBase64InPayload = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor|Camera")
	TObjectPtr<UTextureRenderTarget2D> CameraRenderTarget;

	UPROPERTY(BlueprintAssignable, Category = "Sensor|Camera")
	FOnVirtualCameraFrameReady OnFrameReady;

private:
	USensorDataPublisherComp* ResolvePublisher() const;

	FTimerHandle CaptureTimerHandle;
};
