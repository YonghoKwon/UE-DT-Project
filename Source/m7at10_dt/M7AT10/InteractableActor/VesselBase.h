// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractableActor.h"
#include "VesselBase.generated.h"

UCLASS()
class M7AT10_DT_API AVesselBase : public AInteractableActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVesselBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
