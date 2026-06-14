// VirtualCameraComp.h
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorTypes.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDeviceProfileTypes.h"
#include "VirtualCameraComp.generated.h"

class UTextureRenderTarget2D;
class UVirtualSensorDataTransportComp;
class UVirtualSensorRecorderComp;

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

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|DeviceProfile")
    void ApplyDeviceProfile(EVirtualCameraDeviceProfile NewProfile);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|Performance")
    void ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|Transport")
    void SetTransportComponent(UVirtualSensorDataTransportComp* InTransportComponent);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|Recorder")
    void SetRecorderComponent(UVirtualSensorRecorderComp* InRecorderComponent);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera")
    UTextureRenderTarget2D* GetCameraRenderTarget() const { return CameraRenderTarget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera")
    const FVirtualSensorRuntimeStatus& GetRuntimeStatus() const { return RuntimeStatus; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera|Transport")
    const FString& GetLastJsonPayload() const { return LastJsonPayload; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera|DeviceProfile")
    const FVirtualSensorDeviceSpec& GetDeviceSpec() const { return DeviceSpec; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera|Performance")
    EVirtualSensorSimulationQuality GetSimulationQuality() const { return SimulationQuality; }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|VirtualCamera")
    FOnVirtualCameraFrameCaptured OnFrameCaptured;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    FString SensorId = TEXT("VCAM-001");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera|DeviceProfile")
    EVirtualCameraDeviceProfile DeviceProfile = EVirtualCameraDeviceProfile::IntelRealSenseD455;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera|DeviceProfile")
    FVirtualSensorDeviceSpec DeviceSpec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera|Performance")
    EVirtualSensorSimulationQuality SimulationQuality = EVirtualSensorSimulationQuality::RealTimePreview;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera", meta = (ClampMin = "0.033"))
    float CaptureInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    FIntPoint CaptureResolution = FIntPoint(640, 360);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    int32 JpegQuality = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    EVirtualCameraCaptureMode CaptureMode = EVirtualCameraCaptureMode::PayloadAndOutput;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    EVirtualCameraOutputMode OutputMode = EVirtualCameraOutputMode::LogOnly;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera", meta = (EditCondition = "OutputMode == EVirtualCameraOutputMode::HttpPost"))
    FString HttpEndpoint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    bool bAutoStartCapture = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    bool bApplyDeviceProfileOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    bool bAutoRegisterToManager = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    TObjectPtr<UTextureRenderTarget2D> CameraRenderTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera|Transport")
    TObjectPtr<UVirtualSensorDataTransportComp> TransportComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera|Recorder")
    TObjectPtr<UVirtualSensorRecorderComp> RecorderComponent;

private:
    void EnsureRenderTarget();
    bool ReadRenderTargetAsJpeg(TArray64<uint8>& OutJpegBytes) const;
    FString BuildJsonPayload(const FString& Base64Image, int64 ByteSize) const;
    void DispatchPayload(const FString& JsonPayload, const TArray64<uint8>& JpegBytes) const;
    void PostJson(const FString& JsonPayload) const;
    void SaveJpegToDisk(const TArray64<uint8>& JpegBytes) const;
    void UpdateRuntimeStatus(int32 PayloadLength, const FString& Message);
    void TryAutoRegisterToManager();

private:
    FTimerHandle CaptureTimerHandle;
    int64 FrameId = 0;

    UPROPERTY(Transient)
    FVirtualSensorRuntimeStatus RuntimeStatus;

    FString LastJsonPayload;
};
