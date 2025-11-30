// Fill out your copyright notice in the Description page of Project Settings.

#include "m7at10_dt/M7AT10/Crane/CraneDataSyncComp.h"

#include "CraneMechDriverComp.h"


// Sets default values for this component's properties
UCraneMechDriverComp::UCraneMechDriverComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCraneMechDriverComp::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (UCraneDataSyncComp* DataSyncComp = Owner->FindComponentByClass<UCraneDataSyncComp>())
		{
			DataSyncComp->OnCranePositionChanged.AddDynamic(this, &UCraneMechDriverComp::SetTargetPosition);
		}
	}
}


// Called every frame
void UCraneMechDriverComp::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 트롤리 X축 이동 처리
	UpdateTrolleyMovement(DeltaTime);
}

void UCraneMechDriverComp::SetTargetPosition(const FCranePositionData& InPositionData)
{
	// 기존 목표 위치에 새로운 값을 누적
	TargetPosition.TrolleyPosition += InPositionData.TrolleyPosition * MovementScale;
	TargetPosition.HoistHeight += InPositionData.HoistHeight * MovementScale;
	TargetPosition.GantryPosition += InPositionData.GantryPosition * MovementScale;

	UE_LOG(LogTemp, Log, TEXT("[CraneMechDriverComp] Target - Trolley: %.2f, Hoist: %.2f, Gantry: %.2f"),
		   TargetPosition.TrolleyPosition, TargetPosition.HoistHeight, TargetPosition.GantryPosition);
}

void UCraneMechDriverComp::UpdateTrolleyMovement(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 현재 위치와 목표 위치의 차이 계산
	const float TargetX = TargetPosition.TrolleyPosition;
	const float CurrentX = CurrentPosition.TrolleyPosition;

	// 목표에 거의 도달했으면 스킵
	if (FMath::IsNearlyEqual(CurrentX, TargetX, 0.1f))
	{
		return;
	}

	// 부드러운 보간으로 현재 위치 업데이트
	const float NewX = FMath::FInterpTo(CurrentX, TargetX, DeltaTime, InterpSpeed);
	CurrentPosition.TrolleyPosition = NewX;

	// 이동량 계산
	const float DeltaX = NewX - CurrentX;

	// 액터 위치 업데이트 (X축으로만 이동)
	FVector CurrentLocation = Owner->GetActorLocation();
	CurrentLocation.X += DeltaX;
	Owner->SetActorLocation(CurrentLocation);
}
