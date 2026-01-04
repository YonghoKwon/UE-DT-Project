#include "Manager/DxLevelManagerBase.h"

#include "Core/DxGameStateBase.h"
#include "Core/DxWidgetSubsystem.h"

ADxLevelManagerBase::ADxLevelManagerBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

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

