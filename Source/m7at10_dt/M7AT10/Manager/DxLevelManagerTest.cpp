
#include "DxLevelManagerTest.h"

#include "DxApiServiceTest.h"
#include "m7at10_dt/m7at10_dt.h"
#include "m7at10_dt/M7AT10/Core/DxGameStateBase.h"


ADxLevelManagerTest::ADxLevelManagerTest()
{
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

