// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DxWidget.generated.h"

class ADxPlayerTest;
/**
 * 
 */
UCLASS()
class M7AT10_DT_API UDxWidget : public UUserWidget
{
	GENERATED_BODY()

	// Function
public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
private:
protected:

	// Variable
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<ADxPlayerTest> Player;
private:
protected:
};
