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
		StatusMessage = TEXT("GPU SceneDepth texture unavailable");
		bAcquisitionActive = false;
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
	if (!Readback.IsValid() || !Readback->IsReady())
	{
		return EVirtualSensorBackendPollResult::Pending;
	}

	int32 RowPitchInPixels = 0;
	void* LockedData = Readback->Lock(RowPitchInPixels);
	if (!LockedData || RowPitchInPixels < PendingCaptureWidth)
	{
		if (LockedData) Readback->Unlock();
		CancelAcquisition();
		StatusMessage = TEXT("GPU SceneDepth readback failed");
		return EVirtualSensorBackendPollResult::Failed;
	}

	OutFrame.Request = PendingRequest;
	OutFrame.CaptureWidth = PendingCaptureWidth;
	OutFrame.CaptureHeight = PendingCaptureHeight;
	OutFrame.ForwardDepthCentimeters.SetNumUninitialized(PendingCaptureWidth * PendingCaptureHeight);
	const FFloat16Color* Source = static_cast<const FFloat16Color*>(LockedData);
	for (int32 Y = 0; Y < PendingCaptureHeight; ++Y)
	{
		for (int32 X = 0; X < PendingCaptureWidth; ++X)
		{
			OutFrame.ForwardDepthCentimeters[Y * PendingCaptureWidth + X] = Source[Y * RowPitchInPixels + X].R.GetFloat();
		}
	}
	Readback->Unlock();
	Readback.Reset();
	bAcquisitionActive = false;
	bReadbackQueued = false;
	OutFrame.AcquisitionEndUnixNanoseconds = GpuDepthUtcNowUnixNanoseconds();
	StatusMessage = TEXT("GPU SceneDepth frame completed");
	return EVirtualSensorBackendPollResult::Completed;
}

void UVirtualLidarGpuDepthProjectionComponent::CancelAcquisition()
{
	if (Readback.IsValid() && Readback->IsReady())
	{
		Readback.Reset();
	}
	else
	{
		Readback.Reset();
	}
	bAcquisitionActive = false;
	bReadbackQueued = false;
	StatusMessage = TEXT("GPU depth acquisition cancelled");
}
