
#include "DxLevelManagerTest.h"

#include "DxApiServiceTest.h"
#include "ma0t10_dt/ma0t10_dt.h"
#include "Core/DxGameStateBase.h"


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

	UE_LOG(LogMA0T10, Log, TEXT("[LevelManagerTest] Start Loading..."));
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
		UE_LOG(LogMA0T10, Log, TEXT("[LevelManagerTest] All Data Loaded! Spawning Actors"));
	}
	else
	{
		UE_LOG(LogMA0T10, Error, TEXT("[LevelManagerTest] Error: %s"), *Error);
	}
}