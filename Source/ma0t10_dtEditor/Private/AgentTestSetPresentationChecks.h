#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/AgentTestSetWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/FileHelper.h"
#include "Json.h"

/** Inspect the actual visible owned Slate tree, including ancestor visibility. */
inline TArray<FString> AgentTestSetVisibleText(UAgentTestSetWidget* Widget)
{
    TArray<FString> Texts;TSet<const SWidget*> Seen;
    TFunction<void(TSharedRef<SWidget>)> Visit=[&](TSharedRef<SWidget> W)
    {
        if(Seen.Contains(&W.Get())||!W->GetVisibility().IsVisible())return;Seen.Add(&W.Get());
        if(W->GetType()==FName(TEXT("STextBlock")))Texts.Add(StaticCastSharedRef<STextBlock>(W)->GetText().ToString());
        if(auto* Children=W->GetChildren())for(int32 I=0;I<Children->Num();++I)Visit(Children->GetChildAt(I));
    };
    if(Widget)Visit(Widget->TakeWidget());return Texts;
}
inline bool AgentTestSetHasPauseControl(const TArray<FString>& Texts)
{return Texts.Contains(TEXT("일시정지"))||Texts.Contains(TEXT("재개"));}
inline void CheckAgentTestSetPresentation(FAutomationTestBase* Test,UWorld* World,bool Complete,const FString& Path)
{
    auto* Service=World->GetSubsystem<UAgentTestSetSubsystem>();auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    const auto Texts=AgentTestSetVisibleText(Workspace->GetTestSetWidget());auto* Toolbar=Cast<USensorToolToolbarWidget>(Workspace->GetOwnedNavigation());
    Test->TestNotNull(TEXT("owned status toolbar exists"),Toolbar);if(!Toolbar)return;
    const FString Chip=Toolbar->GetGlobalStatus().TestSet.ToString();
    const FString Expected=FString::Printf(TEXT("테스트셋 %s %d/%d"),Complete?TEXT("완료"):Service->IsPaused()?TEXT("일시정지"):Service->HasBusyWork()?TEXT("대기"):TEXT("진행"),Service->Completed(),Service->Settings().Target);
    // A running counter can advance between the toolbar's 5Hz sample and this read.
    Test->TestTrue(TEXT("status chip reflects completion state"),Complete?Chip==Expected:!Chip.IsEmpty()&&!Chip.Contains(TEXT("완료")));
    Test->TestEqual(TEXT("pause/resume is visible only before completion"),AgentTestSetHasPauseControl(Texts),!Complete);
    for(const auto& Text:Texts)Test->TestFalse(TEXT("duplicate diagnostic never replaces visible progress"),Text.Contains(TEXT("기존 테스트셋 유지"))||Text.Contains(TEXT("새로 생성하려면")));
    const FString Total=FString::Printf(TEXT("총 %.1f초"),Service->TotalSeconds());
    if(Complete)
    {
        Test->TestTrue(TEXT("completion displays frozen total wall time"),Texts.Contains(Total));
        for(const auto& Text:Texts)Test->TestFalse(TEXT("completion hides rate and remaining-time text"),Text.Contains(TEXT("개/초"))||Text.Contains(TEXT("남은 시간")));
    }
    else Test->TestTrue(TEXT("running presentation retains rate/remaining time"),Texts.ContainsByPredicate([](const auto& Text){return Text.Contains(TEXT("남은 시간"));}));
    auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Values;for(const auto& Text:Texts)Values.Add(MakeShared<FJsonValueString>(Text));
    O->SetArrayField(TEXT("visible_texts"),Values);O->SetStringField(TEXT("status_chip"),Chip);O->SetNumberField(TEXT("total_seconds"),Service->TotalSeconds());O->SetBoolField(TEXT("complete"),Complete);O->SetBoolField(TEXT("pause_visible"),AgentTestSetHasPauseControl(Texts));
    FString Json;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
#endif
