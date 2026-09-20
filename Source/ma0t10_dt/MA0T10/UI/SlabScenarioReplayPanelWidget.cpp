#include "SlabScenarioReplayPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "VirtualSensorUiStyle.h"
#include "ma0t10_dt/MA0T10/Core/SlabScenarioReplaySubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "HAL/PlatformApplicationMisc.h"
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
	if(auto* M=Manager()) if(const auto Data=M->GetValidatedScenario(UUID)) { SelectedUUID=Data->ScenarioUUID; return true; }
	return false;
}
bool USlabScenarioReplayPanelWidget::ReplaySelected()
{ auto* M=Manager(); return M&&M->RequestScenarioReplayWithOutputs(SelectedUUID,GetReplayOutputs(),TargetSensorIds); }
void USlabScenarioReplayPanelWidget::SetReplayOutputs(const FVirtualSlabSensorOutputSelection& Outputs)
{ bSendPcd=Outputs.bPointCloud; bSendCameraImage=Outputs.bCameraImage; bSendLidarTelemetry=Outputs.bLidarTelemetry; }
FVirtualSlabSensorOutputSelection USlabScenarioReplayPanelWidget::GetReplayOutputs() const
{ FVirtualSlabSensorOutputSelection S;S.bPointCloud=bSendPcd;S.bCameraImage=bSendCameraImage;S.bLidarTelemetry=bSendLidarTelemetry;return S; }
bool USlabScenarioReplayPanelWidget::CopyScenarioUUID(const FString& UUID)
{
	if(auto* M=Manager()) if(const auto Data=M->GetValidatedScenario(UUID))
	{
		FPlatformApplicationMisc::ClipboardCopy(*Data->ScenarioUUID);
		ActionMessage=TEXT("시나리오 UUID를 복사했습니다."); return true;
	}
	ActionMessage=TEXT("복사할 시나리오를 찾을 수 없습니다."); return false;
}
bool USlabScenarioReplayPanelWidget::RequestScenarioDeletion(const FString& UUID)
{
	auto* M=Manager(); FString Reason;
	if(!M||!M->CanDeleteScenario(UUID,Reason)) { PendingDeleteUUID.Reset(); ActionMessage=Reason.IsEmpty()?TEXT("시나리오 관리자가 없습니다."):Reason; return false; }
	PendingDeleteUUID=M->GetValidatedScenario(UUID)->ScenarioUUID;
	ActionMessage.Reset(); return true;
}
bool USlabScenarioReplayPanelWidget::ConfirmScenarioDeletion()
{
	const FString UUID=PendingDeleteUUID; PendingDeleteUUID.Reset();
	auto* M=Manager(); FString Reason;
	if(UUID.IsEmpty()||!M||!M->DeleteScenario(UUID,Reason)) { ActionMessage=Reason.IsEmpty()?TEXT("삭제할 시나리오를 다시 선택하세요."):Reason; return false; }
	ActionMessage=TEXT("보관 목록에서 삭제했습니다. 현재 Slab와 이미 표시된 결과는 유지됩니다."); return true;
}
void USlabScenarioReplayPanelWidget::CancelScenarioDeletion() { PendingDeleteUUID.Reset(); ActionMessage=TEXT("삭제를 취소했습니다."); }
FSlateFontInfo USlabScenarioReplayPanelWidget::GetListFont(bool Secondary) const
{
	const int32 Base=IsWorkspaceOwned()?(Secondary?14:16):10;
	return FCoreStyle::GetDefaultFontStyle("Regular",CalculateScaledFontSize(Base,IsWorkspaceOwned()?GetSensorToolFontScale():1.0f));
}
bool USlabScenarioReplayPanelWidget::IsDeletionAllowed(const FString& UUID) const
{ FString Reason; auto* M=Manager(); return M&&M->CanDeleteScenario(UUID,Reason); }
FText USlabScenarioReplayPanelWidget::GetDeletionTooltip(const FString& UUID) const
{
	FString Reason; auto* M=Manager();
	return M&&M->CanDeleteScenario(UUID,Reason)?LOCTEXT("DeleteTip","메모리 보관 목록에서만 제거합니다. 이후 같은 UUID를 새로 수신하면 다시 보관할 수 있습니다."):FText::FromString(Reason);
}
FString USlabScenarioReplayPanelWidget::ResolveStableSelection(const TArray<FString>& PreviousIds,const TArray<FString>& CurrentIds,const FString& Selected)
{
	if(CurrentIds.Contains(Selected)) return Selected;
	return CurrentIds.IsEmpty()?FString():CurrentIds[0];
}
void USlabScenarioReplayPanelWidget::RefreshList()
{
	auto* M=Manager(); if(!M) return;
	const auto Entries=M->GetScenarios();
	TArray<FString> CurrentIds; CurrentIds.Reserve(Entries.Num()); for(const auto& E:Entries) CurrentIds.Add(E.UUID);
	SelectedUUID=ResolveStableSelection(PreviousListIds,CurrentIds,SelectedUUID);
	if(!Entries.ContainsByPredicate([&](const auto& E){return E.UUID==PendingDeleteUUID;})) PendingDeleteUUID.Reset();
	PreviousListIds=MoveTemp(CurrentIds);
	if(!List.IsValid()) return; List->ClearChildren();
	if(Entries.IsEmpty()) List->AddSlot().AutoHeight().Padding(4)[SNew(STextBlock).Font_Lambda([this](){return GetListFont();}).ColorAndOpacity_Lambda([this](){return GetToolMutedColor();}).AutoWrapText(true).Text(LOCTEXT("Empty","보관된 시나리오가 없습니다. 시나리오 전문을 수신하면 목록에 표시됩니다."))];
	for(const auto& E:Entries)
	{
		// Dynamic rows use font attributes, not permanent base font callbacks. Refresh never accumulates dead controls.
		const FString Id=E.UUID;
		List->AddSlot().AutoHeight().Padding(0,4)
		[SNew(SBorder).BorderImage_Lambda([this](){return GetToolSectionBrush();}).BorderBackgroundColor_Lambda([this](){return GetToolSectionColor();}).Padding(8)
		[SNew(SVerticalBox)
		 +SVerticalBox::Slot().AutoHeight()
		 [SNew(SButton).ButtonStyle(&GetToolButtonStyle()).OnClicked_Lambda([this,Id](){SelectScenario(Id);return FReply::Handled();})
		  [SNew(STextBlock).Font_Lambda([this](){return GetListFont();}).AutoWrapText(true)
		   .ColorAndOpacity_Lambda([this,Id](){return FSlateColor(Id==SelectedUUID?GetToolAccentColor():GetToolTextColor());})
		   .Text(FText::FromString(FString::Printf(TEXT("%s\n%s · %d행"),*E.Name,*E.MaterialSummary,E.RowCount)))]]
		 +SVerticalBox::Slot().AutoHeight().Padding(0,5)
		 [SNew(STextBlock).Font_Lambda([this](){return GetListFont(true);}).AutoWrapText(true).ColorAndOpacity_Lambda([this](){return GetToolMutedColor();})
		  .Text(FText::FromString(FString::Printf(TEXT("%s: %s"),E.bGeneratedArchiveId?TEXT("내부 보관 ID"):TEXT("UUID"),*Id)))]
		 +SVerticalBox::Slot().AutoHeight()
		 [SNew(SHorizontalBox)
		  +SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
		  [SNew(SButton).ButtonStyle(&GetToolButtonStyle()).OnClicked_Lambda([this,Id](){CopyScenarioUUID(Id);return FReply::Handled();})
		   [SNew(STextBlock).Font_Lambda([this](){return GetListFont(true);}).ColorAndOpacity_Lambda([this](){return GetToolTextColor();}).Text(LOCTEXT("CopyUUID","UUID 복사"))]]
		  +SHorizontalBox::Slot().AutoWidth()
		  [SNew(SButton).ButtonStyle(&GetToolButtonStyle(true)).IsEnabled_Lambda([this,Id](){return IsDeletionAllowed(Id);})
		   .ToolTipText_Lambda([this,Id](){return GetDeletionTooltip(Id);})
		   .OnClicked_Lambda([this,Id](){RequestScenarioDeletion(Id);return FReply::Handled();})
		   [SNew(STextBlock).Font_Lambda([this](){return GetListFont(true);}).ColorAndOpacity_Lambda([this](){return GetToolTextColor();}).Text(LOCTEXT("Delete","삭제"))]]]
		 +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
		 [SNew(SVerticalBox).Visibility_Lambda([this,Id](){return PendingDeleteUUID==Id?EVisibility::Visible:EVisibility::Collapsed;})
		  +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font_Lambda([this](){return GetListFont(true);}).AutoWrapText(true).ColorAndOpacity_Lambda([this](){return GetToolTextColor();}).Text(LOCTEXT("DeleteQuestion","이 항목을 보관 목록에서 삭제할까요?"))]
		  +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
		   +SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)[SNew(SButton).ButtonStyle(&GetToolButtonStyle(true)).IsEnabled_Lambda([this,Id](){return IsDeletionAllowed(Id);}).OnClicked_Lambda([this](){ConfirmScenarioDeletion();return FReply::Handled();})[SNew(STextBlock).Font_Lambda([this](){return GetListFont(true);}).ColorAndOpacity_Lambda([this](){return GetToolTextColor();}).Text(LOCTEXT("ConfirmDelete","삭제 확인"))]]
		   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&GetToolButtonStyle()).OnClicked_Lambda([this](){CancelScenarioDeletion();return FReply::Handled();})[SNew(STextBlock).Font_Lambda([this](){return GetListFont(true);}).ColorAndOpacity_Lambda([this](){return GetToolTextColor();}).Text(LOCTEXT("CancelDelete","취소"))]]]
		 ]]];
	}
}
TSharedRef<SWidget> USlabScenarioReplayPanelWidget::RebuildWidget()
{
    if(WidgetTree&&WidgetTree->RootWidget)return Super::RebuildWidget();
    auto Result=SNew(SBorder).BorderImage_Lambda([this](){return GetToolPanelBrush();}).BorderBackgroundColor_Lambda([this](){return GetToolPanelColor();}).ForegroundColor_Lambda([this](){return GetToolTextColor();}).Padding(12)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("ReplayTitle","시나리오"))]
     +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).AutoWrapText(true).ColorAndOpacity_Lambda([this](){return GetToolAccentColor();}).Text_Lambda([this](){auto* M=Manager();return FText::FromString(M?M->GetRegistrationMessage():TEXT("연결 대기"));})]
     +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).Visibility_Lambda([this](){return ActionMessage.IsEmpty()?EVisibility::Collapsed:GetPanelBodyVisibility();}).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(ActionMessage);})]
     +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})+SScrollBox::Slot()[SAssignNew(List,SVerticalBox)]]
     +SVerticalBox::Slot().AutoHeight().Padding(0,8)
     [SNew(SExpandableArea).InitiallyCollapsed(true).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
      .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Details","선택 항목 상세"))]
      .BodyContent()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){auto* M=Manager();if(M)for(const auto& E:M->GetScenarios())if(E.UUID==SelectedUUID)return FText::FromString(FString::Printf(TEXT("%s: %s\n수신: %s\n마지막 데이터: %.2f초"),E.bGeneratedArchiveId?TEXT("내부 보관 ID (원본 UUID 없음)"):TEXT("원본 UUID"),*E.UUID,*E.ReceivedUtc.ToIso8601(),E.LastElapsedSec));return LOCTEXT("Select","항목을 선택하세요.");})]]
     +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).IsChecked_Lambda([this](){return bSendPcd?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSendPcd=S==ECheckBoxState::Checked;})[SNewSensorTool(STextBlock).Text(LOCTEXT("Send","재생 중 PCD 송신")).ToolTipText(LOCTEXT("SendTip","다음 재생부터 적용됩니다. 기본은 관찰 전용입니다."))]]
     +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).IsChecked_Lambda([this](){return bSendCameraImage?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSendCameraImage=S==ECheckBoxState::Checked;})[SNewSensorTool(STextBlock).Text(LOCTEXT("SendCamera","Camera 이미지 Topic 송신")).ToolTipText(LOCTEXT("CameraTip","다음 재생부터 적용됩니다. 로컬 이미지 캡처 파일을 자동 저장하지 않습니다."))]]
     +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).IsChecked_Lambda([this](){return bSendLidarTelemetry?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSendLidarTelemetry=S==ECheckBoxState::Checked;})[SNewSensorTool(STextBlock).Text(LOCTEXT("SendLidar","LiDAR 정보 Topic 송신")).ToolTipText(LOCTEXT("LidarTip","다음 재생부터 적용됩니다. 고성능 경로에서는 전체 점 배열 대신 telemetry를 보냅니다."))]]
     +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).Text(LOCTEXT("Play","처음부터 재생")).IsEnabled_Lambda([this](){auto* M=Manager();return M&&M->CanReplay()&&!SelectedUUID.IsEmpty();}).OnClicked_Lambda([this](){ReplaySelected();return FReply::Handled();})]
     +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).AutoWrapText(true).Text_Lambda([this](){auto* M=Manager();if(!M)return LOCTEXT("Missing","관리자 없음");const auto S=M->GetReplayStatus();return FText::FromString(S.Message.IsEmpty()?(M->CanReplay()?TEXT("재생 준비됨 · 보관 시나리오를 선택하세요."):TEXT("현재 실행 종료와 재생 adapter 연결을 확인하세요.")):S.Message);})]
    ]; RefreshList();return Result;
}
#undef LOCTEXT_NAMESPACE
