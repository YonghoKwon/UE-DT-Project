#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorAcquisitionBackend.h"
#include "VirtualLidarGpuDepthProjectionComponent.generated.h"

class FRHIGPUTextureReadback;
class UTextureRenderTarget2D;

/**
 * Asynchronous SceneDepth acquisition for dense solid-state LiDAR profiles.
 * It captures only first-surface depth. Multi-echo fidelity remains a CPU/HWRT
 * responsibility and is reported by the owner as a backend limitation.
 */
UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualLidarGpuDepthProjectionComponent
	: public USceneCaptureComponent2D
	, public IVirtualSensorAcquisitionBackend
{
	GENERATED_BODY()

public:
	UVirtualLidarGpuDepthProjectionComponent();

	virtual EVirtualLidarAcquisitionBackend GetBackendType() const override
	{
		return EVirtualLidarAcquisitionBackend::GpuDepthProjection;
	}
	virtual bool IsAvailable() const override;
	virtual bool BeginAcquisition(const FVirtualLidarDepthAcquisitionRequest& Request) override;
	virtual EVirtualSensorBackendPollResult PollAcquisition(FVirtualLidarDepthAcquisitionFrame& OutFrame) override;
	virtual void CancelAcquisition() override;
	virtual FString GetBackendStatusMessage() const override { return StatusMessage; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|GPU", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 MaximumCaptureHeight = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|VirtualLidar|GPU")
	bool bUsePerformanceOptimizedShowFlags = true;

private:
	bool EnsureRenderTarget(const FVirtualLidarDepthAcquisitionRequest& Request);
	void QueueReadback();
	void ReleaseReadbackOnRenderThread();

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> DepthRenderTarget;

	TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback;
	FVirtualLidarDepthAcquisitionRequest PendingRequest;
	int32 PendingCaptureWidth = 0;
	int32 PendingCaptureHeight = 0;
	bool bAcquisitionActive = false;
	bool bReadbackQueued = false;
	bool bReadbackCopyInFlight = false;
	int32 AcquisitionGeneration = 0;
	TOptional<FVirtualLidarDepthAcquisitionFrame> CompletedFrame;
	double AcquisitionSubmittedSeconds = 0.0;
	FString StatusMessage = TEXT("GPU depth backend idle");
};
