// Fill out your copyright notice in the Description page of Project Settings.


#include "Crane.h"

#include "CraneDataSyncComp.h"
#include "CraneMechDriverComp.h"
#include "CraneStatusVisualizerComp.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "m7at10_dt/M7AT10/Core/DxDataSubsystem.h"


// Sets default values
ACrane::ACrane()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MainMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainMesh"));
	RootComponent = MainMesh;

	DataSyncComp = CreateDefaultSubobject<UCraneDataSyncComp>(TEXT("DataSyncComp"));
	MechDriverComp = CreateDefaultSubobject<UCraneMechDriverComp>(TEXT("MechDriverComp"));
	StatusVisualizerComp = CreateDefaultSubobject<UCraneStatusVisualizerComp>(TEXT("StatusVisualizerComp"));
}

// Called when the game starts or when spawned
void ACrane::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
	{
		if (UDxDataSubsystem* DxDataSub = GI->GetSubsystem<UDxDataSubsystem>())
		{
			DxDataSub->RegisterCraneDataSyncComp(CraneId, DataSyncComp);
		}
	}
}

void ACrane::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
	{
		if (UDxDataSubsystem* DxDataSub = GI->GetSubsystem<UDxDataSubsystem>())
		{
			DxDataSub->UnregisterCraneDataSyncComp(CraneId);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ACrane::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
