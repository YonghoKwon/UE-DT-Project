// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "m7at10_dt/M7AT10/WebSocket/FTransactionCodeDataBase.h"
#include "m7at10_dt/M7AT10/WebSocket/TransactionCodeMessage.h"
#include "KP1D0012.generated.h"

struct FKP1D0012Data : FTransactionCodeDataBase
{
	// 전문 데이터 형식 정의
	FString TransactionCode;

	FString CreationTimestamp;
};

/**
 *
 */
UCLASS()
class M7AT10_DT_API UKP1D0012 : public UTransactionCodeMessage, public FTransactionCodeDataBase
{
	GENERATED_BODY()

	// Function
public:
	UKP1D0012();

	virtual TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString& JsonString) override;

	virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) override;
	// virtual void ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode) override;
private:
protected:

	// Variable
public:
private:
protected:
};
