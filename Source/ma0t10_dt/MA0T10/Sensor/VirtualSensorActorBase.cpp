#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

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

void AVirtualSensorActorBase::Click()
{
	Super::Click(); // DTCore activation policy/input priority remains the authority.
	OnSensorActivated.Broadcast(this);
}
void AVirtualSensorActorBase::SetPlacementPickingEnabled(bool Enabled)
{
	if(!Enabled){if(PlacementPickMarker){PlacementPickMarker->DestroyComponent();PlacementPickMarker=nullptr;}return;}
	if(PlacementPickMarker||!GetRootComponent()||IsActorBeingDestroyed())return;
	PlacementPickMarker=NewObject<UStaticMeshComponent>(this,TEXT("SensorPlacementPickMarker"),RF_Transient);
	PlacementPickMarker->SetupAttachment(GetRootComponent());
	PlacementPickMarker->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	PlacementPickMarker->SetRelativeScale3D(FVector(.3));
	PlacementPickMarker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PlacementPickMarker->SetCollisionObjectType(ECC_WorldDynamic);
	PlacementPickMarker->SetCollisionResponseToAllChannels(ECR_Ignore); // DTCore object queries still pick; channel-based sensors never hit it.
	PlacementPickMarker->SetGenerateOverlapEvents(false);PlacementPickMarker->SetCastShadow(false);
	PlacementPickMarker->SetHiddenInSceneCapture(true);PlacementPickMarker->SetAffectDynamicIndirectLighting(false);
	PlacementPickMarker->SetAffectDistanceFieldLighting(false);PlacementPickMarker->SetAffectIndirectLightingWhileHidden(false);
	PlacementPickMarker->SetVisibleInRayTracing(false);PlacementPickMarker->bVisibleInReflectionCaptures=false;PlacementPickMarker->bVisibleInRealTimeSkyCaptures=false;
	PlacementPickMarker->RegisterComponent();
}

bool AVirtualSensorActorBase::BeginInteractiveManipulation(const FVirtualSensorInteractionRequest& Request)
{
	FString Reason;if(!CanEditSensorConfiguration(Reason)||!CanEditSensorPlacement(Reason))return false;
	bInteractiveManipulationActive = true;
	return true;
}

bool AVirtualSensorActorBase::ReadCollectionState(FVirtualSensorEditableState& State,bool& Preview) const
{
    Preview=IsSensorRunning();
    return ReadEditableState(State);
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
	FString Reason;
	if (!bInteractiveManipulationActive || Transform.ContainsNaN() || !CanEditSensorPlacement(Reason)) return false;
	return SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
}

bool AVirtualSensorActorBase::ApplyEditableTransform(const FTransform& Transform, FString& OutError)
{
	OutError.Reset();
	if (!CanEditSensorPlacement(OutError)) return false;
	if (Transform.ContainsNaN())
	{
		OutError = TEXT("Transform 값이 유효하지 않습니다.");
		return false;
	}
	if (bInteractiveManipulationActive) return UpdateInteractiveTransform(Transform);
	return SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
}

bool AVirtualSensorActorBase::CanEditSensorPlacement(FString& Reason) const
{
	Reason.Reset();
	if (IsActorBeingDestroyed()) { Reason=TEXT("센서가 종료 중입니다."); return false; }
	const FString Id=GetSensorId();
	if (GetWorld())
	{
		if (const auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();Session&&Session->IsSensorPlacementLocked(Id))
		{ Reason=TEXT("시나리오 수집·송신 정리가 끝난 뒤 위치를 변경할 수 있습니다."); return false; }
		if (const auto* Files=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>();Files&&Files->HasPendingSave(const_cast<AVirtualSensorActorBase*>(this)))
		{ Reason=TEXT("파일 저장을 마무리한 뒤 위치를 변경할 수 있습니다."); return false; }
		if (const auto* Periodic=GetWorld()->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();Periodic&&Periodic->HasSensorWork(this))
		{ Reason=TEXT("주기 저장을 중지하고 마지막 파일이 저장된 뒤 위치를 변경할 수 있습니다."); return false; }
		if (const auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();Raw&&Raw->GetOutstandingSensorFrameCount(Id)>0)
		{ Reason=TEXT("미확인 송신 프레임을 마무리한 뒤 위치를 변경할 수 있습니다."); return false; }
	}
	if (OutputComponent)
	{
		if (OutputComponent->StreamPublisherComponent&&OutputComponent->StreamPublisherComponent->HasSensorPlacementWork(Id,GetSensorKind()))
		{ Reason=TEXT("실시간 송신을 중지하고 정리가 끝난 뒤 위치를 변경할 수 있습니다."); return false; }
		if (OutputComponent->RecorderComponent&&OutputComponent->RecorderComponent->IsRecording())
		{ Reason=TEXT("센서 녹화를 종료한 뒤 위치를 변경할 수 있습니다."); return false; }
	}
	return true;
}

bool AVirtualSensorActorBase::PrepareForDataCollection(FString& Reason)
{
	Reason.Reset();
	if (IsActorBeingDestroyed()) { Reason=TEXT("센서가 종료 중입니다."); return false; }
	if (bInteractiveManipulationActive)
	{
		OnDataCollectionStarting.Broadcast();
		if(bInteractiveManipulationActive)EndInteractiveManipulation();
	}
	if(bInteractiveManipulationActive){Reason=TEXT("센서 조작·원래 규격 복원이 끝나지 않았습니다.");return false;}
	return true;
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
