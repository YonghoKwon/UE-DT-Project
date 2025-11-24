// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "m7at10_dt/M7AT10/Lib/yyjson.h"

#include "TransactionCodeMessage.generated.h"

class FYyJsonParser;
/**
 *
 */
UCLASS()
class M7AT10_DT_API UTransactionCodeMessage : public UObject
{
	GENERATED_BODY()

	// Function
public:
	virtual void ProcessData(const TSharedPtr<FJsonObject>& DataMap);

	/** yyjson을 사용하는 고성능 처리 함수 오버로드 */
	virtual void ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode);
private:
protected:

	// Variable
public:
	UPROPERTY(BlueprintReadOnly, Category = "TransactionCode")
	FString TransactionCode;
private:
protected:
};
