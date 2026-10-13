#include "DxPlayerTest.h"

#include "Components/TextBlock.h"
#include "Engine/TargetPoint.h"
#include "UI/DxWidget.h"
#include "GameFramework/SpringArmComponent.h"
#include "ma0t10_dt/MA0T10/Player/DxPlayerControllerTest.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementMotion.h"


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
    if(bPlacementNavigation&&Controller&&PlayerViewType==EPlayerViewType::FreeView)
    {
        auto* PC=Cast<ADxPlayerControllerTest>(Controller);auto* G=PC?PC->GetPlacementInputOwner():nullptr;
        if(!G||!G->IsObserverNavigationActive()||!G->IsInputFocusOwned()){PlacementMovement=FVector2D::ZeroVector;PlacementVertical=0;return;}
        const FRotator R=Controller->GetControlRotation();
        const FVector Direction=(FRotationMatrix(R).GetUnitAxis(EAxis::X)*PlacementMovement.Y+FRotationMatrix(R).GetUnitAxis(EAxis::Y)*PlacementMovement.X+FVector::UpVector*PlacementVertical).GetClampedToMaxSize(1);
        AddActorWorldOffset(Direction*ControlSpeed*60*VirtualSensorPlacementMotion::BoundedDelta(DeltaTime),true);
        auto P=GetActorLocation();if(P.Z<0||P.Z>72625){P.Z=FMath::Clamp(P.Z,0.,72625.);SetActorLocation(P);}
    }
}

void ADxPlayerTest::SetPlacementNavigationActive(bool Active)
{
    if(Active==bPlacementNavigation)return;
    if(Active&&SpringArmComponent){SavedCameraLag=SpringArmComponent->bEnableCameraLag;SavedRotationLag=SpringArmComponent->bEnableCameraRotationLag;SpringArmComponent->bEnableCameraLag=false;SpringArmComponent->bEnableCameraRotationLag=false;}
    if(!Active&&SpringArmComponent){SpringArmComponent->bEnableCameraLag=SavedCameraLag;SpringArmComponent->bEnableCameraRotationLag=SavedRotationLag;}
    bPlacementNavigation=Active;PlacementMovement=FVector2D::ZeroVector;PlacementVertical=0;
}

void ADxPlayerTest::Look(const FVector2D& Value)
{
    if(auto* PC=Cast<ADxPlayerControllerTest>(Controller))if(auto* G=PC->GetPlacementInputOwner())
    {if(bPlacementNavigation&&G->IsInputFocusOwned()&&PlayerViewType==EPlayerViewType::FreeView)Super::Look(Value);return;}
    Super::Look(Value);
}

void ADxPlayerTest::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ADxPlayerTest::Move(const FVector2D& MovementVector)
{
    if(auto* PC=Cast<ADxPlayerControllerTest>(Controller))if(auto* G=PC->GetPlacementInputOwner())
    {if(bPlacementNavigation&&G->IsInputFocusOwned())PlacementMovement=MovementVector;return;}
	// TopView일 때는 이동 막기
	if (PlayerViewType == EPlayerViewType::TopView)
	{
		return;
	}

	// FreeView일 때는 부모 클래스의 Move 함수 호출
	Super::Move(MovementVector);
}

void ADxPlayerTest::MoveUpDown(const float Value)
{
    if(auto* PC=Cast<ADxPlayerControllerTest>(Controller))if(auto* G=PC->GetPlacementInputOwner())
    {if(bPlacementNavigation&&G->IsInputFocusOwned())PlacementVertical=Value;return;}
	// TopView일 때는 이동 막기
	if (PlayerViewType == EPlayerViewType::TopView)
	{
		return;
	}

	// FreeView일 때는 부모 클래스의 MoveUpDown 함수 호출
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

		// ViewText 업데이트
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
			// 플레이어 위치 이동
			SetActorLocation(TargetLocation);
			// 플레이어 회전
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

		// 카메라 위치/각도 조정 로직
		if (TopViewTarget)
		{
			FVector TargetLocation = TopViewTarget->GetActorLocation();
			FRotator TargetRotation = TopViewTarget->GetActorRotation();
			SetActorLocation(TargetLocation);
			SetActorRotation(TargetRotation);

			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				PC->SetControlRotation(TargetRotation);
				PC->SetIgnoreLookInput(true); // 이동 막기
			}
		}
	}
}
