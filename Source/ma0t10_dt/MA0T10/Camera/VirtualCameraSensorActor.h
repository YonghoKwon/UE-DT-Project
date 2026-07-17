// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "VirtualCameraSensorActor.generated.h"

class UDrawFrustumComponent;
class UTextureRenderTarget2D;
class UVirtualCameraCaptureComponent;

UCLASS()
class MA0T10_DT_API AVirtualCameraSensorActor : public AVirtualSensorActorBase
{
	GENERATED_BODY()

public:
	AVirtualCameraSensorActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UVirtualCameraCaptureComponent> CaptureComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|EditorVisualization")
	TObjectPtr<UDrawFrustumComponent> EditorFrustumComp;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual FString GetSensorId() const override;
	virtual EVirtualSensorKind GetSensorKind() const override;
	virtual bool IsSensorRunning() const override;
	virtual void StartSensor() override;
	virtual void StopSensor() override;
	virtual void CaptureSensorOnce() override;
	virtual UTexture* GetSensorPreviewTexture() const override;
	virtual FVirtualSensorRuntimeStatus GetSensorRuntimeStatus() const override;
	virtual bool SubmitExternalFrame(const FVirtualSensorFrameEnvelope& Frame, bool bSendTransport) override;
	virtual bool ReadEditableState(FVirtualSensorEditableState& OutState) const override;
	virtual bool ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const override;
	virtual bool ApplyEditableState(const FVirtualSensorEditableState& State, FString& OutError) override;
	virtual bool ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError) override;

private:
	UFUNCTION()
	void HandleCameraFrame(const FString& JsonPayload, UTextureRenderTarget2D* RenderTarget);

	TOptional<bool> PendingExternalSendTransport;
};
