// Fill out your copyright notice in the Description page of Project Settings.


#include "DxPlayerBase.h"


// Sets default values
ADxPlayerBase::ADxPlayerBase()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ADxPlayerBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADxPlayerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ADxPlayerBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

