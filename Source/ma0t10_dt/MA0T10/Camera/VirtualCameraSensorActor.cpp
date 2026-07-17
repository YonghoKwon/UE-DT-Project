// Fill out your copyright notice in the Description page of Project Settings.

#include "VirtualCameraSensorActor.h"

#include "Components/DrawFrustumComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"

AVirtualCameraSensorActor::AVirtualCameraSensorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CaptureComponent = CreateDefaultSubobject<UVirtualCameraCaptureComponent>(TEXT("CameraCaptureComponent"));
	RootComponent = CaptureComponent;

	EditorFrustumComp = CreateDefaultSubobject<UDrawFrustumComponent>(TEXT("EditorFrustumComp"));
	EditorFrustumComp->SetupAttachment(RootComponent);
	EditorFrustumComp->FrustumAngle = 87.0f;
	EditorFrustumComp->FrustumStartDist = 20.0f;
	EditorFrustumComp->FrustumEndDist = 600.0f;
	EditorFrustumComp->FrustumAspectRatio = 16.0f / 9.0f;
	EditorFrustumComp->SetHiddenInGame(true);
}

void AVirtualCameraSensorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (EditorFrustumComp && CaptureComponent)
	{
		EditorFrustumComp->FrustumAngle = CaptureComponent->FOVAngle;
		EditorFrustumComp->FrustumEndDist = CaptureComponent->GetDeviceSpec().TypicalRangeCm > 0.0f
			? CaptureComponent->GetDeviceSpec().TypicalRangeCm
			: 600.0f;
		EditorFrustumComp->FrustumAspectRatio = CaptureComponent->CaptureResolution.Y > 0
			? static_cast<float>(CaptureComponent->CaptureResolution.X) / static_cast<float>(CaptureComponent->CaptureResolution.Y)
			: 16.0f / 9.0f;
	}
}

void AVirtualCameraSensorActor::BeginPlay()
{
	Super::BeginPlay();
	if (EditorFrustumComp)
	{
		EditorFrustumComp->SetHiddenInGame(true);
	}
	if (CaptureComponent)
	{
		CaptureComponent->SetTransportComponent(nullptr);
		CaptureComponent->SetRecorderComponent(nullptr);
		CaptureComponent->OutputMode = EVirtualCameraOutputMode::None;
		CaptureComponent->OnFrameCaptured.AddDynamic(this, &AVirtualCameraSensorActor::HandleCameraFrame);
	}
}

FString AVirtualCameraSensorActor::GetSensorId() const
{
	return CaptureComponent ? CaptureComponent->SensorId : FString();
}

EVirtualSensorKind AVirtualCameraSensorActor::GetSensorKind() const
{
	return EVirtualSensorKind::Camera;
}

bool AVirtualCameraSensorActor::IsSensorRunning() const
{
	return CaptureComponent && CaptureComponent->IsCaptureRunning();
}

void AVirtualCameraSensorActor::StartSensor()
{
	if (CaptureComponent) CaptureComponent->StartCapture();
}

void AVirtualCameraSensorActor::StopSensor()
{
	if (CaptureComponent) CaptureComponent->StopCapture();
}

void AVirtualCameraSensorActor::CaptureSensorOnce()
{
	if (CaptureComponent) CaptureComponent->CaptureAndSendImage();
}

UTexture* AVirtualCameraSensorActor::GetSensorPreviewTexture() const
{
	return CaptureComponent ? CaptureComponent->GetCameraRenderTarget() : nullptr;
}

FVirtualSensorRuntimeStatus AVirtualCameraSensorActor::GetSensorRuntimeStatus() const
{
	return CaptureComponent ? CaptureComponent->GetRuntimeStatus() : FVirtualSensorRuntimeStatus();
}

bool AVirtualCameraSensorActor::SubmitExternalFrame(const FVirtualSensorFrameEnvelope& Frame, bool bSendTransport)
{
	if (!CaptureComponent || Frame.SensorKind != EVirtualSensorKind::Camera || !Frame.HasJsonPayload()) return false;
	PendingExternalSendTransport = bSendTransport;
	const bool bAccepted = CaptureComponent->InjectExternalJsonPayload(*Frame.JsonPayload, false);
	if (!bAccepted)
	{
		PendingExternalSendTransport.Reset();
	}
	return bAccepted;
}

bool AVirtualCameraSensorActor::ReadEditableState(FVirtualSensorEditableState& OutState) const
{
	if (!CaptureComponent) return false;
	OutState.TargetKind = EVirtualSensorTargetKind::Camera;
	OutState.OriginalSensorId = CaptureComponent->SensorId;
	OutState.SensorId = CaptureComponent->SensorId;
	OutState.ActorTransform = GetActorTransform();
	OutState.SimulationQuality = CaptureComponent->SimulationQuality;
	OutState.CameraProfile = CaptureComponent->DeviceProfile;
	OutState.CameraResolution = CaptureComponent->CaptureResolution;
	OutState.CameraCaptureInterval = CaptureComponent->CaptureInterval;
	OutState.CameraFov = CaptureComponent->FOVAngle;
	OutState.CameraJpegQuality = CaptureComponent->JpegQuality;
	OutState.CameraCaptureMode = CaptureComponent->CaptureMode;
	return true;
}

bool AVirtualCameraSensorActor::ApplyEditableState(const FVirtualSensorEditableState& State, FString& OutError)
{
	if (!ValidateEditableState(State, OutError))
	{
		return false;
	}
	SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	CaptureComponent->StopCapture();
	CaptureComponent->ApplyDeviceProfile(State.CameraProfile);
	CaptureComponent->ApplySimulationQuality(State.SimulationQuality);
	CaptureComponent->SensorId = State.SensorId;
	CaptureComponent->CaptureResolution = State.CameraResolution;
	CaptureComponent->CaptureInterval = State.CameraCaptureInterval;
	CaptureComponent->FOVAngle = State.CameraFov;
	CaptureComponent->JpegQuality = State.CameraJpegQuality;
	CaptureComponent->CaptureMode = State.CameraCaptureMode;
	CaptureComponent->StartCapture();
	return true;
}

bool AVirtualCameraSensorActor::BeginInteractiveManipulation(const FVirtualSensorInteractionRequest& Request)
{
	if (!CaptureComponent || bInteractiveManipulationActive) return CaptureComponent != nullptr;
	Super::BeginInteractiveManipulation(Request);
	bWasRunningBeforeInteraction = CaptureComponent->IsCaptureRunning();
	SavedInteractionResolution = CaptureComponent->CaptureResolution;
	SavedInteractionInterval = CaptureComponent->CaptureInterval;
	SavedInteractionCaptureMode = static_cast<uint8>(CaptureComponent->CaptureMode);
	CaptureComponent->StopCapture();
	CaptureComponent->CaptureResolution = Request.CameraPreviewResolution;
	CaptureComponent->CaptureInterval = Request.CameraPreviewInterval;
	CaptureComponent->CaptureMode = EVirtualCameraCaptureMode::PreviewOnly;
	if (bWasRunningBeforeInteraction) CaptureComponent->StartCapture();
	return true;
}

void AVirtualCameraSensorActor::EndInteractiveManipulation()
{
	if (!CaptureComponent || !bInteractiveManipulationActive) return;
	CaptureComponent->StopCapture();
	CaptureComponent->CaptureResolution = SavedInteractionResolution;
	CaptureComponent->CaptureInterval = SavedInteractionInterval;
	CaptureComponent->CaptureMode = static_cast<EVirtualCameraCaptureMode>(SavedInteractionCaptureMode);
	if (bWasRunningBeforeInteraction)
	{
		CaptureComponent->StartCapture();
		CaptureComponent->RequestImmediateScheduledCapture();
	}
	Super::EndInteractiveManipulation();
}

bool AVirtualCameraSensorActor::ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError)
{
	if (!CaptureComponent || RequestedState.TargetKind != EVirtualSensorTargetKind::Camera)
	{
		OutError = TEXT("선택한 Camera 센서를 사용할 수 없습니다.");
		return false;
	}
	const bool bWasRunning = CaptureComponent->IsCaptureRunning();
	CaptureComponent->StopCapture();
	CaptureComponent->ApplyDeviceProfile(RequestedState.CameraProfile);
	CaptureComponent->ApplySimulationQuality(RequestedState.SimulationQuality);
	if (bWasRunning) CaptureComponent->StartCapture();
	return ReadEditableState(OutAppliedState);
}

bool AVirtualCameraSensorActor::ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const
{
	if (!CaptureComponent || State.TargetKind != EVirtualSensorTargetKind::Camera)
	{
		OutError = TEXT("선택한 Camera 센서를 사용할 수 없습니다.");
		return false;
	}
	if (State.SensorId.TrimStartAndEnd().IsEmpty() || State.ActorTransform.ContainsNaN())
	{
		OutError = TEXT("SensorId 또는 Transform 값이 올바르지 않습니다.");
		return false;
	}
	if (State.CameraResolution.X < 160 || State.CameraResolution.X > 4096 ||
		State.CameraResolution.Y < 90 || State.CameraResolution.Y > 2160 ||
		State.CameraCaptureInterval < 0.033f || State.CameraCaptureInterval > 60.0f ||
		State.CameraFov < 5.0f || State.CameraFov > 170.0f ||
		State.CameraJpegQuality < 1 || State.CameraJpegQuality > 100)
	{
		OutError = TEXT("Camera 설정이 지원 범위를 벗어났습니다.");
		return false;
	}
	return true;
}

void AVirtualCameraSensorActor::HandleCameraFrame(const FString& JsonPayload, UTextureRenderTarget2D* RenderTarget)
{
	if (!OutputComponent || JsonPayload.IsEmpty() || !CaptureComponent) return;
	FVirtualSensorFrameEnvelope Frame;
	const FVirtualSensorRuntimeStatus& Status = CaptureComponent->GetRuntimeStatus();
	Frame.SensorId = PendingExternalSendTransport.IsSet() && !Status.SensorId.IsEmpty()
		? Status.SensorId
		: CaptureComponent->SensorId;
	Frame.SensorKind = EVirtualSensorKind::Camera;
	Frame.FrameId = Status.FrameId;
	Frame.TimestampUtc = FDateTime::UtcNow();
	Frame.SchemaVersion = TEXT("virtual-camera.v1");
	Frame.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(JsonPayload);
	Frame.bSendTransport = PendingExternalSendTransport.IsSet()
		? PendingExternalSendTransport.GetValue()
		: CaptureComponent->CaptureMode == EVirtualCameraCaptureMode::PayloadAndOutput;
	Frame.bRecord = true;
	if (Frame.bSendTransport)
	{
		Frame.BinaryPayload = CaptureComponent->GetLastJpegSnapshot();
	}
	PendingExternalSendTransport.Reset();
	OutputComponent->RouteFrame(Frame);
}

void AVirtualCameraSensorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
