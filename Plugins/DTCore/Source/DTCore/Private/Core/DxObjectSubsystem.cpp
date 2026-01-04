#include "Core/DxObjectSubsystem.h"

void UDxObjectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxObjectSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxObjectSubsystem::Tick(float DeltaTime)
{
}

TStatId UDxObjectSubsystem::GetStatId() const
{
	return TStatId();
}
