// Fill out your copyright notice in the Description page of Project Settings.


#include "DxWidget.h"

#include "m7at10_dt/M7AT10/Player/DxPlayerTest.h"

void UDxWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (APlayerController* PC = GetOwningPlayer())
	{
		Player = Cast<ADxPlayerTest>(PC->GetPawn());
	}
}

void UDxWidget::NativeConstruct()
{
	Super::NativeConstruct();
}
