#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/Tests/SensorPresentationTestHelpers.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"

namespace Uxr15ToggleTests
{
    inline void Walk(const TSharedRef<SWidget>& Root,TFunctionRef<void(const TSharedRef<SWidget>&)> Visit)
    {Visit(Root);auto* Children=Root->GetChildren();for(int32 I=0;I<Children->Num();++I)Walk(Children->GetChildAt(I),Visit);}
    inline TSharedPtr<SCheckBox> Check(const TSharedRef<SWidget>& Root,const FString& Prefix,FAutomationTestBase* Test)
    {
        TArray<TSharedPtr<SCheckBox>> Matches;
        Walk(Root,[&](const TSharedRef<SWidget>& Item){if(Item->GetType()!=TEXT("SCheckBox"))return;TArray<FString> Text;SensorPresentationTests::VisibleText(Item,Text);for(const auto& T:Text)if(T.StartsWith(Prefix)){Matches.Add(StaticCastSharedRef<SCheckBox>(Item));break;}});
        Test->TestEqual(*(TEXT("unique real Slate checkbox: ")+Prefix),Matches.Num(),1);return Matches.Num()==1?Matches[0]:nullptr;
    }
    inline bool Toggle(const TSharedRef<SWidget>& Root,const FString& Prefix,FAutomationTestBase* Test)
    {auto Item=Check(Root,Prefix,Test);if(!Item)return false;Item->ToggleCheckedState();return true;}
    inline bool Click(const TSharedRef<SWidget>& Root,const FString& Label,FAutomationTestBase* Test)
    {
        TArray<TSharedPtr<SButton>> Matches;
        Walk(Root,[&](const TSharedRef<SWidget>& Item){if(Item->GetType()!=TEXT("SButton"))return;TArray<FString> Text;SensorPresentationTests::VisibleText(Item,Text);if(Text.Contains(Label))Matches.Add(StaticCastSharedRef<SButton>(Item));});
        if(!Test->TestEqual(*(TEXT("unique real Slate button: ")+Label),Matches.Num(),1))return false;
        // Keyboard activation invokes the widget's bound OnClicked; no direct service call.
        const FKeyEvent Key(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
        Matches[0]->OnKeyDown(FGeometry(),Key);Matches[0]->OnKeyUp(FGeometry(),Key);return true;
    }
}
#endif
