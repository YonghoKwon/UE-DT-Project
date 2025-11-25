// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "m7at10_dt/M7AT10/WebSocket/TransactionCodeMessage.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxDataSubsystem.generated.h"

class UApiMessage;
/**
 * 
 */
UCLASS()
class M7AT10_DT_API UDxDataSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// Function
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	UFUNCTION(Category = "DxData")
	void EnqueueApiData(const FString& DataFromApi);
	UFUNCTION(Category = "DxData")
	void EnqueueWebSocketData(const FString& Data);

	UFUNCTION(Category = "DxData")
	UApiMessage* FindApiMessage(const FString& Resource, const FString& Action);
	UFUNCTION(Category = "DxData")
	UTransactionCodeMessage* FindTransactionCodeMessage(const FString& TransactionCode);
private:
	void ProcessApiQueue();
	void ProcessWebSocketQueue();
protected:

	// Variable
public:
private:
	TQueue<FString> ApiDataQueue;
	TQueue<FString> WebSocketDataQueue;

	UPROPERTY()
	TMap<FString, TObjectPtr<UApiMessage>> ApiMessageMap;
	UPROPERTY()
	TMap<FString, TObjectPtr<UTransactionCodeMessage>> TransactionCodeMessageMap;
protected:
};
