#pragma once

#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Text/STextBlock.h"
#include "SensorOwnedPresentationStyle.h"

/** Observe the typed value before SSpinBox clamps it. Parsing/ranges remain Slate's. */
template<typename Number>
class TSensorToolNumericInput final : public TDefaultNumericTypeInterface<Number>
{
public:
    TSensorToolNumericInput(Number InMin,Number InMax):Min(InMin),Max(InMax) {}
    TOptional<Number> FromString(const FString& Text,const Number& Existing) override
    {
        const auto Parsed=TDefaultNumericTypeInterface<Number>::FromString(Text,Existing);
        Notice=FText::GetEmpty();
        if(Parsed.IsSet()&&(Parsed.GetValue()<Min||Parsed.GetValue()>Max))
            Notice=FText::Format(Parsed.GetValue()<Min?
                NSLOCTEXT("SensorToolNumericInput","Minimum","최소값 {0}으로 조정됨"):
                NSLOCTEXT("SensorToolNumericInput","Maximum","최대값 {0}으로 조정됨"),FText::AsNumber(Parsed.GetValue()<Min?Min:Max));
        return Parsed;
    }
    FText Notice;
private:
    Number Min,Max;
};

template<typename Number>
TSharedRef<SWidget> SensorToolNumericNotice(TSharedRef<TSensorToolNumericInput<Number>> Input,TFunction<FSlateFontInfo()> Font)
{
    return SNew(STextBlock).AutoWrapText(true).Font_Lambda(MoveTemp(Font))
        .ColorAndOpacity(FSensorOwnedPresentationStyle::Warning())
        .Text_Lambda([Input](){return Input->Notice;})
        .Visibility_Lambda([Input](){return Input->Notice.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;});
}
