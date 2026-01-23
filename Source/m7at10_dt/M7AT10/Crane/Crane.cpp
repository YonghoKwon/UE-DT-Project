// Fill out your copyright notice in the Description page of Project Settings.


#include "Crane.h"

#include "CraneDataSyncComp.h"
#include "CraneMechDriverComp.h"
#include "CraneStatusVisualizerComp.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Core/DxProcessSubsystem.h"


// Sets default values
ACrane::ACrane()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MainMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("MainMesh"));
	RootComponent = MainMesh;

	DataSyncComp = CreateDefaultSubobject<UCraneDataSyncComp>(TEXT("DataSyncComp"));
	MechDriverComp = CreateDefaultSubobject<UCraneMechDriverComp>(TEXT("MechDriverComp"));
	StatusVisualizerComp = CreateDefaultSubobject<UCraneStatusVisualizerComp>(TEXT("StatusVisualizerComp"));

	WidgetFlag = TEXT("Freeview");
	HighlightMode = EHighlightMode::IndividualMesh;

	// // 1. 메쉬 컴포넌트 생성 (기존 SkeletalMesh 대신 PoseableMesh 사용 권장)
	// CraneMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("CraneMesh"));
	// RootComponent = CraneMesh;

	// 2. 드라이버 컴포넌트 부착
	// MechDriverComp = CreateDefaultSubobject<UCraneMechDriverComp>(TEXT("MechDriverComp"));
}

// Called when the game starts or when spawned
void ACrane::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
	{
		if (UDxProcessSubsystem* DxProcessSubsystem = GI->GetSubsystem<UDxProcessSubsystem>())
		{
			DxProcessSubsystem->RegisterComponent(CraneId, DataSyncComp);
		}
	}
}

void ACrane::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
	{
		if (UDxProcessSubsystem* DxProcessSubsystem = GI->GetSubsystem<UDxProcessSubsystem>())
		{
			DxProcessSubsystem->UnregisterComponent(CraneId);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ACrane::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
