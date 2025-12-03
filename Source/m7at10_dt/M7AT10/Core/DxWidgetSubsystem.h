// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxWidgetSubsystem.generated.h"

class AInteractableActor;
class UDxWidget;
enum class EDxViewMode : uint8;
/**
 * 
 */
UCLASS()
class M7AT10_DT_API UDxWidgetSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// Function
public:
	UDxWidgetSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	// DxLevelManager 통해서 UI 모드 변경
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SwitchUIMode(EDxViewMode NewMode);
	// 위젯 생성 및 열기
	UFUNCTION(BlueprintCallable, Category = "UI")
	UDxWidget* OpenWidget(AInteractableActor* InteractableActor);
	// 위젯 닫기
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseWidget(UDxWidget* CloseWidget);
private:
protected:

	// Variable
public:
private:
	UPROPERTY()
	TObjectPtr<UDataTable> LevelDataTable;
	UPROPERTY()
	TObjectPtr<UDxWidget> MainWidgetInstance;
protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDxWidget> MainWidgetClass;
};
