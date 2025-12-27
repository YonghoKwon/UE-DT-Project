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
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Widget")
	void OpenWidgetAddLogic();

	UFUNCTION(BlueprintCallable, Category = "Widget")
	void CloseWidget();
private:
protected:
	// Player) 연속 동작 시작/중지 (자식 클래스에서 사용)
	void StartContinuousTimer();
	void StopContinuousTimer();

	// Player) 동작 업데이트 함수
	virtual void ContinuousUpdate();

	// Variable
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<ADxPlayerTest> Player;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "DxWidget")
	TObjectPtr<class ADxPlayerControllerBase> PlayerController;

private:
protected:
	UPROPERTY()
	FTimerHandle ContinuousTimerHandle;
};
