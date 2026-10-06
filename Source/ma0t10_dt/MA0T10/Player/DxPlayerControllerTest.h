// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/DxPlayerControllerBase.h"
#include "DxPlayerControllerTest.generated.h"

/**
 * 
 */
UCLASS()
class MA0T10_DT_API ADxPlayerControllerTest : public ADxPlayerControllerBase
{
	GENERATED_BODY()
public:
	ADxPlayerControllerTest();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void ApplyProjectViewportPolicy();
	FDelegateHandle WorldBeginPlayHandle;
};
