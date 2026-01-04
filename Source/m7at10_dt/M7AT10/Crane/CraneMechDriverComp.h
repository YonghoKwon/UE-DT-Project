// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CraneDataTypes.h"
#include "ActorComponent/MechDriverCompBase.h"
#include "CraneMechDriverComp.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class M7AT10_DT_API UCraneMechDriverComp : public UMechDriverCompBase
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCraneMechDriverComp();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// Function
public:
	UFUNCTION()
	void SetTargetPosition(const FCranePositionData& InPositionData);
private:
	UFUNCTION()
	void UpdateTrolleyMovement(float DeltaTime);
protected:

	// Variable
public:
private:
	// 목표 위치 (보간 등에 사용)
	UPROPERTY()
	FCranePositionData TargetPosition;
	// 현재 위치 (보간 등에 사용)
	UPROPERTY()
	FCranePositionData CurrentPosition;

	// 보간 속도 (값이 클수록 빠르게 목표에 도달)
	UPROPERTY(EditAnywhere, Category = "Movement")
	float InterpSpeed = 5.0f;

	// 이동 스케일 (입력값에 곱해짐)
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MovementScale = 100.0f;
protected:
};
