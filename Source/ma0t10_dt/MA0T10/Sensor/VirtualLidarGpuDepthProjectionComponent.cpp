#include "VirtualLidarGpuDepthProjectionComponent.h"
#include "VirtualLidarSemanticScene.h"
#include "VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorCaptureRendering.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformTime.h"
#include "Math/Float16Color.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "TextureResource.h"

namespace
{
int64 GpuDepthUtcNowUnixNanoseconds()
{
	static const FDateTime UnixEpoch(1970, 1, 1);
	return (FDateTime::UtcNow() - UnixEpoch).GetTicks() * 100;
}
}
UVirtualLidarGpuDepthProjectionComponent::UVirtualLidarGpuDepthProjectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	if (bUsePerformanceOptimizedShowFlags)
	{
		ShowFlags.DisableAdvancedFeatures();
		ShowFlags.SetLighting(false);
		ShowFlags.SetPostProcessing(false);
		ShowFlags.SetAtmosphere(false);
		ShowFlags.SetFog(false);
		ShowFlags.SetDynamicShadows(false);
		ShowFlags.SetMotionBlur(false);
	}
}

UVirtualLidarGpuDepthProjectionComponent::~UVirtualLidarGpuDepthProjectionComponent() = default;

void UVirtualLidarGpuDepthProjectionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelAcquisition();
    SemanticScene.Reset();
    Super::EndPlay(Reason);
}

bool UVirtualLidarGpuDepthProjectionComponent::IsAvailable() const
{
	return FApp::CanEverRender() && GDynamicRHI != nullptr && IsRegistered() && GetWorld() != nullptr;
}

void UVirtualLidarGpuDepthProjectionComponent::OnUnregister()
{
    CancelAcquisition();
    SemanticScene.Reset();
    Super::OnUnregister();
}

bool UVirtualLidarGpuDepthProjectionComponent::EnsureRenderTarget(const FVirtualLidarDepthAcquisitionRequest& Request)
{
	// Each depth frame measures the current scene, not a temporal image history.
	VirtualSensorCaptureRendering::Prepare(*this, true);
	const float VerticalFovDegrees = FMath::Max(
		1.0f,
		Request.MaxVerticalAngleDegrees - Request.MinVerticalAngleDegrees);
	const float HorizontalTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Request.HorizontalFovDegrees, 1.0f, 170.0f) * 0.5f));
	const float VerticalTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(VerticalFovDegrees, 1.0f, 170.0f) * 0.5f));
	const float PerspectiveAspect = HorizontalTan / FMath::Max(0.001f, VerticalTan);
	PendingCaptureWidth = FMath::Max(1, Request.HorizontalSamples);
	PendingCaptureHeight = FMath::Clamp(
		FMath::Max(Request.VerticalChannels, FMath::CeilToInt(static_cast<float>(PendingCaptureWidth) / PerspectiveAspect)),
		1,
		FMath::Max(64, MaximumCaptureHeight));

	if (!DepthRenderTarget)
	{
		DepthRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("VirtualLidarGpuDepthTarget"));
	}
	if (!DepthRenderTarget)
	{
		StatusMessage = TEXT("GPU depth render target allocation failed");
		return false;
	}
	if (DepthRenderTarget->SizeX != PendingCaptureWidth || DepthRenderTarget->SizeY != PendingCaptureHeight)
	{
		DepthRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
		DepthRenderTarget->ClearColor = FLinearColor::Black;
		DepthRenderTarget->InitAutoFormat(PendingCaptureWidth, PendingCaptureHeight);
		DepthRenderTarget->UpdateResourceImmediate(true);
	}
	TextureTarget = DepthRenderTarget;
	FOVAngle = FMath::Clamp(Request.HorizontalFovDegrees, 1.0f, 170.0f);
	return TextureTarget != nullptr;
}

bool UVirtualLidarGpuDepthProjectionComponent::BeginAcquisition(const FVirtualLidarDepthAcquisitionRequest& Request)
{
	if (bAcquisitionActive || !IsAvailable())
	{
		StatusMessage = bAcquisitionActive
			? TEXT("GPU depth acquisition already in flight")
			: TEXT("GPU depth backend unavailable for this RHI/world");
		return false;
	}
	if (!EnsureRenderTarget(Request))
	{
		return false;
	}

	PendingRequest = Request;
	SetWorldTransform(Request.AcquisitionTransform);
	bAcquisitionActive = true;
	bReadbackQueued = false;
	AcquisitionSubmittedSeconds = FPlatformTime::Seconds();

	// CaptureSceneDeferred waits for the next main-view render and the previous
	// implementation then waited another scheduler tick before enqueueing the
	// readback. At 40-60 game FPS that state machine alone limited a nominal
	// 20 Hz ML-X scan to roughly 10-14 Hz. CaptureScene enqueues this component's
	// render pass immediately; QueueReadback follows it on the render command
	// list, so the copy still observes the coherent capture while removing one
	// full game-frame of latency. PollAcquisition remains non-blocking.
	CaptureScene();
	PendingSemanticIdentities.Reset();
	PendingSemanticStatus = TEXT("GPU 의미 분류 비활성");
	if (Request.bRequestSemantics)
	{
		if (auto* Scan = GetOwner() ? GetOwner()->FindComponentByClass<UVirtualLidarScanComponent>() : nullptr)
		{
			if (!SemanticScene) SemanticScene = MakeUnique<FVirtualLidarSemanticScene>();
			if (SemanticScene->Capture(*Scan, Request, PendingCaptureWidth, PendingCaptureHeight))
				PendingSemanticIdentities = SemanticScene->Identities;
			PendingSemanticStatus = SemanticScene->Status;
		}
	}
	QueueReadback();
	StatusMessage = bReadbackQueued
		? TEXT("GPU SceneDepth capture and readback submitted")
		: TEXT("GPU SceneDepth capture submitted; readback pending");
	return true;
}

void UVirtualLidarGpuDepthProjectionComponent::QueueReadback()
{
	if (!DepthRenderTarget || bReadbackQueued) return;
	FTextureRenderTargetResource* Resource = DepthRenderTarget->GameThread_GetRenderTargetResource();
	const FTextureRHIRef Texture = Resource ? Resource->GetRenderTargetTexture() : FTextureRHIRef();
	if (!Texture.IsValid())
	{
		if (FPlatformTime::Seconds() - AcquisitionSubmittedSeconds > 1.0)
		{
			StatusMessage = TEXT("GPU SceneDepth texture initialization failed");
			bAcquisitionActive = false;
		}
		else
		{
			StatusMessage = TEXT("GPU SceneDepth texture initialization pending");
		}
		return;
	}

	Readback = MakeShared<FRHIGPUTextureReadback, ESPMode::ThreadSafe>(TEXT("VirtualLidarGpuDepthReadback"));
	FTextureRHIRef SemanticTexture;
	if (PendingRequest.bRequestSemantics && SemanticScene && PendingSemanticIdentities.Num() > 0)
	{
		SemanticTexture = SemanticScene->GetTarget()->GameThread_GetRenderTargetResource()->GetRenderTargetTexture();
		if (!SemanticTexture.IsValid()) { StatusMessage = TEXT("GPU semantic target initialization pending"); return; }
		if (SemanticTexture.IsValid()) SemanticReadback = MakeShared<FRHIGPUTextureReadback, ESPMode::ThreadSafe>(TEXT("VirtualLidarSemanticReadback"));
	}
	const auto CapturedSemanticReadback = SemanticReadback;
	const TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> CapturedReadback = Readback;
	ENQUEUE_RENDER_COMMAND(VirtualLidarGpuDepthReadback)(
		[CapturedReadback, Texture, CapturedSemanticReadback, SemanticTexture](FRHICommandListImmediate& RHICmdList)
		{
			if (CapturedReadback.IsValid() && Texture.IsValid())
			{
				CapturedReadback->EnqueueCopy(RHICmdList, Texture);
			}
			if (CapturedSemanticReadback.IsValid() && SemanticTexture.IsValid()) CapturedSemanticReadback->EnqueueCopy(RHICmdList, SemanticTexture);
		});
	bReadbackQueued = true;
	StatusMessage = TEXT("GPU SceneDepth readback pending");
}

EVirtualSensorBackendPollResult UVirtualLidarGpuDepthProjectionComponent::PollAcquisition(
	FVirtualLidarDepthAcquisitionFrame& OutFrame)
{
	if (CompletedFrame.IsSet())
	{
		OutFrame = MoveTemp(CompletedFrame.GetValue());
		CompletedFrame.Reset();
		bAcquisitionActive = false;
		bReadbackQueued = false;
		StatusMessage = TEXT("GPU SceneDepth frame completed");
		return EVirtualSensorBackendPollResult::Completed;
	}
	if (!bAcquisitionActive)
	{
		return StatusMessage.Contains(TEXT("failed")) || StatusMessage.Contains(TEXT("unavailable"))
			? EVirtualSensorBackendPollResult::Failed
			: EVirtualSensorBackendPollResult::Idle;
	}
	if (!bReadbackQueued)
	{
		QueueReadback();
		return bAcquisitionActive ? EVirtualSensorBackendPollResult::Pending : EVirtualSensorBackendPollResult::Failed;
	}
	if (bReadbackCopyInFlight || !Readback.IsValid() || !Readback->IsReady() || (SemanticReadback.IsValid() && !SemanticReadback->IsReady()))
	{
		return EVirtualSensorBackendPollResult::Pending;
	}

	const int32 Width = PendingCaptureWidth;
	const int32 Height = PendingCaptureHeight;
	const int32 Generation = AcquisitionGeneration;
	const FVirtualLidarDepthAcquisitionRequest Request = PendingRequest;
	TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> CapturedReadback = MoveTemp(Readback);
	auto CapturedSemanticReadback = MoveTemp(SemanticReadback);
	auto Identities = MoveTemp(PendingSemanticIdentities);
	const FString SemanticStatus = PendingSemanticStatus;
	TWeakObjectPtr<UVirtualLidarGpuDepthProjectionComponent> WeakThis(this);
	bReadbackCopyInFlight = true;
	StatusMessage = TEXT("GPU SceneDepth staging copy pending");

	// Lock/Unlock maps an RHI staging texture and must run on the rendering
	// thread in UE 5.3. Keeping that work off the game-thread scheduler avoids
	// forced RHI command-list flushes and the associated FullSpec hitch.
	ENQUEUE_RENDER_COMMAND(VirtualLidarConsumeGpuDepthReadback)(
		[CapturedReadback = MoveTemp(CapturedReadback), CapturedSemanticReadback = MoveTemp(CapturedSemanticReadback), Identities = MoveTemp(Identities), SemanticStatus, WeakThis, Request, Width, Height, Generation](FRHICommandListImmediate& RHICmdList) mutable
		{
			FVirtualLidarDepthAcquisitionFrame Frame;
			Frame.Request = Request;
			Frame.CaptureWidth = Width;
			Frame.CaptureHeight = Height;
			Frame.SemanticIdentities = MoveTemp(Identities);
			Frame.SemanticStatus = SemanticStatus;
			bool bCopySucceeded = false;
			int32 RowPitchInPixels = 0;
			void* LockedData = CapturedReadback.IsValid() ? CapturedReadback->Lock(RowPitchInPixels) : nullptr;
			if (LockedData && RowPitchInPixels >= Width && Width > 0 && Height > 0)
			{
				Frame.ForwardDepthCentimeters.SetNumUninitialized(Width * Height);
				const FLinearColor* Source = static_cast<const FLinearColor*>(LockedData);
				for (int32 Y = 0; Y < Height; ++Y)
				{
					for (int32 X = 0; X < Width; ++X)
					{
						Frame.ForwardDepthCentimeters[Y * Width + X] = Source[Y * RowPitchInPixels + X].R;
					}
				}
				bCopySucceeded = true;
			}
			if (LockedData)
			{
				CapturedReadback->Unlock();
			}
			if (CapturedSemanticReadback.IsValid())
			{
				int32 Pitch = 0;
				const FLinearColor* Pixels = static_cast<const FLinearColor*>(CapturedSemanticReadback->Lock(Pitch));
				if (Pixels && Pitch >= Width)
				{
					Frame.SemanticIdDepth.SetNumUninitialized(Width * Height);
					for (int32 Y = 0; Y < Height; ++Y) FMemory::Memcpy(Frame.SemanticIdDepth.GetData() + Y * Width, Pixels + Y * Pitch, Width * sizeof(FLinearColor));
				}
				if (Pixels) CapturedSemanticReadback->Unlock();
			}
			Frame.AcquisitionEndUnixNanoseconds = GpuDepthUtcNowUnixNanoseconds();

			AsyncTask(ENamedThreads::GameThread,
				[WeakThis, Generation, bCopySucceeded, Frame = MoveTemp(Frame)]() mutable
				{
					if (!WeakThis.IsValid() || WeakThis->AcquisitionGeneration != Generation)
					{
						return;
					}
					WeakThis->bReadbackCopyInFlight = false;
					if (!bCopySucceeded)
					{
						WeakThis->bAcquisitionActive = false;
						WeakThis->bReadbackQueued = false;
						WeakThis->StatusMessage = TEXT("GPU SceneDepth readback failed");
						return;
					}
					WeakThis->CompletedFrame = MoveTemp(Frame);
					WeakThis->StatusMessage = TEXT("GPU SceneDepth staging copy completed");
				});
		});
	return EVirtualSensorBackendPollResult::Pending;
}

void UVirtualLidarGpuDepthProjectionComponent::CancelAcquisition()
{
	++AcquisitionGeneration;
	ReleaseReadbackOnRenderThread();
	CompletedFrame.Reset();
	bAcquisitionActive = false;
	bReadbackQueued = false;
	bReadbackCopyInFlight = false;
	StatusMessage = TEXT("GPU depth acquisition cancelled");
}

void UVirtualLidarGpuDepthProjectionComponent::ReleaseReadbackOnRenderThread()
{
	auto Semantic = MoveTemp(SemanticReadback);
	if (Semantic.IsValid()) ENQUEUE_RENDER_COMMAND(ReleaseLidarSemantic)([Semantic = MoveTemp(Semantic)](FRHICommandListImmediate&) mutable { Semantic.Reset(); });
	if (!Readback.IsValid())
	{
		return;
	}
	TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> CapturedReadback = MoveTemp(Readback);
	ENQUEUE_RENDER_COMMAND(VirtualLidarReleaseGpuDepthReadback)(
		[CapturedReadback = MoveTemp(CapturedReadback)](FRHICommandListImmediate& RHICmdList) mutable
		{
			CapturedReadback.Reset();
		});
}
