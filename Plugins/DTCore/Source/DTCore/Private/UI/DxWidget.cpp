#include "UI/DxWidget.h"

#include "DTCore.h"
#include "Player/DxPlayerControllerBase.h"
#include "Core/DxWidgetSubsystem.h"

void UDxWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (APlayerController* PC = GetOwningPlayer())
	{
		Player = Cast<ADxPlayerBase>(PC->GetPawn());
		PlayerController = Cast<ADxPlayerControllerBase>(PC);

		// PlayerController의 Pawn 변경 이벤트 구독
		BindToPlayerController();
	}
}

void UDxWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

void UDxWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UDxWidget::NativeDestruct()
{
	// 타이머 정리
	StopContinuousTimer();

	// PlayerController 이벤트 구독 해제
	UnbindFromPlayerController();

	Super::NativeDestruct();
}

void UDxWidget::OpenWidgetAddLogic_Implementation()
{
	// 자식 클래스에서 오버라이드하여 구현 (오픈할 때 세부 로직이 필요할 경우)
}

void UDxWidget::CloseWidget()
{
	// DxWidgetSubsystem을 통해 위젯 닫기
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDxWidgetSubsystem* WidgetSubsystem = GI->GetSubsystem<UDxWidgetSubsystem>())
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

UDxWidget* UDxWidget::OpenChildWidget(EDxWidgetFlag InChildFlag)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDxWidgetSubsystem* WidgetSubsystem = GI->GetSubsystem<UDxWidgetSubsystem>())
		{
			return WidgetSubsystem->OpenWidgetFromWidget(this, InChildFlag);
		}
	}
	return nullptr;
}

void UDxWidget::SetParentWidget(UDxWidget* InParentWidget)
{
	ParentWidget = InParentWidget;
}

void UDxWidget::AddChildWidget(UDxWidget* InChildWidget)
{
	if (InChildWidget && !ChildWidgets.Contains(InChildWidget))
	{
		ChildWidgets.Add(InChildWidget);
	}
}

void UDxWidget::RemoveChildWidget(UDxWidget* InChildWidget)
{
	if (InChildWidget)
	{
		ChildWidgets.Remove(InChildWidget);
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

void UDxWidget::BindToPlayerController()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->GetOnNewPawnNotifier().AddUObject(this, &UDxWidget::HandlerPawnChanged);
	}
}

void UDxWidget::UnbindFromPlayerController()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->GetOnNewPawnNotifier().RemoveAll(this);
	}
}

void UDxWidget::HandlerPawnChanged(APawn* NewPawn)
{
	// 새 Pawn이 DxPlayerBase 타입인지 확인
	ADxPlayerBase* NewPlayer = Cast<ADxPlayerBase>(NewPawn);

	// Player 참조 업데이트
	Player = NewPlayer;

	// 자식 클래스에서 추가 처리 가능하도록 가상 함수 호출
	OnPlayerChanged(NewPlayer);
}

void UDxWidget::OnPlayerChanged(ADxPlayerBase* NewPlayer)
{
	// 자식 클래스에서 오버라이드하여 구현
	// 예: Player 변경 시 UI 업데이트, 델리게이트 재구독 등
}