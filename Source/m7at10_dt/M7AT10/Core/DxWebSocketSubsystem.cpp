// Fill out your copyright notice in the Description page of Project Settings.


#include "DxWebSocketSubsystem.h"

void UDxWebSocketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxWebSocketSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxWebSocketSubsystem::Tick(float DeltaTime)
{
}

TStatId UDxWebSocketSubsystem::GetStatId() const
{
	return TStatId();
}
