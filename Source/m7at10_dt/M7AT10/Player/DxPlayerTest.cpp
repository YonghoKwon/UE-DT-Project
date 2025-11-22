// Fill out your copyright notice in the Description page of Project Settings.


#include "DxPlayerTest.h"


// Sets default values
ADxPlayerTest::ADxPlayerTest()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ADxPlayerTest::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADxPlayerTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ADxPlayerTest::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

