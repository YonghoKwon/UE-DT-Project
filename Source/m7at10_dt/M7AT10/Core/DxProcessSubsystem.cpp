// Fill out your copyright notice in the Description page of Project Settings.


#include "DxProcessSubsystem.h"

#include "m7at10_dt/m7at10_dt.h"

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
		UE_LOG(LogM7AT10, Warning, TEXT("[DxProcessSubsystem] Invalid ComponentId or InComponent for registeration"));
		return;
	}

	if (RegisteredComponents.Contains(ComponentId))
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[DxProcessSubsystem] ComponentId '%s' already registered. Overwriting."), *ComponentId);
	}

	RegisteredComponents.Add(ComponentId, InComponent);
	UE_LOG(LogM7AT10, Log, TEXT("[DxProcessSubsystem] Registered Component for ComponentId: %s"), *ComponentId);
}

void UDxProcessSubsystem::UnregisterComponent(const FString& ComponentId)
{
	if (RegisteredComponents.Remove(ComponentId) > 0)
	{
		UE_LOG(LogM7AT10, Log, TEXT("[DxProcessSubsystem] Unregistered Component for ComponentId: %s"), *ComponentId);
	}
}

UActorComponent* UDxProcessSubsystem::FindComponent(const FString& ComponentId)
{
	if (const TObjectPtr<UActorComponent>* FoundComponent = RegisteredComponents.Find(ComponentId))
	{
		return  *FoundComponent;
	}
	return nullptr;
}
