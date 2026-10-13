#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Widgets/Text/STextBlock.h"
#include "Types/SlateAttributeMetaData.h"

namespace SensorPresentationTests
{
    inline void VisibleText(const TSharedRef<SWidget>& Widget,TArray<FString>& Text)
    {
        FSlateAttributeMetaData::UpdateOnlyVisibilityAttributes(Widget.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
        if(!Widget->GetVisibility().IsVisible())return;
        FSlateAttributeMetaData::UpdateExceptVisibilityAttributes(Widget.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
        if(Widget->GetType()==TEXT("STextBlock"))Text.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
        auto* Children=Widget->GetChildren();
        for(int32 I=0;I<Children->Num();++I)VisibleText(Children->GetChildAt(I),Text);
    }
    inline TArray<FString> ForbiddenWords()
    {
        return {TEXT("Payload"),TEXT("Topic"),TEXT("receipt"),TEXT("Raw TCP"),TEXT("coherent"),TEXT("revision"),TEXT("generation"),TEXT("p95"),TEXT("FIFO"),TEXT("e2e"),TEXT("deadline"),TEXT("wildcard"),TEXT("backend"),TEXT("SensorId"),TEXT("Capture/Export"),TEXT("PIE")};
    }
}
#endif
