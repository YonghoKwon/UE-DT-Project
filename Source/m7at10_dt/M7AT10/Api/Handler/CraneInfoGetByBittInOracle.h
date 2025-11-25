// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "m7at10_dt/M7AT10/Api/ApiMessage.h"
#include "CraneInfoGetByBittInOracle.generated.h"

class FYyJsonParser;
/**
 * 
 */
UCLASS()
class M7AT10_DT_API UCraneInfoGetByBittInOracle : public UApiMessage
{
	GENERATED_BODY()

	// Function
public:
	UCraneInfoGetByBittInOracle();

	virtual void ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode) override;
private:
protected:

	// Variable
public:
private:
protected:
};
