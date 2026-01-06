#include "Core/DxWidgetSubsystem.h"

#include "DTCore.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/DTCoreSettings.h"
#include "InteractableActor/InteractableActor.h"
#include "Manager/DxLevelStruct.h"
#include "UI/DxWidget.h"

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
	if (!InteractableActor)
	{
		UE_LOG(LogBase, Warning, TEXT("OpenWidget: InteractableActor is null"));
		return nullptr;
	}
	// 위젯을 추가할 부모 canvasPanel 찾기
	UPanelWidget* Panel = GetAddWidgetPanel();
	if (!Panel)
	{
		UE_LOG(LogBase, Error, TEXT("OpenWidget: AddWidgetPanel not found in MainWidget"));
		return nullptr;
	}
	// WidgetMap에서 WidgetFlag에 해당하는 위젯 정보 찾기
	FWidgetInfo* WidgetInfoPtr = InteractableActor->WidgetMap.Find(InteractableActor->WidgetFlag);
	if (!WidgetInfoPtr || ! WidgetInfoPtr->WidgetClass)
	{
		UE_LOG(LogBase, Warning, TEXT("OpenWidget: No widget info found for flag '%s'"), *InteractableActor->WidgetFlag);
		return nullptr;
	}

	FWidgetInfo& WidgetInfo = *WidgetInfoPtr;

	// 위젯 생성
	UDxWidget* NewWidget = CreateWidget<UDxWidget>(GetWorld(), WidgetInfo.WidgetClass);
	if (!NewWidget)
	{
		UE_LOG(LogBase, Error, TEXT("OpenWidget: Failed to create widget"));
		return nullptr;
	}

	UPanelSlot* PanelSlot = Panel->AddChild(NewWidget);

	// Canvas Panel인 경우 위치 설정
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
	{
		CanvasSlot->SetPosition(WidgetInfo.Position);
		CanvasSlot->SetZOrder(1);
	}

	// 열린 위젯 목록에 추가
	OpenWidgets.Add(NewWidget);

	// 연결된 액터 설정
	NewWidget->MyActor = InteractableActor;
	// 위젯의 OpenWidget 함수 호출 (추가 로직 실행)
	NewWidget->OpenWidgetAddLogic();

	UE_LOG(LogBase, Log, TEXT("OpenWidget: Widget '%s' opened at position (%f, %f)"),
		*InteractableActor->WidgetFlag,
		WidgetInfo.Position.X,
		WidgetInfo.Position.Y);

	return NewWidget;
}

void UDxWidgetSubsystem::CloseWidget(UDxWidget* CloseWidget)
{
	if (!CloseWidget)
	{
		UE_LOG(LogBase, Warning, TEXT("CloseWidget:Widget is null"));
		return;
	}

	// 열린 위젯 목록에서 제거
	if (OpenWidgets.Contains(CloseWidget))
	{
			OpenWidgets.Remove(CloseWidget);
		UE_LOG(LogBase, Log, TEXT("CloseWidget:Widget removed from OpenWidgets list"));
	}

	// MainWidget의 컨테이너에서 제거
	CloseWidget->RemoveFromParent();

	UE_LOG(LogBase, Log, TEXT("CloseWidget:Widget closed successfully"));
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
