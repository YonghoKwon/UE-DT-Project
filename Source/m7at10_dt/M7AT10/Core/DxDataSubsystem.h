// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxDataSubsystem.generated.h"

class UCraneDataSyncComp;
class UTransactionCodeMessage;
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

	UFUNCTION(Category = "DxData")
	UApiMessage* FindApiMessage(const FString& Resource, const FString& Action);
	UFUNCTION(Category = "DxData")
	UTransactionCodeMessage* FindTransactionCodeMessage(const FString& TransactionCode);

	UFUNCTION(Category = "Domain")
	void RegisterCraneDataSyncComp(const FString& CraneId, UCraneDataSyncComp* Comp);
	UFUNCTION(Category = "Domain")
	void UnregisterCraneDataSyncComp(const FString& CraneId);
	UFUNCTION(Category = "Domain")
	UCraneDataSyncComp* FindCraneDataSyncComp(const FString& CraneId);
private:
	void ProcessApiQueue();
	void ProcessWebSocketQueue();
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

	// TODO: 관련 설비가 많아지면 별도 Subsystem에서 관리 할 수 있도록 변경 예정
	UPROPERTY()
	TMap<FString, TObjectPtr<UCraneDataSyncComp>> CraneDataSyncCompMap;
protected:
};
