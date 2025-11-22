// Fill out your copyright notice in the Description page of Project Settings.


#include "DxProcessSubsystem.h"

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
