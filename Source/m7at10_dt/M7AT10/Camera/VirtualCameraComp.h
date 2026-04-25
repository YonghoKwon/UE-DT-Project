#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "M7AT10/Sensor/VirtualSensorTypes.h"
#include "VirtualCameraComp.generated.h"

class UTextureRenderTarget2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualCameraPacketReady, const FVirtualSensorPacket&, Packet);

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
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void CaptureAndSendImage();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	float CaptureInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	FIntPoint CaptureResolution = FIntPoint(1280, 720);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	int32 JpegQuality = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	bool bPublishToHttpAutomatically = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	TObjectPtr<UTextureRenderTarget2D> CameraRenderTarget;

	UPROPERTY(BlueprintAssignable, Category = "VirtualCamera")
	FOnVirtualCameraPacketReady OnCameraPacketReady;

private:
	FVirtualSensorPacket BuildCameraPacket(const FString& Base64Image) const;

private:
	FTimerHandle CaptureTimerHandle;
};
