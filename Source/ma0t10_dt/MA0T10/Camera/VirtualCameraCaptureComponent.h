// VirtualCameraCaptureComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDeviceProfileTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualCameraPayloadCodec.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorResourceDiagnostics.h"
#include "VirtualCameraCaptureComponent.generated.h"

class UTextureRenderTarget2D;
class UVirtualSensorTransportComponent;
class UVirtualSensorRecorderComponent;
class UVirtualSensorSchedulerSubsystem;
class FRHIGPUTextureReadback;
class UVirtualCameraGroundTruth;
struct FVirtualCameraLocalReadbackState;
struct FVirtualCameraPreviewFence;
struct FVirtualCameraPreviewCompletion
{
    FVirtualCameraPayloadSnapshot Acquisition;
    FTransform AcquisitionPose;
    double StartedMonotonic=0;
    int32 Generation=0;
};
struct MA0T10_DT_API FVirtualCameraLocalSaveFrame
{
	FVirtualCameraPayloadSnapshot Payload;
	FVirtualSensorAcquisitionContext SlabContext;
	TSharedPtr<const TArray64<uint8>,ESPMode::ThreadSafe> Jpeg;
	double AcquisitionSeconds=0;
};
using FVirtualCameraLocalSaveCallback=TFunction<void(TSharedPtr<const FVirtualCameraLocalSaveFrame,ESPMode::ThreadSafe>,const FString&)>;

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

    /** Opt-in only. No additional render/readback allocations while disabled. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DigitalTwin|VirtualCamera|GroundTruth")
    bool bGroundTruthEnabled=false;
    void SetGroundTruthEnabled(bool bEnabled);
    bool RequestGroundTruthSave();
    bool IsGroundTruthSavePending() const;
    void SetGroundTruthPreviewDemand(bool bEnabled);
    UVirtualCameraGroundTruth* GetGroundTruth() const { return GroundTruth; }
    /** Explicit dataset session, stopped camera only. Asynchronous RGB + CAM-GT readback. */
    bool CaptureDatasetFrame(UVirtualCameraGroundTruth& Destination,bool IncludeTruth=true);

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
    /** One render-completion proof for placement UI. No readback or JPEG work. */
    void RequestPlacementPreviewProof();
    void CancelPlacementPreviewProof();
    bool GetPlacementPreviewCompletion(FVirtualCameraPreviewCompletion& Out) const;
    FVirtualSensorResourceSnapshot GetResourceSnapshot() const;
    static int32 GetGlobalEncodeJobCountForDiagnostics();
    static int32 GetGlobalPendingWorkCountForDiagnostics();
	/** File-only request; never enables a stream, changes CaptureMode or calls the sync capture API. */
	bool RequestLocalFileFrame(bool bNewFrame,FVirtualCameraLocalSaveCallback Callback,FString& Error);
	void PollLocalFileFrame();
	void CancelLocalFileFrame();
	int32 GetFileCaptureRevision() const { return ScheduledGeneration; }
	int64 GetAvailableLocalFrameId() const;
	TSharedPtr<const FVirtualSensorFrameEnvelope,ESPMode::ThreadSafe> GetExternalFileFrame() const { return ExternalFileFrame; }
    bool IsScheduledCaptureDue(double NowSeconds) const
    {
        return NextScheduledCaptureTime >= 0.0 && NowSeconds + KINDA_SMALL_NUMBER >= NextScheduledCaptureTime;
    }
    void MarkBudgetSkippedAcquisition()
    {
        ++RuntimeStatus.BudgetSkippedAcquisitionFrameCount;
        ++RuntimeStatus.DeadlineMissCount;
    }

	/** Requests JPEG production for a live stream without changing the persisted CaptureMode setting. */
	void SetRuntimeStreamOutputDemand(bool bEnabled, bool bHighThroughputOnly = false)
	{
		bRuntimeStreamOutputDemand = bEnabled;
		bRuntimeHighThroughputStreamDemand = bEnabled && bHighThroughputOnly;
	}
	bool HasRuntimeStreamOutputDemand() const { return bRuntimeStreamOutputDemand; }
	bool HasHighThroughputStreamOutputDemand() const { return bRuntimeHighThroughputStreamDemand; }

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
    bool IsCaptureRunning() const { return NextScheduledCaptureTime >= 0.0; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualCamera|Transport")
    const FString& GetLastJsonPayload() const { return LastJsonPayload; }

    TSharedPtr<const TArray64<uint8>, ESPMode::ThreadSafe> GetLastJpegSnapshot() const { return LastJpegSnapshot; }
	const FVirtualCameraJpegMetadata& GetLastJpegMetadata() const { return LastJpegMetadata; }
	const FVirtualCameraPayloadSnapshot& GetLastCompletedAcquisition() const { return LastCompletedAcquisition; }
    int64 GetLastJsonPayloadFrameId() const { return LastJsonPayloadFrameId; }
    const FString& GetLastJsonPayloadSensorId() const { return LastJsonPayloadSensorId; }

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
    UPROPERTY(Transient) TObjectPtr<UVirtualCameraGroundTruth> GroundTruth;
    bool bGroundTruthSaveRequested=false;
    double GroundTruthPreviewDemandUntil=0, NextGroundTruthPreview=0;
    void CaptureGroundTruth(const FVirtualCameraPayloadSnapshot& Snapshot);
    void EnsureRenderTarget();
    bool ReadRenderTargetAsJpeg(TArray64<uint8>& OutJpegBytes) const;
    FString BuildJsonPayload(const FString& Base64Image, int64 ByteSize) const;
    void DispatchPayload(const FString& JsonPayload, const TArray64<uint8>& JpegBytes) const;
    void DispatchJsonPayloadOnly(const FString& JsonPayload, const FString& SensorIdOverride = FString()) const;
    void PostJson(const FString& JsonPayload) const;
    void SaveJpegToDisk(const TArray64<uint8>& JpegBytes) const;
    void UpdateRuntimeStatus(int32 PayloadLength, const FString& Message);
    bool ReadExternalPayloadMetadata(const FString& JsonPayload, FString& OutSensorId, int64& OutFrameId, int64& OutByteSize,FDateTime& OutTimestampUtc) const;
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
    bool bPlacementPreviewProofRequested=false;
    TSharedPtr<FVirtualCameraPreviewFence,ESPMode::ThreadSafe> PlacementPreviewFence;
    TSharedPtr<const TArray64<uint8>, ESPMode::ThreadSafe> LastJpegSnapshot;
	FVirtualCameraJpegMetadata LastJpegMetadata;
	TMap<int64,FVirtualSensorAcquisitionContext> SlabCaptureContexts;
	FVirtualSlabFrameContext LastSlabContext;
	FVirtualSensorAcquisitionContext CompleteSlabAcquisition(int64 Id, bool bSuccess=true, bool bFlushOnFailure=true);
public:
    virtual void UpdateSceneCaptureContents(FSceneInterface* Scene) override;
	const FVirtualSlabFrameContext& GetLastSlabContext() const { return LastSlabContext; }
private:
    void RecordLocalFileAcquisition(double CaptureStartedSeconds);
    void QueueLocalFileReadback();
    static bool TryAcquireLocalFileEncodeSlot();
    static void ReleaseLocalFileEncodeSlot();
    TSharedPtr<FVirtualCameraLocalReadbackState,ESPMode::ThreadSafe> LocalFileReadback;
    FVirtualCameraPayloadSnapshot LastFileAcquisition;
    FVirtualCameraPayloadSnapshot PendingFileAcquisition;
    FVirtualSensorAcquisitionContext PendingFileSlabContext;
    FVirtualSensorAcquisitionContext LastFileSlabContext;
    double LastFileAcquisitionSeconds=0;
    bool bHasFileAcquisition=false;
    bool bFileAcquisitionPendingRender=false;
#if WITH_DEV_AUTOMATION_TESTS
    int32 LocalFileRenderDispatchCount=0;
    friend class FSensorCameraLocalDispatchTest;
#endif
    TSharedPtr<const FVirtualSensorFrameEnvelope,ESPMode::ThreadSafe> ExternalFileFrame;

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
	TMap<int64,FVirtualCameraPayloadSnapshot> ScheduledAcquisitionSnapshots;
	FVirtualCameraPayloadSnapshot LastCompletedAcquisition;
    int64 LastJsonPayloadFrameId=-1;
    FString LastJsonPayloadSensorId;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSensorCameraCoherenceUnitTest;
    friend class FCameraOrderedFailureRecoveryTest;
public:
	TFunction<void(int64)> BeforeSceneCaptureForTests;
	TFunction<void(const FVirtualCameraPayloadSnapshot&,const FVirtualSlabFrameContext&,TSharedPtr<const TArray64<uint8>,ESPMode::ThreadSafe>)> OnScheduledFrameForTests;
	int32 GetAcquisitionRevisionForTests() const { return ScheduledGeneration; }
	bool ForceReadbackSaturationForTests=false;
    bool FreezeAcquisitionForTests=false;
	float EncodeDelayForTests=0;
    TFunction<float(int64)> EncodeDelayByFrameForTests;
private:
#endif
	int32 ScheduledEncodeInFlightCount = 0;
    uint64 ScheduledReadbackAllocationCount = 0;
    double NextScheduledCaptureTime = -1.0;
    double LastScheduledCompletionTime = -1.0;
    double LastAcquisitionCompletionTime = -1.0;
    double LastOutputCompletionTime = -1.0;
    int32 ScheduledGeneration = 0;
    bool bRegisteredWithPerformanceSubsystem = false;
	bool bRuntimeStreamOutputDemand = false;
	bool bRuntimeHighThroughputStreamDemand = false;
};
