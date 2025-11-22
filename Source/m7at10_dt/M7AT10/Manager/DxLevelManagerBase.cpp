// Fill out your copyright notice in the Description page of Project Settings.


#include "DxLevelManagerBase.h"


// Sets default values
ADxLevelManagerBase::ADxLevelManagerBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ADxLevelManagerBase::BeginPlay()
{
	Super::BeginPlay();

}

void ADxLevelManagerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ADxLevelManagerBase::SetupForLevel()
{
}

void ADxLevelManagerBase::CleanupForLevel()
{
}

// Called every frame
void ADxLevelManagerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

