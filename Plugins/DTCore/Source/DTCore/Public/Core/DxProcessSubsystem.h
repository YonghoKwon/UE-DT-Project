// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxProcessSubsystem.generated.h"

/**
 *
 */
UCLASS()
class DTCORE_API UDxProcessSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// Function
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	UFUNCTION(BlueprintCallable, Category = "Component")
	void RegisterComponent(const FString& ComponentId, UActorComponent* InComponent);
	UFUNCTION(BlueprintCallable, Category = "Component")
	void UnregisterComponent(const FString& ComponentId);
	UFUNCTION(BlueprintCallable, Category = "Component")
	UActorComponent* FindComponent(const FString& ComponentId);
private:
protected:

	// Variable
public:
private:
	UPROPERTY()
	TMap<FString, TWeakObjectPtr<UActorComponent>> RegisteredComponents;
protected:
};
