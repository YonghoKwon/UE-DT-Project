#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DxPlayerBase.generated.h"
UENUM(BlueprintType)
enum class EDirectionType : uint8
{
	Left       UMETA(DisplayName = "Left"),
	Right      UMETA(DisplayName = "Right"),
	Forward    UMETA(DisplayName = "Forward"),
	Backward   UMETA(DisplayName = "Backward"),
	Up         UMETA(DisplayName = "Up"),
	Down       UMETA(DisplayName = "Down"),
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlSpeedChanged, float, NewSpeed);

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class DTCORE_API ADxPlayerBase : public APawn
{
	GENERATED_BODY()

public:
	ADxPlayerBase();
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
protected:
	virtual void BeginPlay() override;

	// 함수 모음
public:
	UFUNCTION()
	void SetControlSpeed(float NewSpeed);
	UFUNCTION()
	float GetControlSpeed();
	UFUNCTION()
	void CycleControlSpeed(); // 속도 레벨 순환
	UFUNCTION()
	void Look(const FVector2D& LookVector);
	UFUNCTION()
	virtual void Move(const FVector2D& MovementVector);
	UFUNCTION()
	virtual void MoveUpDown(float Value);
private:

protected:

	// 변수 모음
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	USpringArmComponent* SpringArmComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	UCameraComponent* CameraComponent;

	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FOnControlSpeedChanged OnControlSpeedChanged;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float ControlSpeed = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float ControlMinSpeed = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float ControlMaxSpeed = 1000.0f;
private:

protected:
};
