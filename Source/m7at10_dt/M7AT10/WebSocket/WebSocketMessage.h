// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WebSocketMessage.generated.h"

class IStompMessage;
/**
 * 
 */
UCLASS()
class M7AT10_DT_API UWebSocketMessage : public UObject
{
	GENERATED_BODY()

	// Function
public:
	virtual ~UWebSocketMessage();

	UFUNCTION(BlueprintCallable, Category="STOMP")
	const TMap<FName, FString>& GetHeader() const;
	UFUNCTION(BlueprintCallable, Category="STOMP")
	FString GetBodyAsString() const;
	UFUNCTION(BlueprintCallable, Category="STOMP")
	const TArray<uint8> GetRawBody() const;
	UFUNCTION(BlueprintCallable, Category="STOMP")
	int32 GetRawBodyLength() const;

	UFUNCTION(BlueprintCallable, Category="STOMP")
	FString GetSubscriptionId() const;
	UFUNCTION(BlueprintCallable, Category="STOMP")
	FString GetDestination() const;
	UFUNCTION(BlueprintCallable, Category="STOMP")
	FString GetMessageId() const;
	UFUNCTION(BlueprintCallable, Category="STOMP")
	FString GetAckId() const;
private:
protected:

	// Variable
public:
	const IStompMessage* MyMessage;
private:
protected:
};
