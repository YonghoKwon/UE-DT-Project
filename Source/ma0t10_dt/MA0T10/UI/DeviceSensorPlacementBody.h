#pragma once
#include "DevicePanelBodyCall.h"
#include "DevicePanelWidgetDecl.h"
#include "Blueprint/WidgetTree.h"
#include "DeviceSensorPanelTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolNumericInput.h"
#include "SensorToolUiText.h"
#include "SensorToolWidgetDecl.h"
#include "SensorToolWorkspaceStyle.h"
#include "VirtualSensorUiStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "ma0t10_dt/MA0T10/Contracts/DeviceIdentityPolicy.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/DeviceSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#define LOCTEXT_NAMESPACE "VirtualSensorPlacementPanelWidget"
/** Generic View contract: host policy, public BP state and optional extensions stay in the facade.
 * No concrete project widget, Workspace, scenario or job type is included here.
 * Slate callbacks retain only a weak UObject view; bodies create no tick, subscription or readback. */
struct FDeviceSensorPlacementBody
{

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "SensorPlacementPanel"
    template<class TView> static TSharedRef<SWidget> BuildEquipmentSection(TView& View)
    {
        TWeakObjectPtr<TView> DeviceOwner(&View);
        auto* DeviceSelf=&View;
        using FOption=typename TView::FOption;

    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[SNewDeviceTool(DeviceOwner,STextBlock).ColorAndOpacity(DeviceOwner->GetToolMutedColor()).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->CachedCountText;}); })];
    static const FTableRowStyle Card=FTableRowStyle(FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
      .SetActiveBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),8.f))
      .SetActiveHoveredBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),8.f))
      .SetInactiveBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),8.f))
      .SetInactiveHoveredBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Selection(),8.f))
      .SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Section(),8.f))
      .SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(FSensorOwnedPresentationStyle::Section(),8.f));
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SBox).HeightOverride_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FMath::Clamp(Self->SensorOptions.Num()*60.f*Self->GetSensorToolVisualScale(),48.f,240.f*Self->GetSensorToolVisualScale());}); })
     [SAssignNew(DeviceSelf->SensorList,SListView<TSharedPtr<FOption>>).ListItemsSource(&DeviceSelf->SensorOptions).SelectionMode(ESelectionMode::Single)
      .OnSelectionChanged_Lambda([DeviceOwner](TSharedPtr<FOption> O,ESelectInfo::Type Info){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {if(Info!=ESelectInfo::Direct&&O&&O->Actor.IsValid())Self->SelectEquipment(O->Actor.Get());}); })
      // Deferred mouse-up notification lets the 5 Hz model sync restore the old row
      // between down/up. Commit the model on mouse-down before any refresh can run.
      .OnGenerateRow_Lambda([DeviceOwner](TSharedPtr<FOption> O,const TSharedRef<STableViewBase>& Owner){ return DevicePanelBody::InvokeRow(DeviceOwner,Owner,[&](auto* Self) {return SNew(STableRow<TSharedPtr<FOption>>,Owner).Style(&Card).SignalSelectionMode(ETableRowSignalSelectionMode::Instantaneous).Padding(FMargin(10,8))
       [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
         +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FSensorToolWorkspaceStyle::Font(13,Self->GetSensorToolVisualScale());}); }).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText_Lambda([O](){return O->NameText;}).ColorAndOpacity(Self->GetToolTextColor()).Text_Lambda([O](){return O->NameText;})]
         +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FSensorToolWorkspaceStyle::Font(11,Self->GetSensorToolVisualScale());}); }).ColorAndOpacity(Self->GetToolMutedColor()).Text_Lambda([O](){return O->OriginText;})]]
        +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Font_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FSensorToolWorkspaceStyle::Font(13,Self->GetSensorToolVisualScale());}); }).ColorAndOpacity_Lambda([O](){return O->StateColor;}).Text(FText::FromString(TEXT("●")))]
        +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FSensorToolWorkspaceStyle::Font(12,Self->GetSensorToolVisualScale());}); }).ColorAndOpacity(Self->GetToolMutedColor()).Text_Lambda([O](){return O->StateText;})]];}); })]];
    Box->AddSlot().AutoHeight()[SNewDeviceTool(DeviceOwner,STextBlock).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->SensorOptions.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed;}); }).AutoWrapText(true).Text(LOCTEXT("EmptyEquipment","장비가 없습니다 · 아래에서 추가하세요"))];
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1)[SNewDeviceTool(DeviceOwner,SButton).Text(LOCTEXT("AddLidar","+ LiDAR")).ToolTipText(LOCTEXT("AddTip","현재 관찰 위치에 추가합니다. 전송은 별도로 시작합니다.")).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {DeviceOwner->AddSensor(EVirtualSensorKind::Lidar);return FReply::Handled();}); })]
      +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SNewDeviceTool(DeviceOwner,SButton).Text(LOCTEXT("AddCamera","+ Camera")).ToolTipText(LOCTEXT("AddTip","현재 관찰 위치에 추가합니다. 전송은 별도로 시작합니다.")).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {DeviceOwner->AddSensor(EVirtualSensorKind::Camera);return FReply::Handled();}); })]
      +SHorizontalBox::Slot().AutoWidth()[SNew(SComboButton).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->SensorOptions.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;}); }).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).HasDownArrow(false).ToolTipText(LOCTEXT("EquipmentMore","선택 장비 복제·삭제"))
       .OnGetMenuContent_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {auto Menu=SNew(SVerticalBox);
        Menu->AddSlot().AutoHeight()[SNewDeviceTool(DeviceOwner,SButton).Text(LOCTEXT("Duplicate","복제")).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->Selected()!=nullptr;}); }).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {DeviceOwner->CancelLayoutOperation();DeviceOwner->EndPlacement();if(auto* L=Self->Layout())if(L->DuplicateSensor(Self->Selected(),DeviceOwner->Status)){if(Self->GetToolWorkspace())Self->GetToolWorkspace()->SynchronizeOwnedSelection();Self->SetSelectedManipulating(true);}Self->RefreshInventory();FSlateApplication::Get().DismissAllMenus();return FReply::Handled();}); })];
        Menu->AddSlot().AutoHeight().Padding(0,4)[SNewDeviceTool(DeviceOwner,SButton).ButtonStyle(&DeviceOwner->GetToolButtonStyle(true)).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bCanDelete;}); }).ToolTipText_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->CachedDeleteText;}); }).Text(LOCTEXT("DeleteEquipment","삭제…")).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {DeviceOwner->DeleteSelectedSensor();FSlateApplication::Get().DismissAllMenus();return FReply::Handled();}); })];return Menu;}); })
       .ButtonContent()[SNewDeviceTool(DeviceSelf,STextBlock).Text(LOCTEXT("EquipmentMoreMark","..."))]]];
    Box->AddSlot().AutoHeight()[SNew(SHorizontalBox).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingDelete.IsValid()?EVisibility::Visible:EVisibility::Collapsed;}); })
      +SHorizontalBox::Slot().FillWidth(1)[SNewDeviceTool(DeviceOwner,SButton).ButtonStyle(&DeviceOwner->GetToolButtonStyle(true)).Text(LOCTEXT("DeleteConfirm","삭제 확인")).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {DeviceOwner->DeleteSelectedSensor();return FReply::Handled();}); })]
      +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNewDeviceTool(DeviceOwner,SButton).Text(LOCTEXT("CancelDelete","취소")).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->CancelSelectedSensorDeletion();return FReply::Handled();}); })]];
    return Box;

    }
};
#undef LOCTEXT_NAMESPACE
