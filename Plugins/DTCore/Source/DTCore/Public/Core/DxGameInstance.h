// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DxGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class DTCORE_API UDxGameInstance : public UGameInstance
{
	GENERATED_BODY()

	// Function
public:
	virtual void Init() override;
	virtual void Shutdown() override;

	static UDxGameInstance* GetInstance();
private:
protected:

	// Variable
public:
private:
	static UDxGameInstance* Instance;
protected:
};
