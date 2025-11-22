// Fill out your copyright notice in the Description page of Project Settings.


#include "DxDataSubsystem.h"

void UDxDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxDataSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxDataSubsystem::Tick(float DeltaTime)
{
}

TStatId UDxDataSubsystem::GetStatId() const
{
	return TStatId();
}
