// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DxPlayerBase.h"
#include "DxPlayerTest.generated.h"

UCLASS()
class M7AT10_DT_API ADxPlayerTest : public ADxPlayerBase
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ADxPlayerTest();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
