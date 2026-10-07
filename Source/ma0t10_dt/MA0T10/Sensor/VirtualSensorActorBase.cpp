#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"

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
	FString Reason;if(!CanEditSensorConfiguration(Reason))return false;
	bInteractiveManipulationActive = true;
	return true;
}
bool AVirtualSensorActorBase::CanEditSensorConfiguration(FString& OutReason) const
{
	OutReason.Reset();
	if(GetWorld())if(const auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>())
		if(Session->IsSensorConfigurationLocked(GetSensorId())){OutReason=TEXT("데이터 필수 실행의 준비·실행·송신 정리가 끝난 뒤 센서 규격을 편집할 수 있습니다.");return false;}
	return true;
}

bool AVirtualSensorActorBase::UpdateInteractiveTransform(const FTransform& Transform)
{
	if (!bInteractiveManipulationActive || Transform.ContainsNaN()) return false;
	SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	return true;
}

bool AVirtualSensorActorBase::ApplyEditableTransform(const FTransform& Transform, FString& OutError)
{
	OutError.Reset();
	if (Transform.ContainsNaN())
	{
		OutError = TEXT("Transform 값이 유효하지 않습니다.");
		return false;
	}
	if (bInteractiveManipulationActive) return UpdateInteractiveTransform(Transform);
	return SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
}

void AVirtualSensorActorBase::EndInteractiveManipulation()
{
	bInteractiveManipulationActive = false;
}

void AVirtualSensorActorBase::SetSharedOutputServices(
	UVirtualSensorTransportComponent* Transport,
	UVirtualSensorRecorderComponent* Recorder,
	UVirtualSensorStreamPublisherComponent* StreamPublisher)
{
	if (OutputComponent)
	{
		OutputComponent->SetSharedServices(Transport, Recorder, StreamPublisher);
	}
}
