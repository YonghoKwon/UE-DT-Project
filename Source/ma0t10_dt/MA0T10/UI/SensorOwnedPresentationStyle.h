#pragma once
#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

/** Opt-in presentation tokens. Never installs or mutates a global Slate/DTCore style. */
struct FSensorOwnedPresentationStyle
{
    static FLinearColor Color(uint8 R,uint8 G,uint8 B,uint8 A=255){return FLinearColor::FromSRGBColor(FColor(R,G,B,A));}
    static FLinearColor Panel(){return Color(18,24,35,254);}
    static FLinearColor Header(){return Color(12,18,28);}
    static FLinearColor Section(){return Color(27,36,50,255);}
    static FLinearColor Text(){return Color(241,245,249);}
    static FLinearColor Muted(){return Color(176,190,207);}
    static FLinearColor Accent(){return Color(83,202,239);}
    static const FSlateBrush* PanelBrush(){static FSlateRoundedBoxBrush B(FLinearColor::White,8.0f);return &B;}
    static const FSlateBrush* SectionBrush(){static FSlateRoundedBoxBrush B(FLinearColor::White,5.0f);return &B;}
    static const FButtonStyle& Button(bool Danger=false)
    {
        static const FButtonStyle Normal=FButtonStyle()
            .SetNormal(FSlateRoundedBoxBrush(Color(38,51,69),5.0f))
            .SetHovered(FSlateRoundedBoxBrush(Color(52,75,99),5.0f))
            .SetPressed(FSlateRoundedBoxBrush(Color(26,96,126),5.0f))
            .SetDisabled(FSlateRoundedBoxBrush(Color(34,42,54),5.0f))
            .SetNormalPadding(FMargin(10,6)).SetPressedPadding(FMargin(10,7,10,5));
        static const FButtonStyle Destructive=FButtonStyle(Normal)
            .SetNormal(FSlateRoundedBoxBrush(Color(98,38,47),5.0f))
            .SetHovered(FSlateRoundedBoxBrush(Color(142,47,57),5.0f))
            .SetPressed(FSlateRoundedBoxBrush(Color(169,46,59),5.0f));
        return Danger?Destructive:Normal;
    }
    static const FEditableTextBoxStyle& Input()
    {
        static const FEditableTextBoxStyle S=FEditableTextBoxStyle(FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
            .SetBackgroundImageNormal(FSlateRoundedBoxBrush(Color(11,18,28),4.0f))
            .SetBackgroundImageHovered(FSlateRoundedBoxBrush(Color(30,45,62),4.0f))
            .SetBackgroundImageFocused(FSlateRoundedBoxBrush(Color(27,63,82),4.0f))
            .SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Color(24,30,40),4.0f))
            .SetForegroundColor(Text()).SetFocusedForegroundColor(Text()).SetReadOnlyForegroundColor(Muted()).SetPadding(FMargin(8,6));
        return S;
    }
    static const FButtonStyle& HeaderButton()
    {static const FButtonStyle S=FButtonStyle(Button()).SetNormalPadding(FMargin(7,2)).SetPressedPadding(FMargin(7,3,7,1));return S;}
};
