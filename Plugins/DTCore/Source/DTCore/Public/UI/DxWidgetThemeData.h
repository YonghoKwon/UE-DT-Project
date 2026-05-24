#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DxWidgetThemeData.generated.h"

UENUM(BlueprintType)
enum class EDxWidgetStyleType : uint8
{
	Standard,
	Emphasis,
	Alarm,
	Test
};

USTRUCT(BlueprintType)
struct FDxThemeButtonColors
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor TextColor = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor HoverTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor BorderColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Normal = FLinearColor(0.0f, 0.2f, 0.8f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Hover = FLinearColor(0.1f, 0.3f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Pressed = FLinearColor(0.0f, 0.1f, 0.6f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Disabled = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
};

USTRUCT(BlueprintType)
struct FDxContainerStyle
{
	GENERATED_BODY()

	// 위젯 타이틀
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor TitleBackground = FLinearColor(0.0f, 0.222724f, 1.0f, 1.0f);

	// 위젯 메인 배경
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor BodyBackground = FLinearColor(0.0f, 0.022577f, 0.364583f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor BorderColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDxThemeButtonColors ButtonColors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor TitleTextColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor SubTitleTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor LabelTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor ValueTextColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor SeparateImageColor = FLinearColor(0.024158f, 0.090842f, 0.212231f, 1.0f);
};

USTRUCT(BlueprintType)
struct FDxThemeContainerVariations
{
	GENERATED_BODY()

	// [일반]
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDxContainerStyle Standard;
	// [강조]
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDxContainerStyle Emphasis;
	// [알람]
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDxContainerStyle Alarm;

	// [테스트용]
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDxContainerStyle Test;
};

USTRUCT(BlueprintType)
struct FDxThemeSemanticColors
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Normal = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Success = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Warning = FLinearColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Error = FLinearColor::Red;
};

/**
 * 
 */
UCLASS(BlueprintType)
class DTCORE_API UDxWidgetThemeData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 위젯의 "종류/용도"에 따른 스타일
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Containers")
	FDxThemeContainerVariations Containers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Semantic")
	FDxThemeSemanticColors Semantic;

	UFUNCTION(BlueprintPure, Category = "Theme")
	const FDxContainerStyle& GetContainerStyle(EDxWidgetStyleType Type) const
	{
		switch (Type)
		{
		case EDxWidgetStyleType::Standard: return Containers.Standard;
		case EDxWidgetStyleType::Emphasis: return Containers.Emphasis;
		case EDxWidgetStyleType::Alarm:    return Containers.Alarm;
		case EDxWidgetStyleType::Test:     return Containers.Test;
		default:                           return Containers.Standard;
		}
	}

};
