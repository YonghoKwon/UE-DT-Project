#include "VirtualSensorMonitorPanelWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "SensorToolWidgetDecl.h"
#include "SensorToolUiText.h"
#include "VirtualSensorPlacementPanelWidget.h"
#include "Widgets/SOverlay.h"

#include "Async/Async.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "Styling/CoreStyle.h"
#include "TextureResource.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarHeight.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/RealSensorSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"

#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"

namespace
{


FString LidarProjectionDisplayText(ELidarMonitorProjectionMode Mode)
{
	if (Mode == ELidarMonitorProjectionMode::TopDown) return TEXT("센서 로컬 조감도");
	if (Mode == ELidarMonitorProjectionMode::WorldTopDown) return TEXT("월드 XY 조감도");
	if (Mode == ELidarMonitorProjectionMode::Elevation) return TEXT("방사 거리-높이 프로파일");
	if (Mode == ELidarMonitorProjectionMode::ForwardSlice) return TEXT("전방 수직 슬라이스");
    if (Mode == ELidarMonitorProjectionMode::Split) return TEXT("거리 영상 + 조감도");
    return TEXT("거리 영상");
}

FString LidarColorDisplayText(ELidarColorMode Mode)
{
    if (Mode == ELidarColorMode::DistanceViridis) return TEXT("거리 Viridis (색각 친화)");
    if (Mode == ELidarColorMode::RelativeHeight) return TEXT("높이 색상 (기준 선택)");
    if (Mode == ELidarColorMode::SemanticLabel) return TEXT("태그 기반 의미 분류");
    if (Mode == ELidarColorMode::GeometrySeparation) return TEXT("자동 형상 구분 — 태그 불필요");
    if (Mode == ELidarColorMode::VerticalChannel) return TEXT("수직 채널 / Ring");
    if (Mode == ELidarColorMode::ReturnIndex) return TEXT("MultiHit Return 번호");
    if (Mode == ELidarColorMode::HitMask) return TEXT("검출 마스크");
    if (Mode == ELidarColorMode::DistanceGray) return TEXT("거리 회색조");
    return TEXT("거리 Turbo");
}
}


FString LidarFidelitySummary(const UVirtualLidarScanComponent* Lidar)
{
    if (!Lidar) return TEXT("LiDAR 충실도 정보 없음");
    const TCHAR* ProfileBadge =
        Lidar->ProfileClass == EVirtualLidarProfileClass::PublicSpecNative ? TEXT("원본 사양")
        : Lidar->ProfileClass == EVirtualLidarProfileClass::IntegrationDownsampled ? TEXT("통합/다운샘플")
        : Lidar->ProfileClass == EVirtualLidarProfileClass::UserCustom ? TEXT("사용자 설정")
        : TEXT("일반");
    const TCHAR* Fidelity =
        Lidar->FidelityMode == EVirtualSensorFidelityMode::HardwareCalibrated ? TEXT("실장비 캘리브레이션")
        : Lidar->FidelityMode == EVirtualSensorFidelityMode::PublicSpecBased ? TEXT("공개 사양 기반")
        : Lidar->FidelityMode == EVirtualSensorFidelityMode::ReplayOrHardwareInput ? TEXT("Replay/실장비")
        : TEXT("Ideal Truth");
    const FVirtualSensorRuntimeStatus& Status = Lidar->GetRuntimeStatus();
    return FString::Printf(
        TEXT("프로필 %s · 충실도 %s · 프로토콜 %s\n")
        TEXT("요청 %.1fHz · 측정 %.1fHz · 출력 %.1fHz · deadline miss %d · backend %s"),
        ProfileBadge,
        Fidelity,
        Lidar->GetDeviceSpec().bProtocolVerifiedAgainstHardware ? TEXT("검증 완료") : TEXT("미검증"),
        Status.RequestedAcquisitionRateHz,
        Status.MeasuredAcquisitionRateHz,
        Status.MeasuredOutputRateHz,
        Status.DeadlineMissCount,
        *Status.ActiveAcquisitionBackend);
}

TSharedRef<SWidget> UVirtualSensorMonitorPanelWidget::RebuildWidget()
{
    if (!ShouldUseNativeFallbackWidget())
    {
        return Super::RebuildWidget();
    }

    RestoreMonitorUiPreferences();
	RefreshCameraSelectionOptions();
    NativeLidarProjectionOptions.Reset();
    NativeLidarProjectionOptions.Add(MakeShared<ELidarMonitorProjectionMode>(ELidarMonitorProjectionMode::RangeImage));
    NativeLidarProjectionOptions.Add(MakeShared<ELidarMonitorProjectionMode>(ELidarMonitorProjectionMode::TopDown));
    NativeLidarProjectionOptions.Add(MakeShared<ELidarMonitorProjectionMode>(ELidarMonitorProjectionMode::WorldTopDown));
    NativeLidarProjectionOptions.Add(MakeShared<ELidarMonitorProjectionMode>(ELidarMonitorProjectionMode::Elevation));
	NativeLidarProjectionOptions.Add(MakeShared<ELidarMonitorProjectionMode>(ELidarMonitorProjectionMode::ForwardSlice));
    NativeLidarProjectionOptions.Add(MakeShared<ELidarMonitorProjectionMode>(ELidarMonitorProjectionMode::Split));
    NativeLidarColorOptions.Reset();
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::DistanceTurbo));
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::DistanceViridis));
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::RelativeHeight));
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::SemanticLabel));
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::VerticalChannel));
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::ReturnIndex));
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::HitMask));
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::DistanceGray));
    NativeLidarColorOptions.Add(MakeShared<ELidarColorMode>(ELidarColorMode::GeometrySeparation));

    TSharedPtr<ELidarMonitorProjectionMode> InitialProjection = NativeLidarProjectionOptions[0];
    TSharedPtr<ELidarColorMode> InitialColor = NativeLidarColorOptions[0];
    if (const UVirtualSensorUiPreferencesSaveGame* Preferences = bPersistMonitorPreferences ? UVirtualSensorUiPreferencesSaveGame::LoadOrCreate() : nullptr)
    {
        for (const TSharedPtr<ELidarMonitorProjectionMode>& Option : NativeLidarProjectionOptions)
        {
            if (Option.IsValid() && *Option == Preferences->LidarProjectionMode) { InitialProjection = Option; break; }
        }
        for (const TSharedPtr<ELidarColorMode>& Option : NativeLidarColorOptions)
        {
            if (Option.IsValid() && *Option == Preferences->LidarColorMode) { InitialColor = Option; break; }
        }
    }
    if(IsWorkspaceOwned()) return BuildOwnedMonitorWidget(InitialProjection,InitialColor);
    return SNew(SBorder)
        .BorderImage(GetToolPanelBrush())
        .BorderBackgroundColor(GetToolPanelColor())
        .ForegroundColor(GetToolTextColor())
        .Padding(10.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[ BuildToolPanelHeader(LOCTEXT("WorkspaceMonitorTitle","모니터")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(SHorizontalBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
              + SHorizontalBox::Slot().FillWidth(1)[ SAssignSensorTool(NativeStatusTextBlock,STextBlock).AutoWrapText(true).Text(FText::FromString(BuildCompactStatusText())) ]
              + SHorizontalBox::Slot().AutoWidth()[SNewSensorTool(SButton).Text(LOCTEXT("ViewOptions","표시 옵션")).OnClicked_Lambda([this](){bWorkspaceViewOptions=!bWorkspaceViewOptions;return FReply::Handled();})]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [SAssignSensorTool(NativeWarningTextBlock,STextBlock).AutoWrapText(true).ColorAndOpacity(FVirtualSensorUiStyle::Warning).Text(FText::FromString(GetTransportWarningText())).Visibility_Lambda([this](){return !IsPanelCollapsed()&&NativeWarningTextBlock.IsValid()&&!NativeWarningTextBlock->GetText().IsEmpty()?EVisibility::Visible:EVisibility::Collapsed;})]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 8.0f, 0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                .Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
                + SHorizontalBox::Slot().FillWidth(0.61f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(GetToolPanelBrush())
                    .BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.025f, 1.0f))
                    .Padding(2.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().FillHeight(0.7f)
                        [ SAssignNew(NativeViewImage, SImage).Image(&NativeViewBrush) ]
                        + SVerticalBox::Slot().FillHeight(0.3f).Padding(0.0f, 2.0f, 0.0f, 0.0f)
                        [
                            SAssignNew(NativeSecondaryViewImage, SImage)
                            .Visibility_Lambda([this]() { return bShowingLidar && GetLidarProjectionMode() == ELidarMonitorProjectionMode::Split ? EVisibility::Visible : EVisibility::Collapsed; })
                            .Image(&NativeSecondaryViewBrush)
                        ]
						+ SVerticalBox::Slot().FillHeight(0.3f).Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SAssignNew(NativeSecondaryCameraImage, SImage)
							.Visibility_Lambda([this]() { return !bShowingLidar && bDualCameraModeEnabled && SecondaryCameraComp ? EVisibility::Visible : EVisibility::Collapsed; })
							.Image(&NativeSecondaryCameraBrush)
						]
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this](){return bWorkspaceViewOptions?0.39f:0.0f;}))
                [
                    SNew(SScrollBox).Visibility_Lambda([this](){return bWorkspaceViewOptions?EVisibility::Visible:EVisibility::Collapsed;})
                    + SScrollBox::Slot()
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolAccentColor()).Text_Lambda([this]() { return FText::FromString(GetSelectedSensorIdText()); }) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle())
                            .ForegroundColor(GetToolTextColor())
                            .Text_Lambda([this]() { return FText::FromString(bMonitorDetailsExpanded ? TEXT("상세 진단 접기") : TEXT("상세 진단 펼치기")); })
                            .OnClicked_Lambda([this]() { bMonitorDetailsExpanded = !bMonitorDetailsExpanded; SaveMonitorUiPreferences(); return FReply::Handled(); })
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                        [
                            SAssignSensorTool(NativeDetailedStatusTextBlock, STextBlock)
                            .Visibility_Lambda([this]() { return bMonitorDetailsExpanded ? EVisibility::Visible : EVisibility::Collapsed; })
                            .ColorAndOpacity(GetToolMutedColor())
                            .AutoWrapText(true)
                            .Text(FText::FromString(BuildStatusText()))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
						[
							SNewSensorTool(STextBlock)
							.Visibility_Lambda([this]() { return !bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
							.ColorAndOpacity(GetToolMutedColor())
							.Text(LOCTEXT("CameraViewModeLabel", "카메라 보기"))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SCheckBox)
							.Visibility_Lambda([this]() { return !bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
							.IsChecked_Lambda([this]() { return bDualCameraModeEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetDualCameraModeEnabled(State == ECheckBoxState::Checked); })
							[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(LOCTEXT("DualCameraMode", "카메라 2대 동시 보기")) ]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
						[
							SAssignNew(NativePrimaryCameraCombo, SComboBox<TSharedPtr<FString>>)
							.Visibility(EVisibility::Collapsed)
							.OptionsSource(&NativeCameraOptions)
							.OnGenerateWidget_Lambda([this](TSharedPtr<FString> Item) { return SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(FText::FromString(Item.IsValid() ? *Item : TEXT("없음"))); })
							.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { if (Item.IsValid()) SelectPrimaryCameraBySensorId(*Item); })
							[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("주 카메라: %s"), PreferredPrimaryCameraId.IsEmpty() ? TEXT("없음") : *PreferredPrimaryCameraId)); }) ]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
						[
							SAssignNew(NativeSecondaryCameraCombo, SComboBox<TSharedPtr<FString>>)
							.Visibility_Lambda([this]() { return !bShowingLidar && bDualCameraModeEnabled ? EVisibility::Visible : EVisibility::Collapsed; })
							.OptionsSource(&NativeCameraOptions)
							.OnGenerateWidget_Lambda([this](TSharedPtr<FString> Item) { return SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(FText::FromString(Item.IsValid() ? *Item : TEXT("없음"))); })
							.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { if (Item.IsValid()) SelectSecondaryCameraBySensorId(*Item); })
							[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("보조 카메라: %s"), PreferredSecondaryCameraId.IsEmpty() ? TEXT("없음") : *PreferredSecondaryCameraId)); }) ]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
                        [
                            SNewSensorTool(STextBlock)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            .ColorAndOpacity(GetToolMutedColor())
                            .Text(LOCTEXT("LidarModeLabel", "LiDAR 표시 방식"))
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SComboBox<TSharedPtr<ELidarMonitorProjectionMode>>)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            .OptionsSource(&NativeLidarProjectionOptions)
                            .InitiallySelectedItem(InitialProjection)
                            .OnGenerateWidget_Lambda([this](TSharedPtr<ELidarMonitorProjectionMode> Item)
                            {
                                return SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(FText::FromString(Item.IsValid() ? LidarProjectionDisplayText(*Item) : TEXT("없음")));
                            })
                            .OnSelectionChanged_Lambda([this](TSharedPtr<ELidarMonitorProjectionMode> Item, ESelectInfo::Type)
                            {
                                if (Item.IsValid()) SetLidarProjectionMode(*Item);
                            })
                            [ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text_Lambda([this]() { return FText::FromString(LidarProjectionDisplayText(GetLidarProjectionMode())); }) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 2.0f)
                        [
                            SNewSensorTool(STextBlock).Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            .ColorAndOpacity(GetToolMutedColor()).Text(LOCTEXT("LidarColorLabel", "LiDAR 색상 방식"))
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SComboBox<TSharedPtr<ELidarColorMode>>)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            .OptionsSource(&NativeLidarColorOptions).InitiallySelectedItem(InitialColor)
                            .OnGenerateWidget_Lambda([this](TSharedPtr<ELidarColorMode> Item)
                            {
                                return SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(FText::FromString(Item.IsValid() ? LidarColorDisplayText(*Item) : TEXT("없음")));
                            })
                            .OnSelectionChanged_Lambda([this](TSharedPtr<ELidarColorMode> Item, ESelectInfo::Type)
                            {
                                if (Item.IsValid()) SetLidarColorMode(*Item);
                            })
                            [ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text_Lambda([this]() { return FText::FromString(LidarColorDisplayText(GetLidarColorMode())); }) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(SVerticalBox).Visibility_Lambda([this]() { return bShowingLidar && GetLidarColorMode() == ELidarColorMode::RelativeHeight ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SVerticalBox::Slot().AutoHeight()
                            [ SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle())
                                .Text_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); return FText::FromString(V && VirtualLidarHeight::IsWorld(V->Settings) ? TEXT("높이 기준: 월드 Z (클릭 전환)") : TEXT("높이 기준: 센서 로컬 Z (클릭 전환)")); })
                                .OnClicked_Lambda([this]() { if (const auto* V = GetLidarVisualizationComponent()) { const auto S = V->Settings; SetLidarHeightDisplay(VirtualLidarHeight::IsWorld(S) ? ELidarHeightReference::SensorLocalZ : ELidarHeightReference::WorldZ, S.bAutoHeightRange, S.HeightMinMeters, S.HeightMaxMeters); } return FReply::Handled(); }) ]
                            + SVerticalBox::Slot().AutoHeight()
                            [ SNew(SCheckBox).IsChecked_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); return V && V->Settings.bAutoHeightRange ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                                .OnCheckStateChanged_Lambda([this](ECheckBoxState State) { if (const auto* V = GetLidarVisualizationComponent()) { const auto S = V->Settings; SetLidarHeightDisplay(S.HeightReference, State == ECheckBoxState::Checked, S.HeightMinMeters, S.HeightMaxMeters); } })
                                [ SNewSensorTool(STextBlock).Text(LOCTEXT("AutoHeight", "높이 범위 자동 (수동 입력 단위: m)")) ] ]
                            + SVerticalBox::Slot().AutoHeight()
                            [ SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1)[ SNewSensorTool(SEditableTextBox).HintText(LOCTEXT("HeightMin", "최소 높이(m)"))
                                    .Text_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); return FText::FromString(FString::SanitizeFloat(V ? V->Settings.HeightMinMeters : 0.f)); })
                                    .OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { float Value; if (const auto* V = GetLidarVisualizationComponent(); V && LexTryParseString(Value, *T.ToString())) { const auto S = V->Settings; SetLidarHeightDisplay(S.HeightReference, false, Value, S.HeightMaxMeters); } }) ]
                                + SHorizontalBox::Slot().FillWidth(1)[ SNewSensorTool(SEditableTextBox).HintText(LOCTEXT("HeightMax", "최대 높이(m)"))
                                    .Text_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); return FText::FromString(FString::SanitizeFloat(V ? V->Settings.HeightMaxMeters : 1.f)); })
                                    .OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { float Value; if (const auto* V = GetLidarVisualizationComponent(); V && LexTryParseString(Value, *T.ToString())) { const auto S = V->Settings; SetLidarHeightDisplay(S.HeightReference, false, S.HeightMinMeters, Value); } }) ]
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true).Visibility_Lambda([this](){return bShowingLidar?EVisibility::Visible:EVisibility::Collapsed;}).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("LegendFold","범례 · 표시 설명"))].BodyContent()[SNew(SVerticalBox)
+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 2.0f)
                        [
                            SNew(SHorizontalBox)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ SNew(SBox).WidthOverride(28.0f).HeightOverride(12.0f)[ SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor_Lambda([this]() { return GetLidarLegendSwatchColor(0); }) ] ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ SNew(SBox).WidthOverride(28.0f).HeightOverride(12.0f)[ SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor_Lambda([this]() { return GetLidarLegendSwatchColor(1); }) ] ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ SNew(SBox).WidthOverride(28.0f).HeightOverride(12.0f)[ SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor_Lambda([this]() { return GetLidarLegendSwatchColor(2); }) ] ]
                            + SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(28.0f).HeightOverride(12.0f)[ SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor_Lambda([this]() { return GetLidarLegendSwatchColor(3); }) ] ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 4.0f)
                        [ SNewSensorTool(STextBlock).Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; }).ColorAndOpacity(GetToolMutedColor()).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(GetLidarViewModeDescription() + TEXT("\n") + GetLidarViewLegendText()); }) ]

]]
+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 4.0f)
                        [
                            SNew(SHorizontalBox).Visibility_Lambda([this]() { const auto Mode = GetLidarProjectionMode(); return bShowingLidar && Mode != ELidarMonitorProjectionMode::RangeImage ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).AutoWrapText(true).Text(LOCTEXT("ProjectionNavigationHelp", "보기에서 좌클릭 드래그=이동 · 우클릭 드래그=회전 · 휠=확대/축소")) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
                            [ SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("ResetProjectionView", "보기 초기화")).OnClicked_Lambda([this]() { if (auto* V = GetLidarVisualizationComponent()) V->ResetProjectionView(GetLidarProjectionMode()); return FReply::Handled(); }) ]
                        ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(SHorizontalBox)
							.Visibility_Lambda([this]() { return bShowingLidar && GetLidarProjectionMode() == ELidarMonitorProjectionMode::WorldTopDown ? EVisibility::Visible : EVisibility::Collapsed; })
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(SCheckBox)
								.ToolTipText(LOCTEXT("WorldTopDownAutoFitTip", "현재 프레임의 검출점 월드 XY 경계를 찾아 화면에 자동으로 맞춥니다."))
								.IsChecked_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); return V && V->GetVisualizationSettings().bWorldTopDownAutoFit ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetLidarWorldTopDownAutoFit(State == ECheckBoxState::Checked); })
								[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(LOCTEXT("WorldTopDownAutoFit", "검출점 자동 맞춤")) ]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
							[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text(LOCTEXT("WorldTopDownAxes", "가로=월드 Y · 세로=월드 X")) ]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(SHorizontalBox)
							.Visibility_Lambda([this]() { return bShowingLidar && GetLidarProjectionMode() == ELidarMonitorProjectionMode::ForwardSlice ? EVisibility::Visible : EVisibility::Collapsed; })
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); return FText::FromString(FString::Printf(TEXT("슬라이스 두께 %.2fm"), V ? V->GetVisualizationSettings().ForwardSliceThicknessCm * 0.01f : 1.0f)); }) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)
							[ SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("SliceThinner", "두께 -")).OnClicked_Lambda([this]() { if (auto* V = GetLidarVisualizationComponent()) { auto S = V->GetVisualizationSettings(); S.ForwardSliceThicknessCm = FMath::Max(10.0f, S.ForwardSliceThicknessCm - 25.0f); V->SetVisualizationSettings(S); } return FReply::Handled(); }) ]
							+ SHorizontalBox::Slot().AutoWidth()
							[ SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("SliceThicker", "두께 +")).OnClicked_Lambda([this]() { if (auto* V = GetLidarVisualizationComponent()) { auto S = V->GetVisualizationSettings(); S.ForwardSliceThicknessCm = FMath::Min(10000.0f, S.ForwardSliceThicknessCm + 25.0f); V->SetVisualizationSettings(S); } return FReply::Handled(); }) ]
						]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                        [
                            SNew(SHorizontalBox)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [
                                SNew(SCheckBox)
                                .ToolTipText(LOCTEXT("WorldPointsTip", "선택한 LiDAR의 최신 검출점을 월드 공간 3D 포인트로 표시합니다."))
                                .IsChecked_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); return V && V->GetVisualizationSettings().bShowWorldPointCloud ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                                .OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SetLidarWorldPointCloudEnabled(V == ECheckBoxState::Checked); })
                                [ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(LOCTEXT("WorldPoints", "월드 3D 포인트 표시")) ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)
                            [ SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("PointSmaller", "포인트 -")).OnClicked_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); SetLidarPointSize(V ? V->GetVisualizationSettings().PointSize - 0.5f : 1.5f); return FReply::Handled(); }) ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [ SNewSensorTool(SButton).ButtonStyle(&GetToolButtonStyle()).Text(LOCTEXT("PointLarger", "포인트 +")).OnClicked_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); SetLidarPointSize(V ? V->GetVisualizationSettings().PointSize + 0.5f : 2.5f); return FReply::Handled(); }) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SWrapBox).UseAllottedSize(true).Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SWrapBox::Slot()[ SNew(SCheckBox).ToolTipText(LOCTEXT("AdaptiveTip", "현재 프레임의 검출 거리 범위를 기준으로 색상 대비를 자동 조정합니다.")).IsChecked_Lambda([this]() { return bUseAdaptiveLidarDepthRange ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SetLidarOverlayOptions(V == ECheckBoxState::Checked, bOverlayLidarMonitorGrid, bOverlayLidarDepthEdges); })[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(LOCTEXT("Adaptive", "적응형 거리")) ] ]
                            + SWrapBox::Slot()[ SNew(SCheckBox).ToolTipText(LOCTEXT("EdgeTip", "인접 측정점의 거리 차이가 큰 경계를 흰색 선으로 강조합니다.")).IsChecked_Lambda([this]() { return bOverlayLidarDepthEdges ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SetLidarOverlayOptions(bUseAdaptiveLidarDepthRange, bOverlayLidarMonitorGrid, V == ECheckBoxState::Checked); })[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(LOCTEXT("Edges", "깊이 경계")) ] ]
                            + SWrapBox::Slot()[ SNew(SCheckBox).ToolTipText(LOCTEXT("GridTip", "LiDAR 행과 열의 간격을 확인할 수 있도록 기준 격자를 겹쳐 표시합니다.")).IsChecked_Lambda([this]() { return bOverlayLidarMonitorGrid ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SetLidarOverlayOptions(bUseAdaptiveLidarDepthRange, V == ECheckBoxState::Checked, bOverlayLidarDepthEdges); })[ SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(LOCTEXT("Grid", "격자")) ] ]
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SWrapBox).UseAllottedSize(true).Visibility_Lambda([this]() { return bShowingLidar?GetPanelBodyVisibility():EVisibility::Collapsed; })
                + SWrapBox::Slot()
                [
                    SNewSensorTool(SButton)
                    .ButtonStyle(&GetToolButtonStyle())
                    .ForegroundColor(GetToolTextColor())
                    .OnClicked_Lambda([this]() { HandlePointCloudOnlyButtonClicked(); RefreshNativeFallbackText(); return FReply::Handled(); })
                    [
                        SNewSensorTool(STextBlock)
                        .ColorAndOpacity(GetToolTextColor())
                        .Text_Lambda([this]() { return BuildPointCloudOnlyButtonText(); })
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
            [
                SNewSensorTool(STextBlock)
                .Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
                .ColorAndOpacity(GetToolMutedColor())
                .Text(LOCTEXT("MonitorResizeHint", "◢"))
                .ToolTipText(LOCTEXT("MonitorResizeHintTip", "패널 오른쪽 아래를 드래그해 가로와 세로 크기를 자유롭게 조절합니다."))
            ]
        ];
}
void UVirtualSensorMonitorPanelWidget::RefreshOwnedMonitorText()
{
    if(!IsWorkspaceOwned()||(WidgetTree&&WidgetTree->RootWidget)||!IsVisible()||IsPanelCollapsed())return;
    const auto SetText=[](FText& Cached,const FString& Value){if(Cached.ToString()!=Value)Cached=FText::FromString(Value);};
    auto* Actor=GetOwnedMonitoredActor();OwnedPreviewTarget=Actor;
    bOwnedHasEquipment=IsValid(Actor);bOwnedPreviewEmpty=!bOwnedHasEquipment||!Actor->IsSensorRunning();
    const FString Warning=BuildOwnedMeasurementWarning();bOwnedHasWarning=!Warning.IsEmpty();
    FString Status=bOwnedHasEquipment?BuildCompactStatusText():TEXT("장비 없음 · 배치에서 장비를 추가하세요");
    if(bOwnedHasWarning)Status+=TEXT(" · ⚠ ")+Warning;
    SetText(OwnedMonitorStatus,Status);if(NativeStatusTextBlock){NativeStatusTextBlock->SetText(OwnedMonitorStatus);NativeStatusTextBlock->SetToolTipText(OwnedMonitorStatus);}
    auto* Workspace=GetToolWorkspace();auto* Placement=Workspace?Cast<UVirtualSensorPlacementPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Placement)):nullptr;
    FString Reason;bOwnedCanStart=bOwnedHasEquipment&&Placement&&SensorManager&&SensorManager->GetSensorActors().Contains(Actor)&&Actor->CanEditSensorPlacement(Reason);
    SetText(OwnedEmptyText,bOwnedHasEquipment?TEXT("측정 중지 상태입니다"):TEXT("장비가 없습니다 · 배치에서 장비를 추가하세요"));
    OwnedEmptyActionText=bOwnedHasEquipment?LOCTEXT("OwnedStartMeasurement","측정 시작"):LOCTEXT("OwnedAddEquipment","장비 추가");
    SetText(OwnedEmptyActionTip,bOwnedHasEquipment?bOwnedCanStart?FString(TEXT("배치 창과 같은 측정 시작·잠금 규칙을 사용합니다")):SensorToolUiText::UserReason(Reason.IsEmpty()?FString(TEXT("배치 창과 장비 연결을 확인하세요")):Reason):FString(TEXT("기존 배치 창에서 장비를 추가합니다")));
    if(bOwnedAdvancedMenuOpen)
    {
        SetText(OwnedProjectionText,TEXT("투영 · ")+LidarProjectionDisplayText(GetLidarProjectionMode()));SetText(OwnedColorText,TEXT("색상 · ")+LidarColorDisplayText(GetLidarColorMode()));SetText(OwnedSecondaryCameraText,TEXT("보조 카메라 · ")+PreferredSecondaryCameraId);
        const auto* Visualization=GetLidarVisualizationComponent();
        if(bShowingLidar&&Visualization)
        {
            const auto& View=Visualization->GetVisualizationSettings();
            if(GetLidarColorMode()==ELidarColorMode::RelativeHeight){OwnedHeightMinText=FText::AsNumber(View.HeightMinMeters);OwnedHeightMaxText=FText::AsNumber(View.HeightMaxMeters);}
            if(GetLidarProjectionMode()==ELidarMonitorProjectionMode::ForwardSlice)OwnedSliceWidthText=FText::AsNumber(View.ForwardSliceThicknessCm);
            if(GetMonitorPresentation()!=EVirtualSensorMonitorPresentation::TwoDimensional)OwnedPointSizeText=FText::AsNumber(View.PointSize);
        }
        if(bOwnedLegendExpanded)SetText(OwnedLegendText,GetLidarViewModeDescription()+TEXT("\n")+GetLidarViewLegendText()+TEXT("\n드래그: 이동 · 우클릭: 회전 · 휠: 확대/축소"));
        if(bMonitorDetailsExpanded)
        {
            if(NativeDetailedStatusTextBlock)NativeDetailedStatusTextBlock->SetText(FText::FromString(BuildStatusText()));
            SetText(OwnedRendererText,GetPointCloudRendererStatusText());
        }
    }
}

TSharedRef<SWidget> UVirtualSensorMonitorPanelWidget::BuildOwnedMonitorWidget(TSharedPtr<ELidarMonitorProjectionMode> Projection,TSharedPtr<ELidarColorMode> Color)
{
    RefreshOwnedMonitorText();
    NativePresentationOptions={MakeShared<EVirtualSensorMonitorPresentation>(EVirtualSensorMonitorPresentation::TwoDimensional),MakeShared<EVirtualSensorMonitorPresentation>(EVirtualSensorMonitorPresentation::WorldOverlay),MakeShared<EVirtualSensorMonitorPresentation>(EVirtualSensorMonitorPresentation::PointCloudOnly)};
    auto PresentationText=[](EVirtualSensorMonitorPresentation P){return P==EVirtualSensorMonitorPresentation::PointCloudOnly?LOCTEXT("OnlyPoints","포인트 전용"):P==EVirtualSensorMonitorPresentation::WorldOverlay?LOCTEXT("OverlayPoints","2D + 월드 포인트"):LOCTEXT("Only2D","2D");};
    auto Quick=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,5)).Visibility_Lambda([this](){return bShowingLidar?GetPanelBodyVisibility():EVisibility::Collapsed;});
    Quick->AddSlot()[SNew(SComboBox<TSharedPtr<ELidarMonitorProjectionMode>>).OptionsSource(&NativeLidarProjectionOptions).InitiallySelectedItem(Projection)
      .OnGenerateWidget_Lambda([this](TSharedPtr<ELidarMonitorProjectionMode> P){return SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(FText::FromString(P?LidarProjectionDisplayText(*P):TEXT("없음")));})
      .OnSelectionChanged_Lambda([this](TSharedPtr<ELidarMonitorProjectionMode> P,ESelectInfo::Type){if(P)SetLidarProjectionMode(*P);})
      [SNewSensorTool(STextBlock).Text_Lambda([this](){return OwnedProjectionText;})]];
    Quick->AddSlot()[SNew(SComboBox<TSharedPtr<ELidarColorMode>>).OptionsSource(&NativeLidarColorOptions).InitiallySelectedItem(Color)
      .OnGenerateWidget_Lambda([this](TSharedPtr<ELidarColorMode> C){return SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(FText::FromString(C?LidarColorDisplayText(*C):TEXT("없음")));})
      .OnSelectionChanged_Lambda([this](TSharedPtr<ELidarColorMode> C,ESelectInfo::Type){if(C)SetLidarColorMode(*C);})
      [SNewSensorTool(STextBlock).Text_Lambda([this](){return OwnedColorText;})]];
    Quick->AddSlot()[SNew(SComboBox<TSharedPtr<EVirtualSensorMonitorPresentation>>).OptionsSource(&NativePresentationOptions)
      .OnGenerateWidget_Lambda([this,PresentationText](TSharedPtr<EVirtualSensorMonitorPresentation> P){return SNewSensorTool(STextBlock).ColorAndOpacity(GetToolTextColor()).Text(P?PresentationText(*P):FText());})
      .OnSelectionChanged_Lambda([this](TSharedPtr<EVirtualSensorMonitorPresentation> P,ESelectInfo::Type){if(P)SetMonitorPresentation(*P);})
      [SNewSensorTool(STextBlock).Text_Lambda([this,PresentationText](){return PresentationText(GetMonitorPresentation());})]];
    OwnedQuickViewControls=Quick;
    return SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).ForegroundColor(GetToolTextColor()).Padding(10)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[BuildToolPanelHeader(SensorToolUiText::Preview())]
     +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SHorizontalBox).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
       +SHorizontalBox::Slot().FillWidth(1)[SAssignSensorTool(NativeStatusTextBlock,STextBlock).AutoWrapText(false).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ColorAndOpacity_Lambda([this](){return bOwnedHasWarning?FVirtualSensorUiStyle::Warning:GetToolTextColor();}).Text(OwnedMonitorStatus)]
       +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SComboButton).ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).ButtonStyle(&FSensorOwnedPresentationStyle::HeaderButton()).HasDownArrow(false).ToolTipText(LOCTEXT("AdvancedViewTip","보기 설정")).OnGetMenuContent_UObject(this,&ThisClass::BuildOwnedAdvancedViewMenu).OnMenuOpenChanged_Lambda([this](bool Open){bOwnedAdvancedMenuOpen=Open;}).ButtonContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("AdvancedViewIcon","..."))]]]
     +SVerticalBox::Slot().FillHeight(1).Padding(0,8,0,2)[SNew(SBorder).BorderImage(GetToolSectionBrush()).BorderBackgroundColor(FLinearColor(.01,.015,.025,1)).Padding(2).Visibility_Lambda([this](){return GetPanelBodyVisibility();})
       [SNew(SVerticalBox)
        +SVerticalBox::Slot().FillHeight(.7f)[SNew(SOverlay)
          +SOverlay::Slot()[SAssignNew(NativeViewImage,SImage).Visibility_Lambda([this](){return bOwnedPreviewEmpty?EVisibility::Collapsed:EVisibility::Visible;}).Image(&NativeViewBrush)]
          +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride_Lambda([this](){return FMath::Clamp(float(GetEffectivePanelSize().X)-60.f,100.f,600.f);}).Visibility_Lambda([this](){return bOwnedPreviewEmpty?EVisibility::Visible:EVisibility::Collapsed;})[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(8)[SNewSensorTool(STextBlock).AutoWrapText(true).Justification(ETextJustify::Center).Text_Lambda([this](){return OwnedEmptyText;})]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNewSensorTool(SButton).IsEnabled_Lambda([this](){return !bOwnedHasEquipment||bOwnedCanStart;}).Text_Lambda([this](){return OwnedEmptyActionText;}).ToolTipText_Lambda([this](){return OwnedEmptyActionTip;})
              .OnClicked_Lambda([this](){if(bOwnedHasEquipment)StartOwnedMonitoredEquipment();else OpenOwnedEquipmentPlacement();return FReply::Handled();})]
          ]]]
        +SVerticalBox::Slot().FillHeight(.3f).Padding(0,2)[SAssignNew(NativeSecondaryViewImage,SImage).Visibility_Lambda([this](){return !bOwnedPreviewEmpty&&bShowingLidar&&GetLidarProjectionMode()==ELidarMonitorProjectionMode::Split?EVisibility::Visible:EVisibility::Collapsed;}).Image(&NativeSecondaryViewBrush)]
        +SVerticalBox::Slot().FillHeight(.3f).Padding(0,2)[SAssignNew(NativeSecondaryCameraImage,SImage).Visibility_Lambda([this](){return bOwnedHasEquipment&&!bShowingLidar&&bDualCameraModeEnabled&&SecondaryCameraComp?EVisibility::Visible:EVisibility::Collapsed;}).Image(&NativeSecondaryCameraBrush)]
       ]]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNewSensorTool(STextBlock).Visibility_Lambda([this](){return IsWorkspaceDocked()?EVisibility::Collapsed:EVisibility::Visible;}).Text(LOCTEXT("OwnedGrip","◢")).ToolTipText(LOCTEXT("OwnedGripTip","모서리를 드래그해 미리보기 크기를 조절합니다."))]
    ];
}
TSharedRef<SWidget> UVirtualSensorMonitorPanelWidget::BuildOwnedAdvancedViewMenu()
{
    bOwnedLegendExpanded=false;
    OwnedProjectionText=FText::FromString(TEXT("투영 · ")+LidarProjectionDisplayText(GetLidarProjectionMode()));
    OwnedColorText=FText::FromString(TEXT("색상 · ")+LidarColorDisplayText(GetLidarColorMode()));
    OwnedSecondaryCameraText=FText::FromString(TEXT("보조 카메라 · ")+PreferredSecondaryCameraId);
    const auto* Visualization=GetLidarVisualizationComponent();const auto View=Visualization?Visualization->GetVisualizationSettings():FVirtualLidarVisualizationSettings();
    OwnedHeightMinText=FText::AsNumber(View.HeightMinMeters);OwnedHeightMaxText=FText::AsNumber(View.HeightMaxMeters);OwnedSliceWidthText=FText::AsNumber(View.ForwardSliceThicknessCm);OwnedPointSizeText=FText::AsNumber(View.PointSize);
    RefreshCameraSelectionOptions();
    auto Content=SNew(SVerticalBox);
    if(OwnedQuickViewControls)Content->AddSlot().AutoHeight().Padding(0,8)[OwnedQuickViewControls.ToSharedRef()];
    Content->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).Text(LOCTEXT("ViewHelp","표시만 변경합니다. 측정값·PCD 출력은 바꾸지 않습니다.")).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor())];
    Content->AddSlot().AutoHeight().Padding(0,8)[SNew(SVerticalBox).Visibility_Lambda([this](){return bShowingLidar?EVisibility::Collapsed:EVisibility::Visible;})
      +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).IsChecked_Lambda([this](){return bDualCameraModeEnabled?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){SetDualCameraModeEnabled(S==ECheckBoxState::Checked);})[SNewSensorTool(STextBlock).Text(LOCTEXT("DualOwned","카메라 2대 동시 보기"))]]
      +SVerticalBox::Slot().AutoHeight()[SAssignNew(NativeSecondaryCameraCombo,SComboBox<TSharedPtr<FString>>).OptionsSource(&NativeCameraOptions).Visibility_Lambda([this](){return bDualCameraModeEnabled?EVisibility::Visible:EVisibility::Collapsed;})
        .OnGenerateWidget_Lambda([this](TSharedPtr<FString> I){return SNewSensorTool(STextBlock).Text(FText::FromString(I?*I:TEXT("없음")));})
        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> I,ESelectInfo::Type){if(I)SelectSecondaryCameraBySensorId(*I);})
        [SNewSensorTool(STextBlock).Text_Lambda([this](){return OwnedSecondaryCameraText;})]]];
    auto Lidar=SNew(SVerticalBox).Visibility_Lambda([this](){return bShowingLidar?EVisibility::Visible:EVisibility::Collapsed;});
    Lidar->AddSlot().AutoHeight().Padding(0,6)[SNew(SCheckBox).Visibility_Lambda([this](){return SupportsAdaptiveDistance(GetLidarColorMode())?EVisibility::Visible:EVisibility::Collapsed;})
      .IsChecked_Lambda([this](){return bUseAdaptiveLidarDepthRange?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){SetLidarOverlayOptions(S==ECheckBoxState::Checked,bOverlayLidarMonitorGrid,bOverlayLidarDepthEdges);})[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedAdaptive","거리 색상 대비 자동 조정"))]];
    Lidar->AddSlot().AutoHeight()[SNew(SHorizontalBox).Visibility_Lambda([this](){return SupportsRangeOverlays(GetLidarProjectionMode())?EVisibility::Visible:EVisibility::Collapsed;})
      +SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsChecked_Lambda([this](){return bOverlayLidarMonitorGrid?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){SetLidarOverlayOptions(bUseAdaptiveLidarDepthRange,S==ECheckBoxState::Checked,bOverlayLidarDepthEdges);})[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedGrid","거리 영상 격자"))]]
      +SHorizontalBox::Slot().AutoWidth().Padding(12,0)[SNew(SCheckBox).IsChecked_Lambda([this](){return bOverlayLidarDepthEdges?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){SetLidarOverlayOptions(bUseAdaptiveLidarDepthRange,bOverlayLidarMonitorGrid,S==ECheckBoxState::Checked);})[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedEdges","깊이 경계"))]]];
    auto Height=SNew(SVerticalBox).Visibility_Lambda([this](){return GetLidarColorMode()==ELidarColorMode::RelativeHeight?EVisibility::Visible:EVisibility::Collapsed;});
    Height->AddSlot().AutoHeight().Padding(0,6)[SNewSensorTool(SButton).Text_Lambda([this](){const auto* V=GetLidarVisualizationComponent();return V&&VirtualLidarHeight::IsWorld(V->GetVisualizationSettings())?LOCTEXT("OwnedWorldHeight","높이 기준 · 월드 Z"):LOCTEXT("OwnedLocalHeight","높이 기준 · 센서 로컬 Z");})
      .OnClicked_Lambda([this](){if(const auto* V=GetLidarVisualizationComponent()){const auto S=V->GetVisualizationSettings();SetLidarHeightDisplay(VirtualLidarHeight::IsWorld(S)?ELidarHeightReference::SensorLocalZ:ELidarHeightReference::WorldZ,S.bAutoHeightRange,S.HeightMinMeters,S.HeightMaxMeters);}return FReply::Handled();})];
    Height->AddSlot().AutoHeight()[SNew(SCheckBox).IsChecked_Lambda([this](){const auto* V=GetLidarVisualizationComponent();return V&&V->GetVisualizationSettings().bAutoHeightRange?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
      .OnCheckStateChanged_Lambda([this](ECheckBoxState C){if(const auto* V=GetLidarVisualizationComponent()){const auto S=V->GetVisualizationSettings();SetLidarHeightDisplay(S.HeightReference,C==ECheckBoxState::Checked,S.HeightMinMeters,S.HeightMaxMeters);}})[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedAutoHeight","높이 범위 자동"))]];
    auto HeightInputs=SNew(SHorizontalBox).Visibility_Lambda([this](){const auto* V=GetLidarVisualizationComponent();return V&&!V->GetVisualizationSettings().bAutoHeightRange?EVisibility::Visible:EVisibility::Collapsed;});
    for(bool Minimum:{true,false})HeightInputs->AddSlot().FillWidth(1).Padding(2,4)[SNewSensorTool(SEditableTextBox).Style(&GetToolInputStyle()).HintText(Minimum?LOCTEXT("OwnedHMin","최소 높이(m)"):LOCTEXT("OwnedHMax","최대 높이(m)"))
      .Text_Lambda([this,Minimum](){return Minimum?OwnedHeightMinText:OwnedHeightMaxText;})
      .OnTextCommitted_Lambda([this,Minimum](const FText& T,ETextCommit::Type){float Value;if(const auto* V=GetLidarVisualizationComponent();V&&LexTryParseString(Value,*T.ToString())){const auto S=V->GetVisualizationSettings();SetLidarHeightDisplay(S.HeightReference,false,Minimum?Value:S.HeightMinMeters,Minimum?S.HeightMaxMeters:Value);}})];
    Height->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedHeightLabels","수동 범위 · 왼쪽 최소 / 오른쪽 최대 (m)")).Visibility_Lambda([this](){const auto* V=GetLidarVisualizationComponent();return V&&!V->GetVisualizationSettings().bAutoHeightRange?EVisibility::Visible:EVisibility::Collapsed;})];
    Height->AddSlot().AutoHeight()[HeightInputs];Lidar->AddSlot().AutoHeight()[Height];
    Lidar->AddSlot().AutoHeight().Padding(0,6)[SNew(SCheckBox).Visibility_Lambda([this](){return GetLidarProjectionMode()==ELidarMonitorProjectionMode::WorldTopDown?EVisibility::Visible:EVisibility::Collapsed;})
      .IsChecked_Lambda([this](){const auto* V=GetLidarVisualizationComponent();return V&&V->GetVisualizationSettings().bWorldTopDownAutoFit?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
      .OnCheckStateChanged_Lambda([this](ECheckBoxState S){SetLidarWorldTopDownAutoFit(S==ECheckBoxState::Checked);})[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedFit","검출점에 조감도 자동 맞춤"))]];
    Lidar->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedSliceLabel","전방 슬라이스 두께 (cm)")).Visibility_Lambda([this](){return GetLidarProjectionMode()==ELidarMonitorProjectionMode::ForwardSlice?EVisibility::Visible:EVisibility::Collapsed;})];
    Lidar->AddSlot().AutoHeight()[SNewSensorTool(SEditableTextBox).Style(&GetToolInputStyle()).HintText(LOCTEXT("OwnedSliceWidth","슬라이스 두께(cm)")).Visibility_Lambda([this](){return GetLidarProjectionMode()==ELidarMonitorProjectionMode::ForwardSlice?EVisibility::Visible:EVisibility::Collapsed;})
      .Text_Lambda([this](){return OwnedSliceWidthText;})
      .OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type){float N;if(auto* V=GetLidarVisualizationComponent();V&&LexTryParseString(N,*T.ToString())&&FMath::IsFinite(N)){auto S=V->GetVisualizationSettings();S.ForwardSliceThicknessCm=FMath::Clamp(N,10.f,10000.f);V->SetVisualizationSettings(S);}})];
    auto Points=SNew(SVerticalBox).Visibility_Lambda([this](){return GetMonitorPresentation()!=EVirtualSensorMonitorPresentation::TwoDimensional?EVisibility::Visible:EVisibility::Collapsed;});
    Points->AddSlot().AutoHeight().Padding(0,6)[SNewSensorTool(STextBlock).Text(LOCTEXT("Owned3DTitle","월드 포인트 고급"))];
    Points->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedPointLabel","포인트 크기"))];
    Points->AddSlot().AutoHeight()[SNewSensorTool(SEditableTextBox).Style(&GetToolInputStyle()).HintText(LOCTEXT("OwnedPointSize","포인트 크기"))
      .Text_Lambda([this](){return OwnedPointSizeText;})
      .OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type){float N;if(LexTryParseString(N,*T.ToString())&&FMath::IsFinite(N))SetLidarPointSize(N);})];
    auto Backends=SNew(SHorizontalBox);for(auto Policy:{ELidarPointCloudRenderPolicy::AutoPreferNiagara,ELidarPointCloudRenderPolicy::ForceNiagara,ELidarPointCloudRenderPolicy::ForceCpu})
      Backends->AddSlot().AutoWidth().Padding(0,4,8,4)[SNewSensorTool(SButton).Text(Policy==ELidarPointCloudRenderPolicy::AutoPreferNiagara?LOCTEXT("OwnedAuto","자동"):Policy==ELidarPointCloudRenderPolicy::ForceNiagara?LOCTEXT("OwnedGpu","Niagara 강제"):LOCTEXT("OwnedCpu","CPU 강제"))
        .OnClicked_Lambda([this,Policy](){if(auto* V=GetLidarVisualizationComponent())V->SetPointCloudRenderPolicy(Policy);return FReply::Handled();})];
    Points->AddSlot().AutoHeight()[Backends];
    Lidar->AddSlot().AutoHeight()[Points];
    Lidar->AddSlot().AutoHeight().Padding(0,5)[SNewSensorTool(SButton).Text(LOCTEXT("OwnedResetView","투영 보기 초기화")).OnClicked_Lambda([this](){if(auto* V=GetLidarVisualizationComponent())V->ResetProjectionView(GetLidarProjectionMode());return FReply::Handled();})];
    Lidar->AddSlot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true).OnAreaExpansionChanged_Lambda([this](bool Open){bOwnedLegendExpanded=Open;}).HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedLegend","범례와 표시 설명"))]
      .BodyContent()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return OwnedLegendText;})]];
    Content->AddSlot().AutoHeight()[Lidar];
    Content->AddSlot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).InitiallyCollapsed(!bMonitorDetailsExpanded).OnAreaExpansionChanged_Lambda([this](bool Open){bMonitorDetailsExpanded=Open;SaveMonitorUiPreferences();})
      .HeaderContent()[SNewSensorTool(STextBlock).Text(LOCTEXT("OwnedDiagnostics","개발자 정보"))]
      .BodyContent()[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SAssignSensorTool(NativeDetailedStatusTextBlock,STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor())]
        +SVerticalBox::Slot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).ColorAndOpacity(GetToolMutedColor()).Text_Lambda([this](){return OwnedRendererText;})]]];
    return SNew(SBox).WidthOverride(420).MaxDesiredHeight(560)[SNew(SBorder).BorderImage(GetToolPanelBrush()).BorderBackgroundColor(GetToolPanelColor()).ForegroundColor(GetToolTextColor()).Padding(12)[SNew(SScrollBox)+SScrollBox::Slot()[Content]]];
}

FString UVirtualSensorMonitorPanelWidget::BuildStatusText() const
{
    FString Text;
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent();
        const UVirtualSensorSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>() : nullptr;
        const FVirtualSensorPerformanceTelemetry* Telemetry = Scheduler ? &Scheduler->GetTelemetry() : nullptr;
        Text = FString::Printf(
            TEXT("센서: %s\n프레임: %lld · 완료 주기: %.2f Hz\n광선/측정점/검출점: %d / %d / %d\n")
            TEXT("Payload/미리보기 점: %d / %d · 대기 작업: %d\n")
            TEXT("투영/색상: %s · %s\n3D 렌더러: %s · 표시점: %d\n")
            TEXT("렌더러 상태: %s\n")
            TEXT("성능 단계: %d FPS · 평균 %.1f FPS · 1%% low %.1f FPS · p95 %.1f ms\n")
            TEXT("완료 공정성(Camera/LiDAR): %s / %s · 파생 프레임 생략: %d\n")
            TEXT("경고: %s\n메시지: %s"),
            *Status.SensorId,
            Status.FrameId,
            Status.MeasuredCompletionRateHz,
            LidarComp->HorizontalSamples * LidarComp->VerticalChannels,
            Status.TotalPointCount,
            Status.HitPointCount,
            Status.ServerPayloadPointCount,
            Status.PreviewPointCount,
            Status.bAcquisitionInFlight || Status.bDerivedWorkInFlight ? 1 : 0,
            *LidarProjectionDisplayText(GetLidarProjectionMode()),
            *LidarColorDisplayText(GetLidarColorMode()),
            Visualization ? *Visualization->GetActiveRendererName() : TEXT("사용 안 함"),
            Visualization ? Visualization->GetVisiblePointCount() : 0,
            Visualization && !Visualization->GetRendererFallbackReason().IsEmpty()
                ? *Visualization->GetRendererFallbackReason() : TEXT("정상"),
            Telemetry ? Telemetry->TargetFps : 0,
            Telemetry ? Telemetry->AverageFps : 0.0f,
            Telemetry ? Telemetry->OnePercentLowFps : 0.0f,
            Telemetry ? Telemetry->P95FrameTimeMs : 0.0f,
            Telemetry && Telemetry->bCameraFairnessEvaluable ? *FString::Printf(TEXT("%.2f"), Telemetry->CameraCompletionFairnessRatio) : TEXT("판정 불가"),
            Telemetry && Telemetry->bLidarFairnessEvaluable ? *FString::Printf(TEXT("%.2f"), Telemetry->LidarCompletionFairnessRatio) : TEXT("판정 불가"),
            Telemetry ? Telemetry->DroppedDerivedFrameCount : Status.DroppedDerivedFrameCount,
            Status.PerformanceWarning.IsEmpty() ? TEXT("없음") : *Status.PerformanceWarning,
            *Status.LastMessage);
        Text += TEXT("\n") + GetPointCloudRendererStatusText();
        Text += TEXT("\n") + LidarFidelitySummary(LidarComp);
        if (Telemetry && !Telemetry->StarvedSensorIds.IsEmpty()) Text += TEXT("\n측정 정체: ") + FString::Join(Telemetry->StarvedSensorIds, TEXT(", "));
        Text += FString::Printf(
            TEXT("\n스캔 주기: %.3f초 · 광선=%d")
            TEXT("\n서버 Payload: 점=%d 바이트=%d 간격=%d 최대=%d 미검출점=%s")
            TEXT("\n미리보기: %s 점=%d 간격=%d 최대=%d 검출점만=%s")
            TEXT("\nSlab 분석: %s 점=%d 각도=%.2f 편차=%.2f 신뢰도=%.2f")
            TEXT("\n%s\n전송/경고: %s\nLiDAR 표시: %s\n%s\n%s")
            TEXT("\nCSV 열: row,col,returnIndex,x,y,z"),
            LidarComp->ScanInterval,
            LidarComp->HorizontalSamples * LidarComp->VerticalChannels,
            Status.ServerPayloadPointCount,
            Status.LastPayloadLength,
            LidarComp->ServerPayloadStride,
            LidarComp->MaxServerPayloadPoints,
            LidarComp->bIncludeMissPointsInServerPayload ? TEXT("예") : TEXT("아니요"),
            LidarComp->IsPointCloudPreviewEnabled() || LidarComp->IsGpuPreviewBackendActive() ? TEXT("켜짐") : TEXT("꺼짐"),
            Status.PreviewPointCount,
            LidarComp->PreviewPointStride,
            LidarComp->MaxPreviewPoints,
            LidarComp->bPointCloudPreviewHitOnly ? TEXT("예") : TEXT("아니요"),
            Status.SlabAnalysis.bValid ? TEXT("유효") : *Status.SlabAnalysis.StatusMessage,
            Status.SlabAnalysis.SlabHitPointCount,
            Status.SlabAnalysis.EstimatedYawDegrees,
            Status.SlabAnalysis.AngleDeviationDegrees,
            Status.SlabAnalysis.Confidence,
            *GetLazExportSummaryText(),
            Status.PerformanceWarning.IsEmpty() ? TEXT("없음") : *Status.PerformanceWarning,
            *GetLidarViewModeDisplayText(),
            *GetAcceptanceGateSummaryText(),
            *GetRealSensorDeploymentSummaryText());
        Text += FString::Printf(TEXT("\n%s"), *GetTransportStatusSummaryText());
        if (!LastRealSensorControlMessage.IsEmpty()) Text += FString::Printf(TEXT("\n%s"), *LastRealSensorControlMessage);
        return Text;
    }
    if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        const FString DisplaySensorId = Status.SensorId.IsEmpty() ? CameraComp->SensorId : Status.SensorId;
        Text = FString::Printf(
            TEXT("센서: %s\n프레임: %lld · 완료 주기: %.2f Hz\n해상도: %d × %d · 캡처 주기: %.3f초\n")
            TEXT("Payload: %d bytes · 대기: %s · 파생 프레임 생략: %d\n경고: %s\n메시지: %s"),
            *DisplaySensorId,
            Status.FrameId,
            Status.MeasuredCompletionRateHz,
            CameraComp->CaptureResolution.X,
            CameraComp->CaptureResolution.Y,
            CameraComp->CaptureInterval,
            Status.LastPayloadLength,
            Status.bAcquisitionInFlight || Status.bDerivedWorkInFlight ? TEXT("있음") : TEXT("없음"),
            Status.DroppedDerivedFrameCount,
            Status.PerformanceWarning.IsEmpty() ? TEXT("없음") : *Status.PerformanceWarning,
            *Status.LastMessage);
        Text += FString::Printf(
            TEXT("\n스키마: virtual-camera.v1\n캡처: 모드=%d 품질=%d\nPayload 캐시: %s\n%s\n%s"),
            static_cast<int32>(CameraComp->CaptureMode),
            static_cast<int32>(CameraComp->GetSimulationQuality()),
            CameraComp->GetLastJsonPayload().IsEmpty() ? TEXT("없음") : TEXT("있음"),
            *GetAcceptanceGateSummaryText(),
            *GetRealSensorDeploymentSummaryText());
        Text += FString::Printf(TEXT("\n%s"), *GetTransportStatusSummaryText());
        if (!LastRealSensorControlMessage.IsEmpty()) Text += FString::Printf(TEXT("\n%s"), *LastRealSensorControlMessage);
        return Text;
    }
    return bShowingLidar ? TEXT("LiDAR 센서가 연결되지 않았습니다.") : TEXT("카메라 센서가 연결되지 않았습니다.");
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        const FVirtualLidarSlabAnalysisResult& Slab = Status.SlabAnalysis;
        Text = FString::Printf(TEXT("센서: %s\n프레임: %lld\n스캔 주기: %.3f초 / 광선=%d\n측정점/검출점: %d/%d\n서버 Payload: 점=%d 바이트=%d 간격=%d 최대=%d 미검출점=%s\n미리보기: %s 점=%d 간격=%d 최대=%d 검출점만=%s\nSlab 분석: %s 점=%d 각도=%.2f 기준=%.2f 편차=%.2f 신뢰도=%.2f\nSlab 중심: X=%.1f Y=%.1f Z=%.1f\n%s\n전송/경고: %s\nLiDAR 표시: %s\n%s\n%s\n고급 표시: %s 적응형=%s 경계=%s 격자=%s\nCSV 열: row,col,returnIndex,x,y,z\n메시지: %s"),
            *Status.SensorId,
            Status.FrameId,
            LidarComp->ScanInterval,
            LidarComp->HorizontalSamples * LidarComp->VerticalChannels,
            Status.TotalPointCount,
            Status.HitPointCount,
            Status.ServerPayloadPointCount,
            Status.LastPayloadLength,
            LidarComp->ServerPayloadStride,
            LidarComp->MaxServerPayloadPoints,
            LidarComp->bIncludeMissPointsInServerPayload ? TEXT("예") : TEXT("아니요"),
            LidarComp->IsPointCloudPreviewEnabled() ? TEXT("켜짐") : TEXT("꺼짐"),
            Status.PreviewPointCount,
            LidarComp->PreviewPointStride,
            LidarComp->MaxPreviewPoints,
            LidarComp->bPointCloudPreviewHitOnly ? TEXT("예") : TEXT("아니요"),
            Slab.bValid ? TEXT("유효") : *Slab.StatusMessage,
            Slab.SlabHitPointCount,
            Slab.EstimatedYawDegrees,
            Slab.ReferenceYawDegrees,
            Slab.AngleDeviationDegrees,
            Slab.Confidence,
            Slab.Center.X,
            Slab.Center.Y,
            Slab.Center.Z,
            *GetLazExportSummaryText(),
            Status.PerformanceWarning.IsEmpty() ? TEXT("없음") : *Status.PerformanceWarning,
            *GetLidarViewModeDisplayText(),
            *GetAcceptanceGateSummaryText(),
            *GetRealSensorDeploymentSummaryText(),
            bUseEnhancedLidarMonitorView ? TEXT("켜짐") : TEXT("꺼짐"),
            bUseAdaptiveLidarDepthRange ? TEXT("켜짐") : TEXT("꺼짐"),
            bOverlayLidarDepthEdges ? TEXT("켜짐") : TEXT("꺼짐"),
            bOverlayLidarMonitorGrid ? TEXT("켜짐") : TEXT("꺼짐"),
            *Status.LastMessage);
    }
    else if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        const FString DisplaySensorId = Status.SensorId.IsEmpty() ? CameraComp->SensorId : Status.SensorId;
        Text = FString::Printf(TEXT("센서: %s\n프레임: %lld\n스키마: virtual-camera.v1\n해상도: %dx%d\n캡처: 모드=%d 품질=%d 주기=%.3f초\nPayload: 바이트=%d 캐시=%s\nRenderTarget: %s\n%s\n%s\n메시지: %s"),
            *DisplaySensorId,
            Status.FrameId,
            CameraComp->CaptureResolution.X,
            CameraComp->CaptureResolution.Y,
            static_cast<int32>(CameraComp->CaptureMode),
            static_cast<int32>(CameraComp->GetSimulationQuality()),
            CameraComp->CaptureInterval,
            Status.LastPayloadLength,
            CameraComp->GetLastJsonPayload().IsEmpty() ? TEXT("없음") : TEXT("있음"),
            CameraComp->GetCameraRenderTarget() ? TEXT("준비됨") : TEXT("없음"),
            *GetAcceptanceGateSummaryText(),
            *GetRealSensorDeploymentSummaryText(),
            *Status.LastMessage);
    }
    else
    {
        Text = bShowingLidar ? TEXT("LiDAR 센서가 연결되지 않았습니다") : TEXT("카메라 센서가 연결되지 않았습니다");
    }

    Text += FString::Printf(TEXT("\n%s"), *GetTransportStatusSummaryText());

    if (!LastRealSensorControlMessage.IsEmpty())
    {
        Text += FString::Printf(TEXT("\n%s"), *LastRealSensorControlMessage);
    }

    if (SensorManager)
    {
        Text += FString::Printf(TEXT("\n\n상태: %s"), *SensorManager->GetHealthSummary().Summary);
    }
    return Text;
}
#undef LOCTEXT_NAMESPACE
