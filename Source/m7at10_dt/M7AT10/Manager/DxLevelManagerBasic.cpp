// Fill out your copyright notice in the Description page of Project Settings.


#include "DxLevelManagerBasic.h"

#include "m7at10_dt/M7AT10/Core/DxGameStateBase.h"


// Sets default values
ADxLevelManagerBasic::ADxLevelManagerBasic()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ViewMode = EDxViewMode::Basic;
}

// Called when the game starts or when spawned
void ADxLevelManagerBasic::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADxLevelManagerBasic::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

