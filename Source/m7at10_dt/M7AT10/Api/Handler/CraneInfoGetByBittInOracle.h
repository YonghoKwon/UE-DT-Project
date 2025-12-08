// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "m7at10_dt/M7AT10/Api/ApiMessage.h"
#include "m7at10_dt/M7AT10/Api/FApiDataBase.h"
#include "CraneInfoGetByBittInOracle.generated.h"

class FYyJsonParser;

struct FCraneInfoGetByBittInOracleData : FApiDataBase
{
	// 전문 데이터 형식 정의
	FString ApiName;
};

/**
 * 
 */
UCLASS()
class M7AT10_DT_API UCraneInfoGetByBittInOracle : public UApiMessage, public FApiDataBase
{
	GENERATED_BODY()

	// Function
public:
	UCraneInfoGetByBittInOracle();

	virtual TSharedPtr<FApiDataBase> ParseToStruct(const FString& JsonString) override;

	virtual void ProcessStructData(const TSharedPtr<FApiDataBase>& Data) override;
	// virtual void ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode) override;
private:
protected:

	// Variable
public:
private:
protected:
};
