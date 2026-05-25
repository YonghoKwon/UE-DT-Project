#include "UI/DxWidget.h"

#include "DTCore.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/DTCoreSettings.h"
#include "Player/DxPlayerControllerBase.h"
#include "Core/DxWidgetSubsystem.h"

void UDxWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 위젯에 할당된 테마가 없다면, 프로젝트 세팅의 글로벌 테마를 자동 세팅
	if (!ThemeData)
	{
		const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();
		if (Settings && Settings->DefaultWidgetTheme.ToSoftObjectPath().IsValid())
		{
			ThemeData = Settings->DefaultWidgetTheme.LoadSynchronous();
		}
	}

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

	if (!ThemeData)
	{
		const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();
		if (Settings && Settings->DefaultWidgetTheme.ToSoftObjectPath().IsValid())
		{
			ThemeData = Settings->DefaultWidgetTheme.LoadSynchronous();
		}
	}

	// 공통 요소에 대해서만 적용 처리
	ApplyTheme();
}

void UDxWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddDynamic(this, &UDxWidget::OnCloseClicked);
	}
	if (Btn_Retry)
	{
		Btn_Retry->OnClicked.AddDynamic(this, &UDxWidget::OnRetryClicked);
	}
}

void UDxWidget::NativeDestruct()
{
	// 타이머 정리
	StopContinuousTimer();

	// PlayerController 이벤트 구독 해제
	UnbindFromPlayerController();

	Super::NativeDestruct();
}

FReply UDxWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDxWidgetSubsystem* WidgetSubsystem = GI->GetSubsystem<UDxWidgetSubsystem>())
			{
				WidgetSubsystem->BringToFront(this);
			}
		}
		return FReply::Unhandled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
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
			CloseWidgetAddLogic_Implementation();
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

void UDxWidget::CloseWidgetAddLogic_Implementation()
{
}

void UDxWidget::RetryWidget_Implementation()
{
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

void UDxWidget::CloseChildWidget(EDxWidgetFlag InChildFlag)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDxWidgetSubsystem* WidgetSubsystem = GI->GetSubsystem<UDxWidgetSubsystem>())
		{
			return WidgetSubsystem->CloseWidgetFromWidget(this, InChildFlag);
		}
	}
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

void UDxWidget::ApplyTheme_Implementation()
{
	if (!ThemeData) return;

	const FDxContainerStyle& Style = ThemeData->GetContainerStyle(CurrentStyleType);

	if (Img_TitleBg)
	{
		Img_TitleBg->SetColorAndOpacity(Style.TitleBackground);
	}
	if (Img_BodyBg)
	{
		Img_BodyBg->SetColorAndOpacity(Style.BodyBackground);
	}
	if (Txt_Title)
	{
		Txt_Title->SetColorAndOpacity(Style.TitleTextColor);
	}
}

void UDxWidget::BindToPlayerController()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->GetOnNewPawnNotifier().AddUObject(this, &UDxWidget::HandlePawnChanged);
	}
}

void UDxWidget::UnbindFromPlayerController()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->GetOnNewPawnNotifier().RemoveAll(this);
	}
}

void UDxWidget::HandlePawnChanged(APawn* NewPawn)
{
	// 새 Pawn이 DxPlayerBase 타입인지 확인
	ADxPlayerBase* NewPlayer = Cast<ADxPlayerBase>(NewPawn);

	// Player 참조 업데이트
	Player = NewPlayer;

	// 자식 클래스에서 추가 처리 가능하도록 가상 함수 호출
	OnPlayerChanged(NewPlayer);
}

void UDxWidget::OnCloseClicked()
{
	CloseWidget();
}

void UDxWidget::OnRetryClicked()
{
	RetryWidget();
}

void UDxWidget::OnPlayerChanged(ADxPlayerBase* NewPlayer)
{
	// 자식 클래스에서 오버라이드하여 구현
	// 예: Player 변경 시 UI 업데이트, 델리게이트 재구독 등
}
