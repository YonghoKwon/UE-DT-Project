// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DxPlayerBase.h"
#include "DxPlayerTest.generated.h"

class ATargetPoint;

UENUM(BlueprintType)
enum class EPlayerViewType : uint8
{
	TopView		UMETA(DisplayName = "TopView"),
	FreeView	UMETA(DisplayName = "FreeView"),
};
UCLASS()
class M7AT10_DT_API ADxPlayerTest : public ADxPlayerBase
{
	GENERATED_BODY()

public:
	ADxPlayerTest();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	EPlayerViewType GetPlayerViewType();
	void ChangePlayerViewType(class UTextBlock* ViewText = nullptr);
	void Move(const FVector2D& MovementVector) override;
	void MoveUpDown(const float Value) override;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	EPlayerViewType PlayerViewType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraPoint")
	ATargetPoint* TopViewTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraPoint")
	ATargetPoint* FreeViewTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraPoint")
	float TransitionSpeed = 5.0f;
};
