// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxDataSubsystem.generated.h"

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
private:
	void ProcessApiQueue();
	void ProcessWebSocketQueue();
protected:

	// Variable
public:
private:
	TQueue<FString> ApiDataQueue;
	TQueue<FString> WebSocketDataQueue;
protected:
};
