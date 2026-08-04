#include "VirtualLidarGpuDepthProjectionComponent.h"

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

bool UVirtualLidarGpuDepthProjectionComponent::IsAvailable() const
{
	return FApp::CanEverRender() && GDynamicRHI != nullptr && IsRegistered() && GetWorld() != nullptr;
}

bool UVirtualLidarGpuDepthProjectionComponent::EnsureRenderTarget(const FVirtualLidarDepthAcquisitionRequest& Request)
{
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
		DepthRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
		DepthRenderTarget->ClearColor = FLinearColor::Black;
		DepthRenderTarget->InitCustomFormat(PendingCaptureWidth, PendingCaptureHeight, PF_FloatRGBA, false);
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
	CaptureSceneDeferred();
	bAcquisitionActive = true;
	bReadbackQueued = false;
	AcquisitionSubmittedSeconds = FPlatformTime::Seconds();
	StatusMessage = TEXT("GPU SceneDepth capture submitted");
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
	const TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> CapturedReadback = Readback;
	ENQUEUE_RENDER_COMMAND(VirtualLidarGpuDepthReadback)(
		[CapturedReadback, Texture](FRHICommandListImmediate& RHICmdList)
		{
			if (CapturedReadback.IsValid() && Texture.IsValid())
			{
				CapturedReadback->EnqueueCopy(RHICmdList, Texture);
			}
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
	if (bReadbackCopyInFlight || !Readback.IsValid() || !Readback->IsReady())
	{
		return EVirtualSensorBackendPollResult::Pending;
	}

	const int32 Width = PendingCaptureWidth;
	const int32 Height = PendingCaptureHeight;
	const int32 Generation = AcquisitionGeneration;
	const FVirtualLidarDepthAcquisitionRequest Request = PendingRequest;
	TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> CapturedReadback = MoveTemp(Readback);
	TWeakObjectPtr<UVirtualLidarGpuDepthProjectionComponent> WeakThis(this);
	bReadbackCopyInFlight = true;
	StatusMessage = TEXT("GPU SceneDepth staging copy pending");

	// Lock/Unlock maps an RHI staging texture and must run on the rendering
	// thread in UE 5.3. Keeping that work off the game-thread scheduler avoids
	// forced RHI command-list flushes and the associated FullSpec hitch.
	ENQUEUE_RENDER_COMMAND(VirtualLidarConsumeGpuDepthReadback)(
		[CapturedReadback = MoveTemp(CapturedReadback), WeakThis, Request, Width, Height, Generation](FRHICommandListImmediate& RHICmdList) mutable
		{
			FVirtualLidarDepthAcquisitionFrame Frame;
			Frame.Request = Request;
			Frame.CaptureWidth = Width;
			Frame.CaptureHeight = Height;
			bool bCopySucceeded = false;
			int32 RowPitchInPixels = 0;
			void* LockedData = CapturedReadback.IsValid() ? CapturedReadback->Lock(RowPitchInPixels) : nullptr;
			if (LockedData && RowPitchInPixels >= Width && Width > 0 && Height > 0)
			{
				Frame.ForwardDepthCentimeters.SetNumUninitialized(Width * Height);
				const FFloat16Color* Source = static_cast<const FFloat16Color*>(LockedData);
				for (int32 Y = 0; Y < Height; ++Y)
				{
					for (int32 X = 0; X < Width; ++X)
					{
						Frame.ForwardDepthCentimeters[Y * Width + X] = Source[Y * RowPitchInPixels + X].R.GetFloat();
					}
				}
				bCopySucceeded = true;
			}
			if (LockedData)
			{
				CapturedReadback->Unlock();
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
