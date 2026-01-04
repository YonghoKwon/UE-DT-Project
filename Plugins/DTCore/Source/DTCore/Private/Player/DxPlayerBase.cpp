#include "Player/DxPlayerBase.h"

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
	AddControllerPitchInput(LookVector.Y * -0.5f);
	// 좌 + / 우 -
	AddControllerYawInput(LookVector.X * 0.5f);
}
// 앞뒤,좌우 카메라 이동, 배속 적용
void ADxPlayerBase::Move(const FVector2D& MovementVector)
{
	if (!Controller) return;

	// 컨트롤러의 회전(카메라가 보는 방향)을 기준으로 앞/오른쪽 벡터 계산
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	// 카메라가 보는 방향의 전방/우측 벡터
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	FVector MoveDirection = (ForwardDirection * MovementVector.Y) + (RightDirection * MovementVector.X);

	// 현재 위치에 더하기
	AddActorWorldOffset(MoveDirection * ControlSpeed, true);
}
// 상하 이동, 배속 적용
void ADxPlayerBase::MoveUpDown(float Value)
{
	FVector NewLocation = GetActorLocation() + FVector(0.0f, 0.0f, Value * ControlSpeed);
	SetActorLocation(NewLocation);
}