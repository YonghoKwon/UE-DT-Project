#include "SlabScenarioReplayPanelWidget.h"
#include "VirtualSensorUiStyle.h"
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SlabScenarioReplayPanel"
USlabScenarioReplaySubsystem* USlabScenarioReplayPanelWidget::Manager() const
{ return GetGameInstance()?GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>():nullptr; }
void USlabScenarioReplayPanelWidget::NativeConstruct()
{
	Super::NativeConstruct(); if(auto* M=Manager()) M->OnScenariosChanged.AddUniqueDynamic(this,&ThisClass::RefreshList); RefreshList();
}
void USlabScenarioReplayPanelWidget::NativeDestruct()
{ if(auto* M=Manager()) M->OnScenariosChanged.RemoveDynamic(this,&ThisClass::RefreshList); Super::NativeDestruct(); }
void USlabScenarioReplayPanelWidget::ReleaseSlateResources(bool Children)
{ Super::ReleaseSlateResources(Children); List.Reset(); }
bool USlabScenarioReplayPanelWidget::SelectScenario(const FString& UUID)
{
	if(auto* M=Manager()) for(const auto& E:M->GetScenarios()) if(E.UUID==UUID) { SelectedUUID=UUID; return true; }
	return false;
}
bool USlabScenarioReplayPanelWidget::ReplaySelected()
{ auto* M=Manager(); return M&&M->RequestScenarioReplay(SelectedUUID,bSendPcd,TargetSensorIds); }
void USlabScenarioReplayPanelWidget::RefreshList()
{
	if(!List.IsValid()) return; List->ClearChildren(); auto* M=Manager(); if(!M) return;
	const auto Entries=M->GetScenarios();
	if(!Entries.ContainsByPredicate([&](const auto& E){return E.UUID==SelectedUUID;})) SelectedUUID=Entries.IsEmpty()?FString():Entries[0].UUID;
	for(const auto& E:Entries)
	{
		List->AddSlot().AutoHeight().Padding(0,3)
		[SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
		.OnClicked_Lambda([this,Id=E.UUID](){SelectScenario(Id);return FReply::Handled();})
		[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity_Lambda([this,Id=E.UUID](){return FSlateColor(Id==SelectedUUID?FVirtualSensorUiStyle::Accent:FVirtualSensorUiStyle::PrimaryText);})
		.Text(FText::FromString(FString::Printf(TEXT("%s\nUUID: %s\n소재: %s · %d행 · 마지막 데이터 %.2f초\n수신: %s"),*E.Name,*E.UUID,*E.MaterialSummary,E.RowCount,E.LastElapsedSec,*E.ReceivedUtc.ToIso8601())))]];
	}
}
TSharedRef<SWidget> USlabScenarioReplayPanelWidget::RebuildWidget()
{
	if(WidgetTree&&WidgetTree->RootWidget) return Super::RebuildWidget();
	auto Result=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FVirtualSensorUiStyle::PanelBackground).Padding(10)
	[SNew(SVerticalBox)
	+SVerticalBox::Slot().AutoHeight()
	[SNew(SHorizontalBox)
	 +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Title","Slab 시나리오 재생 | 제목 드래그"))]
	 +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this](){return IsPanelCollapsed()?LOCTEXT("Expand","펼치기"):LOCTEXT("Collapse","접기");}).OnClicked_Lambda([this](){TogglePanelCollapsed();return FReply::Handled();})]
	 +SHorizontalBox::Slot().AutoWidth().Padding(3,0)[SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("Reset","배치 초기화")).OnClicked_Lambda([this](){ResetPanelPosition();ResetPanelSize();return FReply::Handled();})]]
	+SVerticalBox::Slot().FillHeight(1).Padding(0,8)
	[SNew(SScrollBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
	 +SScrollBox::Slot()[SNew(SVerticalBox)
	  +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).Text(LOCTEXT("Memory","최근 10개 · 이번 실행 중에만 보관 · 재생 중 자유 시점 관찰 가능"))]
	  +SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text_Lambda([this](){auto* M=Manager();return FText::FromString(M?M->GetRegistrationMessage():TEXT("관리자 연결 대기"));})]
	  +SVerticalBox::Slot().AutoHeight()[SAssignNew(List,SVerticalBox)]
	  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SCheckBox).IsChecked_Lambda([this](){return bSendPcd?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState V){bSendPcd=V==ECheckBoxState::Checked;})[SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Send","재생 중 PCD 송신 (기본 꺼짐)"))]]
	  +SVerticalBox::Slot().AutoHeight()[SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("Play","선택한 시나리오 처음부터 재생")).IsEnabled_Lambda([this](){auto* M=Manager();return M&&M->CanReplay()&&!SelectedUUID.IsEmpty();}).OnClicked_Lambda([this](){ReplaySelected();return FReply::Handled();})]
	  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this](){auto* M=Manager();if(!M)return LOCTEXT("NoManager","관리자 없음");const auto S=M->GetReplayStatus();return FText::FromString(S.Message.IsEmpty()?TEXT("대기 · 재생 adapter가 연결되어야 시작할 수 있습니다."):S.Message+TEXT("\n실행 UUID: ")+S.RunUUID);})]
	 ]]
	+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).Justification(ETextJustify::Right).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).Text(LOCTEXT("Grip","우하단 드래그로 크기 조절 ◢"))]
	];
	RefreshList(); return Result;
}
#undef LOCTEXT_NAMESPACE
