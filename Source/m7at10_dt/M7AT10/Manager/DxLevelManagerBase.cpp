// Fill out your copyright notice in the Description page of Project Settings.


#include "DxLevelManagerBase.h"

#include "m7at10_dt/M7AT10/Core/DxGameStateBase.h"
#include "m7at10_dt/M7AT10/Core/DxWidgetSubsystem.h"


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

	// DxGameStateBase에 전달
	if (ADxGameStateBase* GameState = GetWorld()->GetGameState<ADxGameStateBase>())
	{
		GameState->CurrentViewMode = ViewMode;
	}

	// DxWidgetSubsystem에 UI 생성 요청
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDxWidgetSubsystem* WidgetSubsystem = GameInstance->GetSubsystem<UDxWidgetSubsystem>())
		{
			WidgetSubsystem->SwitchUIMode(ViewMode);
		}
	}
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

