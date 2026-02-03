#include "Player/DxPlayerBase.h"

#include "DTCore.h"
#include "Player/DxPlayerControllerBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

ADxPlayerBase::ADxPlayerBase()
{
	PrimaryActorTick.bCanEverTick = true;

	ControlSpeed = 100.0f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// 스프링 암 생성 및 설정
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArmComponent->SetupAttachment(RootComponent);
	SpringArmComponent->TargetArmLength = 0.0f; // 1인칭 0, 3인칭 늘리기
	SpringArmComponent->bDoCollisionTest = false; // 자유 이동 false
	SpringArmComponent->bUsePawnControlRotation = true; // 컨트롤러 회전을 따름 (마우스 회전 시 암이 회전)
	SpringArmComponent->bEnableCameraLag = true; // 부드러운 이동
	SpringArmComponent->CameraLagSpeed = 10.0f;

	// 카메라 생성 및 설정
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComponent->SetupAttachment(SpringArmComponent); // 스프링 암 끝에 부착
	CameraComponent->bUsePawnControlRotation = false; // 암이 회전하므로 카메라는 암을 따라가기만 하면 됨

	// Pawn 설정
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
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
		UE_LOG(LogBase, Error, TEXT("CameraComponent is null"));
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
		UE_LOG(LogBase, Error, TEXT("Controller is null in Look!"));
		return;
	}

	// 현재 회전 값 가져오기
	const FRotator CurrentRotation = Controller->GetControlRotation();

	// 새로운 Pitch 값 계산 (위 - / 아래 +)
	float NewPitch = CurrentRotation.Pitch + (LookVector.Y * 1.0f);

	// Pitch를 -85도에서 +85도 사이로 제한
	NewPitch = FMath::Clamp(NewPitch, -80.0f, 80.0f);

	// 새로운 Yaw 값 계산 (좌 + / 우 -)
	float NewYaw = CurrentRotation.Yaw + (LookVector.X * 1.0f);

	// 새로운 회전 적용
	FRotator NewRotation = CurrentRotation;
	NewRotation.Pitch = NewPitch;
	NewRotation.Yaw = NewYaw;
	Controller->SetControlRotation(NewRotation);
}
// 앞뒤,좌우 카메라 이동, 배속 적용
void ADxPlayerBase::Move(const FVector2D& MovementVector)
{
	if (!Controller) return;

	// 컨트롤러의 회전(카메라가 보는 방향)을 기준으로 앞/오른쪽 벡터 계산
	const FRotator Rotation = Controller->GetControlRotation();

	// 카메라가 실제로 바라보는 방향의 전방/우측 벡터 (Pitch 포함)
	const FVector ForwardDirection = FRotationMatrix(Rotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);

	FVector MoveDirection = (ForwardDirection * MovementVector.Y) + (RightDirection * MovementVector.X);

	// 현재 위치에 더하기
	AddActorWorldOffset(MoveDirection * ControlSpeed, true);
}
// 상하 이동, 배속 적용
void ADxPlayerBase::MoveUpDown(float Value)
{
	FVector CurrentLocation = GetActorLocation();
	FVector NewLocation = CurrentLocation + FVector(0.0f, 0.0f, Value * ControlSpeed);

	// Z축 높이를 72625 이하로 제한
	if (NewLocation.Z > 72625.0f)
	{
		NewLocation.Z = 72625.0f;
	}

	SetActorLocation(NewLocation);
}