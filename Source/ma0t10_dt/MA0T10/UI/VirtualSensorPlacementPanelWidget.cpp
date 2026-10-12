#include "VirtualSensorPlacementPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "SensorToolWorkspaceStyle.h"
#include "SensorToolUiText.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorPlacementInteraction.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SBoxPanel.h"
#define LOCTEXT_NAMESPACE "SensorPlacementPanel"
UVirtualSensorLayoutSubsystem* UVirtualSensorPlacementPanelWidget::Layout() const {return GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorLayoutSubsystem>():nullptr;}
USensorPlacementInteraction* UVirtualSensorPlacementPanelWidget::Interaction() const {return GetToolWorkspace()?GetToolWorkspace()->GetPlacementInteraction():nullptr;}
AVirtualSensorActorBase* UVirtualSensorPlacementPanelWidget::Selected() const
{auto* C=GetToolWorkspace()?GetToolWorkspace()->GetCoordinator():CachedCoordinator.Get();if(!C&&FPlatformTime::Seconds()>=NextCoordinatorLookup){NextCoordinatorLookup=FPlatformTime::Seconds()+.5;if(auto* L=Layout())CachedCoordinator=L->Coordinator();C=CachedCoordinator.Get();}return C?C->GetSelectedSensorActor():nullptr;}
void UVirtualSensorPlacementPanelWidget::BindPlacementControls(UVirtualSensorSettingsPanelWidget* In){Controls=In;} // Compatibility only; never builds UI or delegates actions to Settings.
void UVirtualSensorPlacementPanelWidget::EndPlacement(){if(auto* I=Interaction())I->Finish();if(Status.Contains(TEXT("Esc")))Status=TEXT("조작 종료 · 원래 측정 설정으로 복원했습니다.");}
void UVirtualSensorPlacementPanelWidget::NativeDestruct(){CancelLayoutOperation();EndPlacement();Super::NativeDestruct();}
void UVirtualSensorPlacementPanelWidget::CancelLayoutOperation(){if(auto* L=Layout())L->CancelLayoutRestorePreview(OperationId);OperationId.Reset();}
void UVirtualSensorPlacementPanelWidget::SelectLayoutForRestore(const FString& Name){if(SelectedLayout==Name)return;CancelLayoutOperation();SelectedLayout=Name;Status=TEXT("배치를 선택했습니다 · 변경 미리보기로 차이를 확인하세요.");}
FString UVirtualSensorPlacementPanelWidget::PreviewSelectedLayoutRestore(){CancelLayoutOperation();if(auto* L=Layout()){OperationId=L->PreviewLayoutRestore(SelectedLayout);Status=L->GetOperationStatus(OperationId).Message;}return OperationId;}
bool UVirtualSensorPlacementPanelWidget::ApplySelectedLayoutRestore(){auto* L=Layout();if(!L)return false;const bool Ok=L->ApplyLayoutRestore(OperationId,Status);if(Ok)Status=TEXT("전체 배치 적용 완료 · 장비와 위치를 확인하세요.");CancelLayoutOperation();return Ok;}
FString UVirtualSensorPlacementPanelWidget::SaveWholeLayout(const FString& Name,bool Overwrite){CancelLayoutOperation();if(auto* L=Layout()){OperationId=L->RequestSaveLayout(Name,Overwrite);Status=L->GetOperationStatus(OperationId).Message;}return OperationId;}
bool UVirtualSensorPlacementPanelWidget::SelectEquipment(AVirtualSensorActorBase* A)
{auto* L=Layout();if(!L||!L->Coordinator()||!A)return false;EndPlacement();const bool Ok=L->Coordinator()->SelectSensorActor(A);if(GetToolWorkspace())GetToolWorkspace()->SynchronizeOwnedSelection();PendingDelete.Reset();RefreshPoses();return Ok;}
bool UVirtualSensorPlacementPanelWidget::SetSelectedMeasuring(bool Measuring)
{auto* A=Selected();if(!A)return false;if(!A->CanEditSensorPlacement(Status))return false;EndPlacement();if(Measuring)A->StartSensor();else A->StopSensor();bSelectedMeasuring=A->IsSensorRunning();CachedMeasureCaption=bSelectedMeasuring?LOCTEXT("StopMeasurement","측정 중지"):LOCTEXT("StartMeasurement","측정 시작");InvalidateLayoutAndVolatility();Status=Measuring?TEXT("측정 시작 · 미리보기에서 확인하세요."):TEXT("측정 중지 · 배치를 계속 편집할 수 있습니다.");return true;}
bool UVirtualSensorPlacementPanelWidget::SetSelectedManipulating(bool Enabled){auto* I=Interaction();return I&&I->SetManipulation(Enabled,this,Status);}
FString UVirtualSensorPlacementPanelWidget::SaveCurrentPose(const FString& Name)
{auto* I=Interaction();if(!I)return FString();PoseRequest=I->SavePose(Name);PoseRequestRevision=PoseSelectionRevision;Status=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->GetPoseSaveStatus(PoseRequest).Message;return PoseRequest;}
bool UVirtualSensorPlacementPanelWidget::MoveToBookmark(const FString& Id){auto* I=Interaction();if(!I)return false;const bool Ok=I->ApplyBookmark(Id,Status);if(Ok){for(const auto& B:GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->GetSensorPoseBookmarks(Selected()))if(B.BookmarkId==Id){SelectedPoseId=Id;PoseName=B.Name;PoseDeleteId.Reset();++PoseSelectionRevision;break;}Status=TEXT("저장 위치로 이동했습니다 · 미리보기 갱신을 확인하세요.");}return Ok;}
bool UVirtualSensorPlacementPanelWidget::AddSensor(EVirtualSensorKind Kind)
{auto* L=Layout();if(!L)return false;CancelLayoutOperation();EndPlacement();const bool Ok=L->CreateSensorAtView(Kind,Status)!=nullptr;if(Ok){if(GetToolWorkspace())GetToolWorkspace()->SynchronizeOwnedSelection();SetSelectedManipulating(true);Status=TEXT("장비 추가 완료 · 위치를 조정하고 Esc로 확정하세요.");}RefreshInventory();return Ok;}
bool UVirtualSensorPlacementPanelWidget::DeleteSelectedSensor()
{auto* L=Layout();auto* A=Selected();if(!L||!A)return false;CancelLayoutOperation();if(PendingDelete.Get()!=A){if(!L->CanDeleteSensor(A,Status))return false;PendingDelete=A;Status=TEXT("삭제할 장비를 확인한 뒤 삭제 확인을 누르세요.");return false;}EndPlacement();const bool Ok=L->DeleteSensor(A,Status);PendingDelete.Reset();if(Ok)Status=TEXT("장비 삭제 완료 · 다른 장비를 선택하거나 추가하세요.");RefreshInventory();return Ok;}
void UVirtualSensorPlacementPanelWidget::CancelSelectedSensorDeletion(){PendingDelete.Reset();Status=TEXT("삭제 취소 · 장비를 유지합니다.");}
FText UVirtualSensorPlacementPanelWidget::SelectedText() const {return CachedSelectedText;}
void UVirtualSensorPlacementPanelWidget::RefreshInventory()
{
    auto* L=Layout();if(!L)return;
    const bool Changed=InventoryRevision!=L->GetRevision();
    if(Changed){InventoryRevision=L->GetRevision();SensorOptions.Reset();for(const auto& Item:L->GetLayoutSensors()){auto O=MakeShared<FOption>();O->Actor=Item.Actor;SensorOptions.Add(O);}if(SensorList)SensorList->RequestListRefresh();}
    const auto* Scheduler=GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>();int32 Measuring=0;
    for(auto& O:SensorOptions)if(auto* A=O->Actor.Get())
    {
        const bool Running=A->IsSensorRunning();Measuring+=Running?1:0;FString State=Running?TEXT("측정 중"):TEXT("정지");FLinearColor Color=Running?FLinearColor(.15f,.85f,.4f):GetToolMutedColor();
        if(Scheduler)for(const auto& R:Scheduler->GetTelemetry().SensorRates)if(R.SensorId==A->GetSensorId())
        {if(Running&&R.State==EVirtualSensorRateState::Starved){State=TEXT("정체");Color=FLinearColor(1,.6f,.1f);}else if(R.State==EVirtualSensorRateState::Failed){State=TEXT("오류");Color=FLinearColor(1,.15f,.2f);}else if(R.State==EVirtualSensorRateState::Paused)State=TEXT("일시정지");}
        const FString Name=A->GetSensorId();const FString Origin=FString(A->GetSensorKind()==EVirtualSensorKind::Camera?TEXT("Camera · "):TEXT("LiDAR · "))+(L->OwnsSensor(A)?TEXT("추가 장비"):TEXT("레벨 배치"));const FString Label=Name+Origin+State;
        if(O->Label!=Label){O->Label=Label;O->NameText=FText::FromString(Name);O->OriginText=FText::FromString(Origin);O->StateText=FText::FromString(State);O->StateColor=Color;}
    }
    EquipmentCountText=FString::Printf(TEXT("장비 %d대 · 측정 %d대"),SensorOptions.Num(),Measuring);
    CachedCountText=FText::FromString(EquipmentCountText);
    auto* A=Selected();bCanMove=A&&A->CanEditSensorPlacement(PlacementLock);bCanDelete=L->CanDeleteSensor(A,DeleteReason);
    if(SensorList){const auto* Match=SensorOptions.FindByPredicate([A](const auto& O){return O->Actor.Get()==A;});const auto SelectedRows=SensorList->GetSelectedItems();if(Match&&(SelectedRows.IsEmpty()||SelectedRows[0]!=*Match))SensorList->SetSelection(*Match,ESelectInfo::Direct);else if(!Match&&!SelectedRows.IsEmpty())SensorList->ClearSelection();}
    if(!A)PlacementLock=TEXT("장비를 선택하세요.");
    const FText NewSelected=FText::FromString(A?FString(A->GetSensorKind()==EVirtualSensorKind::Camera?TEXT("Camera "):TEXT("LiDAR "))+A->GetSensorId():TEXT("장비 없음 · 위에서 추가하세요"));
    const bool Running=A&&A->IsSensorRunning();
    if(!NewSelected.EqualTo(CachedSelectedText)||Running!=bSelectedMeasuring){CachedSelectedText=NewSelected;bSelectedMeasuring=Running;CachedMeasureCaption=Running?LOCTEXT("StopMeasurement","측정 중지"):LOCTEXT("StartMeasurement","측정 시작");InvalidateLayoutAndVolatility();}
    if(CachedLockSource!=PlacementLock){CachedLockSource=PlacementLock;CachedLockText=FText::FromString(SensorToolUiText::UserReason(PlacementLock));}
    const FString DeleteSource=SensorOptions.IsEmpty()?TEXT("장비가 없습니다 · LiDAR 또는 Camera를 추가하세요."):A&&!L->OwnsSensor(A)?TEXT("레벨 장비는 삭제할 수 없습니다."):DeleteReason;
    if(CachedDeleteSource!=DeleteSource){CachedDeleteSource=DeleteSource;CachedDeleteText=FText::FromString(SensorToolUiText::UserReason(DeleteSource));}
}
void UVirtualSensorPlacementPanelWidget::RefreshLayouts(){LayoutOptions.Reset();if(auto* L=Layout())for(const auto& N:L->GetSavedLayoutNames())LayoutOptions.Add(MakeShared<FString>(N));if(LayoutCombo)LayoutCombo->RefreshOptions();}
void UVirtualSensorPlacementPanelWidget::RefreshPoses()
{
    auto* P=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>();auto* A=Selected();const FString Id=A?A->GetSensorId():FString();
    if(PoseSensor.Get()!=A||PoseSensorId!=Id){PoseSensor=A;PoseSensorId=Id;PoseRevision=-1;SelectedPoseId.Reset();PoseDeleteId.Reset();PoseName.Reset();PoseRequest.Reset();++PoseSelectionRevision;}
    if(PoseRevision==P->GetLibraryRevision()||!PoseList)return;PoseRevision=P->GetLibraryRevision();PoseList->ClearChildren();
    const auto Bookmarks=P->GetSensorPoseBookmarks(A);
    if(!SelectedPoseId.IsEmpty()&&!Bookmarks.ContainsByPredicate([this](const auto& B){return B.BookmarkId==SelectedPoseId;})){SelectedPoseId.Reset();PoseDeleteId.Reset();}
    for(const auto& Bookmark:Bookmarks)
    {
        const FString BookmarkId=Bookmark.BookmarkId,Name=Bookmark.Name;
        PoseList->AddSlot().AutoHeight().Padding(0,3)[SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).ButtonStyle(&GetToolButtonStyle()).OnClicked_Lambda([this,BookmarkId,Name](){SelectedPoseId=BookmarkId;PoseName=Name;PoseDeleteId.Reset();++PoseSelectionRevision;return FReply::Handled();})
          [SNew(STextBlock).Font_Lambda([this](){return FSensorToolWorkspaceStyle::Font(16,GetSensorToolFontScale());}).ColorAndOpacity_Lambda([this,BookmarkId](){return SelectedPoseId==BookmarkId?GetToolAccentColor():GetToolTextColor();}).Text(FText::FromString(Name))]]
         +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).ButtonStyle(&GetToolButtonStyle()).IsEnabled_Lambda([this](){return bCanMove;}).OnClicked_Lambda([this,BookmarkId](){MoveToBookmark(BookmarkId);return FReply::Handled();})[SNew(STextBlock).Font_Lambda([this](){return FSensorToolWorkspaceStyle::Font(16,GetSensorToolFontScale());}).ColorAndOpacity(GetToolTextColor()).Text(LOCTEXT("MovePose","이동"))]]];
    }
}
void UVirtualSensorPlacementPanelWidget::NativeTick(const FGeometry& G,float D)
{
    Super::NativeTick(G,D);if(!IsVisible()||IsPanelCollapsed()||FPlatformTime::Seconds()<NextRefresh)return;NextRefresh=FPlatformTime::Seconds()+.2;
    RefreshInventory();RefreshPoses();if(auto* I=Interaction()){I->SynchronizeSelection();const bool Manipulating=I->IsManipulating();if(bPreviousManipulating&&!Manipulating&&Status.Contains(TEXT("Esc")))Status=TEXT("조작 종료 · 원래 측정 설정으로 복원했습니다.");bPreviousManipulating=Manipulating;CachedManipulationCaption=Manipulating?LOCTEXT("EndManipulation","조작 종료 · Esc"):LOCTEXT("BeginManipulation","이동·회전 시작");}
    const auto Preview=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->GetPlacementPreviewStatus(Selected());
    if(Preview.RequestId!=LastPreview.RequestId||Preview.State!=LastPreview.State||Preview.CompletedFrameId!=LastPreview.CompletedFrameId){LastPreview=Preview;CachedPreviewText=FText::FromString(SensorToolUiText::UserReason(Preview.Message));}
    if(auto* L=Layout())if(!OperationId.IsEmpty()){const auto S=L->GetOperationStatus(OperationId);Status=S.Message;if(S.State!=EVirtualSensorLayoutOperation::Writing&&S.State!=EVirtualSensorLayoutOperation::ReadyToApply){if(S.State==EVirtualSensorLayoutOperation::Saved)RefreshLayouts();OperationId.Reset();}}
    if(!PoseRequest.IsEmpty()){auto* P=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>();const auto S=P->GetPoseSaveStatus(PoseRequest);if(PoseRequestRevision==PoseSelectionRevision){Status=S.Message;if(S.State==EVirtualSensorPoseSaveState::Saved){SelectedPoseId=S.BookmarkId;PoseRevision=-1;RefreshPoses();}}if(S.State!=EVirtualSensorPoseSaveState::Writing)PoseRequest.Reset();}
    if(PendingDelete.IsValid()&&PendingDelete.Get()!=Selected())PendingDelete.Reset();
    if(PreviousStatus!=Status){PreviousStatus=Status;CachedStatusText=FText::FromString(SensorToolUiText::UserReason(Status));}
}
TSharedRef<SWidget> UVirtualSensorPlacementPanelWidget::BuildEquipmentSection()
{
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return CachedCountText;})];
    static const FTableRowStyle Card=FTableRowStyle(FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
      .SetActiveBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),8.f))
      .SetActiveHoveredBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),8.f))
      .SetInactiveBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),8.f))
      .SetInactiveHoveredBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),8.f))
      .SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Section(),8.f))
      .SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Section(),8.f));
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SBox).HeightOverride_Lambda([this](){return FMath::Clamp(SensorOptions.Num()*60.f*GetSensorToolVisualScale(),48.f,240.f*GetSensorToolVisualScale());})
     [SAssignNew(SensorList,SListView<TSharedPtr<FOption>>).ListItemsSource(&SensorOptions).SelectionMode(ESelectionMode::Single)
      .OnSelectionChanged_Lambda([this](TSharedPtr<FOption> O,ESelectInfo::Type Info){if(Info!=ESelectInfo::Direct&&O&&O->Actor.IsValid())SelectEquipment(O->Actor.Get());})
      .OnGenerateRow_Lambda([this](TSharedPtr<FOption> O,const TSharedRef<STableViewBase>& Owner){return SNew(STableRow<TSharedPtr<FOption>>,Owner).Style(&Card).Padding(FMargin(10,8))
       [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
         +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font_Lambda([this](){return FSensorToolWorkspaceStyle::Font(13,GetSensorToolVisualScale());}).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText_Lambda([O](){return O->NameText;}).ColorAndOpacity(GetToolTextColor()).Text_Lambda([O](){return O->NameText;})]
         +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font_Lambda([this](){return FSensorToolWorkspaceStyle::Font(11,GetSensorToolVisualScale());}).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([O](){return O->OriginText;})]]
        +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Font_Lambda([this](){return FSensorToolWorkspaceStyle::Font(13,GetSensorToolVisualScale());}).ColorAndOpacity_Lambda([O](){return O->StateColor;}).Text(FText::FromString(TEXT("●")))]
        +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font_Lambda([this](){return FSensorToolWorkspaceStyle::Font(12,GetSensorToolVisualScale());}).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([O](){return O->StateText;})]];})]];
    Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).Visibility_Lambda([this](){return SensorOptions.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed;}).AutoWrapText(true).Text(LOCTEXT("EmptyEquipment","장비가 없습니다 · 아래에서 추가하세요"))];
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(SButton).Text(LOCTEXT("AddLidar","+ LiDAR")).ToolTipText(LOCTEXT("AddTip","현재 관찰 위치에 추가합니다. 전송은 별도로 시작합니다.")).OnClicked_Lambda([this](){AddSensor(EVirtualSensorKind::Lidar);return FReply::Handled();})]
      +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SNewSensorTool(SButton).Text(LOCTEXT("AddCamera","+ Camera")).ToolTipText(LOCTEXT("AddTip","현재 관찰 위치에 추가합니다. 전송은 별도로 시작합니다.")).OnClicked_Lambda([this](){AddSensor(EVirtualSensorKind::Camera);return FReply::Handled();})]
      +SHorizontalBox::Slot().AutoWidth()[SNew(SComboButton).Visibility_Lambda([this](){return SensorOptions.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;}).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).HasDownArrow(false).ToolTipText(LOCTEXT("EquipmentMore","선택 장비 복제·삭제"))
       .OnGetMenuContent_Lambda([this](){auto Menu=SNew(SVerticalBox);
        Menu->AddSlot().AutoHeight()[SNewSensorTool(SButton).Text(LOCTEXT("Duplicate","복제")).IsEnabled_Lambda([this](){return Selected()!=nullptr;}).OnClicked_Lambda([this](){CancelLayoutOperation();EndPlacement();if(auto* L=Layout())if(L->DuplicateSensor(Selected(),Status)){if(GetToolWorkspace())GetToolWorkspace()->SynchronizeOwnedSelection();SetSelectedManipulating(true);}RefreshInventory();FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})];
        Menu->AddSlot().AutoHeight().Padding(0,4)[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle(true)).IsEnabled_Lambda([this](){return bCanDelete;}).ToolTipText_Lambda([this](){return CachedDeleteText;}).Text(LOCTEXT("DeleteEquipment","삭제…")).OnClicked_Lambda([this](){DeleteSelectedSensor();FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})];return Menu;})
       .ButtonContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("EquipmentMoreMark","..."))]]];
    Box->AddSlot().AutoHeight()[SNew(SHorizontalBox).Visibility_Lambda([this](){return PendingDelete.IsValid()?EVisibility::Visible:EVisibility::Collapsed;})
      +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle(true)).Text(LOCTEXT("DeleteConfirm","삭제 확인")).OnClicked_Lambda([this](){DeleteSelectedSensor();return FReply::Handled();})]
      +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNewSensorTool(SButton).Text(LOCTEXT("CancelDelete","취소")).OnClicked_Lambda([this](){CancelSelectedSensorDeletion();return FReply::Handled();})]];
    return Box;
}
TSharedRef<SWidget> UVirtualSensorPlacementPanelWidget::BuildSelectedSection()
{
    auto Box=SNew(SVerticalBox);Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolAccentColor()).Text_Lambda([this](){return SelectedText();})];
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::PrimaryButton()).IsEnabled_Lambda([this](){return bCanMove;}).ToolTipText_Lambda([this](){return CachedLockText;}).Text_Lambda([this](){return CachedManipulationCaption;}).OnClicked_Lambda([this](){if(auto* I=Interaction())SetSelectedManipulating(!I->IsManipulating());return FReply::Handled();})]
     +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNewSensorTool(SButton).IsEnabled_Lambda([this](){return bCanMove;}).ToolTipText_Lambda([this](){return CachedLockText;}).Text_Lambda([this](){return CachedMeasureCaption;}).OnClicked_Lambda([this](){SetSelectedMeasuring(!bSelectedMeasuring);return FReply::Handled();})]];
    Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Visibility_Lambda([this](){return CachedPreviewText.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;}).Text_Lambda([this](){return CachedPreviewText;})];
    auto Segments=[this](const TArray<FText>& Names,TFunction<int32()> Value,TFunction<void(int32)> Set)->TSharedRef<SWidget>
    {auto Row=SNew(SHorizontalBox);for(int32 Index=0;Index<Names.Num();++Index)Row->AddSlot().FillWidth(1)[SNew(SCheckBox).Style(&FSensorOwnedPresentationStyle::Segment()).IsChecked_Lambda([Value,Index](){return Value()==Index?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([Set,Index](ECheckBoxState State){if(State==ECheckBoxState::Checked)Set(Index);})[SNewSensorTool(STextBlock).Justification(ETextJustify::Center).Text(Names[Index])]];return Row;};
    Box->AddSlot().AutoHeight().Padding(0,5)[SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1)[Segments({LOCTEXT("Move","이동"),LOCTEXT("Rotate","회전")},[this](){return Interaction()?int32(Interaction()->GetMode()):0;},[this](int32 V){if(Interaction())Interaction()->SetMode(EVirtualSensorGizmoMode(V));})]
      +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[Segments({LOCTEXT("Local","로컬"),LOCTEXT("World","월드")},[this](){return Interaction()?int32(Interaction()->GetSpace()):0;},[this](int32 V){if(Interaction())Interaction()->SetSpace(EVirtualSensorCoordinateSpace(V));})]];
    Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text(LOCTEXT("ShortKeys","WASD·QE 이동 · RMB 관찰 · Esc 종료"))];
    auto Numeric=SNew(SVerticalBox);
    Numeric->AddSlot().AutoHeight().Padding(0,4)[Segments({LOCTEXT("Continuous","연속"),LOCTEXT("Precision","정밀")},[this](){return Interaction()?int32(Interaction()->GetInputMode()):0;},[this](int32 V){if(Interaction())Interaction()->SetInputMode(EVirtualSensorPlacementInputMode(V));})];
    for(int32 Axis=0;Axis<6;++Axis)Numeric->AddSlot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(STextBlock).Text(FText::FromString(Axis<3?FString::Printf(TEXT("위치 %c · cm"),TEXT('X')+Axis):Axis==3?TEXT("Pitch · °"):Axis==4?TEXT("Yaw · °"):TEXT("Roll · °")))]
      +SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<float>).IsEnabled_Lambda([this](){return bCanMove;}).MinValue(Axis<3?-100000.f:-180.f).MaxValue(Axis<3?100000.f:180.f)
       .Value_Lambda([this,Axis](){const auto Pose=Selected()?Selected()->GetActorTransform():FTransform::Identity;const auto R=Pose.Rotator();return float(Axis<3?Pose.GetLocation()[Axis]:Axis==3?R.Pitch:Axis==4?R.Yaw:R.Roll);})
       .OnValueCommitted_Lambda([this,Axis](float V,ETextCommit::Type){if(!Selected()||!Interaction()||!FMath::IsFinite(V))return;auto Pose=Selected()->GetActorTransform();if(Axis<3){auto L=Pose.GetLocation();L[Axis]=V;Pose.SetLocation(L);}else{auto R=Pose.Rotator();if(Axis==3)R.Pitch=V;else if(Axis==4)R.Yaw=V;else R.Roll=V;Pose.SetRotation(R.Quaternion());}if(Interaction()->ApplyPose(Pose,Status))Status=TEXT("위치 적용 완료 · 미리보기를 확인하세요.");})]];
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("Numeric","입력 방식·숫자 입력"))].BodyContent()[Numeric]];
    Box->AddSlot().AutoHeight()[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton()).Text(LOCTEXT("Details","장비 상세 설정 →")).OnClicked_Lambda([this](){if(GetToolWorkspace())GetToolWorkspace()->SetPanelOpen(ESensorToolPanelRole::Settings,true);return FReply::Handled();})];return SNew(SBorder).BorderImage(GetToolSectionBrush()).BorderBackgroundColor(GetToolSectionColor()).Padding(12)[Box];
}
TSharedRef<SWidget> UVirtualSensorPlacementPanelWidget::BuildPoseSection()
{
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().HAlign(HAlign_Right)[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton()).HasDownArrow(false)
      .OnGetMenuContent_Lambda([this](){return SNew(SBox).WidthOverride(240)[SNew(SVerticalBox)
       +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(SEditableTextBox).HintText(LOCTEXT("PoseName","저장 위치 이름")).Text_Lambda([this](){return FText::FromString(PoseName);}).OnTextChanged_Lambda([this](const FText& T){PoseName=T.ToString();++PoseSelectionRevision;})]
       +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::PrimaryButton()).Text(LOCTEXT("SavePose","현재 위치 저장")).IsEnabled_Lambda([this](){return Selected()!=nullptr;}).OnClicked_Lambda([this](){SaveCurrentPose(PoseName);return FReply::Handled();})]];})
      .ButtonContent()[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolAccentColor()).Text(LOCTEXT("SavePoseLink","+ 현재 위치 저장"))]];
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
     +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton()).Text(LOCTEXT("Undo","되돌리기")).IsEnabled_Lambda([this](){return GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->CanUndoSensorPlacement(Selected());}).OnClicked_Lambda([this](){if(Interaction())Interaction()->Undo(Status);return FReply::Handled();})]
     +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton()).Text(LOCTEXT("Redo","다시 적용")).IsEnabled_Lambda([this](){return GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->CanRedoSensorPlacement(Selected());}).OnClicked_Lambda([this](){if(Interaction())Interaction()->Redo(Status);return FReply::Handled();})]];
    Box->AddSlot().AutoHeight()[SAssignNew(PoseList,SVerticalBox)];PoseRevision=-1;RefreshPoses();
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8)).Visibility_Lambda([this](){return SelectedPoseId.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;})
     +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("OverwritePose","선택 위치 덮어쓰기")).OnClicked_Lambda([this](){if(Interaction())PoseRequest=Interaction()->SavePose(PoseName,SelectedPoseId);PoseRequestRevision=PoseSelectionRevision;return FReply::Handled();})]
     +SWrapBox::Slot()[SNewSensorTool(SEditableTextBox).HintText(LOCTEXT("PoseName","저장 위치 이름")).Text_Lambda([this](){return FText::FromString(PoseName);}).OnTextChanged_Lambda([this](const FText& T){PoseName=T.ToString();++PoseSelectionRevision;})]
     +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("Rename","이름 변경")).OnClicked_Lambda([this](){PoseRequest=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->RenameSensorPoseBookmark(Selected(),SelectedPoseId,PoseName);PoseRequestRevision=PoseSelectionRevision;return FReply::Handled();})]
     +SWrapBox::Slot()[SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle(true)).Text_Lambda([this](){return FText::FromString(PoseDeleteId==SelectedPoseId?TEXT("삭제 확인"):TEXT("삭제"));}).OnClicked_Lambda([this](){if(PoseDeleteId==SelectedPoseId){PoseRequest=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->DeleteSensorPoseBookmark(Selected(),SelectedPoseId);PoseRequestRevision=PoseSelectionRevision;SelectedPoseId.Reset();PoseDeleteId.Reset();}else PoseDeleteId=SelectedPoseId;return FReply::Handled();})]
     +SWrapBox::Slot()[SNewSensorTool(SButton).Visibility_Lambda([this](){return PoseDeleteId.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;}).Text(LOCTEXT("CancelPoseDelete","취소")).OnClicked_Lambda([this](){PoseDeleteId.Reset();Status=TEXT("삭제 취소 · 저장 위치를 유지합니다.");return FReply::Handled();})]];return Box;
}
TSharedRef<SWidget> UVirtualSensorPlacementPanelWidget::BuildLayoutSaveMenu()
{
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0,8)[SNewSensorTool(SEditableTextBox).HintText(LOCTEXT("LayoutName","전체 배치 이름")).OnTextChanged_Lambda([this](const FText& T){LayoutName=T.ToString();})];
    Box->AddSlot().AutoHeight()[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::PrimaryButton()).Text(LOCTEXT("SaveLayout","전체 배치 저장")).ToolTipText(LOCTEXT("LayoutSaveTip","추가 장비·측정 설정·자세를 보관합니다. 전송은 시작하지 않습니다.")).OnClicked_Lambda([this](){SaveWholeLayout(LayoutName);return FReply::Handled();})];
    return SNew(SBox).WidthOverride(280)[Box];
}
TSharedRef<SWidget> UVirtualSensorPlacementPanelWidget::BuildLayoutSection()
{
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0,12)[SAssignNew(LayoutCombo,SComboBox<TSharedPtr<FString>>).OptionsSource(&LayoutOptions).OnComboBoxOpening_Lambda([this](){RefreshLayouts();}).OnGenerateWidget_Lambda([this](TSharedPtr<FString> N){return SNewSensorTool(STextBlock).Text(FText::FromString(N?*N:FString()));}).OnSelectionChanged_Lambda([this](TSharedPtr<FString> N,ESelectInfo::Type){SelectLayoutForRestore(N?*N:FString());})[SNewSensorTool(STextBlock).Text_Lambda([this](){return FText::FromString(SelectedLayout.IsEmpty()?TEXT("저장 배치 선택"):SelectedLayout);})]];
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))
     +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("PreviewLayout","변경 미리보기")).IsEnabled_Lambda([this](){return !SelectedLayout.IsEmpty();}).OnClicked_Lambda([this](){PreviewSelectedLayoutRestore();return FReply::Handled();})]
     +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("ApplyLayout","적용")).IsEnabled_Lambda([this](){auto* L=Layout();return L&&L->GetOperationStatus(OperationId).State==EVirtualSensorLayoutOperation::ReadyToApply;}).OnClicked_Lambda([this](){ApplySelectedLayoutRestore();return FReply::Handled();})]
     +SWrapBox::Slot()[SNewSensorTool(SButton).Text(LOCTEXT("CancelLayout","취소")).OnClicked_Lambda([this](){CancelLayoutOperation();Status=TEXT("복원 취소 · 현재 배치를 유지합니다.");return FReply::Handled();})]];
    Box->AddSlot().AutoHeight()[SNewSensorTool(SButton).Visibility_Lambda([this](){return SelectedLayout.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;}).Text(LOCTEXT("OverwriteLayout","선택 배치 덮어쓰기")).OnClicked_Lambda([this](){SaveWholeLayout(SelectedLayout,true);return FReply::Handled();})];return SNew(SBox).WidthOverride(280).MaxDesiredHeight(360)[SNew(SScrollBox)+SScrollBox::Slot()[Box]];
}
TSharedRef<SWidget> UVirtualSensorPlacementPanelWidget::RebuildWidget()
{
    if(WidgetTree&&WidgetTree->RootWidget)return Super::RebuildWidget();RefreshInventory();RefreshLayouts();CachedStatusText=FText::FromString(Status);
    auto Sections=SNew(SVerticalBox);
    auto AddSection=[this,Sections](FText Title,TSharedRef<SWidget> Content,bool NeedsSelection=false){Sections->AddSlot().AutoHeight().Padding(8,0,8,16)[SNew(SVerticalBox).Visibility_Lambda([this,NeedsSelection](){return NeedsSelection&&!Selected()?EVisibility::Collapsed:EVisibility::Visible;})+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text(Title)]+SVerticalBox::Slot().AutoHeight()[Content]];};
    AddSection(LOCTEXT("EquipmentClean","장비"),BuildEquipmentSection());AddSection(LOCTEXT("SelectedClean","선택 장비"),BuildSelectedSection(),true);AddSection(LOCTEXT("SavedPoseClean","저장 위치"),BuildPoseSection(),true);
    const TSharedRef<SWidget> Content=SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).Padding(8)[SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(LOCTEXT("SidebarTitle","배치"))]
     +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})+SScrollBox::Slot()[Sections]]
     +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SHorizontalBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
       +SHorizontalBox::Slot().FillWidth(1)[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).HasDownArrow(false).OnGetMenuContent_Lambda([this](){return BuildLayoutSaveMenu();}).ButtonContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("SaveLayout","전체 배치 저장"))]]
       +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).HasDownArrow(false).OnGetMenuContent_Lambda([this](){RefreshLayouts();return BuildLayoutSection();}).ButtonContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("LoadLayout","불러오기"))]]]
     +SVerticalBox::Slot().AutoHeight().Padding(8,0)[SNewSensorTool(STextBlock).Visibility_Lambda([this](){return GetPanelBodyVisibility();}).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ColorAndOpacity(GetToolAccentColor()).ToolTipText_Lambda([this](){return CachedStatusText;}).Text_Lambda([this](){return CachedStatusText;})]];
    return Content;
}
#undef LOCTEXT_NAMESPACE
