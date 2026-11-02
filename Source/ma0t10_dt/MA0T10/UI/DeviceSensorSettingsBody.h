#pragma once
#include "DevicePanelBodyCall.h"
#include "DevicePanelWidgetDecl.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SceneCaptureComponent2D.h"
#include "DeviceSensorPanelTypes.h"
#include "EngineUtils.h"
#include "LidarNativeSafetyText.h"
#include "SensorLidarHelp.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolNumericInput.h"
#include "SensorToolToolbarWidget.h"
#include "SensorToolUiText.h"
#include "SensorToolWidgetDecl.h"
#include "SensorToolWorkspaceStyle.h"
#include "Styling/CoreStyle.h"
#include "VirtualSensorUiStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Contracts/DeviceIdentityPolicy.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorIdentity.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHitchDiagnostics.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/DeviceSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#define LOCTEXT_NAMESPACE "VirtualSensorSettingsPanelWidget"
/** Generic View contract: host policy, public BP state and optional extensions stay in the facade.
 * No concrete project widget, Workspace, scenario or job type is included here.
 * Slate callbacks retain only a weak UObject view; bodies create no tick, subscription or readback. */
struct FDeviceSensorSettingsBody
{
    static FString QualityText(EVirtualSensorSimulationQuality Quality)
{
    if (Quality == EVirtualSensorSimulationQuality::Debug) return TEXT("디버그");
    if (Quality == EVirtualSensorSimulationQuality::RealTimePreview) return TEXT("실시간 미리보기");
    if (Quality == EVirtualSensorSimulationQuality::Balanced) return TEXT("균형");
    if (Quality == EVirtualSensorSimulationQuality::FullSpec) return TEXT("FullSpec (고부하)");
    return TEXT("사용자 설정");
}

    static FString OwnedQualityLabel(EVirtualSensorSimulationQuality Quality)
{return Quality==EVirtualSensorSimulationQuality::FullSpec?TEXT("전체 사양(고부하)"):QualityText(Quality);}

    static constexpr int32 OwnedBasicTab=0,OwnedAdvancedTab=1,OwnedAutomationTab=2;

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorSettingsPanelWidget"
    template<class TView> static TSharedRef<SWidget> BuildOwnedSettingsWidget(TView& View)
    {
        TWeakObjectPtr<TView> DeviceOwner(&View);
        auto* DeviceSelf=&View;

    DeviceSelf->RefreshOwnedSettingsText();
    auto Basic=SNew(SVerticalBox).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->WorkspaceSettingsTab==0?EVisibility::Visible:EVisibility::Collapsed;}); }).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); });
    auto Advanced=SNew(SVerticalBox).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->WorkspaceSettingsTab==1?EVisibility::Visible:EVisibility::Collapsed;}); });
    auto Automation=SNew(SVerticalBox).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->WorkspaceSettingsTab==2?EVisibility::Visible:EVisibility::Collapsed;}); });
    auto Compatibility=SNew(SVerticalBox);
    const auto CameraVisible=[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.TargetKind==EVirtualSensorTargetKind::Camera?EVisibility::Visible:EVisibility::Collapsed;}); };
    const auto LidarVisible=[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.TargetKind==EVirtualSensorTargetKind::Lidar?EVisibility::Visible:EVisibility::Collapsed;}); };
    Basic->AddSlot().AutoHeight().Padding(0,0,0,FSensorOwnedPresentationStyle::SectionGap)
      [SNewDeviceTool(DeviceSelf,STextBlock).AutoWrapText(true).ColorAndOpacity(DeviceSelf->GetToolMutedColor()).Text(LOCTEXT("OwnedBasicHelp","프로필·품질은 측정값을 함께 적용합니다."))];
    Advanced->AddSlot().AutoHeight().Padding(0,0,0,FSensorOwnedPresentationStyle::SectionGap)
      [SNewDeviceTool(DeviceSelf,STextBlock).AutoWrapText(true).ColorAndOpacity(DeviceSelf->GetToolMutedColor()).Text(LOCTEXT("OwnedAdvancedHelp","정답·사실성과 측정 세부값을 조정합니다. IMU는 배치 화면에서 설정합니다."))];
    Basic->AddSlot().AutoHeight().Padding(0,4)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6,6))
      +SWrapBox::Slot()[SNewDeviceTool(DeviceOwner,SButton).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanMeasure;}); }).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->OwnedMeasureText;}); })
        .OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->ToggleDeviceSelectedMeasuring();Self->RefreshOwnedSettingsText();return FReply::Handled();}); })]
      +SWrapBox::Slot()[SNewDeviceTool(DeviceOwner,SButton).Text(SensorToolUiText::Preview()).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->OpenDeviceMonitorPanel();return FReply::Handled();}); })]];
    Advanced->AddSlot().AutoHeight().Padding(0,4)[SNewDeviceTool(DeviceSelf,SButton)
      .OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->OpenOwnedSettingsOwner(false);return FReply::Handled();}); })
      [SNewDeviceTool(DeviceSelf,STextBlock).AutoWrapText(true).Text(LOCTEXT("OwnedImuLink","IMU 부착·노이즈·설정 열기"))]];
    Basic->AddSlot().AutoHeight()[DeviceSelf->BuildToolFormRow(LOCTEXT("OwnedProfileLabel","장비 프로필"),SNew(SVerticalBox)
      +SVerticalBox::Slot().FillHeight(1)[SNew(SComboBox<TSharedPtr<EVirtualCameraDeviceProfile>>).ComboBoxStyle(&DeviceSelf->GetToolComboBoxStyle()).ContentPadding(FMargin(FSensorOwnedPresentationStyle::Gap,6))
        .Visibility_Lambda(CameraVisible).OptionsSource(&DeviceSelf->NativeCameraProfileOptions)
        .OnGenerateWidget_Lambda([DeviceOwner](auto Item){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return SNewDeviceTool(Self,STextBlock).Text(FText::FromString(Item?CameraProfileText(*Item):FString()));}); })
        .OnSelectionChanged_Lambda([DeviceOwner](auto Item,ESelectInfo::Type){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {if(Item)Self->ApplyOwnedProfileSelection(*Item,Self->PendingState.LidarProfile);}); })
        [SNewDeviceTool(DeviceOwner,STextBlock).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->OwnedProfileText;}); })]]
      +SVerticalBox::Slot().FillHeight(1)[SNew(SComboBox<TSharedPtr<EVirtualLidarDeviceProfile>>).ComboBoxStyle(&DeviceSelf->GetToolComboBoxStyle()).ContentPadding(FMargin(FSensorOwnedPresentationStyle::Gap,6))
        .Visibility_Lambda(LidarVisible).OptionsSource(&DeviceSelf->NativeLidarProfileOptions)
        .OnGenerateWidget_Lambda([DeviceOwner](auto Item){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return SNewDeviceTool(Self,STextBlock).Text(FText::FromString(Item?LidarProfileText(*Item):FString()));}); })
        .OnSelectionChanged_Lambda([DeviceOwner](auto Item,ESelectInfo::Type){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {if(Item)Self->ApplyOwnedProfileSelection(Self->PendingState.CameraProfile,*Item);}); })
        [SNewDeviceTool(DeviceOwner,STextBlock).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->OwnedProfileText;}); })]],DeviceSelf->BuildOwnedSettingDetail(TEXT("DeviceProfile")))];
    Basic->AddSlot().AutoHeight()[DeviceSelf->BuildToolFormRow(LOCTEXT("OwnedQualityLabel","시뮬레이션 품질"),
      SNew(SComboBox<TSharedPtr<EVirtualSensorSimulationQuality>>).ComboBoxStyle(&DeviceSelf->GetToolComboBoxStyle()).ContentPadding(FMargin(FSensorOwnedPresentationStyle::Gap,6)).OptionsSource(&DeviceSelf->NativeQualityOptions)
        .OnGenerateWidget_Lambda([DeviceOwner](auto Item){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return SNewDeviceTool(Self,STextBlock).Text(FText::FromString(Item?OwnedQualityLabel(*Item):FString()));}); })
        .OnSelectionChanged_Lambda([DeviceOwner](auto Item,ESelectInfo::Type){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {if(Item){Self->SelectSettingHelp(TEXT("SimulationQuality"));Self->SetSelectedSimulationQuality(*Item);}}); })
        [SNewDeviceTool(DeviceOwner,STextBlock).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->OwnedQualityText;}); })],DeviceSelf->BuildOwnedSettingDetail(TEXT("SimulationQuality")))];
    auto CameraBasic=SNew(SVerticalBox).Visibility_Lambda(CameraVisible);
    CameraBasic->AddSlot().AutoHeight()[DeviceOwner->MakeFloatRow(LOCTEXT("OwnedCameraInterval","측정 주기 (s)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.CameraCaptureInterval;}); },[DeviceOwner](float V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.CameraCaptureInterval=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },.033f,60,true,TEXT("CameraInterval"))];
    Basic->AddSlot().AutoHeight()[CameraBasic];
    auto LidarBasic=SNew(SVerticalBox).Visibility_Lambda(LidarVisible);
    LidarBasic->AddSlot().AutoHeight()[DeviceOwner->MakeFloatRow(LOCTEXT("OwnedLidarInterval","측정 주기 (s)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.LidarScanInterval;}); },[DeviceOwner](float V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.LidarScanInterval=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },.033f,60,true,TEXT("LidarInterval"))];
    LidarBasic->AddSlot().AutoHeight()[DeviceOwner->MakeFloatRow(LOCTEXT("OwnedLidarRange","최대 거리 (cm)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.LidarMaxDistance;}); },[DeviceOwner](float V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.LidarMaxDistance=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },10,20000,true,TEXT("LidarRange"))];
    auto Backend=SNew(SVerticalBox).Visibility_Lambda(LidarVisible);
    Backend->AddSlot().AutoHeight().Padding(0,6)[SNewDeviceTool(DeviceSelf,STextBlock).Text(LOCTEXT("LidarBackendLabel","LiDAR 측정 방식"))];
    Backend->AddSlot().AutoHeight()[SNew(SComboBox<TSharedPtr<EVirtualLidarAcquisitionBackend>>).ComboBoxStyle(&DeviceSelf->GetToolComboBoxStyle()).ContentPadding(FMargin(FSensorOwnedPresentationStyle::Gap,6))
        .IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); }).OptionsSource(&DeviceSelf->NativeLidarBackendOptions)
        .OnGenerateWidget_Lambda([DeviceOwner](auto Item){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return SNewDeviceTool(Self,STextBlock).Text(Item?LidarBackendText(*Item):FText::GetEmpty());}); })
        .OnSelectionChanged_Lambda([DeviceOwner](auto Item,ESelectInfo::Type){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {if(!Item)return;auto* Actor=Cast<AVirtualLidarSensorActor>(Self->GetSelectedSensorActor());FString Reason;
            if(Actor&&Actor->SetLidarAcquisitionBackend(*Item,Reason)){Self->OwnedLidarBackendText=LidarBackendText(*Item);DeviceOwner->LastControlMessage=TEXT("LiDAR 측정 방식 적용");}
            else DeviceOwner->LastControlMessage=Reason.IsEmpty()?TEXT("선택한 LiDAR를 사용할 수 없습니다."):Reason;Self->RefreshNativeText();}); })
        [SNewDeviceTool(DeviceOwner,STextBlock).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->OwnedLidarBackendText;}); })]];
    Backend->AddSlot().AutoHeight().Padding(0,3)[SNewDeviceTool(DeviceSelf,STextBlock).AutoWrapText(true).ColorAndOpacity(DeviceSelf->GetToolMutedColor()).Text(LOCTEXT("LidarBackendHint","이번 실행에 적용합니다. 지원하지 않는 장면은 CPU로 측정하며, 변경 시 진행 중인 측정은 새 설정으로 다시 시작합니다."))];
    Advanced->AddSlot().AutoHeight()[Backend];
    auto Fidelity=SNew(SVerticalBox).Visibility_Lambda(LidarVisible);
    Fidelity->AddSlot().AutoHeight().Padding(0,8)[SNewDeviceTool(DeviceSelf,STextBlock).Text(LOCTEXT("LidarFidelityGroup","LiDAR 사실성"))];
    const auto FidelityToggle=[DeviceOwner](TSharedRef<SVerticalBox> Box,const FText& Label,bool FVirtualSensorEditableState::*Field,FName Key)
    { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {
        Box->AddSlot().AutoHeight().Padding(0,3)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[SNew(SCheckBox).CheckBoxContentUsesAutoWidth(false).ToolTipText(Self->GetDeviceSettingTooltip(Key))
                .IsChecked_Lambda([DeviceOwner,Field](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.*Field?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}); })
                .OnCheckStateChanged_Lambda([DeviceOwner,Field](ECheckBoxState Value){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {
                    Self->PendingState.*Field=Value==ECheckBoxState::Checked;
                    if(Field==&FVirtualSensorEditableState::bLidarMotionDistortion&&Self->PendingState.*Field)Self->PendingState.bLidarPointTime=true;
                    if(Field==&FVirtualSensorEditableState::bLidarPointTime&&!(Self->PendingState.*Field))Self->PendingState.bLidarMotionDistortion=false;
                    Self->ApplyPendingState();}); })[SNewDeviceTool(Self,STextBlock).AutoWrapText(true)
                        .Text_Lambda([DeviceOwner,Field,On=SensorToolUiText::OptionState(Label,true),Off=SensorToolUiText::OptionState(Label,false)](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.*Field?On:Off;}); })]]
            +SHorizontalBox::Slot().AutoWidth()[Self->BuildOwnedSettingDetail(Key)]];
        Box->AddSlot().AutoHeight().Padding(0,0,0,6)[SNewDeviceTool(Self,STextBlock).AutoWrapText(true).ColorAndOpacity(Self->GetToolMutedColor()).Text(SensorLidarHelp::Summary(Key))];
    }); };
    FidelityToggle(Fidelity,LOCTEXT("LidarPointTime","점별 시간차"),&FVirtualSensorEditableState::bLidarPointTime,TEXT("PointTime"));
    FidelityToggle(Fidelity,LOCTEXT("LidarMotion","움직임 왜곡"),&FVirtualSensorEditableState::bLidarMotionDistortion,TEXT("MotionDistortion"));
    FidelityToggle(Fidelity,LOCTEXT("LidarMaterial","재질 반사"),&FVirtualSensorEditableState::bLidarMaterialResponse,TEXT("MaterialResponse"));
    Advanced->AddSlot().AutoHeight()[SNew(SBox).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); })[Fidelity]];
    Basic->AddSlot().AutoHeight()[LidarBasic];
    auto CameraAdvanced=SNew(SVerticalBox).Visibility_Lambda(CameraVisible).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); });
    CameraAdvanced->AddSlot().AutoHeight().Padding(0,6)[SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1)[SNew(SCheckBox).CheckBoxContentUsesAutoWidth(false).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); })
        .IsChecked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.bCameraGroundTruth?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}); })
        .OnCheckStateChanged_Lambda([DeviceOwner](ECheckBoxState V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.bCameraGroundTruth=V==ECheckBoxState::Checked;Self->ApplyPendingState();}); })
        [SNewDeviceTool(DeviceOwner,STextBlock).AutoWrapText(true).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.bCameraGroundTruth?LOCTEXT("CameraTruthOn","정답 데이터 · 켜짐"):LOCTEXT("CameraTruthOff","정답 데이터 · 꺼짐");}); })]]
      +SHorizontalBox::Slot().AutoWidth()[SNewDeviceTool(DeviceSelf,STextBlock).Text(LOCTEXT("CameraTruthHelp","ⓘ"))
        .ToolTipText(LOCTEXT("CameraTruthHelpText","기본 꺼짐 · 선택하면 즉시 적용합니다. 미리보기의 보기 설정에서 깊이·의미 분류·객체 구분을 확인하세요. GPU 캡처와 읽기 비용이 추가됩니다. 1회 저장은 Saved/CameraGroundTruth에 기록합니다. 서버 전송은 추가하지 않습니다."))]];

    CameraAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedCameraWidth","가로 해상도 (px)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.CameraResolution.X;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.CameraResolution.X=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },160,4096,TEXT("CameraWidth"))];
    CameraAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedCameraHeight","세로 해상도 (px)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.CameraResolution.Y;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.CameraResolution.Y=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },90,2160,TEXT("CameraHeight"))];
    CameraAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeFloatRow(LOCTEXT("OwnedCameraFov","수평 FOV (°)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.CameraFov;}); },[DeviceOwner](float V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.CameraFov=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },5,170,true,TEXT("CameraFov"))];
    Advanced->AddSlot().AutoHeight()[CameraAdvanced];
    auto LidarAdvanced=SNew(SVerticalBox).Visibility_Lambda(LidarVisible).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); });
    LidarAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedSamples","수평 샘플 수"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.LidarHorizontalSamples;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.LidarHorizontalSamples=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },1,1440,TEXT("HorizontalSamples"))];
    LidarAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedChannels","수직 채널 수"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.LidarVerticalChannels;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.LidarVerticalChannels=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },1,256,TEXT("VerticalChannels"))];
    LidarAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeFloatRow(LOCTEXT("OwnedHorizontalFov","수평 FOV (°)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.LidarHorizontalFov;}); },[DeviceOwner](float V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.LidarHorizontalFov=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },1,360,true,TEXT("HorizontalFov"))];
    LidarAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeFloatRow(LOCTEXT("OwnedVerticalMin","수직 최소 각도 (°)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.LidarMinVerticalAngle;}); },[DeviceOwner](float V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.LidarMinVerticalAngle=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },-90,89,true,TEXT("VerticalMin"))];
    LidarAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeFloatRow(LOCTEXT("OwnedVerticalMax","수직 최대 각도 (°)"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.LidarMaxVerticalAngle;}); },[DeviceOwner](float V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.LidarMaxVerticalAngle=V;Self->PendingState.SimulationQuality=EVirtualSensorSimulationQuality::Custom;}); },-89,90,true,TEXT("VerticalMax"))];
    LidarBasic->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedPreviewStride","미리보기 점 간격"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.PreviewPointStride;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.PreviewPointStride=V;}); },1,100,TEXT("PreviewStride"))];
    LidarBasic->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedPreviewMax","미리보기 최대 점 수"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.MaxPreviewPoints;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.MaxPreviewPoints=V;}); },0,1000000,TEXT("PreviewMax"))];
    const auto Toggle=[DeviceOwner](const FText& Label,bool FVirtualSensorEditableState::*Field,FName Key){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).CheckBoxContentUsesAutoWidth(false).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); }).ToolTipText(Self->GetDeviceSettingTooltip(Key)).IsChecked_Lambda([DeviceOwner,Field](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.*Field?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}); })
          .OnCheckStateChanged_Lambda([DeviceOwner,Field,Key](ECheckBoxState V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.*Field=V==ECheckBoxState::Checked;Self->SelectSettingHelp(Key);Self->ApplyPendingState();}); })[SNewDeviceTool(Self,STextBlock).AutoWrapText(true).Text(Label)]]
        ;}); };
    LidarBasic->AddSlot().AutoHeight()[Toggle(LOCTEXT("OwnedPreviewHitOnly","미리보기에서 검출점만 표시"),&FVirtualSensorEditableState::bPreviewHitOnly,TEXT("PreviewHitOnly"))];
    FidelityToggle(LidarAdvanced,LOCTEXT("OwnedMultiHit","다중 검출 사용"),&FVirtualSensorEditableState::bUseMultiHit,TEXT("MultiHit"));
    LidarAdvanced->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedMaxHits","광선당 최대 검출 수"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.MaxHitsPerRay;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.MaxHitsPerRay=V;}); },1,16,TEXT("MaxHits"))];
    Advanced->AddSlot().AutoHeight()[LidarAdvanced];
    auto CameraCompatibility=SNew(SVerticalBox).Visibility_Lambda(CameraVisible).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); });
    CameraCompatibility->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedJpeg","JPEG 품질"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.CameraJpegQuality;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.CameraJpegQuality=V;}); },1,100,TEXT("JpegQuality"))];
    Compatibility->AddSlot().AutoHeight()[CameraCompatibility];
    auto LidarCompatibility=SNew(SVerticalBox).Visibility_Lambda(LidarVisible).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); });
    LidarCompatibility->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedServerStride","서버 Payload 점 간격"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.ServerPayloadStride;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.ServerPayloadStride=V;}); },1,100,TEXT("ServerStride"))];
    LidarCompatibility->AddSlot().AutoHeight()[DeviceOwner->MakeIntRow(LOCTEXT("OwnedServerMax","서버 Payload 최대 점 수"),[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->PendingState.MaxServerPayloadPoints;}); },[DeviceOwner](int32 V){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->PendingState.MaxServerPayloadPoints=V;}); },0,1000000,TEXT("ServerMax"))];
    LidarCompatibility->AddSlot().AutoHeight()[Toggle(LOCTEXT("OwnedMiss","서버 Payload에 미검출점 포함"),&FVirtualSensorEditableState::bIncludeMissPointsInServerPayload,TEXT("IncludeMisses"))];
    Compatibility->AddSlot().AutoHeight()[LidarCompatibility];
    Compatibility->AddSlot().AutoHeight()[DeviceSelf->BuildOwnedAdministrativeControls()];
    Advanced->AddSlot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(true)
      .HeaderContent()[SNewDeviceTool(DeviceSelf,STextBlock).Text(LOCTEXT("OwnedCompatibilityDetails","호환 출력·관리"))].BodyContent()[Compatibility]];
    Automation->AddSlot().AutoHeight()[DeviceSelf->BuildDeviceSettingsExtension()];
    auto AutoFiles=SNew(SVerticalBox).Visibility_Lambda(LidarVisible).IsEnabled_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedCanEdit;}); });
    for(const auto& Item:TArray<TPair<FText,bool FVirtualSensorEditableState::*>>{{LOCTEXT("OwnedCsvAuto","스캔 시 자동 파일 저장 · CSV"),&FVirtualSensorEditableState::bExportCsvOnScan},{LOCTEXT("OwnedJsonAuto","스캔 시 자동 파일 저장 · JSONL"),&FVirtualSensorEditableState::bExportJsonLinesOnScan},{LOCTEXT("OwnedPcdAuto","스캔 시 자동 파일 저장 · PCD"),&FVirtualSensorEditableState::bExportPcdOnScan}})
        AutoFiles->AddSlot().AutoHeight()[Toggle(Item.Key,Item.Value,TEXT("AutoExport"))];
    Automation->AddSlot().AutoHeight().Padding(0,8)[AutoFiles];
    auto Tabs=SNew(SHorizontalBox);
    const FText TabLabels[]={LOCTEXT("OwnedBasic","기본"),SensorToolUiText::Advanced(),LOCTEXT("OwnedAutomationTab","자동화")};
    const FText TabHints[]={LOCTEXT("OwnedBasicTabHint","자주 쓰는 프로필·품질·측정·미리보기 설정입니다."),
        LOCTEXT("OwnedAdvancedTabHint","선택 장비의 정답·점별 시간차·움직임 왜곡·재질 반사·다중 검출과 부착 IMU를 셉니다."),
        LOCTEXT("OwnedAutomationTabHint","자동 제어(API·일괄·접수 작업)는 한 묶음입니다. 데이터셋 사용·변환 형식·자동 파일 저장 형식·미완료 테스트셋도 각각 셉니다.")};
    // UXR-12 review: a transparent button tint showed no selected tab. Use the same
    // segmented style as the other owned one-of-N choices.
    for(int32 I=0;I<3;++I)Tabs->AddSlot().FillWidth(1).Padding(0,0,I<2?FSensorOwnedPresentationStyle::Gap*.5f:0.f,0)[SNew(SCheckBox).Style(&FSensorOwnedPresentationStyle::Segment())
        .ToolTipText(TabHints[I])
        .IsChecked_Lambda([DeviceOwner,I](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->WorkspaceSettingsTab==I?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}); })
        .OnCheckStateChanged_Lambda([DeviceOwner,I](ECheckBoxState){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->SelectOwnedSettingsTab(I);}); })
        [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNewDeviceTool(DeviceSelf,STextBlock).Text(TabLabels[I]).ColorAndOpacity_Lambda([DeviceOwner,I](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->WorkspaceSettingsTab==I?Self->GetToolTextColor():DeviceOwner->GetToolMutedColor();}); })]
          +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNewDeviceTool(DeviceSelf,STextBlock).Justification(ETextJustify::Center)
            .Visibility_Lambda([DeviceOwner,I](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->OwnedTabEnabledCounts[I]>0?EVisibility::Visible:EVisibility::Collapsed;}); })
            .Text_Lambda([DeviceOwner,I](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->OwnedTabActivity[I];}); }).ColorAndOpacity(DeviceSelf->GetToolAccentColor())]]];
    return SNew(SBorder).BorderImage(DeviceSelf->GetToolPanelBrush()).BorderBackgroundColor(DeviceSelf->GetToolPanelColor()).ForegroundColor(DeviceSelf->GetToolTextColor()).Padding(FSensorOwnedPresentationStyle::PanelPadding)
      [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[DeviceSelf->BuildToolPanelHeader(LOCTEXT("OwnedSettingsTitle","센서 설정"))]
        +SVerticalBox::Slot().FillHeight(1)[SNew(SVerticalBox).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->GetPanelBodyVisibility();}); })
          +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Tabs]
          +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNewDeviceTool(DeviceOwner,STextBlock).AutoWrapText(true).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->OwnedSelectionText;}); })]
          +SVerticalBox::Slot().FillHeight(1)[SAssignNew(DeviceSelf->OwnedSettingsScroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[Basic]+SVerticalBox::Slot().AutoHeight()[Advanced]+SVerticalBox::Slot().AutoHeight()[Automation]
            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBorder).BorderImage(DeviceOwner->GetToolSectionBrush()).BorderBackgroundColor(DeviceOwner->GetToolSectionColor()).Padding(8).Visibility_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return Self->bOwnedSettingHelpOpen?EVisibility::Visible:EVisibility::Collapsed;}); })
              [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNewDeviceTool(DeviceOwner,STextBlock).AutoWrapText(true).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->OwnedHelpText;}); })]
                +SVerticalBox::Slot().AutoHeight()[SNewDeviceTool(DeviceOwner,STextBlock).AutoWrapText(true).Text_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return DeviceOwner->OwnedLoadText;}); })]
                +SVerticalBox::Slot().AutoHeight()[SNewDeviceTool(DeviceOwner,SButton).Text(LOCTEXT("OwnedCloseHelp","도움말 닫기")).OnClicked_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Self->bOwnedSettingHelpOpen=false;return FReply::Handled();}); })]]]
          ]]
          +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SAssignDeviceTool(DeviceSelf,DeviceSelf->NativeStatusText,STextBlock).AutoWrapText(true).ColorAndOpacity(DeviceSelf->GetToolMutedColor()).Text(DeviceSelf->OwnedStatusText)]
        ]
      ];

    }

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorSettingsPanelWidget"
    template<class TView> static TSharedRef<SWidget> MakeFloatRow(TView& View,const FText& Label, TFunction<float()> Getter, TFunction<void(float)> Setter, float Min, float Max, bool bApplyOnCommit, FName HelpKey, bool bTransformOnly)
    {
        TWeakObjectPtr<TView> DeviceOwner(&View);
        auto* DeviceSelf=&View;

    const FVirtualSensorSettingHelpDescriptor* Help = DeviceSelf->FindSettingHelp(HelpKey);
    const FText ToolTip = DeviceSelf->IsWorkspaceOwned()?DeviceSelf->GetDeviceSettingTooltip(HelpKey):Help ? FText::Format(LOCTEXT("SettingTipFormat", "{0}\n성능 영향: {1}\n권장값: {2}"), Help->Description, Help->PerformanceImpact, Help->RecommendedValue) : FText::GetEmpty();
    if(DeviceSelf->IsWorkspaceOwned())
    {
    auto Input=MakeShared<TSensorToolNumericInput<float>>(Min,Max);
    return DeviceSelf->BuildToolFormRow(Label,
        SNew(SSpinBox<float>).Style(&DeviceSelf->GetToolSpinBoxStyle())
        .TypeInterface(Input)
        .Font_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FSensorOwnedPresentationStyle::Font(FSensorOwnedPresentationStyle::BodySize,Self->GetSensorToolVisualScale());}); })
        .ToolTipText(ToolTip).MinValue(Min).MaxValue(Max).Value_Lambda([Getter](){return Getter();})
        .OnValueChanged_Lambda([Setter](float Value){Setter(Value);})
        .OnValueCommitted_Lambda([DeviceOwner, Setter, bApplyOnCommit, bTransformOnly](float Value,ETextCommit::Type){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Setter(Value);if (bApplyOnCommit) { if(bTransformOnly) Self->ApplyPendingTransform(); else Self->ApplyPendingState(); }}); })
        .OnEndSliderMovement_Lambda([DeviceOwner, Setter, bApplyOnCommit, bTransformOnly](float Value){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Setter(Value);if (bApplyOnCommit) { if(bTransformOnly) Self->ApplyPendingTransform(); else Self->ApplyPendingState(); }}); }),
        Help?DeviceSelf->BuildOwnedSettingDetail(HelpKey):SNullWidget::NullWidget,
        SensorToolNumericNotice<float>(Input,[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FSensorOwnedPresentationStyle::Font(FSensorOwnedPresentationStyle::LabelSize,Self->GetSensorToolVisualScale());}); }));
    }

    auto Row=SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.47f)[ SNewDeviceTool(DeviceSelf,STextBlock).Text(Label).AutoWrapText(DeviceSelf->IsWorkspaceOwned()).ToolTipText(ToolTip) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
        [
            SNewDeviceTool(DeviceSelf,SButton)
            .Visibility(Help ? EVisibility::Visible : EVisibility::Collapsed)
            .ButtonStyle(&DeviceSelf->GetToolButtonStyle())
            .ForegroundColor(DeviceSelf->GetToolAccentColor())
            .Text(LOCTEXT("InfoButton", "ⓘ"))
            .ToolTipText(ToolTip)
            .OnClicked_Lambda([DeviceOwner, HelpKey]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { if(Self->IsWorkspaceOwned())Self->OpenOwnedSettingHelp(HelpKey);else Self->SelectSettingHelp(HelpKey);return FReply::Handled(); }); })
        ]
        + SHorizontalBox::Slot().FillWidth(0.45f)
        [
            SNew(SSpinBox<float>)
            .Font_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { const int32 Size=Self->IsWorkspaceOwned()?14:10; auto Font=FCoreStyle::GetDefaultFontStyle("Regular",Size); Font.Size=FMath::RoundToInt(Size*Self->GetSensorToolFontScale()); return Font; }); })
            .MinValue(Min).MaxValue(Max)
            .Value_Lambda([Getter]() { return Getter(); })
            .OnValueChanged_Lambda([Setter](float Value) { Setter(Value); })
            .OnValueCommitted_Lambda([DeviceOwner, Setter, bApplyOnCommit, bTransformOnly](float Value, ETextCommit::Type) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { Setter(Value); if (bApplyOnCommit) { if(bTransformOnly) Self->ApplyPendingTransform(); else Self->ApplyPendingState(); } }); })
            .OnEndSliderMovement_Lambda([DeviceOwner, Setter, bApplyOnCommit, bTransformOnly](float Value) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { Setter(Value); if (bApplyOnCommit) { if(bTransformOnly) Self->ApplyPendingTransform(); else Self->ApplyPendingState(); } }); })
        ];
    return Row;

    }

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorSettingsPanelWidget"
    template<class TView> static TSharedRef<SWidget> MakeIntRow(TView& View,const FText& Label, TFunction<int32()> Getter, TFunction<void(int32)> Setter, int32 Min, int32 Max, FName HelpKey)
    {
        TWeakObjectPtr<TView> DeviceOwner(&View);
        auto* DeviceSelf=&View;

    const FVirtualSensorSettingHelpDescriptor* Help = DeviceSelf->FindSettingHelp(HelpKey);
    const FText ToolTip = DeviceSelf->IsWorkspaceOwned()?DeviceSelf->GetDeviceSettingTooltip(HelpKey):Help ? FText::Format(LOCTEXT("SettingIntTipFormat", "{0}\n성능 영향: {1}\n권장값: {2}"), Help->Description, Help->PerformanceImpact, Help->RecommendedValue) : FText::GetEmpty();
    if(DeviceSelf->IsWorkspaceOwned())
    {
    auto Input=MakeShared<TSensorToolNumericInput<int32>>(Min,Max);
    return DeviceSelf->BuildToolFormRow(Label,
        SNew(SSpinBox<int32>).Style(&DeviceSelf->GetToolSpinBoxStyle())
        .TypeInterface(Input)
        .Font_Lambda([DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FSensorOwnedPresentationStyle::Font(FSensorOwnedPresentationStyle::BodySize,Self->GetSensorToolVisualScale());}); })
        .ToolTipText(ToolTip).MinValue(Min).MaxValue(Max).Value_Lambda([Getter](){return Getter();})
        .OnValueChanged_Lambda([Setter](int32 Value){Setter(Value);})
        .OnValueCommitted_Lambda([DeviceOwner, Setter](int32 Value,ETextCommit::Type){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Setter(Value);Self->ApplyPendingState();}); })
        .OnEndSliderMovement_Lambda([DeviceOwner, Setter](int32 Value){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {Setter(Value);Self->ApplyPendingState();}); }),
        Help?DeviceSelf->BuildOwnedSettingDetail(HelpKey):SNullWidget::NullWidget,
        SensorToolNumericNotice<int32>(Input,[DeviceOwner](){ return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) {return FSensorOwnedPresentationStyle::Font(FSensorOwnedPresentationStyle::LabelSize,Self->GetSensorToolVisualScale());}); }));
    }

    auto Row=SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.47f)[ SNewDeviceTool(DeviceSelf,STextBlock).Text(Label).AutoWrapText(DeviceSelf->IsWorkspaceOwned()).ToolTipText(ToolTip) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
        [
            SNewDeviceTool(DeviceSelf,SButton)
            .Visibility(Help ? EVisibility::Visible : EVisibility::Collapsed)
            .ButtonStyle(&DeviceSelf->GetToolButtonStyle())
            .ForegroundColor(DeviceSelf->GetToolAccentColor())
            .Text(LOCTEXT("InfoIntButton", "ⓘ"))
            .ToolTipText(ToolTip)
            .OnClicked_Lambda([DeviceOwner, HelpKey]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { if(Self->IsWorkspaceOwned())Self->OpenOwnedSettingHelp(HelpKey);else Self->SelectSettingHelp(HelpKey);return FReply::Handled(); }); })
        ]
        + SHorizontalBox::Slot().FillWidth(0.45f)
        [
            SNew(SSpinBox<int32>)
            .Font_Lambda([DeviceOwner]() { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { const int32 Size=Self->IsWorkspaceOwned()?14:10; auto Font=FCoreStyle::GetDefaultFontStyle("Regular",Size); Font.Size=FMath::RoundToInt(Size*Self->GetSensorToolFontScale()); return Font; }); })
            .MinValue(Min).MaxValue(Max)
            .Value_Lambda([Getter]() { return Getter(); })
            .OnValueChanged_Lambda([Setter](int32 Value) { Setter(Value); })
            .OnValueCommitted_Lambda([DeviceOwner, Setter](int32 Value, ETextCommit::Type) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { Setter(Value); Self->ApplyPendingState(); }); })
            .OnEndSliderMovement_Lambda([DeviceOwner, Setter](int32 Value) { return DevicePanelBody::Invoke(DeviceOwner,[&](auto* Self) { Setter(Value); Self->ApplyPendingState(); }); })
        ];
    return Row;

    }
    static double CalculateCameraMegaPixelsPerSecond(const FVirtualSensorEditableState& State)
    {
    if (State.CameraCaptureInterval <= SMALL_NUMBER) return 0.0;
    const double RateHz = 1.0 / static_cast<double>(State.CameraCaptureInterval);
    return static_cast<double>(State.CameraResolution.X) * State.CameraResolution.Y * RateHz / 1000000.0;
}
    static double CalculateLidarRaysPerSecond(const FVirtualSensorEditableState& State)
    {
    if (State.LidarScanInterval <= SMALL_NUMBER) return 0.0;
    const double RateHz = 1.0 / static_cast<double>(State.LidarScanInterval);
    return static_cast<double>(State.LidarHorizontalSamples) * State.LidarVerticalChannels * RateHz;
}
    static bool ValidateEditableStateValues(const FVirtualSensorEditableState& State, const TArray<FString>& OtherSensorIds, FString& OutError)
    {
    if (State.SensorId.TrimStartAndEnd().IsEmpty()) { OutError = TEXT("SensorId가 비어 있습니다"); return false; }
    if (State.ActorTransform.ContainsNaN()) { OutError = TEXT("Transform에 유효하지 않은 숫자가 있습니다"); return false; }
    const FString Id=DeviceIdentityPolicy::Canonical(State.SensorId);
    if (OtherSensorIds.ContainsByPredicate([&](const FString& Other){return DeviceIdentityPolicy::Canonical(Other)==Id;})) { OutError = TEXT("이미 사용 중인 SensorId입니다"); return false; }
    if (State.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        if (!FMath::IsFinite(State.CameraCaptureInterval) || !FMath::IsFinite(State.CameraFov) ||
            State.CameraResolution.X < 160 || State.CameraResolution.X > 4096 || State.CameraResolution.Y < 90 || State.CameraResolution.Y > 2160 ||
            State.CameraCaptureInterval < 0.033f || State.CameraCaptureInterval > 60.0f || State.CameraFov < 5.0f || State.CameraFov > 170.0f ||
            State.CameraJpegQuality < 1 || State.CameraJpegQuality > 100)
        {
            OutError = TEXT("카메라 값이 지원 범위를 벗어났습니다");
            return false;
        }
    }
    else if (!FMath::IsFinite(State.LidarScanInterval) || !FMath::IsFinite(State.LidarMaxDistance) || !FMath::IsFinite(State.LidarHorizontalFov) ||
        !FMath::IsFinite(State.LidarMinVerticalAngle) || !FMath::IsFinite(State.LidarMaxVerticalAngle) ||
        State.LidarScanInterval < 0.033f || State.LidarScanInterval > 60.0f || State.LidarMaxDistance < 10.0f || State.LidarMaxDistance > 20000.0f ||
        State.LidarHorizontalSamples < 1 || State.LidarHorizontalSamples > 1440 || State.LidarVerticalChannels < 1 || State.LidarVerticalChannels > 256 ||
        State.LidarHorizontalFov < 1.0f || State.LidarHorizontalFov > 360.0f || State.LidarMinVerticalAngle < -90.0f || State.LidarMaxVerticalAngle > 90.0f ||
        State.LidarMinVerticalAngle >= State.LidarMaxVerticalAngle || State.ServerPayloadStride < 1 || State.ServerPayloadStride > 100 ||
        State.MaxServerPayloadPoints < 0 || State.MaxServerPayloadPoints > 1000000 || State.PreviewPointStride < 1 || State.PreviewPointStride > 100 ||
        State.MaxPreviewPoints < 0 || State.MaxPreviewPoints > 1000000 || State.MaxHitsPerRay < 1 || State.MaxHitsPerRay > 16)
    {
        OutError = TEXT("LiDAR 값이 지원 범위를 벗어났습니다"); return false;
    }
    return true;
}
};
#undef LOCTEXT_NAMESPACE
