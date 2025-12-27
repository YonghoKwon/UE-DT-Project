#include "DxPlayerTest.h"

#include "Components/TextBlock.h"
#include "Engine/TargetPoint.h"


ADxPlayerTest::ADxPlayerTest()
{
	PrimaryActorTick.bCanEverTick = true;

	PlayerViewType = EPlayerViewType::FreeView;
}

void ADxPlayerTest::BeginPlay()
{
	Super::BeginPlay();
	
}

void ADxPlayerTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADxPlayerTest::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ADxPlayerTest::Move(const FVector2D& MovementVector)
{
	// TopView일 때는 이동 막기
	if (PlayerViewType == EPlayerViewType::TopView)
	{
		return;
	}

	// FreeView일 떄는 부모 클래스 Move 함수 호출
	Super::Move(MovementVector);
}

void ADxPlayerTest::MoveUpDown(const float Value)
{
	// TopView일 때는 이동 막기
	if (PlayerViewType == EPlayerViewType::TopView)
	{
		return;
	}

	// FreeView일 떄는 부모 클래스 Move 함수 호출
	Super::MoveUpDown(Value);
}

EPlayerViewType ADxPlayerTest::GetPlayerViewType()
{
	return PlayerViewType;
}

void ADxPlayerTest::ChangePlayerViewType(UTextBlock* ViewText)
{
	// 시점 전환 로직
	if (PlayerViewType == EPlayerViewType::TopView)
	{
		PlayerViewType = EPlayerViewType::FreeView;
		if (ViewText)
		{
			ViewText->SetText(FText::FromString(TEXT("프리뷰")));
		}

		// 카메라 위치/각도 조정 로직
		// FreeView 타겟 포인트로 이동
		if (FreeViewTarget)
		{
			FVector TargetLocation = FreeViewTarget->GetActorLocation();
			FRotator TargetRotation = FreeViewTarget->GetActorRotation();
			SetActorLocation(TargetLocation);
			SetActorRotation(TargetRotation);

			// 카메라 방향도 맞추기
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				PC->SetControlRotation(TargetRotation);
				PC->SetIgnoreLookInput(false);
			}
		}
	}
	else
	{
		PlayerViewType = EPlayerViewType::TopView;
		if (ViewText)
		{
			ViewText->SetText(FText::FromString(TEXT("탑뷰")));
		}

		if (TopViewTarget)
		{
			FVector TargetLocation = TopViewTarget->GetActorLocation();
			FRotator TargetRotation = TopViewTarget->GetActorRotation();
			SetActorLocation(TargetLocation);
			SetActorRotation(TargetRotation);

			// 카메라 방향도 맞추기
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				PC->SetControlRotation(TargetRotation);
				PC->SetIgnoreLookInput(true);
			}
		}
	}
}