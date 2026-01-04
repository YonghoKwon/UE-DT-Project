
#include "DxLevelManagerBasic.h"

#include "Core/DxGameStateBase.h"


ADxLevelManagerBasic::ADxLevelManagerBasic()
{
	PrimaryActorTick.bCanEverTick = true;

	ViewMode = EDxViewMode::Basic;
}

void ADxLevelManagerBasic::BeginPlay()
{
	Super::BeginPlay();
	
}

void ADxLevelManagerBasic::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

