#pragma once

#include "CoreMinimal.h"
#include "DxWidgetInfo.generated.h"

class UDxWidget;

USTRUCT(BlueprintType)
struct FWidgetInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	TSubclassOf<UDxWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	FVector2D Position = FVector2D(1000.0f, 500.0f);
};

USTRUCT(BlueprintType)
struct FWidgetInfoList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	TArray<FWidgetInfo> Widgets;
};
