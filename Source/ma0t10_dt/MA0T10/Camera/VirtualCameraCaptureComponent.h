// VirtualCameraCaptureComponent.h
#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorCadence.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDeviceProfileTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualCameraCaptureComponent.generated.h"

class UTextureRenderTarget2D;
class UVirtualSensorTransportComponent;
class UVirtualSensorRecorderComponent;
class UVirtualSensorSchedulerSubsystem;
class FRHIGPUTextureReadback;

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
class MA0T10_DT_API UVirtualCameraCaptureComponent : public USceneCaptureComponent2D, public IVirtualSensorScheduledTask
{
    GENERATED_BODY()

public:
    UVirtualCameraCaptureComponent();

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

    // Called by UVirtualSensorSchedulerSubsystem. Automatic capture uses this
    // bounded asynchronous path; the public one-shot API above remains synchronous.
    bool TickScheduledCapture(double NowSeconds, bool bAllowNewCapture = true);
    void RequestImmediateScheduledCapture();
    bool IsScheduledCaptureDue(double NowSeconds) const
    {
        return CadenceState.IsDue(NowSeconds);
    }
    void MarkBudgetSkippedAcquisition()
    {
        ++RuntimeStatus.BudgetSkippedAcquisitionFrameCount;
    }

	/** Requests JPEG production for a live stream without changing the persisted CaptureMode setting. */
	void SetRuntimeStreamOutputDemand(bool bEnabled, bool bHighThroughputOnly = false)
	{
		bRuntimeStreamOutputDemand = bEnabled;
		bRuntimeHighThroughputStreamDemand = bEnabled && bHighThroughputOnly;
	}
	bool HasRuntimeStreamOutputDemand() const { return bRuntimeStreamOutputDemand; }
	bool HasHighThroughputStreamOutputDemand() const { return bRuntimeHighThroughputStreamDemand; }
	void ResumeRealtimeCadence(double NowMonotonicSeconds, int64 NowUnixNanoseconds);
	const FVirtualSensorCadenceTelemetry& GetCadenceTelemetry() const { return CadenceState.GetTelemetry(); }

    virtual EVirtualSensorKind GetScheduledSensorKind() const override { return EVirtualSensorKind::Camera; }
    virtual bool IsScheduledTaskActive() const override { return IsCaptureRunning(); }
    virtual bool TickScheduledTask(const FVirtualSensorScheduleContext& Context) override
    {
        return TickScheduledCapture(Context.NowSeconds, Context.bAllowNewAcquisition);
    }

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|External")
    bool InjectExternalJsonPayload(const FString& JsonPayload, bool bSendTransport = true);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|DeviceProfile")
    void ApplyDeviceProfile(EVirtualCameraDeviceProfile NewProfile);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|Performance")
    void ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|Transport")
    void SetTransportComponent(UVirtualSensorTransportComponent* InTransportComponent);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualCamera|Recorder")
    void SetRecorderComponent(UVirtualSensorRecorderComponent* InRecorderComponent);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera")
    UTextureRenderTarget2D* GetCameraRenderTarget() const { return CameraRenderTarget; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera")
    const FVirtualSensorRuntimeStatus& GetRuntimeStatus() const { return RuntimeStatus; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera")
    bool IsCaptureRunning() const { return CadenceState.IsRunning(); }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera|Transport")
    const FString& GetLastJsonPayload() const { return LastJsonPayload; }

    TSharedPtr<const TArray64<uint8>, ESPMode::ThreadSafe> GetLastJpegSnapshot() const { return LastJpegSnapshot; }
	const FVirtualCameraJpegMetadata& GetLastJpegMetadata() const { return LastJpegMetadata; }

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

    // Keeps sensor preview geometry and direct lighting while disabling costly
    // cinematic post effects, Lumen GI/reflections, AO, fog, temporal AA and
    // the SceneCapture-only dynamic shadow pass.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera|Performance")
    bool bUsePerformanceOptimizedShowFlags = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    bool bApplyDeviceProfileOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    bool bAutoRegisterToManager = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera")
    TObjectPtr<UTextureRenderTarget2D> CameraRenderTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera|Transport")
    TObjectPtr<UVirtualSensorTransportComponent> TransportComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualCamera|Recorder")
    TObjectPtr<UVirtualSensorRecorderComponent> RecorderComponent;

private:
    void EnsureRenderTarget();
    bool ReadRenderTargetAsJpeg(TArray64<uint8>& OutJpegBytes) const;
    FString BuildJsonPayload(const FString& Base64Image, int64 ByteSize) const;
    void DispatchPayload(const FString& JsonPayload, const TArray64<uint8>& JpegBytes) const;
    void DispatchJsonPayloadOnly(const FString& JsonPayload, const FString& SensorIdOverride = FString()) const;
    void PostJson(const FString& JsonPayload) const;
    void SaveJpegToDisk(const TArray64<uint8>& JpegBytes) const;
    void UpdateRuntimeStatus(int32 PayloadLength, const FString& Message);
    bool ReadExternalPayloadMetadata(const FString& JsonPayload, FString& OutSensorId, int64& OutFrameId, int64& OutByteSize) const;
    void TryAutoRegisterToManager();
    void RegisterWithPerformanceSubsystem();
    void UnregisterFromPerformanceSubsystem();
	void QueuePendingGpuReadbacks();
	bool QueueScheduledGpuReadback(int64 CapturedFrameId, double CaptureStartedSeconds);
    void PollScheduledGpuReadback(double NowSeconds);
    void ReleaseScheduledReadbackOnRenderThread();
	void PumpScheduledEncodeQueue();
	bool StartScheduledEncode(TArray<FColor>&& RawPixels, int32 Width, int32 Height, int64 CapturedFrameId, double CaptureStartedSeconds);
	void CompleteScheduledEncode(int64 CapturedFrameId, int32 Width, int32 Height, int32 Quality, FString&& Checksum, TArray64<uint8>&& JpegBytes, FString&& JsonPayload, double CaptureStartedSeconds);
	void FlushCompletedEncodes();
	bool ShouldGeneratePayload() const { return CaptureMode != EVirtualCameraCaptureMode::PreviewOnly || bRuntimeStreamOutputDemand; }
	bool ShouldBuildCompatibilityJson() const { return CaptureMode != EVirtualCameraCaptureMode::PreviewOnly || !bRuntimeHighThroughputStreamDemand; }

private:
    FTimerHandle CaptureTimerHandle;
    int64 FrameId = 0;

    UPROPERTY(Transient)
    FVirtualSensorRuntimeStatus RuntimeStatus;

    FString LastJsonPayload;
    TSharedPtr<const TArray64<uint8>, ESPMode::ThreadSafe> LastJpegSnapshot;
	FVirtualCameraJpegMetadata LastJpegMetadata;

	struct FScheduledReadbackSlot
	{
		TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback;
		bool bInFlight = false;
		bool bConsumeQueued = false;
		int32 Width = 0;
		int32 Height = 0;
		int64 FrameId = 0;
		double CaptureStartedSeconds = 0.0;
		int32 Generation = 0;
	};
	struct FPendingReadbackRequest
	{
		int64 FrameId = 0;
		double CaptureStartedSeconds = 0.0;
	};
	struct FPendingEncodeInput
	{
		TArray<FColor> RawPixels;
		int32 Width = 0;
		int32 Height = 0;
		int64 FrameId = 0;
		double CaptureStartedSeconds = 0.0;
	};
	struct FCompletedEncode
	{
		TArray64<uint8> JpegBytes;
		FString JsonPayload;
		FString Checksum;
		int32 Width = 0;
		int32 Height = 0;
		int32 Quality = 0;
		double CaptureStartedSeconds = 0.0;
	};
	TArray<FScheduledReadbackSlot> ScheduledReadbackSlots;
	TArray<FPendingReadbackRequest> PendingReadbackRequests;
	TArray<FPendingEncodeInput> PendingEncodeInputs;
	TArray<int64> EncodeOrder;
	TMap<int64, FCompletedEncode> CompletedEncodes;
	int32 ScheduledEncodeInFlightCount = 0;
    double NextScheduledCaptureTime = -1.0;
	FVirtualSensorCadenceState CadenceState;
    double LastScheduledCompletionTime = -1.0;
    double LastAcquisitionCompletionTime = -1.0;
    double LastOutputCompletionTime = -1.0;
    int32 ScheduledGeneration = 0;
    bool bRegisteredWithPerformanceSubsystem = false;
	bool bRuntimeStreamOutputDemand = false;
	bool bRuntimeHighThroughputStreamDemand = false;
};
