// Fill out your copyright notice in the Description page of Project Settings.


#include "DxApiSubsystem.h"

void UDxApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxApiSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxApiSubsystem::Tick(float DeltaTime)
{
}

TStatId UDxApiSubsystem::GetStatId() const
{
	return TStatId();
}
