#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"

AVirtualSensorActorBase::AVirtualSensorActorBase()
{
	PrimaryActorTick.bCanEverTick = false;
	OutputComponent = CreateDefaultSubobject<UVirtualSensorOutputComponent>(TEXT("SensorOutputComponent"));
}

FString AVirtualSensorActorBase::GetSensorId() const
{
	return FString();
}

EVirtualSensorKind AVirtualSensorActorBase::GetSensorKind() const
{
	return EVirtualSensorKind::Camera;
}

bool AVirtualSensorActorBase::IsSensorRunning() const
{
	return false;
}

void AVirtualSensorActorBase::StartSensor()
{
}

void AVirtualSensorActorBase::StopSensor()
{
}

void AVirtualSensorActorBase::CaptureSensorOnce()
{
}

void AVirtualSensorActorBase::RefreshSensorPreview()
{
	CaptureSensorOnce();
}

UTexture* AVirtualSensorActorBase::GetSensorPreviewTexture() const
{
	return nullptr;
}

FVirtualSensorRuntimeStatus AVirtualSensorActorBase::GetSensorRuntimeStatus() const
{
	return FVirtualSensorRuntimeStatus();
}

bool AVirtualSensorActorBase::SubmitExternalFrame(const FVirtualSensorFrameEnvelope& Frame, bool bSendTransport)
{
	return bSendTransport && OutputComponent ? OutputComponent->RouteFrame(Frame) : Frame.HasJsonPayload();
}

bool AVirtualSensorActorBase::ReadEditableState(FVirtualSensorEditableState& OutState) const
{
	return false;
}

bool AVirtualSensorActorBase::ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const
{
	OutError = TEXT("이 센서는 런타임 설정 검증을 지원하지 않습니다.");
	return false;
}

bool AVirtualSensorActorBase::ApplyEditableState(const FVirtualSensorEditableState& State, FString& OutError)
{
	OutError = TEXT("이 센서 Actor는 런타임 설정 편집을 지원하지 않습니다.");
	return false;
}

bool AVirtualSensorActorBase::ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError)
{
	OutError = TEXT("이 센서 Actor는 장비 프로필/품질 프리셋 적용을 지원하지 않습니다.");
	return false;
}

bool AVirtualSensorActorBase::BeginInteractiveManipulation(const FVirtualSensorInteractionRequest& Request)
{
	bInteractiveManipulationActive = true;
	return true;
}

bool AVirtualSensorActorBase::UpdateInteractiveTransform(const FTransform& Transform)
{
	if (!bInteractiveManipulationActive || Transform.ContainsNaN()) return false;
	SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	return true;
}

void AVirtualSensorActorBase::EndInteractiveManipulation()
{
	bInteractiveManipulationActive = false;
}

void AVirtualSensorActorBase::SetSharedOutputServices(
	UVirtualSensorTransportComponent* Transport,
	UVirtualSensorRecorderComponent* Recorder)
{
	if (OutputComponent)
	{
		OutputComponent->SetSharedServices(Transport, Recorder);
	}
}
