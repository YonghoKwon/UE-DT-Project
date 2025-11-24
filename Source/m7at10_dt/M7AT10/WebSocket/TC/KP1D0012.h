// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "m7at10_dt/M7AT10/WebSocket/TransactionCodeMessage.h"
#include "KP1D0012.generated.h"

/**
 *
 */
UCLASS()
class M7AT10_DT_API UKP1D0012 : public UTransactionCodeMessage
{
	GENERATED_BODY()

	// Function
public:
	UKP1D0012();

	virtual void ProcessData(const TSharedPtr<FJsonObject>& DataMap) override;

	// yyjson 데이터 처리 오버라이드
	virtual void ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode) override;
private:
protected:

	// Variable
public:
private:
protected:
};
