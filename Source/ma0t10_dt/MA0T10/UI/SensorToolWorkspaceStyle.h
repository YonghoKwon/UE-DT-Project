#pragma once
#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
/** Private presentation tokens. Never modifies the engine or shared legacy style. */
struct FSensorToolWorkspaceStyle
{
	static FSlateFontInfo Font(int32 Size,float Scale=1){return FCoreStyle::GetDefaultFontStyle("Regular",FMath::RoundToInt(Size*Scale));}
	static FLinearColor Panel(){return FLinearColor(.025f,.038f,.065f,.96f);}
	static FLinearColor Text(){return FLinearColor(.93f,.96f,1,1);}
	static FLinearColor Accent(){return FLinearColor(.22f,.68f,.95f,1);}
	static const FButtonStyle& Button(){static FButtonStyle S=[](){auto V=FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");V.Normal.TintColor=FSlateColor(FLinearColor(.08f,.12f,.2f));V.Hovered.TintColor=FSlateColor(FLinearColor(.14f,.24f,.36f));V.Pressed.TintColor=FSlateColor(FLinearColor(.08f,.32f,.48f));return V;}();return S;}
};
