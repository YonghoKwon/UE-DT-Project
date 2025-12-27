#include "DxPlayerBase.h"

#include "DxPlayerControllerBase.h"
#include "Camera/CameraComponent.h"


ADxPlayerBase::ADxPlayerBase()
{
	PrimaryActorTick.bCanEverTick = true;

	ControlSpeed = 100.0f;

	// Pawn은 Yaw만 회전, Pitch는 카메라만 회전
	bUseControllerRotationYaw = false; // Pawn은 회전 안 함
	bUseControllerRotationPitch = true;
	bUseControllerRotationRoll = false;
}

void ADxPlayerBase::BeginPlay()
{
	Super::BeginPlay();
	if (CameraComponent)
	{
		CameraComponent->bUsePawnControlRotation = true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CameraComponent is null"));
	}
	if (ADxPlayerControllerBase* DxPlayerController = Cast<ADxPlayerControllerBase>(GetController()))
	{
		DxPlayerController->SetControlRotation(GetActorRotation());
	}
}

void ADxPlayerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADxPlayerBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ADxPlayerBase::SetControlSpeed(float NewSpeed)
{
	ControlSpeed = FMath::Clamp(NewSpeed, ControlMinSpeed, ControlMaxSpeed);
	OnControlSpeedChanged.Broadcast(ControlSpeed);
}

float ADxPlayerBase::GetControlSpeed()
{
	return ControlSpeed;
}
// 속도 레벨 순환
void ADxPlayerBase::CycleControlSpeed()
{
	float NewSpeed = ControlSpeed + 100.0f;

	// 최대 속도를 초과하면 최소 속도로 되돌림
	if (NewSpeed > ControlMaxSpeed)
	{
		NewSpeed = ControlMinSpeed;
	}
	SetControlSpeed(NewSpeed);
}
// 시점 변경
void ADxPlayerBase::Look(const FVector2D& LookVector)
{
	if (!Controller)
	{
		UE_LOG(LogTemp, Error, TEXT("Controller is null in Look!"));
		return;
	}

	// 위 - / 아래 +
	AddControllerPitchInput(-LookVector.Y * 0.5);
	// 좌 + / 우 -
	AddControllerYawInput(LookVector.X * 0.5);
}
// 앞뒤,좌우 카메라 이동, 배속 적용
void ADxPlayerBase::Move(const FVector2D& MovementVector)
{
	if (CameraComponent)
	{
		// 카메라의 전체 회전(Pitch 포함)을 사용
		const FVector Forward = CameraComponent->GetForwardVector();
		const FVector Right = CameraComponent->GetRightVector();

		FVector NewLocation = GetActorLocation() +
			Forward * MovementVector.Y  * ControlSpeed +
			Right * MovementVector.X * ControlSpeed;

		SetActorLocation(NewLocation);
	}
}
// 상하 이동, 배속 적용
void ADxPlayerBase::MoveUpDown(float Value)
{
	FVector NewLocation = GetActorLocation() + FVector(0.0f, 0.0f, Value * ControlSpeed);
	SetActorLocation(NewLocation);
}