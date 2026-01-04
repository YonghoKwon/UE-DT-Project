// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxDataSubsystem.generated.h"

class UCraneDataSyncComp;
class UTransactionCodeMessage;
class UApiMessage;
/**
 * 
 */
UCLASS()
class DTCORE_API UDxDataSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// Function
public:
	UDxDataSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	UFUNCTION(Category = "DxData")
	void EnqueueApiData(const FString& DataFromApi);
	UFUNCTION(Category = "DxData")
	void EnqueueWebSocketData(const FString& Data);

private:
	void ProcessApiQueue();
	void ProcessWebSocketQueue();

	// API 핸들러 조회 함수
	UApiMessage* GetOrLoadApiHandler(UClass* HandlerClass);
	UTransactionCodeMessage* GetOrLoadTransactionHandler(UClass* HandlerClass);
protected:

	// Variable
public:
private:
	UPROPERTY()
	TObjectPtr<UDataTable> ApiDataTable;
	UPROPERTY()
	TObjectPtr<UDataTable> WebSocketDataTable;

	TQueue<FString> ApiDataQueue;
	TQueue<FString> WebSocketDataQueue;

	UPROPERTY()
	TMap<FString, TObjectPtr<UApiMessage>> ApiMessageMap;
	UPROPERTY()
	TMap<FString, TObjectPtr<UTransactionCodeMessage>> TransactionCodeMessageMap;
	UPROPERTY()
	TMap<UClass*, UApiMessage*> ApiHandlerInstanceCache;
	UPROPERTY()
	TMap<UClass*, UTransactionCodeMessage*> WebSocketHandlerInstanceCache;


	// 디버그용
	// int32 TotalProcessedCount = 0; // 처리된 총 개수
	// 디버그용 전체 처리된 데이터 개수를 세는 카운터
	FThreadSafeCounter TotalProcessedCount;
protected:
};
