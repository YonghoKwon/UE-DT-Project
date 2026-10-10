#include "VirtualSensorPlacementPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SensorPlacementPanel"
UVirtualSensorLayoutSubsystem* UVirtualSensorPlacementPanelWidget::Layout() const {return GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorLayoutSubsystem>():nullptr;}
AVirtualSensorActorBase* UVirtualSensorPlacementPanelWidget::Selected() const {const auto* L=Layout();return L&&L->Coordinator()?L->Coordinator()->GetSelectedSensorActor():nullptr;}
void UVirtualSensorPlacementPanelWidget::BindPlacementControls(UVirtualSensorSettingsPanelWidget* In){Controls=In;}
void UVirtualSensorPlacementPanelWidget::EndPlacement(){if(Controls)Controls->SetSensorManipulationEnabled(false);}
void UVirtualSensorPlacementPanelWidget::NativeDestruct(){CancelLayoutOperation();EndPlacement();Super::NativeDestruct();}
void UVirtualSensorPlacementPanelWidget::CancelLayoutOperation()
{if(auto* L=Layout())L->CancelLayoutRestorePreview(OperationId);OperationId.Reset();}
void UVirtualSensorPlacementPanelWidget::SelectLayoutForRestore(const FString& Name)
{if(SelectedLayout==Name)return;CancelLayoutOperation();SelectedLayout=Name;Status=TEXT("선택 배치: ")+Name+TEXT(" · 변경 미리보기 후 적용하세요.");}
FString UVirtualSensorPlacementPanelWidget::PreviewSelectedLayoutRestore()
{CancelLayoutOperation();if(auto* L=Layout()){OperationId=L->PreviewLayoutRestore(SelectedLayout);Status=L->GetOperationStatus(OperationId).Message;}return OperationId;}
bool UVirtualSensorPlacementPanelWidget::ApplySelectedLayoutRestore()
{auto* L=Layout();if(!L)return false;const bool Ok=L->ApplyLayoutRestore(OperationId,Status);if(Ok)Status=L->GetOperationStatus(OperationId).Message;CancelLayoutOperation();return Ok;}
bool UVirtualSensorPlacementPanelWidget::AddSensor(EVirtualSensorKind Kind)
{auto* L=Layout();if(!L)return false;CancelLayoutOperation();EndPlacement();const bool Ok=L->CreateSensorAtView(Kind,Status)!=nullptr;if(Ok){Status=TEXT("장비 추가 완료 · 미리보기 실행 중 · PCD 송신은 별도");if(GetToolWorkspace())GetToolWorkspace()->SynchronizeOwnedSelection();if(Controls)Controls->SetSensorManipulationEnabled(true);}RefreshInventory();return Ok;}
bool UVirtualSensorPlacementPanelWidget::DeleteSelectedSensor()
{auto* L=Layout();auto* S=Selected();if(!L||!S)return false;CancelLayoutOperation();if(PendingDelete.Get()!=S){if(!L->CanDeleteSensor(S,Status))return false;PendingDelete=S;Status=TEXT("선택 장비를 삭제하려면 ‘삭제 확인’을 누르세요.");return false;}EndPlacement();const bool Ok=L->DeleteSensor(S,Status);PendingDelete.Reset();if(Ok)Status=TEXT("도구 장비 삭제 완료");RefreshInventory();return Ok;}
FText UVirtualSensorPlacementPanelWidget::SelectedText() const
{auto* S=Selected();return FText::FromString(S?(S->GetSensorKind()==EVirtualSensorKind::Camera?TEXT("Camera · "):TEXT("LiDAR · "))+S->GetSensorId():TEXT("장비 없음 · 위에서 추가하세요"));}
void UVirtualSensorPlacementPanelWidget::RefreshInventory()
{
    auto* L=Layout();if(!L||InventoryRevision==L->GetRevision())return;
    InventoryRevision=L->GetRevision();SensorOptions.Reset();for(const auto& I:L->GetLayoutSensors()){auto O=MakeShared<FOption>();O->Actor=I.Actor;O->Label=(I.Kind==EVirtualSensorKind::Camera?TEXT("Camera · "):TEXT("LiDAR · "))+I.SensorId+(I.bToolCreated?TEXT(" [도구 생성]"):TEXT(" [레벨 배치]"));SensorOptions.Add(O);}if(SensorCombo)SensorCombo->RefreshOptions();
}
void UVirtualSensorPlacementPanelWidget::RefreshLayouts(){LayoutOptions.Reset();if(auto* L=Layout())for(const auto& N:L->GetSavedLayoutNames())LayoutOptions.Add(MakeShared<FString>(N));if(LayoutCombo)LayoutCombo->RefreshOptions();}
void UVirtualSensorPlacementPanelWidget::NativeTick(const FGeometry& G,float D)
{
    Super::NativeTick(G,D);if(!IsVisible()||IsPanelCollapsed()||FPlatformTime::Seconds()<NextRefresh)return;NextRefresh=FPlatformTime::Seconds()+.2;
    RefreshInventory();if(Controls)Controls->RefreshWorkspacePlacementControls();
    if(auto* L=Layout())if(!OperationId.IsEmpty())
    {const auto S=L->GetOperationStatus(OperationId);Status=S.Message;if(S.State!=EVirtualSensorLayoutOperation::Writing&&S.State!=EVirtualSensorLayoutOperation::ReadyToApply){if(S.State==EVirtualSensorLayoutOperation::Saved)RefreshLayouts();OperationId.Reset();}}
    if(PendingDelete.IsValid()&&PendingDelete.Get()!=Selected())PendingDelete.Reset();
}
TSharedRef<SWidget> UVirtualSensorPlacementPanelWidget::RebuildWidget()
{
    if(WidgetTree&&WidgetTree->RootWidget)return Super::RebuildWidget();
    RefreshInventory();RefreshLayouts();auto Body=SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0,8)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("AddLidar","LiDAR 추가")).OnClicked_Lambda([this](){AddSensor(EVirtualSensorKind::Lidar);return FReply::Handled();})]
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("AddCamera","Camera 추가")).OnClicked_Lambda([this](){AddSensor(EVirtualSensorKind::Camera);return FReply::Handled();})]
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("Settings","측정 설정")).OnClicked_Lambda([this](){if(GetToolWorkspace())GetToolWorkspace()->SetPanelOpen(ESensorToolPanelRole::Settings,true);return FReply::Handled();})]];
    Body->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor())
        .Text(LOCTEXT("AddHelp","현재 관찰 위치·방향에 추가 · PCD 송신은 별도"))];
    Body->AddSlot().AutoHeight()[SAssignNew(SensorCombo,SComboBox<TSharedPtr<FOption>>).OptionsSource(&SensorOptions)
        .OnGenerateWidget_Lambda([this](TSharedPtr<FOption> O){return SNewSensorTool(STextBlock).Text(FText::FromString(O?O->Label:FString()));})
        .OnSelectionChanged_Lambda([this](TSharedPtr<FOption> O,ESelectInfo::Type){if(O&&O->Actor.IsValid())if(auto* L=Layout()){EndPlacement();L->Coordinator()->SelectSensorActor(O->Actor.Get());if(GetToolWorkspace())GetToolWorkspace()->SynchronizeOwnedSelection();}})
        [SNewSensorTool(STextBlock).Text_Lambda([this](){return SelectedText();})]];
    Body->AddSlot().AutoHeight().Padding(0,8)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text_Lambda([this](){auto* S=Selected();return FText::FromString(S&&S->IsSensorRunning()?TEXT("미리보기 중지"):TEXT("미리보기 시작"));}).OnClicked_Lambda([this](){if(auto* S=Selected()){FString E;if(!S->CanEditSensorPlacement(E))Status=E;else{EndPlacement();if(S->IsSensorRunning())S->StopSensor();else S->StartSensor();Status=TEXT("미리보기 변경 · Topic 송신은 별도");}}return FReply::Handled();})]
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("Duplicate","복제")).OnClicked_Lambda([this](){if(auto* L=Layout()){CancelLayoutOperation();EndPlacement();if(L->DuplicateSensor(Selected(),Status)){Status=TEXT("현재 관찰 시점에 복제 완료");if(Controls){Controls->SynchronizeWorkspaceSelection(true);Controls->SetSensorManipulationEnabled(true);}}RefreshInventory();}return FReply::Handled();})]
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle(true)).IsEnabled_Lambda([this](){auto* L=Layout();FString R;return L&&L->CanDeleteSensor(Selected(),R);}).ToolTipText_Lambda([this](){auto* L=Layout();FString R;if(L)L->CanDeleteSensor(Selected(),R);return FText::FromString(R);}).Text_Lambda([this](){return FText::FromString(PendingDelete.IsValid()?TEXT("삭제 확인"):TEXT("삭제"));}).OnClicked_Lambda([this](){DeleteSelectedSensor();return FReply::Handled();})]
        +SWrapBox::Slot()[SNewSensorTool(SButton).Visibility_Lambda([this](){return PendingDelete.IsValid()?EVisibility::Visible:EVisibility::Collapsed;}).Text(LOCTEXT("CancelDelete","취소")).OnClicked_Lambda([this](){PendingDelete.Reset();return FReply::Handled();})]];
    Body->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){auto* S=Selected();FString R;if(S)S->CanEditSensorPlacement(R);return FText::FromString(R.IsEmpty()?TEXT("수집 중 배치 잠금 · 레벨 장비 삭제 불가"):R);})];
    auto Scrolled=SNew(SVerticalBox);
    if(Controls)Scrolled->AddSlot().AutoHeight()[Controls->BuildWorkspacePlacementControls()];
    Scrolled->AddSlot().AutoHeight().Padding(0,12)[SNewSensorTool(STextBlock).Text(LOCTEXT("WholeLayout","전체 장비 구성 · 저장/불러오기"))];
    Scrolled->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor())
        .Text(LOCTEXT("LayoutHelp","추가한 장비까지 다음 PIE에서 복원하려면 전체 배치를 저장하세요. 개별 저장 위치는 선택 장비의 자세만 보관합니다."))];
    Scrolled->AddSlot().AutoHeight()[SNewSensorTool(SEditableTextBox).Style(&GetToolInputStyle()).HintText(LOCTEXT("LayoutName","전체 배치 이름")).OnTextChanged_Lambda([this](const FText& T){LayoutName=T.ToString();})];
    Scrolled->AddSlot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("SaveLayout","전체 배치 저장")).OnClicked_Lambda([this](){CancelLayoutOperation();if(auto* L=Layout())OperationId=L->RequestSaveLayout(LayoutName);return FReply::Handled();})]
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("OverwriteLayout","선택 배치 덮어쓰기")).OnClicked_Lambda([this](){CancelLayoutOperation();if(auto* L=Layout())OperationId=L->RequestSaveLayout(SelectedLayout,true);return FReply::Handled();})]];
    Scrolled->AddSlot().AutoHeight()[SAssignNew(LayoutCombo,SComboBox<TSharedPtr<FString>>).OptionsSource(&LayoutOptions).OnComboBoxOpening_Lambda([this](){RefreshLayouts();})
        .OnGenerateWidget_Lambda([this](TSharedPtr<FString> N){return SNewSensorTool(STextBlock).Text(FText::FromString(N?*N:FString()));})
        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> N,ESelectInfo::Type){SelectLayoutForRestore(N?*N:FString());})
        [SNewSensorTool(STextBlock).Text_Lambda([this](){return FText::FromString(SelectedLayout.IsEmpty()?TEXT("저장 배치 선택"):SelectedLayout);})]];
    Scrolled->AddSlot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("PreviewLayout","불러오기 변경 미리보기")).OnClicked_Lambda([this](){PreviewSelectedLayoutRestore();return FReply::Handled();})]
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("ApplyLayout","확인 후 적용")).IsEnabled_Lambda([this](){auto* L=Layout();return L&&L->GetOperationStatus(OperationId).State==EVirtualSensorLayoutOperation::ReadyToApply;}).OnClicked_Lambda([this](){ApplySelectedLayoutRestore();return FReply::Handled();})]
        +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("CancelRestore","취소")).OnClicked_Lambda([this](){CancelLayoutOperation();Status=TEXT("복원 취소 · 현재 배치 유지");return FReply::Handled();})]];
    Body->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Scrolled]];
    Body->AddSlot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolAccentColor()).Text_Lambda([this](){return FText::FromString(Status);})];
    return SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).Padding(8)[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("Title","센서·카메라 배치"))]
        +SVerticalBox::Slot().FillHeight(1)[SNew(SBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})[Body]]];
}
#undef LOCTEXT_NAMESPACE
