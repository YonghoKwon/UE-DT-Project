#include "UI/DxWidget.h"

#include "DTCore.h"
#include "Core/DxWidgetSubsystem.h"
#include "Player/DxPlayerControllerBase.h"
// #include "Player/DxPlayerTest.h"

void UDxWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// if (APlayerController* PC = GetOwningPlayer())
	// {
	// 	Player = Cast<ADxPlayerTest>(PC->GetPawn());
	// 	PlayerController = Cast<ADxPlayerControllerBase>(PC);
	// }
}

void UDxWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UDxWidget::NativeDestruct()
{
	// 타이머 정리
	StopContinuousTimer();

	Super::NativeDestruct();
}

void UDxWidget::OpenWidgetAddLogic_Implementation()
{
	// 자식 클래스에서 오버라이드하여 구현 (오픈할 때 세부 로직이 필요할 경우)
}

void UDxWidget::CloseWidget()
{
	// DxWidgetSubsystem을 통해 위젯 닫기
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDxWidgetSubsystem* WidgetSubsystem = GameInstance->GetSubsystem<UDxWidgetSubsystem>())
		{
			WidgetSubsystem->CloseWidget(this);
		}
		else
		{
			UE_LOG(LogBase, Warning, TEXT("DxWidget::CloseWidget - DxWidgetSubsystem not found, removing from parent directly"));
			RemoveFromParent();
		}
	}
	else
	{
		UE_LOG(LogBase, Warning, TEXT("DxWidget::CloseWidget - GameInstance is null, removing from parent directly"));
		RemoveFromParent();
	}
}

void UDxWidget::StartContinuousTimer()
{
	// 타이머 시작 (0.016초마다 = 약 60fps)
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ContinuousTimerHandle,
			this,
			&UDxWidget::ContinuousUpdate,
			0.016f,
			true
			);
	}
}

void UDxWidget::StopContinuousTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ContinuousTimerHandle);
	}
}

void UDxWidget::ContinuousUpdate()
{
	// 자식 클래스에서 오버라이드하여 구현
}