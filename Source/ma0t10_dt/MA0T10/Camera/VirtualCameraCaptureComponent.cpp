// VirtualCameraCaptureComponent.cpp
#include "VirtualCameraCaptureComponent.h"
#include "VirtualCameraPayloadCodec.h"

#include "Async/Async.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Json.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include <atomic>

namespace
{
std::atomic<int32> GVirtualCameraEncodeJobs{0};
constexpr int32 GVirtualCameraEncodeJobLimit = 4;
constexpr int32 GVirtualCameraEncodeJobsPerCamera = 2;
constexpr int32 GVirtualCameraReadbackSlotsPerCamera = 3;
int64 CameraUtcNowUnixNanoseconds()
{
	static const FDateTime UnixEpoch(1970, 1, 1);
	return (FDateTime::UtcNow() - UnixEpoch).GetTicks() * 100;
}
bool IsWholeNumber(double Value)
{
    return FMath::IsFinite(Value) && FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value));
}

bool IsPositiveWholeNumber(double Value)
{
    return Value > 0.0 && IsWholeNumber(Value);
}

bool HasNumberArrayField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, int32 ExpectedCount)
{
    if (!Object.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object->TryGetArrayField(FieldName, Values) || !Values || Values->Num() != ExpectedCount)
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        double Number = 0.0;
        if (!Value.IsValid() || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number))
        {
            return false;
        }
    }

    return true;
}

FString ToPayloadSimulationQuality(EVirtualSensorSimulationQuality Quality)
{
    switch (Quality)
    {
    case EVirtualSensorSimulationQuality::Debug:
        return TEXT("Debug");
    case EVirtualSensorSimulationQuality::RealTimePreview:
        return TEXT("RealTimePreview");
    case EVirtualSensorSimulationQuality::Balanced:
        return TEXT("Balanced");
    case EVirtualSensorSimulationQuality::FullSpec:
        return TEXT("FullSpec");
    default:
        return TEXT("RealTimePreview");
    }
}

bool IsAllowedPayloadSimulationQuality(const FString& Quality)
{
    return Quality == TEXT("Debug") ||
        Quality == TEXT("RealTimePreview") ||
        Quality == TEXT("Balanced") ||
        Quality == TEXT("FullSpec");
}
}

UVirtualCameraCaptureComponent::UVirtualCameraCaptureComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bCaptureEveryFrame = false;
    bCaptureOnMovement = false;
    CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
}

void UVirtualCameraCaptureComponent::BeginPlay()
{
    Super::BeginPlay();
	ScheduledReadbackSlots.SetNum(GVirtualCameraReadbackSlotsPerCamera);

    if (bUsePerformanceOptimizedShowFlags)
    {
        ShowFlags.DisableAdvancedFeatures();
        ShowFlags.SetMotionBlur(false);
        // The virtual sensor payload needs stable object colour/geometry, not
        // a second high-cost shadow render. Keeping direct lighting while
        // disabling dynamic shadow maps removes the capture-frame hitch that
        // otherwise dominates FullSpec 1% lows when LiDAR tracing is active.
        ShowFlags.SetDynamicShadows(false);
    }

    if (bApplyDeviceProfileOnBeginPlay)
    {
        ApplyDeviceProfile(DeviceProfile);
    }

    EnsureRenderTarget();
    TryAutoRegisterToManager();

    if (bAutoStartCapture)
    {
        StartCapture();
    }
}

void UVirtualCameraCaptureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCapture();
    ++ScheduledGeneration;
    ReleaseScheduledReadbackOnRenderThread();
	PendingReadbackRequests.Reset();
	PendingEncodeInputs.Reset();
	EncodeOrder.Reset();
	CompletedEncodes.Reset();
	CaptureTimings.Reset();
	ScheduledEncodeInFlightCount = 0;
    Super::EndPlay(EndPlayReason);
}

void UVirtualCameraCaptureComponent::StartCapture()
{
    EnsureRenderTarget();

    if (!GetWorld() || CaptureInterval <= 0.0f)
    {
        return;
    }

    GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
	const double NowMonotonicSeconds = FPlatformTime::Seconds();
	const double SafeInterval = FMath::Max(0.001, static_cast<double>(CaptureInterval));
	const double PhaseSeconds = (GetTypeHash(SensorId) % 1000) / 1000.0 * SafeInterval;
	CadenceState.Start(NowMonotonicSeconds, CameraUtcNowUnixNanoseconds(), SafeInterval, PhaseSeconds);
	NextScheduledCaptureTime = CadenceState.GetNextDeadlineMonotonicSeconds();
    RegisterWithPerformanceSubsystem();
}

void UVirtualCameraCaptureComponent::StopCapture()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
    }
    UnregisterFromPerformanceSubsystem();
    NextScheduledCaptureTime = -1.0;
	CadenceState.Stop();
    ReleaseScheduledReadbackOnRenderThread();
	PendingReadbackRequests.Reset();
	PendingEncodeInputs.Reset();
	EncodeOrder.Reset();
	CompletedEncodes.Reset();
	CaptureTimings.Reset();
	ScheduledEncodeInFlightCount = 0;
    RuntimeStatus.bAcquisitionInFlight = false;
    RuntimeStatus.bDerivedWorkInFlight = false;
    ++ScheduledGeneration;
}

void UVirtualCameraCaptureComponent::RequestImmediateScheduledCapture()
{
	if (CadenceState.IsRunning())
	{
		CadenceState.ForceDue(FPlatformTime::Seconds(), CameraUtcNowUnixNanoseconds());
		NextScheduledCaptureTime = CadenceState.GetNextDeadlineMonotonicSeconds();
	}
}

void UVirtualCameraCaptureComponent::ResumeRealtimeCadence(double NowMonotonicSeconds, int64 NowUnixNanoseconds)
{
	CadenceState.Resume(NowMonotonicSeconds, NowUnixNanoseconds);
	NextScheduledCaptureTime = CadenceState.GetNextDeadlineMonotonicSeconds();
}

void UVirtualCameraCaptureComponent::RegisterWithPerformanceSubsystem()
{
    if (!GetWorld() || bRegisteredWithPerformanceSubsystem) return;
    if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>())
    {
        Subsystem->RegisterTask(this);
        bRegisteredWithPerformanceSubsystem = true;
    }
}

void UVirtualCameraCaptureComponent::UnregisterFromPerformanceSubsystem()
{
    if (!GetWorld() || !bRegisteredWithPerformanceSubsystem) return;
    if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>()) Subsystem->UnregisterTask(this);
    bRegisteredWithPerformanceSubsystem = false;
}

bool UVirtualCameraCaptureComponent::TickScheduledCapture(double NowSeconds, bool bAllowNewCapture)
{
    PollScheduledGpuReadback(NowSeconds);
	QueuePendingGpuReadbacks();
	PumpScheduledEncodeQueue();
    if (!bAllowNewCapture) return false;
    if (!CadenceState.IsDue(NowSeconds)) return false;

    const double SafeInterval = FMath::Max(0.001, static_cast<double>(CaptureInterval));
    RuntimeStatus.RequestedAcquisitionRateHz = static_cast<float>(1.0 / SafeInterval);
	RuntimeStatus.RequestedAcquisitionBackend = TEXT("scene_capture_gpu");
	RuntimeStatus.ActiveAcquisitionBackend = TEXT("scene_capture_gpu");
	FVirtualSensorCadenceDeadline Deadline;
	const int64 AcquisitionStartUnixNanoseconds = CameraUtcNowUnixNanoseconds();
	if (!CadenceState.ConsumeDeadline(NowSeconds, AcquisitionStartUnixNanoseconds, Deadline)) return false;
	NextScheduledCaptureTime = CadenceState.GetNextDeadlineMonotonicSeconds();
	RuntimeStatus.DeadlineMissCount = static_cast<int32>(FMath::Min<int64>(MAX_int32, CadenceState.GetTelemetry().DeadlineMissCount));

    const double CaptureStart = FPlatformTime::Seconds();
    EnsureRenderTarget();
	CaptureSceneDeferred();
	++FrameId;
	FCaptureTiming& Timing = CaptureTimings.Add(FrameId);
	Timing.ScheduledUnixNanoseconds = Deadline.ScheduledUnixNanoseconds;
	Timing.AcquisitionStartUnixNanoseconds = AcquisitionStartUnixNanoseconds;
	RuntimeStatus.LastAcquisitionDurationMs = static_cast<float>((FPlatformTime::Seconds() - CaptureStart) * 1000.0);
    RuntimeStatus.bAcquisitionInFlight = false;
    RuntimeStatus.MeasuredAcquisitionRateHz = LastAcquisitionCompletionTime >= 0.0
        ? static_cast<float>(1.0 / FMath::Max(0.001, NowSeconds - LastAcquisitionCompletionTime))
        : 0.0f;
    LastAcquisitionCompletionTime = NowSeconds;

    if (!ShouldGeneratePayload())
    {
        const double PreviousCompletion = LastScheduledCompletionTime;
        LastScheduledCompletionTime = NowSeconds;
        RuntimeStatus.MeasuredCompletionRateHz = PreviousCompletion >= 0.0 ? static_cast<float>(1.0 / FMath::Max(0.001, NowSeconds - PreviousCompletion)) : 0.0f;
        UpdateRuntimeStatus(0, TEXT("비동기 미리보기"));
        OnFrameCaptured.Broadcast(TEXT(""), CameraRenderTarget);
		CaptureTimings.Remove(FrameId);
        return true;
    }

	if (PendingReadbackRequests.Num() >= GVirtualCameraReadbackSlotsPerCamera)
	{
		++RuntimeStatus.QueueOverflowCount;
		++RuntimeStatus.DroppedDerivedFrameCount;
		RuntimeStatus.AcquisitionBackendMessage = TEXT("Camera readback FIFO is overloaded; acquisition continued without blocking.");
		CaptureTimings.Remove(FrameId);
		return true;
	}
	FPendingReadbackRequest& Request = PendingReadbackRequests.AddDefaulted_GetRef();
	Request.FrameId = FrameId;
	Request.CaptureStartedSeconds = CaptureStart;
    RuntimeStatus.bAcquisitionInFlight = true;
    RuntimeStatus.bDerivedWorkInFlight = true;
    return true;
}

void UVirtualCameraCaptureComponent::QueuePendingGpuReadbacks()
{
	while (!PendingReadbackRequests.IsEmpty())
	{
		const FPendingReadbackRequest Request = PendingReadbackRequests[0];
		if (!QueueScheduledGpuReadback(Request.FrameId, Request.CaptureStartedSeconds)) break;
		PendingReadbackRequests.RemoveAt(0, 1, false);
	}
}

bool UVirtualCameraCaptureComponent::QueueScheduledGpuReadback(int64 CapturedFrameId, double CaptureStartedSeconds)
{
	if (!CameraRenderTarget) return false;
	int32 SlotIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ScheduledReadbackSlots.Num(); ++Index)
	{
		if (!ScheduledReadbackSlots[Index].bInFlight && !ScheduledReadbackSlots[Index].bConsumeQueued)
		{
			SlotIndex = Index;
			break;
		}
	}
	if (SlotIndex == INDEX_NONE) return false;

	FTextureRenderTargetResource* Resource = CameraRenderTarget->GameThread_GetRenderTargetResource();
	FTextureRHIRef Texture = Resource ? Resource->GetRenderTargetTexture() : FTextureRHIRef();
	if (!Texture.IsValid())
	{
		++RuntimeStatus.DroppedDerivedFrameCount;
		return true;
	}

	FScheduledReadbackSlot& Slot = ScheduledReadbackSlots[SlotIndex];
	Slot.Width = CameraRenderTarget->SizeX;
	Slot.Height = CameraRenderTarget->SizeY;
	Slot.FrameId = CapturedFrameId;
	Slot.CaptureStartedSeconds = CaptureStartedSeconds;
	Slot.Generation = ScheduledGeneration;
	Slot.Readback = MakeShared<FRHIGPUTextureReadback, ESPMode::ThreadSafe>(TEXT("VirtualCameraScheduledReadback"));
	const TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback = Slot.Readback;
	ENQUEUE_RENDER_COMMAND(VirtualCameraScheduledReadback)([Readback, Texture](FRHICommandListImmediate& RHICmdList)
	{
		if (Readback.IsValid() && Texture.IsValid()) Readback->EnqueueCopy(RHICmdList, Texture);
	});
	Slot.bInFlight = true;
	RuntimeStatus.bAcquisitionInFlight = true;
	RuntimeStatus.bDerivedWorkInFlight = true;
	return true;
}

void UVirtualCameraCaptureComponent::PollScheduledGpuReadback(double NowSeconds)
{
	for (int32 SlotIndex = 0; SlotIndex < ScheduledReadbackSlots.Num(); ++SlotIndex)
	{
		FScheduledReadbackSlot& Slot = ScheduledReadbackSlots[SlotIndex];
		if (!Slot.Readback.IsValid() || !Slot.bInFlight || Slot.bConsumeQueued || !Slot.Readback->IsReady()) continue;
		Slot.bConsumeQueued = true;
		const int32 Width = Slot.Width;
		const int32 Height = Slot.Height;
		const int64 CapturedFrameId = Slot.FrameId;
		if (FCaptureTiming* Timing = CaptureTimings.Find(CapturedFrameId))
		{
			Timing->AcquisitionEndUnixNanoseconds = CameraUtcNowUnixNanoseconds();
			CadenceState.MarkAcquisitionEnd(Timing->AcquisitionEndUnixNanoseconds);
		}
		const double CaptureStartedSeconds = Slot.CaptureStartedSeconds;
		const int32 Generation = Slot.Generation;
		TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback = MoveTemp(Slot.Readback);
		TWeakObjectPtr<UVirtualCameraCaptureComponent> WeakThis(this);

		ENQUEUE_RENDER_COMMAND(VirtualCameraConsumeScheduledReadback)(
			[Readback = MoveTemp(Readback), WeakThis, SlotIndex, Width, Height, CapturedFrameId, CaptureStartedSeconds, Generation](FRHICommandListImmediate& RHICmdList) mutable
			{
				TArray<FColor> RawPixels;
				bool bCopySucceeded = false;
				int32 RowPitchInPixels = 0;
				void* LockedData = Readback.IsValid() ? Readback->Lock(RowPitchInPixels) : nullptr;
				if (LockedData && RowPitchInPixels >= Width && Width > 0 && Height > 0)
				{
					RawPixels.SetNumUninitialized(Width * Height);
					const FColor* SourcePixels = static_cast<const FColor*>(LockedData);
					for (int32 Y = 0; Y < Height; ++Y)
					{
						FMemory::Memcpy(RawPixels.GetData() + Y * Width, SourcePixels + Y * RowPitchInPixels, Width * sizeof(FColor));
					}
					bCopySucceeded = true;
				}
				if (LockedData) Readback->Unlock();

				AsyncTask(ENamedThreads::GameThread,
					[WeakThis, SlotIndex, Width, Height, CapturedFrameId, CaptureStartedSeconds, Generation, bCopySucceeded, RawPixels = MoveTemp(RawPixels)]() mutable
					{
						if (!WeakThis.IsValid() || WeakThis->ScheduledGeneration != Generation) return;
						if (WeakThis->ScheduledReadbackSlots.IsValidIndex(SlotIndex))
						{
							FScheduledReadbackSlot& CompletedSlot = WeakThis->ScheduledReadbackSlots[SlotIndex];
							CompletedSlot.bInFlight = false;
							CompletedSlot.bConsumeQueued = false;
							CompletedSlot.Readback.Reset();
						}
						WeakThis->RuntimeStatus.bAcquisitionInFlight = false;
						if (!bCopySucceeded)
						{
							++WeakThis->RuntimeStatus.DroppedDerivedFrameCount;
							WeakThis->CaptureTimings.Remove(CapturedFrameId);
							return;
						}
						if (WeakThis->PendingEncodeInputs.Num() >= 4)
						{
							++WeakThis->RuntimeStatus.QueueOverflowCount;
							++WeakThis->RuntimeStatus.DroppedDerivedFrameCount;
							WeakThis->CaptureTimings.Remove(CapturedFrameId);
							return;
						}
						FPendingEncodeInput& Input = WeakThis->PendingEncodeInputs.AddDefaulted_GetRef();
						Input.RawPixels = MoveTemp(RawPixels);
						Input.Width = Width;
						Input.Height = Height;
						Input.FrameId = CapturedFrameId;
						Input.CaptureStartedSeconds = CaptureStartedSeconds;
						WeakThis->PumpScheduledEncodeQueue();
					});
			});
	}
}

void UVirtualCameraCaptureComponent::ReleaseScheduledReadbackOnRenderThread()
{
	for (FScheduledReadbackSlot& Slot : ScheduledReadbackSlots)
	{
		if (Slot.Readback.IsValid())
		{
			TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback = MoveTemp(Slot.Readback);
			ENQUEUE_RENDER_COMMAND(VirtualCameraReleaseScheduledReadback)(
				[Readback = MoveTemp(Readback)](FRHICommandListImmediate& RHICmdList) mutable { Readback.Reset(); });
		}
		Slot.bInFlight = false;
		Slot.bConsumeQueued = false;
	}
}

void UVirtualCameraCaptureComponent::PumpScheduledEncodeQueue()
{
	while (ScheduledEncodeInFlightCount < GVirtualCameraEncodeJobsPerCamera && !PendingEncodeInputs.IsEmpty())
	{
		if (GVirtualCameraEncodeJobs.load(std::memory_order_acquire) >= GVirtualCameraEncodeJobLimit) break;
		FPendingEncodeInput Input = MoveTemp(PendingEncodeInputs[0]);
		PendingEncodeInputs.RemoveAt(0, 1, false);
		if (!StartScheduledEncode(MoveTemp(Input.RawPixels), Input.Width, Input.Height, Input.FrameId, Input.CaptureStartedSeconds))
		{
			++RuntimeStatus.QueueOverflowCount;
			++RuntimeStatus.DroppedDerivedFrameCount;
			break;
		}
	}
	RuntimeStatus.bDerivedWorkInFlight = ScheduledEncodeInFlightCount > 0 || !PendingEncodeInputs.IsEmpty() || !PendingReadbackRequests.IsEmpty();
}

bool UVirtualCameraCaptureComponent::StartScheduledEncode(TArray<FColor>&& RawPixels, int32 Width, int32 Height, int64 CapturedFrameId, double CaptureStartedSeconds)
{
	if (ScheduledEncodeInFlightCount >= GVirtualCameraEncodeJobsPerCamera) return false;
	if (GVirtualCameraEncodeJobs.fetch_add(1, std::memory_order_acq_rel) >= GVirtualCameraEncodeJobLimit)
	{
		GVirtualCameraEncodeJobs.fetch_sub(1, std::memory_order_acq_rel);
		return false;
	}
	++ScheduledEncodeInFlightCount;
	EncodeOrder.Add(CapturedFrameId);
    RuntimeStatus.bDerivedWorkInFlight = true;
    const int32 Generation = ScheduledGeneration;
    const int32 Quality = FMath::Clamp(JpegQuality, 1, 100);
    FVirtualCameraPayloadSnapshot Snapshot;
    Snapshot.SensorId = SensorId;
    Snapshot.Manufacturer = DeviceSpec.Manufacturer;
    Snapshot.Model = DeviceSpec.Model;
    Snapshot.SimulationQuality = ToPayloadSimulationQuality(SimulationQuality);
    Snapshot.FrameId = CapturedFrameId;
    Snapshot.Width = Width;
    Snapshot.Height = Height;
    Snapshot.HorizontalFov = DeviceSpec.HorizontalFovDegrees;
    Snapshot.VerticalFov = DeviceSpec.VerticalFovDegrees;
    Snapshot.Location = GetComponentLocation();
    Snapshot.Rotation = GetComponentRotation();
    Snapshot.Forward = GetForwardVector();
    Snapshot.Up = GetUpVector();
    IImageWrapperModule* ImageWrapperModule = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TWeakObjectPtr<UVirtualCameraCaptureComponent> WeakThis(this);

	const bool bBuildCompatibilityJson = ShouldBuildCompatibilityJson();
    Async(EAsyncExecution::ThreadPool, [WeakThis, Generation, ImageWrapperModule, Snapshot = MoveTemp(Snapshot), RawPixels = MoveTemp(RawPixels), Quality, CaptureStartedSeconds, bBuildCompatibilityJson]() mutable
    {
        TArray64<uint8> JpegBytes;
        FString JsonPayload;
		FString Checksum;
        if (ImageWrapperModule)
        {
            TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::JPEG);
            if (Wrapper.IsValid() && Wrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), Snapshot.Width, Snapshot.Height, ERGBFormat::BGRA, 8))
            {
                JpegBytes = Wrapper->GetCompressed(Quality);
                if (JpegBytes.Num() > 0)
                {
					uint8 Hash[FSHA1::DigestSize];
					FSHA1::HashBuffer(JpegBytes.GetData(), static_cast<uint32>(JpegBytes.Num()), Hash);
					Checksum = BytesToHex(Hash, FSHA1::DigestSize).ToLower();
					if (bBuildCompatibilityJson)
					{
						const FString Base64Image = FBase64::Encode(JpegBytes.GetData(), static_cast<uint32>(JpegBytes.Num()));
						JsonPayload = FVirtualCameraPayloadCodec::EncodeJpegBase64(Snapshot, Base64Image, JpegBytes.Num());
					}
                }
            }
        }
        GVirtualCameraEncodeJobs.fetch_sub(1, std::memory_order_acq_rel);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Generation, CapturedFrameId = Snapshot.FrameId, Width = Snapshot.Width, Height = Snapshot.Height, Quality, Checksum = MoveTemp(Checksum), JpegBytes = MoveTemp(JpegBytes), JsonPayload = MoveTemp(JsonPayload), CaptureStartedSeconds]() mutable
        {
            if (!WeakThis.IsValid() || WeakThis->ScheduledGeneration != Generation) return;
			WeakThis->CompleteScheduledEncode(CapturedFrameId, Width, Height, Quality, MoveTemp(Checksum), MoveTemp(JpegBytes), MoveTemp(JsonPayload), CaptureStartedSeconds);
        });
    });
	return true;
}

void UVirtualCameraCaptureComponent::CompleteScheduledEncode(
	int64 CapturedFrameId,
	int32 Width,
	int32 Height,
	int32 Quality,
	FString&& Checksum,
	TArray64<uint8>&& JpegBytes,
	FString&& JsonPayload,
	double CaptureStartedSeconds)
{
	ScheduledEncodeInFlightCount = FMath::Max(0, ScheduledEncodeInFlightCount - 1);
	FCompletedEncode& Completed = CompletedEncodes.FindOrAdd(CapturedFrameId);
	Completed.JpegBytes = MoveTemp(JpegBytes);
	Completed.JsonPayload = MoveTemp(JsonPayload);
	Completed.Checksum = MoveTemp(Checksum);
	Completed.Width = Width;
	Completed.Height = Height;
	Completed.Quality = Quality;
	Completed.CaptureStartedSeconds = CaptureStartedSeconds;
	if (FCaptureTiming* Timing = CaptureTimings.Find(CapturedFrameId))
	{
		Timing->DerivedCompleteUnixNanoseconds = CameraUtcNowUnixNanoseconds();
	}
	FlushCompletedEncodes();
	PumpScheduledEncodeQueue();
}

void UVirtualCameraCaptureComponent::FlushCompletedEncodes()
{
	while (!EncodeOrder.IsEmpty())
	{
		const int64 CompletedFrameId = EncodeOrder[0];
		FCompletedEncode* Completed = CompletedEncodes.Find(CompletedFrameId);
		if (!Completed) break;
		FCompletedEncode Result = MoveTemp(*Completed);
		CompletedEncodes.Remove(CompletedFrameId);
		EncodeOrder.RemoveAt(0, 1, false);
		const FCaptureTiming Timing = CaptureTimings.FindRef(CompletedFrameId);
		CaptureTimings.Remove(CompletedFrameId);

		RuntimeStatus.LastPostProcessDurationMs = static_cast<float>((FPlatformTime::Seconds() - Result.CaptureStartedSeconds) * 1000.0);
		if (Result.JpegBytes.IsEmpty())
		{
			++RuntimeStatus.DroppedDerivedFrameCount;
			UpdateRuntimeStatus(0, TEXT("비동기 JPEG 생성 실패"));
			continue;
		}

		LastJsonPayload = MoveTemp(Result.JsonPayload);
		FString StatusMessage = TEXT("비동기 캡처 완료");
		if (CaptureMode == EVirtualCameraCaptureMode::PayloadAndOutput && !LastJsonPayload.IsEmpty())
		{
			DispatchPayload(LastJsonPayload, Result.JpegBytes);
			if (TransportComponent) StatusMessage = TransportComponent->LastResult.Message;
		}
		if (RecorderComponent && !LastJsonPayload.IsEmpty())
		{
			RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_camera"), CompletedFrameId, LastJsonPayload);
		}

		const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		RuntimeStatus.MeasuredCompletionRateHz = LastScheduledCompletionTime >= 0.0
			? static_cast<float>(1.0 / FMath::Max(0.001, NowSeconds - LastScheduledCompletionTime)) : 0.0f;
		RuntimeStatus.MeasuredOutputRateHz = LastOutputCompletionTime >= 0.0
			? static_cast<float>(1.0 / FMath::Max(0.001, NowSeconds - LastOutputCompletionTime)) : 0.0f;
		LastOutputCompletionTime = NowSeconds;
		LastScheduledCompletionTime = NowSeconds;
		LastJpegMetadata.Schema = TEXT("virtual-camera.jpeg.v1");
		LastJpegMetadata.Width = Result.Width;
		LastJpegMetadata.Height = Result.Height;
		LastJpegMetadata.JpegQuality = Result.Quality;
		LastJpegMetadata.ByteCount = static_cast<int32>(Result.JpegBytes.Num());
		LastJpegMetadata.ChecksumSha1 = Result.Checksum;
		LastJpegMetadata.ScheduledUnixNanoseconds = Timing.ScheduledUnixNanoseconds;
		LastJpegMetadata.AcquisitionStartUnixNanoseconds = Timing.AcquisitionStartUnixNanoseconds;
		LastJpegMetadata.AcquisitionEndUnixNanoseconds = Timing.AcquisitionEndUnixNanoseconds;
		LastJpegMetadata.DerivedCompleteUnixNanoseconds = Timing.DerivedCompleteUnixNanoseconds;
		LastJpegSnapshot = MakeShared<const TArray64<uint8>, ESPMode::ThreadSafe>(MoveTemp(Result.JpegBytes));
		UpdateRuntimeStatus(LastJsonPayload.Len(), StatusMessage);
		RuntimeStatus.FrameId = CompletedFrameId;
		OnFrameCaptured.Broadcast(LastJsonPayload, CameraRenderTarget);
	}
}

void UVirtualCameraCaptureComponent::SetTransportComponent(UVirtualSensorTransportComponent* InTransportComponent)
{
    TransportComponent = InTransportComponent;
}

void UVirtualCameraCaptureComponent::SetRecorderComponent(UVirtualSensorRecorderComponent* InRecorderComponent)
{
    RecorderComponent = InRecorderComponent;
}

void UVirtualCameraCaptureComponent::ApplyDeviceProfile(EVirtualCameraDeviceProfile NewProfile)
{
    DeviceProfile = NewProfile;

    if (DeviceProfile == EVirtualCameraDeviceProfile::IntelRealSenseD455)
    {
        DeviceSpec.Manufacturer = TEXT("Intel");
        DeviceSpec.Model = TEXT("RealSense D455");
        DeviceSpec.HorizontalFovDegrees = 87.0f;
        DeviceSpec.VerticalFovDegrees = 58.0f;
        DeviceSpec.MinRangeCm = 52.0f;
        DeviceSpec.TypicalRangeCm = 600.0f;
        DeviceSpec.MaxRangeCm = 600.0f;
        DeviceSpec.FrameRateHz = 30.0f;
        DeviceSpec.Width = 1280;
        DeviceSpec.Height = 720;
        DeviceSpec.Notes = TEXT("Intel RealSense D455 depth envelope: 87x58 FOV, ~0.52m Min-Z at max resolution, ideal 0.6-6m. SceneCapture preview is not calibrated depth. Simulation quality controls runtime cost separately.");
        FOVAngle = DeviceSpec.HorizontalFovDegrees;
        ApplySimulationQuality(SimulationQuality);
    }
    else
    {
        DeviceSpec.Manufacturer = TEXT("Generic");
        DeviceSpec.Model = TEXT("Generic Camera");
        ApplySimulationQuality(SimulationQuality);
    }
}

void UVirtualCameraCaptureComponent::ApplySimulationQuality(EVirtualSensorSimulationQuality NewQuality)
{
    SimulationQuality = NewQuality;

    if (SimulationQuality == EVirtualSensorSimulationQuality::Debug)
    {
        CaptureResolution = FIntPoint(320, 180);
        CaptureInterval = 0.25f;
        JpegQuality = 60;
    }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::RealTimePreview)
    {
        CaptureResolution = FIntPoint(640, 360);
        CaptureInterval = 0.1f;
        JpegQuality = 70;
    }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::Balanced)
    {
        CaptureResolution = FIntPoint(960, 540);
        CaptureInterval = 1.0f / 15.0f;
        JpegQuality = 75;
    }
    else if (SimulationQuality == EVirtualSensorSimulationQuality::FullSpec)
    {
        CaptureResolution = FIntPoint(DeviceSpec.Width > 0 ? DeviceSpec.Width : 1280, DeviceSpec.Height > 0 ? DeviceSpec.Height : 720);
        CaptureInterval = 1.0f / FMath::Max(1.0f, DeviceSpec.FrameRateHz);
        JpegQuality = 80;
    }
}

void UVirtualCameraCaptureComponent::EnsureRenderTarget()
{
    if (!CameraRenderTarget)
    {
        CameraRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("VirtualCameraRenderTarget"));
    }

    if (CameraRenderTarget->SizeX != CaptureResolution.X || CameraRenderTarget->SizeY != CaptureResolution.Y)
    {
        CameraRenderTarget->InitCustomFormat(CaptureResolution.X, CaptureResolution.Y, PF_B8G8R8A8, true);
        CameraRenderTarget->ClearColor = FLinearColor::Black;
        CameraRenderTarget->UpdateResourceImmediate(true);
    }

    TextureTarget = CameraRenderTarget;
}

void UVirtualCameraCaptureComponent::CaptureAndSendImage()
{
	const int64 AcquisitionStartUnixNanoseconds = CameraUtcNowUnixNanoseconds();
    EnsureRenderTarget();
    CaptureScene();
	const int64 AcquisitionEndUnixNanoseconds = CameraUtcNowUnixNanoseconds();
    ++FrameId;

    if (!ShouldGeneratePayload())
    {
        LastJpegSnapshot.Reset();
        UpdateRuntimeStatus(0, TEXT("Preview only"));
        OnFrameCaptured.Broadcast(TEXT(""), CameraRenderTarget);
        return;
    }

    TArray64<uint8> JpegBytes;
    if (!ReadRenderTargetAsJpeg(JpegBytes))
    {
        UpdateRuntimeStatus(0, TEXT("Read render target failed"));
        return;
    }

    const FString Base64Image = FBase64::Encode(JpegBytes.GetData(), static_cast<uint32>(JpegBytes.Num()));
    const FString JsonPayload = BuildJsonPayload(Base64Image, JpegBytes.Num());
    LastJsonPayload = JsonPayload;

    FString StatusMessage = TEXT("Captured");
    if (CaptureMode == EVirtualCameraCaptureMode::PayloadAndOutput)
    {
        DispatchPayload(JsonPayload, JpegBytes);
        if (TransportComponent)
        {
            StatusMessage = TransportComponent->LastResult.Message;
        }
    }

    if (RecorderComponent)
    {
        RecorderComponent->RecordJsonFrame(SensorId, TEXT("virtual_camera"), FrameId, JsonPayload);
    }

    LastJpegSnapshot = MakeShared<const TArray64<uint8>, ESPMode::ThreadSafe>(MoveTemp(JpegBytes));
	LastJpegMetadata.Schema = TEXT("virtual-camera.jpeg.v1");
	LastJpegMetadata.Width = CaptureResolution.X;
	LastJpegMetadata.Height = CaptureResolution.Y;
	LastJpegMetadata.JpegQuality = JpegQuality;
	LastJpegMetadata.ByteCount = LastJpegSnapshot.IsValid() ? static_cast<int32>(LastJpegSnapshot->Num()) : 0;
	LastJpegMetadata.ScheduledUnixNanoseconds = AcquisitionStartUnixNanoseconds;
	LastJpegMetadata.AcquisitionStartUnixNanoseconds = AcquisitionStartUnixNanoseconds;
	LastJpegMetadata.AcquisitionEndUnixNanoseconds = AcquisitionEndUnixNanoseconds;
	LastJpegMetadata.DerivedCompleteUnixNanoseconds = CameraUtcNowUnixNanoseconds();

    UpdateRuntimeStatus(JsonPayload.Len(), StatusMessage);
    OnFrameCaptured.Broadcast(JsonPayload, CameraRenderTarget);
}

bool UVirtualCameraCaptureComponent::InjectExternalJsonPayload(const FString& JsonPayload, bool bSendTransport)
{
    FString PayloadSensorId;
    int64 PayloadFrameId = 0;
    int64 PayloadByteSize = 0;
    if (!ReadExternalPayloadMetadata(JsonPayload, PayloadSensorId, PayloadFrameId, PayloadByteSize))
    {
        UpdateRuntimeStatus(0, TEXT("External camera payload rejected"));
        return false;
    }

    LastJsonPayload = JsonPayload;
    LastJpegSnapshot.Reset();
    FrameId = FMath::Max(FrameId, PayloadFrameId);
    const FString RecordSensorId = PayloadSensorId.IsEmpty() ? SensorId : PayloadSensorId;

    if (bSendTransport)
    {
        DispatchJsonPayloadOnly(JsonPayload, RecordSensorId);
    }

    if (RecorderComponent)
    {
        RecorderComponent->RecordJsonFrame(RecordSensorId, TEXT("virtual_camera"), PayloadFrameId, JsonPayload);
    }

    UpdateRuntimeStatus(JsonPayload.Len(), FString::Printf(TEXT("Injected external camera payload bytes=%lld sendTransport=%s"),
        static_cast<long long>(PayloadByteSize),
        bSendTransport ? TEXT("true") : TEXT("false")));
    RuntimeStatus.SensorId = RecordSensorId;
    RuntimeStatus.FrameId = PayloadFrameId;
    OnFrameCaptured.Broadcast(JsonPayload, CameraRenderTarget);
    return true;
}

bool UVirtualCameraCaptureComponent::ReadRenderTargetAsJpeg(TArray64<uint8>& OutJpegBytes) const
{
    if (!CameraRenderTarget)
    {
        return false;
    }

    FTextureRenderTargetResource* Resource = CameraRenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return false;
    }

    TArray<FColor> RawPixels;
    if (!Resource->ReadPixels(RawPixels) || RawPixels.Num() == 0)
    {
        return false;
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), CameraRenderTarget->SizeX, CameraRenderTarget->SizeY, ERGBFormat::BGRA, 8);
    OutJpegBytes = ImageWrapper->GetCompressed(FMath::Clamp(JpegQuality, 1, 100));
    return OutJpegBytes.Num() > 0;
}

FString UVirtualCameraCaptureComponent::BuildJsonPayload(const FString& Base64Image, int64 ByteSize) const
{
    FVirtualCameraPayloadSnapshot Snapshot;
    Snapshot.SensorId = SensorId;
    Snapshot.Manufacturer = DeviceSpec.Manufacturer;
    Snapshot.Model = DeviceSpec.Model;
    Snapshot.SimulationQuality = ToPayloadSimulationQuality(SimulationQuality);
    Snapshot.FrameId = FrameId;
    Snapshot.TimestampUtc = FDateTime::UtcNow();
    Snapshot.Width = CaptureResolution.X;
    Snapshot.Height = CaptureResolution.Y;
    Snapshot.HorizontalFov = DeviceSpec.HorizontalFovDegrees;
    Snapshot.VerticalFov = DeviceSpec.VerticalFovDegrees;
    Snapshot.Location = GetComponentLocation();
    Snapshot.Rotation = GetComponentRotation();
    Snapshot.Forward = GetForwardVector();
    Snapshot.Up = GetUpVector();
    return FVirtualCameraPayloadCodec::EncodeJpegBase64(Snapshot, Base64Image, ByteSize);
}

void UVirtualCameraCaptureComponent::UpdateRuntimeStatus(int32 PayloadLength, const FString& Message)
{
    RuntimeStatus.SensorId = SensorId;
    RuntimeStatus.SensorType = TEXT("virtual_camera");
    RuntimeStatus.FrameId = FrameId;
    RuntimeStatus.LastUpdateUtc = FDateTime::UtcNow();
    RuntimeStatus.LastPayloadLength = PayloadLength;
    RuntimeStatus.LastMessage = FString::Printf(TEXT("%s Quality=%d Res=%dx%d"), *Message, static_cast<int32>(SimulationQuality), CaptureResolution.X, CaptureResolution.Y);
}

void UVirtualCameraCaptureComponent::DispatchPayload(const FString& JsonPayload, const TArray64<uint8>& JpegBytes) const
{
    if (TransportComponent)
    {
        TransportComponent->SendJson(SensorId, TEXT("virtual_camera"), JsonPayload);
        TArray<uint8> Bytes32;
        Bytes32.Append(JpegBytes.GetData(), static_cast<int32>(JpegBytes.Num()));
        TransportComponent->SendBinary(SensorId, TEXT("virtual_camera_frame"), TEXT("jpg"), Bytes32);
        return;
    }

    switch (OutputMode)
    {
    case EVirtualCameraOutputMode::None:
        break;
    case EVirtualCameraOutputMode::LogOnly:
        UE_LOG(LogTemp, Log, TEXT("[VirtualCamera:%s] payloadLength=%d"), *SensorId, JsonPayload.Len());
        break;
    case EVirtualCameraOutputMode::SaveJpeg:
        SaveJpegToDisk(JpegBytes);
        break;
    case EVirtualCameraOutputMode::HttpPost:
        PostJson(JsonPayload);
        break;
    default:
        break;
    }
}

void UVirtualCameraCaptureComponent::DispatchJsonPayloadOnly(const FString& JsonPayload, const FString& SensorIdOverride) const
{
    const FString EffectiveSensorId = SensorIdOverride.IsEmpty() ? SensorId : SensorIdOverride;

    if (TransportComponent)
    {
        TransportComponent->SendJson(EffectiveSensorId, TEXT("virtual_camera"), JsonPayload);
        return;
    }

    switch (OutputMode)
    {
    case EVirtualCameraOutputMode::None:
        break;
    case EVirtualCameraOutputMode::LogOnly:
        UE_LOG(LogTemp, Log, TEXT("[VirtualCamera:%s] externalPayloadLength=%d"), *EffectiveSensorId, JsonPayload.Len());
        break;
    case EVirtualCameraOutputMode::HttpPost:
        PostJson(JsonPayload);
        break;
    case EVirtualCameraOutputMode::SaveJpeg:
        UE_LOG(LogTemp, Log, TEXT("[VirtualCamera:%s] external payload has no local JPEG bytes to save."), *EffectiveSensorId);
        break;
    default:
        break;
    }
}

bool UVirtualCameraCaptureComponent::ReadExternalPayloadMetadata(const FString& JsonPayload, FString& OutSensorId, int64& OutFrameId, int64& OutByteSize) const
{
    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        return false;
    }

    FString SchemaVersion;
    FString SensorType;
    FString Encoding;
    FString Image;
    FString SensorIdField;
    FString Manufacturer;
    FString Model;
    FString TimestampUtc;
    FString PayloadSimulationQuality;
    if (!RootObject->TryGetStringField(TEXT("schemaVersion"), SchemaVersion) || SchemaVersion != TEXT("virtual-camera.v1"))
    {
        return false;
    }
    if (!RootObject->TryGetStringField(TEXT("sensorType"), SensorType) || SensorType != TEXT("virtual_camera"))
    {
        return false;
    }
    if (!RootObject->TryGetStringField(TEXT("encoding"), Encoding) || Encoding != TEXT("jpeg/base64"))
    {
        return false;
    }
    if (!RootObject->TryGetStringField(TEXT("sensorId"), SensorIdField) || SensorIdField.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }
    if (!RootObject->TryGetStringField(TEXT("manufacturer"), Manufacturer) || Manufacturer.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }
    if (!RootObject->TryGetStringField(TEXT("model"), Model) || Model.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }
    if (!RootObject->TryGetStringField(TEXT("timestampUtc"), TimestampUtc) || !TimestampUtc.EndsWith(TEXT("Z")))
    {
        return false;
    }
    if (!RootObject->TryGetStringField(TEXT("simulationQuality"), PayloadSimulationQuality) || !IsAllowedPayloadSimulationQuality(PayloadSimulationQuality))
    {
        return false;
    }

    FDateTime ParsedTimestampUtc;
    if (!FDateTime::ParseIso8601(*TimestampUtc, ParsedTimestampUtc))
    {
        return false;
    }

    if (!RootObject->TryGetStringField(TEXT("image"), Image) || Image.IsEmpty())
    {
        return false;
    }

    TArray<uint8> DecodedImageBytes;
    if (!FBase64::Decode(Image, DecodedImageBytes) || DecodedImageBytes.Num() <= 0)
    {
        return false;
    }
    if (DecodedImageBytes.Num() < 2 || DecodedImageBytes[0] != 0xff || DecodedImageBytes[1] != 0xd8)
    {
        return false;
    }

    double Width = 0.0;
    double Height = 0.0;
    double ByteSize = 0.0;
    double FrameIdNumber = 0.0;
    double HorizontalFov = 0.0;
    double VerticalFov = 0.0;
    if (!RootObject->TryGetNumberField(TEXT("width"), Width) || !IsPositiveWholeNumber(Width))
    {
        return false;
    }
    if (!RootObject->TryGetNumberField(TEXT("height"), Height) || !IsPositiveWholeNumber(Height))
    {
        return false;
    }
    if (!RootObject->TryGetNumberField(TEXT("byteSize"), ByteSize) || !IsPositiveWholeNumber(ByteSize))
    {
        return false;
    }
    if (static_cast<int64>(ByteSize) != DecodedImageBytes.Num())
    {
        return false;
    }
    if (!RootObject->TryGetNumberField(TEXT("frameId"), FrameIdNumber) || FrameIdNumber < 0.0 || !IsWholeNumber(FrameIdNumber))
    {
        return false;
    }
    if (!RootObject->TryGetNumberField(TEXT("horizontalFov"), HorizontalFov) || HorizontalFov <= 0.0 || !FMath::IsFinite(HorizontalFov))
    {
        return false;
    }
    if (!RootObject->TryGetNumberField(TEXT("verticalFov"), VerticalFov) || VerticalFov <= 0.0 || !FMath::IsFinite(VerticalFov))
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* TransformObject = nullptr;
    if (!RootObject->TryGetObjectField(TEXT("sensorTransform"), TransformObject) || !TransformObject || !TransformObject->IsValid())
    {
        return false;
    }
    if (!HasNumberArrayField(*TransformObject, TEXT("location"), 3) ||
        !HasNumberArrayField(*TransformObject, TEXT("rotation"), 3) ||
        !HasNumberArrayField(*TransformObject, TEXT("forward"), 3) ||
        !HasNumberArrayField(*TransformObject, TEXT("up"), 3))
    {
        return false;
    }
    if (!HasNumberArrayField(RootObject, TEXT("sensorWorldLocation"), 3))
    {
        return false;
    }

    OutSensorId = SensorIdField.TrimStartAndEnd();
    OutFrameId = static_cast<int64>(FrameIdNumber);
    OutByteSize = static_cast<int64>(ByteSize);
    return true;
}

void UVirtualCameraCaptureComponent::PostJson(const FString& JsonPayload) const
{
    if (HttpEndpoint.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualCamera:%s] HttpEndpoint is empty."), *SensorId);
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(HttpEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonPayload);
    Request->ProcessRequest();
}

void UVirtualCameraCaptureComponent::SaveJpegToDisk(const TArray64<uint8>& JpegBytes) const
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId);
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FString Timestamp = FString::Printf(TEXT("%s_%lld"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::UtcNow().GetTicks());
    const FString FileName = FString::Printf(TEXT("%s_%s.jpg"), *SensorId, *Timestamp);
    const FString Path = FPaths::Combine(Directory, FileName);

    TArray<uint8> Bytes32;
    Bytes32.Append(JpegBytes.GetData(), static_cast<int32>(JpegBytes.Num()));
    FFileHelper::SaveArrayToFile(Bytes32, *Path);
}

void UVirtualCameraCaptureComponent::TryAutoRegisterToManager()
{
    if (!bAutoRegisterToManager || !GetWorld())
    {
        return;
    }

    for (TActorIterator<AVirtualSensorCoordinator> It(GetWorld()); It; ++It)
    {
        It->RegisterCamera(this);
        break;
    }
}
