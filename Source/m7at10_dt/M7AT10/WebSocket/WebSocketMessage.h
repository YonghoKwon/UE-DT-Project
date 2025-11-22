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

	// 데이터 복사 초기화 함수 추가
	void InitializeFromStompMessage(const IStompMessage& Message);
private:
protected:

	// Variable
public:
	UPROPERTY(BlueprintReadOnly, Category = "STOMP")
	FString BodyString;
	UPROPERTY(BlueprintReadOnly, Category = "STOMP")
	TArray<uint8> RawBody;
	UPROPERTY(BlueprintReadOnly, Category = "STOMP")
	TMap<FName, FString> Headers;
	UPROPERTY(BlueprintReadOnly, Category = "STOMP")
	FString SubscriptionId;
	UPROPERTY(BlueprintReadOnly, Category = "STOMP")
	FString Destination;
	UPROPERTY(BlueprintReadOnly, Category = "STOMP")
	FString MessageId;
	UPROPERTY(BlueprintReadOnly, Category = "STOMP")
	FString AckId;
private:
protected:
};
