// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/StatusVisualizerCompBase.h"
#include "CraneStatusVisualizerComp.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class M7AT10_DT_API UCraneStatusVisualizerComp : public UStatusVisualizerCompBase
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCraneStatusVisualizerComp();

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
	void OnColorChanged(const FLinearColor& NewColor);
private:
protected:

	// Variable
public:
private:
protected:
};
