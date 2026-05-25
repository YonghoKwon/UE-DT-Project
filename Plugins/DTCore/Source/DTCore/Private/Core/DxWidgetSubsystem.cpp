#include "Core/DxWidgetSubsystem.h"

#include "DTCore.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/DTCoreSettings.h"
#include "InteractableActor/InteractableActor.h"
#include "Manager/DxLevelStruct.h"
#include "UI/DxWidget.h"
#include "UI/DxWidgetConfigData.h"

UDxWidgetSubsystem::UDxWidgetSubsystem()
{
	const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();

	if (Settings->LevelDataTable.ToSoftObjectPath().IsValid())
	{
		LevelDataTable = Settings->LevelDataTable.LoadSynchronous();
	}
}

void UDxWidgetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxWidgetSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxWidgetSubsystem::Tick(float DeltaTime)
{
}

TStatId UDxWidgetSubsystem::GetStatId() const
{
	return TStatId();
}

void UDxWidgetSubsystem::SwitchUIMode(EDxViewMode NewMode)
{
	// 1. 기존 메인 위젯 정리
	if (MainWidgetInstance)
	{
		MainWidgetInstance->RemoveFromParent();
		MainWidgetInstance = nullptr;
	}

	// 2. 데이터 테이블에서 모드에 맞는 위젯 클래스 찾기 (중복 제거)
	MainWidgetClass = nullptr;
	if (LevelDataTable)
	{
		LevelDataTable->ForeachRow<FDxLevelStruct>(TEXT("UDxWidgetSubsystem::SwitchUIMode"),
			[this, NewMode](const FName& RowName, const FDxLevelStruct& Row)
			{
				if (NewMode == Row.DxViewMode)
				{
					MainWidgetClass = Row.UseMainWidget;
					// return false; // 일치하는 첫 행을 찾으면 반복 중단
				}
			});
	}

	// 3. 새 메인 위젯 띄우기
	if (MainWidgetClass)
	{
		MainWidgetInstance = CreateWidget<UDxWidget>(GetWorld(), MainWidgetClass);
		if (MainWidgetInstance)
		{
			MainWidgetInstance->AddToViewport();
		}
	}
}

UDxWidget* UDxWidgetSubsystem::OpenWidget(AInteractableActor* InteractableActor)
{
	if (!InteractableActor) return nullptr;

	UDxWidgetConfigData* Config = InteractableActor->WidgetConfig;
	if (!Config) return nullptr;

	// Config 찾기
	FWidgetInfoList* WidgetInfoList = Config->WidgetMap.Find(InteractableActor->WidgetFlag);
	if (!WidgetInfoList || WidgetInfoList->Widgets.Num() == 0) return nullptr;

	UDxWidget* FirstWidget = nullptr;
	for (const FWidgetInfo& Info : WidgetInfoList->Widgets)
	{
		if (!Info.WidgetClass) continue;
		UDxWidget* Created = CreateWidgetInternal(Info.WidgetClass, Info.Position, InteractableActor, nullptr, InteractableActor->WidgetFlag);
		if (!FirstWidget && Created)
		{
			FirstWidget = Created;
		}
	}

	return FirstWidget;
}

UDxWidget* UDxWidgetSubsystem::OpenWidgetFromWidget(UDxWidget* ParentDxWidget, EDxWidgetFlag TargetFlag)
{
	if (!ParentDxWidget) return nullptr;

	// ParentWidget의 Config를 사용
	UDxWidgetConfigData* Config = ParentDxWidget->WidgetConfig;
	if (!Config) return nullptr;

	FWidgetInfoList* WidgetInfoList = Config->WidgetMap.Find(TargetFlag);
	if (!WidgetInfoList || WidgetInfoList->Widgets.Num() == 0) return nullptr;

	UDxWidget* FirstWidget = nullptr;
	for (const FWidgetInfo& Info : WidgetInfoList->Widgets)
	{
		if (!Info.WidgetClass) continue;
		UDxWidget* Created = CreateWidgetInternal(Info.WidgetClass, Info.Position, ParentDxWidget->MyActor, ParentDxWidget, TargetFlag);
		if (!FirstWidget && Created)
		{
			FirstWidget = Created;
		}
	}

	return FirstWidget;
}

void UDxWidgetSubsystem::CloseWidget(UDxWidget* CloseWidget)
{
	if (!IsValid(CloseWidget)) return;

	// 1. 자식 위젯부터 재귀적으로 닫기
	TArray<UDxWidget*> ChildrenToClose = CloseWidget->ChildWidgets;
	for (UDxWidget* Child : ChildrenToClose)
	{
		if (IsValid(Child))
		{
			UE_LOG(LogTemp, Log, TEXT("CloseWidget: Cascading close child %s"), *Child->GetName());

			this->CloseWidget(Child);
		}
	}

	// 2. 부모 위젯과의 연결 끊기
	if (UDxWidget* Parent = CloseWidget->ParentWidget.Get())
	{
		Parent->RemoveChildWidget(CloseWidget);
	}

	// 3. 서브시스템 관리 목록(OpenWidgets)에서 제거
	if (OpenWidgets.Contains(CloseWidget))
	{
		OpenWidgets.Remove(CloseWidget);
	}

	// 4. 화면 (Viewport 또는 부모 패널)에서 제거
	CloseWidget->RemoveFromParent();

	UE_LOG(LogTemp, Log, TEXT("CloseWidget: Widget '%s' closed successfully"), *CloseWidget->GetName());
}

void UDxWidgetSubsystem::CloseWidgetFromWidget(UDxWidget* ParentDxWidget, EDxWidgetFlag TargetFlag)
{
	if (!ParentDxWidget) return;

	TArray<UDxWidget*> WidgetsToClose;
	for (UDxWidget* Child : ParentDxWidget->ChildWidgets)
	{
		if (IsValid(Child) && Child->WidgetFlag == TargetFlag)
		{
			WidgetsToClose.Add(Child);
		}
	}

	for (UDxWidget* Child : WidgetsToClose)
	{
		CloseWidget(Child);
	}
}

TArray<TObjectPtr<UDxWidget>> UDxWidgetSubsystem::GetOpenWidgets()
{
	return this->OpenWidgets;
}

UDxWidget* UDxWidgetSubsystem::CreateWidgetInternal(TSubclassOf<UDxWidget> WidgetClass, const FVector2D& Position,
	AInteractableActor* OwnerActor, UDxWidget* ParentWidget, EDxWidgetFlag Flag)
{
	// 1. 중복 체크
	if (Flag != EDxWidgetFlag::CctvWidget)
	{
		for (UDxWidget* ExistingWidget : OpenWidgets)
		{
			if (ExistingWidget &&
				ExistingWidget->GetClass() == WidgetClass &&
				ExistingWidget->MyActor == OwnerActor &&
				ExistingWidget->WidgetFlag == Flag)
			{
				const bool bSameSpawn = ExistingWidget->SpawnPosition.Equals(Position, 0.5f);
				if (bSameSpawn)
				{
					BringToFront(ExistingWidget);
					DX_LOG(GetWorld(), TEXT("CreateWidgetInternal: Widget already open."));
					return ExistingWidget;
				}
			}
		}
	}


	// 2. 패널 찾기
	UPanelWidget* Panel = GetAddWidgetPanel();
	if (!Panel) return nullptr;

	// 3. 생성
	UDxWidget* NewWidget = CreateWidget<UDxWidget>(GetWorld(), WidgetClass);
	if (!NewWidget) return nullptr;

	// 4. 패널에 추가 및 위치 설정
	UPanelSlot* PanelSlot = Panel->AddChild(NewWidget);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
	{
		CanvasSlot->SetPosition(Position);
	}

	// 5. 데이터 설정
	OpenWidgets.Add(NewWidget);

	BringToFront(NewWidget);

	if (ParentWidget)
	{
		if (UCanvasPanelSlot* ParentSlot = Cast<UCanvasPanelSlot>(ParentWidget->Slot))
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(NewWidget->Slot))
			{
				int32 NewZOrder = FMath::Max(CanvasSlot->GetZOrder(), ParentSlot->GetZOrder() + 1);
				CanvasSlot->SetZOrder(NewZOrder);
			}
		}
	}

	// 5. 데이터 설정
	NewWidget->MyActor = OwnerActor;
	NewWidget->WidgetFlag = Flag;
	NewWidget->SpawnPosition = Position;

	// 계층 연결
	if (ParentWidget)
	{
		NewWidget->SetParentWidget(ParentWidget);
		ParentWidget->AddChildWidget(NewWidget);
	}

	// 6. 로직 실행
	NewWidget->OpenWidgetAddLogic();

	return NewWidget;
}

void UDxWidgetSubsystem::BringToFront(UDxWidget* Widget)
{
	if (!Widget || !Widget->Slot) return;

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		int32 MaxZOrder = 0;
		for (const auto& OpenW : OpenWidgets)
		{
			if (OpenW && OpenW->Slot && OpenW != Widget)
			{
				if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(OpenW->Slot))
				{
					MaxZOrder = FMath::Max(MaxZOrder, Slot->GetZOrder());
				}
			}
		}
		CanvasSlot->SetZOrder(MaxZOrder + 1);
	}
}

class UPanelWidget* UDxWidgetSubsystem::GetAddWidgetPanel() const
{
	if (!MainWidgetInstance)
	{
		return nullptr;
	}

	// WidgetTree에서 "AddWidgetPanel" 이름으로 위젯 찾기
	if (UWidgetTree* WidgetTree = MainWidgetInstance->WidgetTree)
	{
		UWidget* FoundWidget = WidgetTree->FindWidget(FName("AddWidgetPanel"));
		return Cast<UPanelWidget>(FoundWidget);
	}

	return nullptr;
}
