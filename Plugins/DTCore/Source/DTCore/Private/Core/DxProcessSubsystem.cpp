#include "Core/DxProcessSubsystem.h"

#include "DTCore.h"


void UDxProcessSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxProcessSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxProcessSubsystem::Tick(float DeltaTime)
{
}

TStatId UDxProcessSubsystem::GetStatId() const
{
	return TStatId();
}

void UDxProcessSubsystem::RegisterComponent(const FString& ComponentId, UActorComponent* InComponent)
{
	if (ComponentId.IsEmpty() || !IsValid(InComponent))
	{
		DX_LOG(GetWorld(), TEXT("[DxProcessSubsystem] Invalid ComponentId or InComponent for registeration"));
		return;
	}

	if (RegisteredComponents.Contains(ComponentId))
	{
		DX_LOG(GetWorld(), TEXT("[DxProcessSubsystem] ComponentId '%s' already registered. Overwriting."), *ComponentId);
	}

	RegisteredComponents.Add(ComponentId, InComponent);
	DX_LOG(GetWorld(), TEXT("[DxProcessSubsystem] Registered Component for ComponentId: %s"), *ComponentId);
}

void UDxProcessSubsystem::UnregisterComponent(const FString& ComponentId)
{
	if (RegisteredComponents.Remove(ComponentId) > 0)
	{
		DX_LOG(GetWorld(), TEXT("[DxProcessSubsystem] Unregistered Component for ComponentId: %s"), *ComponentId);
	}
}

UActorComponent* UDxProcessSubsystem::FindComponent(const FString& ComponentId)
{
	if (const TWeakObjectPtr<UActorComponent>* FoundComponent = RegisteredComponents.Find(ComponentId))
	{
		if (FoundComponent->IsValid())
		{
			return FoundComponent->Get();
		}
		else
		{
			RegisteredComponents.Remove(ComponentId);
		}
	}
	return nullptr;
}
