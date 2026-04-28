// Fill out your copyright notice in the Description page of Project Settings.

#include "VirtualCameraAct.h"

#include "DrawFrustumComponent.h"
#include "VirtualCameraComp.h"

AVirtualCameraAct::AVirtualCameraAct()
{
	PrimaryActorTick.bCanEverTick = false;

	VirtualCameraComp = CreateDefaultSubobject<UVirtualCameraComp>(TEXT("VirtualCameraComp"));
	RootComponent = VirtualCameraComp;

	EditorFrustumComp = CreateDefaultSubobject<UDrawFrustumComponent>(TEXT("EditorFrustumComp"));
	EditorFrustumComp->SetupAttachment(RootComponent);
	EditorFrustumComp->FrustumAngle = 87.0f;
	EditorFrustumComp->FrustumStartDist = 20.0f;
	EditorFrustumComp->FrustumEndDist = 600.0f;
	EditorFrustumComp->FrustumAspectRatio = 16.0f / 9.0f;
}

void AVirtualCameraAct::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (EditorFrustumComp && VirtualCameraComp)
	{
		EditorFrustumComp->FrustumAngle = VirtualCameraComp->FOVAngle;
		EditorFrustumComp->FrustumEndDist = VirtualCameraComp->GetDeviceSpec().TypicalRangeCm > 0.0f
			? VirtualCameraComp->GetDeviceSpec().TypicalRangeCm
			: 600.0f;
		EditorFrustumComp->FrustumAspectRatio = VirtualCameraComp->CaptureResolution.Y > 0
			? static_cast<float>(VirtualCameraComp->CaptureResolution.X) / static_cast<float>(VirtualCameraComp->CaptureResolution.Y)
			: 16.0f / 9.0f;
	}
}

void AVirtualCameraAct::BeginPlay()
{
	Super::BeginPlay();
}

void AVirtualCameraAct::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
