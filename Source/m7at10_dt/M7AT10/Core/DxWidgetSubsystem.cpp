// Fill out your copyright notice in the Description page of Project Settings.


#include "DxWidgetSubsystem.h"

#include "m7at10_dt/M7AT10/Manager/DxLevelStruct.h"
#include "m7at10_dt/M7AT10/UI/DxWidget.h"

UDxWidgetSubsystem::UDxWidgetSubsystem()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> LevelDataTableFinder(TEXT("/Game/M7AT10/Common/DataTables/DT_DxLevel.DT_DxLevel"));

	if (LevelDataTableFinder.Succeeded())
	{
		LevelDataTable = LevelDataTableFinder.Object;
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
				if (NewMode == Row.DxViewMode && !MainWidgetClass)
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
	return nullptr;
}

void UDxWidgetSubsystem::CloseWidget(UDxWidget* CloseWidget)
{

}
