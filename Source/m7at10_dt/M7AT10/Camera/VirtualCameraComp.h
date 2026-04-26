// VirtualCameraComp.h
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorTypes.h"
#include "VirtualCameraComp.generated.h"

class UTextureRenderTarget2D;

UENUM(BlueprintType)
enum class EVirtualCameraOutputMode : uint8
{
    None UMETA(DisplayName = "None"),
    LogOnly UMETA(DisplayName = "Log Only"),
    SaveJpeg UMETA(DisplayName = "Save JPEG"),
    HttpPost UMETA(DisplayName = "HTTP POST")
};

UENUM(BlueprintType)
enum class EVirtualCameraCaptureMode : uint8
{
    PreviewOnly UMETA(DisplayName = "Preview Only"),
    Payload UMETA(DisplayName = "Payload"),
    PayloadAndOutput UMETA(DisplayName = "Payload And Output")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVirtualCameraFrameCaptured, const FString&, JsonPayload, UTextureRenderTarget2D*, RenderTarget);

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualCameraComp : public USceneCaptureComponent2D
{
    GENERATED_BODY()

public:
    UVirtualCameraComp();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera")
    void StartCapture();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera")
    void StopCapture();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera")
    void CaptureAndSendImage();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera")
    UTextureRenderTarget2D* GetCameraRenderTarget() const { return CameraRenderTarget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera")
    const FVirtualSensorRuntimeStatus& GetRuntimeStatus() const { return RuntimeStatus; }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|VirtualCamera")
    FOnVirtualCameraFrameCaptured OnFrameCaptured;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    FString SensorId = TEXT("VCAM-001");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera", meta = (ClampMin = "0.033"))
    float CaptureInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    FIntPoint CaptureResolution = FIntPoint(1280, 720);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    int32 JpegQuality = 80;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    EVirtualCameraCaptureMode CaptureMode = EVirtualCameraCaptureMode::PayloadAndOutput;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    EVirtualCameraOutputMode OutputMode = EVirtualCameraOutputMode::LogOnly;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera", meta = (EditCondition = "OutputMode == EVirtualCameraOutputMode::HttpPost"))
    FString HttpEndpoint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    bool bAutoStartCapture = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    TObjectPtr<UTextureRenderTarget2D> CameraRenderTarget;

private:
    void EnsureRenderTarget();
    bool ReadRenderTargetAsJpeg(TArray64<uint8>& OutJpegBytes) const;
    FString BuildJsonPayload(const FString& Base64Image, int64 ByteSize) const;
    void DispatchPayload(const FString& JsonPayload, const TArray64<uint8>& JpegBytes) const;
    void PostJson(const FString& JsonPayload) const;
    void SaveJpegToDisk(const TArray64<uint8>& JpegBytes) const;
    void UpdateRuntimeStatus(int32 PayloadLength, const FString& Message);

private:
    FTimerHandle CaptureTimerHandle;
    int64 FrameId = 0;

    UPROPERTY(Transient)
    FVirtualSensorRuntimeStatus RuntimeStatus;
};
