
#include "DxLevelManagerBasic.h"

#include "m7at10_dt/M7AT10/Core/DxGameStateBase.h"


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

