// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystemInterface.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "DxPlayerControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class M7AT10_DT_API ADxPlayerControllerBase : public APlayerController
{
	GENERATED_BODY()

	// 함수
public:
	ADxPlayerControllerBase();
	void BeginPlay() override;
	UFUNCTION()
	void SetUIMoveInput(const FVector2D& MoveInput);
	UFUNCTION()
	void SetUILookInput(const FVector2D& LookInput);
	UFUNCTION()
	void SetUIVerticalInput(float Value);
	UFUNCTION()
	void CyclePlayerControlSpeed();
	UFUNCTION()
	float GetPlayerControlSpeed();
	UFUNCTION()
	void ControlMoveSpeed(const FInputActionValue& Value);
	UFUNCTION()
	void ClickLeftMouseButton(const FInputActionValue& Value);

private:

protected:
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void MoveUpDown(const FInputActionValue& Value);
	void ClickRightMouseButton(const FInputActionValue& Value);

	// 마우스 호버 감지
	void CheckMouseHover();

	// 변수
public:
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputMappingContext* PlayerContext;
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MovementAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LookAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MouseWheelAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* UpDownAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LeftMouseButtonAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* RightMouseButtonAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector2D UIMoveInput;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector2D UILookInput;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float UIVerticalInput = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AActor* HitActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Control")
	int32 ControlSpeedStep = 100;
	UPROPERTY(BlueprintReadWrite)
	bool PossibleClick = true;
private:
	UPROPERTY()
	FHitResult HitResult;
	UPROPERTY()
	bool bIsClickRightMouseButton;
	UPROPERTY()
	bool bIsHitDoOnce;
	UPROPERTY()
	bool bIsNotHitDoOnce;
	UPROPERTY()
	bool bIsHitActor;
	UPROPERTY()
	bool bIsWidgetUnderMouse = false;

	// 현재 호버되 InteractableActor
	UPROPERTY()
	class AInteractableActor* CurrentHoveredActor = nullptr;
	// 현재 호버되 매쉬 컴포넌트
	UPROPERTY()
	UPrimitiveComponent* CurrentHoveredMesh;
protected:
};
