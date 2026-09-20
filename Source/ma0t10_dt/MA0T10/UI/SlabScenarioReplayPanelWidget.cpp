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
{ auto* M=Manager(); return M&&M->RequestScenarioReplayWithOutputs(SelectedUUID,GetReplayOutputs(),TargetSensorIds); }
void USlabScenarioReplayPanelWidget::SetReplayOutputs(const FVirtualSlabSensorOutputSelection& Outputs)
{ bSendPcd=Outputs.bPointCloud; bSendCameraImage=Outputs.bCameraImage; bSendLidarTelemetry=Outputs.bLidarTelemetry; }
FVirtualSlabSensorOutputSelection USlabScenarioReplayPanelWidget::GetReplayOutputs() const
{ FVirtualSlabSensorOutputSelection S;S.bPointCloud=bSendPcd;S.bCameraImage=bSendCameraImage;S.bLidarTelemetry=bSendLidarTelemetry;return S; }
void USlabScenarioReplayPanelWidget::RefreshList()
{
	if(!List.IsValid()) return; List->ClearChildren(); auto* M=Manager(); if(!M) return;
	const auto Entries=M->GetScenarios();
	if(!Entries.ContainsByPredicate([&](const auto& E){return E.UUID==SelectedUUID;})) SelectedUUID=Entries.IsEmpty()?FString():Entries[0].UUID;
	for(const auto& E:Entries)
	{
		List->AddSlot().AutoHeight().Padding(0,3)
		[SNewSensorTool(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
		.OnClicked_Lambda([this,Id=E.UUID](){SelectScenario(Id);return FReply::Handled();})
		[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity_Lambda([this,Id=E.UUID](){return FSlateColor(Id==SelectedUUID?FVirtualSensorUiStyle::Accent:FVirtualSensorUiStyle::PrimaryText);})
		.Text(FText::FromString(FString::Printf(TEXT("%s\n%s · %d행"),*E.Name,*E.MaterialSummary,E.RowCount)))]];
	}
}
TSharedRef<SWidget> USlabScenarioReplayPanelWidget::RebuildWidget()
{
    if(WidgetTree&&WidgetTree->RootWidget)return Super::RebuildWidget();
    auto Result=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FVirtualSensorUiStyle::PanelBackground).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Padding(12)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("ReplayTitle","시나리오"))]
     +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).AutoWrapText(true).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text_Lambda([this](){auto* M=Manager();return FText::FromString(M?M->GetRegistrationMessage():TEXT("연결 대기"));})]
     +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})+SScrollBox::Slot()[SAssignNew(List,SVerticalBox)]]
     +SVerticalBox::Slot().AutoHeight().Padding(0,8)
     [SNew(SExpandableArea).InitiallyCollapsed(true).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
      .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Details","선택 항목 상세"))]
      .BodyContent()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){auto* M=Manager();if(M)for(const auto& E:M->GetScenarios())if(E.UUID==SelectedUUID)return FText::FromString(FString::Printf(TEXT("%s: %s\n수신: %s\n마지막 데이터: %.2f초"),E.bGeneratedArchiveId?TEXT("내부 보관 ID (원본 UUID 없음)"):TEXT("원본 UUID"),*E.UUID,*E.ReceivedUtc.ToIso8601(),E.LastElapsedSec));return LOCTEXT("Select","항목을 선택하세요.");})]]
     +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).IsChecked_Lambda([this](){return bSendPcd?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSendPcd=S==ECheckBoxState::Checked;})[SNewSensorTool(STextBlock).Text(LOCTEXT("Send","재생 중 PCD 송신")).ToolTipText(LOCTEXT("SendTip","다음 재생부터 적용됩니다. 기본은 관찰 전용입니다."))]]
     +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).IsChecked_Lambda([this](){return bSendCameraImage?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSendCameraImage=S==ECheckBoxState::Checked;})[SNewSensorTool(STextBlock).Text(LOCTEXT("SendCamera","Camera 이미지 Topic 송신")).ToolTipText(LOCTEXT("CameraTip","다음 재생부터 적용됩니다. 로컬 이미지 캡처 파일을 자동 저장하지 않습니다."))]]
     +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).IsChecked_Lambda([this](){return bSendLidarTelemetry?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bSendLidarTelemetry=S==ECheckBoxState::Checked;})[SNewSensorTool(STextBlock).Text(LOCTEXT("SendLidar","LiDAR 정보 Topic 송신")).ToolTipText(LOCTEXT("LidarTip","다음 재생부터 적용됩니다. 고성능 경로에서는 전체 점 배열 대신 telemetry를 보냅니다."))]]
     +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).Text(LOCTEXT("Play","처음부터 재생")).IsEnabled_Lambda([this](){auto* M=Manager();return M&&M->CanReplay()&&!SelectedUUID.IsEmpty();}).OnClicked_Lambda([this](){ReplaySelected();return FReply::Handled();})]
     +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).AutoWrapText(true).Text_Lambda([this](){auto* M=Manager();if(!M)return LOCTEXT("Missing","관리자 없음");const auto S=M->GetReplayStatus();return FText::FromString(S.Message.IsEmpty()?(M->CanReplay()?TEXT("재생 준비됨 · 보관 시나리오를 선택하세요."):TEXT("현재 실행 종료와 재생 adapter 연결을 확인하세요.")):S.Message);})]
    ]; RefreshList();return Result;
}
#undef LOCTEXT_NAMESPACE
