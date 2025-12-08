// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "m7at10_dt/M7AT10/Lib/yyjson.h"
#include "UObject/Object.h"
#include "ApiMessage.generated.h"

class FYyJsonParser;
/**
 *
 */
UCLASS()
class M7AT10_DT_API UApiMessage : public UObject
{
	GENERATED_BODY()

	// Function
public:
	virtual TSharedPtr<struct FApiDataBase> ParseToStruct(const FString& JsonString)
	{
		return nullptr;
	}
	virtual void ProcessStructData(const TSharedPtr<FApiDataBase>& Data) {}
	virtual UWorld* GetWorld() const override;
private:
protected:

	// Variable
public:
	UPROPERTY(BlueprintReadOnly, Category = "Api")
	FString ResourceAndAction;
private:
protected:
};
