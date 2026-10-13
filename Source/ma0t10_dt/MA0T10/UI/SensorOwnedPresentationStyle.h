#pragma once
#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

/** Opt-in presentation tokens. Never installs or mutates a global Slate/DTCore style. */
struct FSensorOwnedPresentationStyle
{
    static FLinearColor Color(uint8 R,uint8 G,uint8 B,uint8 A=255){return FLinearColor::FromSRGBColor(FColor(R,G,B,A));}
    // UXR-09 A: all values are sRGB design tokens, scoped to explicitly owned native UI.
    static constexpr float RailWidth=72, SidebarWidth=372, WideSidebarWidth=560, StatusHeight=36;
    static constexpr float Gap=8, SectionGap=16, PanelPadding=16, ControlHeight=32, ProgressHeight=4;
    static constexpr int32 TitleSize=16, BodySize=13, LabelSize=11;
    static FSlateFontInfo Font(int32 Size,float Scale=1.f,bool Bold=false)
    {return FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",FMath::Max(8,FMath::RoundToInt(Size*Scale)));}
    static FLinearColor Panel(){return Color(20,23,27);}
    static FLinearColor Header(){return Panel();}
    static FLinearColor Section(){return Color(26,30,35);}
    static FLinearColor Border(){return Color(38,43,50);}
    static FLinearColor Selection(){return Color(38,48,61);} // Accent at 15% over Panel, sRGB.
    static FLinearColor Text(){return Color(230,233,237);}
    static FLinearColor Muted(){return Color(140,150,161);}
    static FLinearColor Accent(){return Color(141,187,255);}
    static FLinearColor Primary(){return Color(61,127,224);}
    static FLinearColor Success(){return Color(63,185,127);}
    static FLinearColor Warning(){return Color(242,194,107);}
    static FLinearColor WarningSurface(){return Color(58,46,23);}
    static FLinearColor Danger(){return Color(229,83,75);}
    static const FSlateBrush* PanelBrush(){static FSlateRoundedBoxBrush B(FLinearColor::White,8.0f);return &B;}
    static const FSlateBrush* SectionBrush(){static FSlateRoundedBoxBrush B(FLinearColor::White,10.0f);return &B;}
    static const FSlateBrush* ChipBrush(){static FSlateRoundedBoxBrush B(FLinearColor::White,4.f);return &B;}
    static const FButtonStyle& Button(bool Danger=false)
    {
        static const FButtonStyle Normal=FButtonStyle()
            .SetNormal(FSlateRoundedBoxBrush(Section(),8.0f,Border(),1.f))
            .SetHovered(FSlateRoundedBoxBrush(Color(39,48,61),8.0f,Accent(),1.f))
            .SetPressed(FSlateRoundedBoxBrush(Selection(),8.0f,Primary(),1.f))
            .SetDisabled(FSlateRoundedBoxBrush(Section(),8.0f,Border(),1.f))
            .SetNormalForeground(Text()).SetHoveredForeground(Text()).SetPressedForeground(Text())
            .SetDisabledForeground(Muted())
            .SetNormalPadding(FMargin(Gap,6)).SetPressedPadding(FMargin(Gap,7,Gap,5));
        static const FButtonStyle Destructive=FButtonStyle(Normal)
            .SetNormal(FSlateRoundedBoxBrush(Color(62,29,30),8.0f,FSensorOwnedPresentationStyle::Danger(),1.f))
            .SetHovered(FSlateRoundedBoxBrush(Color(108,38,40),8.0f,FSensorOwnedPresentationStyle::Danger(),1.f))
            .SetPressed(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Danger(),8.0f));
        return Danger?Destructive:Normal;
    }
    static const FButtonStyle& PrimaryButton()
    {static const FButtonStyle S=FButtonStyle(Button()).SetNormal(FSlateRoundedBoxBrush(Primary(),8.f)).SetHovered(FSlateRoundedBoxBrush(Color(79,145,239),8.f)).SetNormalForeground(FLinearColor::White);return S;}
    static const FButtonStyle& QuietButton()
    {static const FButtonStyle S=FButtonStyle(Button()).SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent,8.f)).SetHovered(FSlateRoundedBoxBrush(Selection(),8.f));return S;}
    static const FButtonStyle& RailButton()
    {static const FButtonStyle S=FButtonStyle(QuietButton()).SetNormal(FSlateRoundedBoxBrush(FLinearColor::White,10.f));return S;}
    static const FCheckBoxStyle& Segment()
    {static const FCheckBoxStyle S=FCheckBoxStyle(FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox"))
        .SetUncheckedImage(FSlateRoundedBoxBrush(Color(18,21,26),6.f))
        .SetUncheckedHoveredImage(FSlateRoundedBoxBrush(Section(),6.f))
        .SetCheckedImage(FSlateRoundedBoxBrush(Selection(),6.f,Primary(),1.f))
        .SetCheckedHoveredImage(FSlateRoundedBoxBrush(Selection(),6.f,Accent(),1.f))
        .SetUncheckedPressedImage(FSlateRoundedBoxBrush(Section(),6.f))
        .SetCheckedPressedImage(FSlateRoundedBoxBrush(Selection(),6.f,Accent(),1.f)).SetPadding(FMargin(Gap,6));return S;}
    static const FCheckBoxStyle& Switch()
    {static const FCheckBoxStyle S=FCheckBoxStyle(Segment()).SetUncheckedImage(FSlateRoundedBoxBrush(Panel(),16.f,Border(),1.f))
        .SetUncheckedHoveredImage(FSlateRoundedBoxBrush(Panel(),16.f,Accent(),1.f))
        .SetCheckedImage(FSlateRoundedBoxBrush(Selection(),16.f,Accent(),1.f)).SetCheckedHoveredImage(FSlateRoundedBoxBrush(Selection(),16.f,Accent(),1.f));return S;}
    static const FProgressBarStyle& Progress()
    {static const FProgressBarStyle S=FProgressBarStyle().SetBackgroundImage(FSlateRoundedBoxBrush(Border(),2.f))
        .SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White,2.f));return S;}
    static const FEditableTextBoxStyle& Input()
    {
        static const FEditableTextBoxStyle S=FEditableTextBoxStyle(FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
            .SetBackgroundImageNormal(FSlateRoundedBoxBrush(Color(18,21,26),8.0f,Border(),1.f))
            .SetBackgroundImageHovered(FSlateRoundedBoxBrush(Section(),8.0f,Accent(),1.f))
            .SetBackgroundImageFocused(FSlateRoundedBoxBrush(Selection(),8.0f,Primary(),1.f))
            .SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Section(),8.0f))
            .SetForegroundColor(Text()).SetFocusedForegroundColor(Text()).SetReadOnlyForegroundColor(Muted()).SetPadding(FMargin(8,6));
        return S;
    }
    static const FButtonStyle& HeaderButton()
    {static const FButtonStyle S=FButtonStyle(QuietButton()).SetNormalPadding(FMargin(4)).SetPressedPadding(FMargin(4));return S;}
    static const FComboButtonStyle& ComboButton()
    {
        static const FComboButtonStyle S=FComboButtonStyle(FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
            .SetButtonStyle(FButtonStyle(Button()).SetNormal(FSlateRoundedBoxBrush(Color(18,21,26),8.f,Border(),1.f)).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0)))
            .SetContentPadding(FMargin(Gap,6))
            .SetMenuBorderBrush(FSlateRoundedBoxBrush(Panel(),6.0f)).SetMenuBorderPadding(FMargin(Gap));
        return S;
    }
    static const FComboBoxStyle& ComboBox()
    {static const FComboBoxStyle S=FComboBoxStyle(FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox")).SetComboButtonStyle(ComboButton()).SetContentPadding(FMargin(Gap,6));return S;}
    static const FSpinBoxStyle& SpinBox()
    {static const FSpinBoxStyle S=FSpinBoxStyle(FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
        .SetBackgroundBrush(FSlateRoundedBoxBrush(Color(18,21,26),8.f,Border(),1.f))
        .SetActiveBackgroundBrush(FSlateRoundedBoxBrush(Selection(),8.f,Primary(),1.f))
        .SetHoveredBackgroundBrush(FSlateRoundedBoxBrush(Section(),8.f,Accent(),1.f))
        .SetActiveFillBrush(FSlateRoundedBoxBrush(Selection(),8.f)).SetInactiveFillBrush(FSlateRoundedBoxBrush(Color(18,21,26),8.f))
        .SetHoveredFillBrush(FSlateRoundedBoxBrush(Section(),8.f))
        .SetForegroundColor(Text()).SetTextPadding(FMargin(Gap,6));return S;}
};
