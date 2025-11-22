// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxApiSubsystem.generated.h"

class FHttpModule;

// 응답을 받아 처리할 델리게이트 선언 (성공 여부, 응답 코드, 본문 내용)
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FDxApiCallback, bool, bSuccess, int32, ResponseCode, const FString&, Content);

/**
 *
 */
UCLASS()
class M7AT10_DT_API UDxApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// Function
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	UFUNCTION(BlueprintCallable, Category = "DxApi")
	void DxHttpCall(const FString& FullUrl, const FString& Verb, const FString& ContentString, const TMap<FString, FString>& Headers, FDxApiCallback Callback);
	UFUNCTION(BlueprintCallable, Category = "DxApi")
	void DxRequestApi(const FString& Server, const FString& RequestApiType, const FString& MethodType);
	UFUNCTION(BlueprintCallable, Category = "DxApi")
	void DxRequestApiWithParameter(const FString& Server, const FString& RequestApiType, const FString& MethodType, const TArray<FString>& Parameters);
private:
	// 내부적으로 요청 완료 시 처리할 핸들러 (람다 바인딩용)
	void InternalOnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FDxApiCallback Callback);

protected:

	// Variable
public:
	FHttpModule* HttpModule;
private:
protected:
};
