// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DxLevelManagerBase.h"
#include "DxLevelManagerTest.generated.h"

class UDxApiServiceTest;

UCLASS()
class M7AT10_DT_API ADxLevelManagerTest : public ADxLevelManagerBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADxLevelManagerTest();

	virtual void BeginPlay() override;

protected:
	// Called when the game starts or when spawned
	virtual void SetupForLevel() override;
	virtual void CleanupForLevel() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Function
public:
	UFUNCTION()
	void OnLevelInitApiFinished(bool bSuccess, const FString& Error);
private:
protected:

	// Variable
public:
	UPROPERTY()
	TObjectPtr<UDxApiServiceTest> DxApiService;
private:
protected:
};
