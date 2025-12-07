// Fill out your copyright notice in the Description page of Project Settings.


#include "DxLevelManagerTest.h"

#include "DxApiServiceTest.h"
#include "m7at10_dt/m7at10_dt.h"
#include "m7at10_dt/M7AT10/Core/DxGameStateBase.h"


// Sets default values
ADxLevelManagerTest::ADxLevelManagerTest()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ViewMode = EDxViewMode::Test;
}

void ADxLevelManagerTest::BeginPlay()
{
	Super::BeginPlay();

	DxApiService = NewObject<UDxApiServiceTest>(this);
	DxApiService->Initialize(GetGameInstance());

	DxApiService->OnDataReady.AddDynamic(this, &ADxLevelManagerTest::OnLevelInitApiFinished);

	UE_LOG(LogM7AT10, Log, TEXT("[LevelManagerTest] Start Loading..."));
	DxApiService->LevelInitApiCall();
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

void ADxLevelManagerTest::OnLevelInitApiFinished(bool bSuccess, const FString& Error)
{
	if (bSuccess)
	{
		UE_LOG(LogM7AT10, Log, TEXT("[LevelManagerTest] All Data Loaded! Spawning Actors"));
	}
	else
	{
		UE_LOG(LogM7AT10, Error, TEXT("[LevelManagerTest] Error: %s"), *Error);
	}
}

