// Fill out your copyright notice in the Description page of Project Settings.


#include "DxLevelManagerTest.h"


// Sets default values
ADxLevelManagerTest::ADxLevelManagerTest()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void ADxLevelManagerTest::SetupForLevel()
{
	Super::SetupForLevel();
}

void ADxLevelManagerTest::CleanupForLevel()
{
	Super::CleanupForLevel();
}

// Called every frame
void ADxLevelManagerTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

