// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DxWidgetDataType.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/DataAsset.h"
#include "m7at10_dt/M7AT10/Player/DxPlayerTest.h"
#include "DxWidgetConfigData.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class DTCORE_API UDxWidgetConfigData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Config")
	TMap<EDxWidgetFlag, FWidgetInfo> WidgetMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Config")
	TMap<EPlayerViewType, EDxWidgetFlag> DefaultViewModeWidgetFlags;
};
