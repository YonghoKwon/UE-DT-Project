// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DxLevelManagerBase.generated.h"

enum class EDxViewMode : uint8;

UCLASS()
class M7AT10_DT_API ADxLevelManagerBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADxLevelManagerBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupForLevel();
	virtual void CleanupForLevel();
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Function
public:
private:
protected:

	// Variable
public:
private:
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	EDxViewMode ViewMode;
};
