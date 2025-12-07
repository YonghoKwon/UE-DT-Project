// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DxApiServiceTest.generated.h"


// 결과 알려주는 델리게이트 (성공 여부, 메시지)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDataReady, bool, bSuccess, const FString&, ErrorMessage);

/**
 * 
 */
UCLASS()
class M7AT10_DT_API UDxApiServiceTest : public UObject
{
	GENERATED_BODY()

	// Function
public:
	void Initialize(UGameInstance* GameInstance);

	void LevelInitApiCall();
private:
	void Step1_RequestOne();
	void Step2_RequestTwo();

	UFUNCTION()
	void OnOneResponse(bool bSuccess, int32 Code, const FString& Content);
	UFUNCTION()
	void OnTwoResponse(bool bSuccess, int32 Code, const FString& Content);
protected:

	// Variable
public:
	// 결과 알림 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnDataReady OnDataReady;
private:
	UPROPERTY()
	TWeakObjectPtr<class UDxApiSubsystem> ApiSubsystem;
protected:
};
