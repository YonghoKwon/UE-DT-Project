#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"

#include "Brushes/SlateColorBrush.h"

const FLinearColor FVirtualSensorUiStyle::PanelBackground = FLinearColor::FromSRGBColor(FColor(0x11, 0x18, 0x27, 235));
const FLinearColor FVirtualSensorUiStyle::HeaderBackground = FLinearColor::FromSRGBColor(FColor(0x0B, 0x12, 0x20, 250));
const FLinearColor FVirtualSensorUiStyle::SectionBackground = FLinearColor::FromSRGBColor(FColor(0x1F, 0x29, 0x37, 225));
const FLinearColor FVirtualSensorUiStyle::ControlBackground = FLinearColor::FromSRGBColor(FColor(0x27, 0x34, 0x49));
const FLinearColor FVirtualSensorUiStyle::ControlHovered = FLinearColor::FromSRGBColor(FColor(0x33, 0x41, 0x55));
const FLinearColor FVirtualSensorUiStyle::ControlPressed = FLinearColor::FromSRGBColor(FColor(0x0E, 0x74, 0xA5));
const FLinearColor FVirtualSensorUiStyle::InputBackground = FLinearColor::FromSRGBColor(FColor(0x0F, 0x17, 0x2A));
const FLinearColor FVirtualSensorUiStyle::PrimaryText = FLinearColor::FromSRGBColor(FColor(0xF8, 0xFA, 0xFC));
const FLinearColor FVirtualSensorUiStyle::SecondaryText = FLinearColor::FromSRGBColor(FColor(0xCB, 0xD5, 0xE1));
const FLinearColor FVirtualSensorUiStyle::Accent = FLinearColor::FromSRGBColor(FColor(0x38, 0xBD, 0xF8));
const FLinearColor FVirtualSensorUiStyle::Success = FLinearColor::FromSRGBColor(FColor(0x4A, 0xDE, 0x80));
const FLinearColor FVirtualSensorUiStyle::Warning = FLinearColor::FromSRGBColor(FColor(0xFB, 0xBF, 0x24));
const FLinearColor FVirtualSensorUiStyle::Error = FLinearColor::FromSRGBColor(FColor(0xFB, 0x71, 0x85));

const FButtonStyle& FVirtualSensorUiStyle::ButtonStyle()
{
    static const FButtonStyle Style = FButtonStyle()
        .SetNormal(FSlateColorBrush(ControlBackground))
        .SetHovered(FSlateColorBrush(ControlHovered))
        .SetPressed(FSlateColorBrush(ControlPressed))
        .SetDisabled(FSlateColorBrush(FLinearColor(0.08f, 0.10f, 0.14f, 0.65f)))
        .SetNormalPadding(FMargin(8.0f, 4.0f))
        .SetPressedPadding(FMargin(8.0f, 5.0f, 8.0f, 3.0f));
    return Style;
}

const FEditableTextBoxStyle& FVirtualSensorUiStyle::EditableTextBoxStyle()
{
    static const FEditableTextBoxStyle Style = FEditableTextBoxStyle()
        .SetBackgroundImageNormal(FSlateColorBrush(InputBackground))
        .SetBackgroundImageHovered(FSlateColorBrush(ControlBackground))
        .SetBackgroundImageFocused(FSlateColorBrush(ControlPressed))
        .SetBackgroundImageReadOnly(FSlateColorBrush(FLinearColor(0.06f, 0.08f, 0.12f, 0.8f)))
        .SetForegroundColor(PrimaryText)
        .SetFocusedForegroundColor(PrimaryText)
        .SetReadOnlyForegroundColor(SecondaryText)
        .SetPadding(FMargin(6.0f, 4.0f));
    return Style;
}

float FVirtualSensorUiStyle::ContrastRatio(const FLinearColor& Foreground, const FLinearColor& Background)
{
    const auto Luminance = [](const FLinearColor& Color)
    {
        return 0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B;
    };
    const float ForegroundLuminance = Luminance(Foreground);
    const float BackgroundLuminance = Luminance(Background);
    return (FMath::Max(ForegroundLuminance, BackgroundLuminance) + 0.05f) /
        (FMath::Min(ForegroundLuminance, BackgroundLuminance) + 0.05f);
}
