// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualCameraAct.generated.h"

class UDrawFrustumComponent;
class UVirtualCameraComp;

UCLASS()
class M7AT10_DT_API AVirtualCameraAct : public AActor
{
	GENERATED_BODY()

public:
	AVirtualCameraAct();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UVirtualCameraComp> VirtualCameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|EditorVisualization")
	TObjectPtr<UDrawFrustumComponent> EditorFrustumComp;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	virtual void Tick(float DeltaTime) override;
};
