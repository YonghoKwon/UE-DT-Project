#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SceneCaptureComponent2D.h"
#include "EngineUtils.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraAct.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorAct.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"

#define LOCTEXT_NAMESPACE "VirtualSensorSettingsWidget"

namespace
{
FString QualityText(EVirtualSensorSimulationQuality Quality)
{
    if (Quality == EVirtualSensorSimulationQuality::Debug) return TEXT("디버그");
    if (Quality == EVirtualSensorSimulationQuality::RealTimePreview) return TEXT("실시간 미리보기");
    if (Quality == EVirtualSensorSimulationQuality::Balanced) return TEXT("균형");
    if (Quality == EVirtualSensorSimulationQuality::FullSpec) return TEXT("FullSpec (고부하)");
    return TEXT("사용자 설정");
}

EVirtualSensorSimulationQuality NextQuality(EVirtualSensorSimulationQuality Quality)
{
    const int32 Next = (static_cast<int32>(Quality) + 1) % 5;
    return static_cast<EVirtualSensorSimulationQuality>(Next);
}

FString CaptureModeText(EVirtualCameraCaptureMode Mode)
{
    if (Mode == EVirtualCameraCaptureMode::PreviewOnly) return TEXT("미리보기만");
    if (Mode == EVirtualCameraCaptureMode::Payload) return TEXT("Payload만");
    return TEXT("Payload 및 출력");
}

EVirtualCameraCaptureMode NextCaptureMode(EVirtualCameraCaptureMode Mode)
{
    return static_cast<EVirtualCameraCaptureMode>((static_cast<int32>(Mode) + 1) % 3);
}
}

void UVirtualSensorSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RestoreSettingsUiPreferences();
    SpawnGizmoIfNeeded();
    SyncGizmoTarget();
}

void UVirtualSensorSettingsWidget::NativeDestruct()
{
    if (GizmoActor)
    {
        GizmoActor->OnTransformChanged.RemoveAll(this);
        GizmoActor->OnTransformCommitted.RemoveAll(this);
        GizmoActor->Destroy();
        GizmoActor = nullptr;
    }
    Super::NativeDestruct();
}

void UVirtualSensorSettingsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (AActor* SelectedActor = GetSelectedSensorActor(); SelectedActor != LastSyncedSensorActor.Get())
    {
        RefreshPendingState(true);
        SyncGizmoTarget();
    }
    if (GizmoActor)
    {
        bManipulationEnabled = GizmoActor->IsManipulationEnabled();
        GizmoActor->SetStepSizes(TranslationStepCm, RotationStepDegrees);
    }
}

void UVirtualSensorSettingsWidget::BindSensorManager(AVirtualSensorManager* InSensorManager)
{
    SensorManager = InSensorManager;
    RefreshPendingState(true);
    SpawnGizmoIfNeeded();
    SyncGizmoTarget();
}

void UVirtualSensorSettingsWidget::BindHostActor(AVirtualSensorMonitorHostActor* InHostActor)
{
    HostActor = InHostActor;
}

void UVirtualSensorSettingsWidget::SelectTargetKind(EVirtualSensorTargetKind InTargetKind)
{
    PendingState.TargetKind = InTargetKind;
    RefreshPendingState(true);
    SyncGizmoTarget();
}

void UVirtualSensorSettingsWidget::SelectNextTarget()
{
    if (!SensorManager)
    {
        LastControlMessage = TEXT("SensorManager가 연결되지 않았습니다.");
        RefreshNativeText();
        return;
    }
    if (PendingState.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        SensorManager->SelectNextCamera();
    }
    else
    {
        SensorManager->SelectNextLidar();
    }
    RefreshPendingState(true);
    SyncGizmoTarget();
}

bool UVirtualSensorSettingsWidget::ApplyPendingState()
{
    FString Error;
    if (!ValidateState(PendingState, Error) || !ApplyStateToRuntime(PendingState, Error))
    {
        FVirtualSensorEditableState CurrentRuntimeState;
        CurrentRuntimeState.TargetKind = PendingState.TargetKind;
        if (ReadSelectedSensorState(CurrentRuntimeState))
        {
            PendingState = CurrentRuntimeState;
        }
        LastControlMessage = FString::Printf(TEXT("적용 실패: %s"), *Error);
        RefreshNativeText();
        return false;
    }
    LastControlMessage = FString::Printf(TEXT("PIE에 적용됨: %s"), *PendingState.SensorId);
    RefreshSelectedSensorNow(true);
    RefreshNativeText();
    return true;
}

void UVirtualSensorSettingsWidget::ResetPendingStateToMapValue()
{
    const FVirtualSensorEditableState* Initial = InitialStates.Find(PendingState.PersistentActorTag);
    if (Initial)
    {
        PendingState = *Initial;
        ApplyPendingState();
        LastControlMessage = TEXT("PIE 시작 시점의 맵 값으로 되돌렸습니다.");
    }
    else
    {
        LastControlMessage = TEXT("선택한 센서의 PIE 시작 값이 없습니다.");
    }
    RefreshNativeText();
}

bool UVirtualSensorSettingsWidget::QueuePendingStateForSensorTestMap()
{
    if (!ApplyPendingState() || !HostActor)
    {
        if (!HostActor)
        {
            LastControlMessage = TEXT("맵 저장 예약 실패: HostActor가 연결되지 않았습니다.");
            RefreshNativeText();
        }
        return false;
    }
    HostActor->QueueSensorStateForMapApply(PendingState);
    LastControlMessage = TEXT("SensorTestMap 저장 대기: PIE를 종료하면 맵에 저장됩니다.");
    RefreshNativeText();
    return true;
}

void UVirtualSensorSettingsWidget::NudgeSelectedSensor(FVector TranslationDelta, FRotator RotationDelta)
{
    PendingState.ActorTransform = AVirtualSensorTransformGizmoActor::ApplyKeyboardDelta(PendingState.ActorTransform, CoordinateSpace, TranslationDelta, RotationDelta);
    ApplyPendingState();
}

void UVirtualSensorSettingsWidget::SetSensorManipulationEnabled(bool bEnabled)
{
    bManipulationEnabled = bEnabled;
    SpawnGizmoIfNeeded();
    if (GizmoActor) GizmoActor->SetManipulationEnabled(bEnabled);
    LastControlMessage = bEnabled
        ? TEXT("센서 조작 모드가 켜졌습니다. WASD/QE와 방향키, Z/C를 사용할 수 있습니다.")
        : TEXT("센서 조작 모드가 꺼졌습니다.");
    RefreshNativeText();
}

void UVirtualSensorSettingsWidget::SetSensorGizmoMode(EVirtualSensorGizmoMode InMode)
{
    GizmoMode = InMode;
    if (GizmoActor) GizmoActor->SetGizmoMode(InMode);
}

void UVirtualSensorSettingsWidget::SetSensorCoordinateSpace(EVirtualSensorCoordinateSpace InSpace)
{
    CoordinateSpace = InSpace;
    if (GizmoActor) GizmoActor->SetCoordinateSpace(InSpace);
}

void UVirtualSensorSettingsWidget::SetProjectionDebugEnabled(bool bEnabled)
{
    bProjectionDebugEnabled = bEnabled;
    SpawnGizmoIfNeeded();
    if (GizmoActor) GizmoActor->SetProjectionDebugEnabled(bEnabled);
    LastControlMessage = bEnabled ? TEXT("선택 센서의 투사 범위를 표시합니다.") : TEXT("투사 범위 표시를 껐습니다.");
    RefreshNativeText();
}

FString UVirtualSensorSettingsWidget::GetControlStatusText() const
{
    const FVector Location = PendingState.ActorTransform.GetLocation();
    const FRotator Rotation = PendingState.ActorTransform.Rotator();
    return FString::Printf(TEXT("%s\n대상=%s  SensorId=%s\n위치(cm): X %.1f  Y %.1f  Z %.1f\n회전: P %.1f  Y %.1f  R %.1f\n품질=%s\n%s"),
        *LastControlMessage,
        PendingState.TargetKind == EVirtualSensorTargetKind::Camera ? TEXT("카메라") : TEXT("LiDAR"),
        *PendingState.SensorId,
        Location.X, Location.Y, Location.Z,
        Rotation.Pitch, Rotation.Yaw, Rotation.Roll,
        *QualityText(PendingState.SimulationQuality),
        *BuildDeviceSpecText());
}

TSharedRef<SWidget> UVirtualSensorSettingsWidget::RebuildWidget()
{
    if (WidgetTree && WidgetTree->RootWidget)
    {
        return Super::RebuildWidget();
    }

    RestoreSettingsUiPreferences();
    return SNew(SBorder)
    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
    .BorderBackgroundColor(FVirtualSensorUiStyle::PanelBackground)
    .ForegroundColor(FVirtualSensorUiStyle::PrimaryText)
    .Padding(10.0f)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FVirtualSensorUiStyle::HeaderBackground)
            .Padding(FMargin(8.0f, 6.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("SettingsTitle", "센서 설정  |  제목을 드래그해 이동")) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(IsPanelCollapsed() ? TEXT("펼치기") : TEXT("접기")); }).OnClicked_Lambda([this]() { TogglePanelCollapsed(); return FReply::Handled(); }) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ResetUi", "위치 초기화")).OnClicked_Lambda([this]() { ResetPanelPosition(); return FReply::Handled(); }) ]
                + SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ResetAllUi", "전체 UI 초기화")).ToolTipText(LOCTEXT("ResetAllUiTip", "세 패널의 위치·접힘·LiDAR 표시 옵션을 기본값으로 되돌립니다.")).OnClicked_Lambda([this]() { if (HostActor) HostActor->ResetAllPanelUiPreferences(); return FReply::Handled(); }) ]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f, 5.0f, 2.0f, 0.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("선택: %s · SensorId: %s"), PendingState.TargetKind == EVirtualSensorTargetKind::Camera ? TEXT("카메라") : TEXT("LiDAR"), PendingState.SensorId.IsEmpty() ? TEXT("없음") : *PendingState.SensorId)); }) ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            SNew(SScrollBox).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(LOCTEXT("SelectSection", "1. 센서 선택")) ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Camera", "카메라")).OnClicked_Lambda([this]() { SelectTargetKind(EVirtualSensorTargetKind::Camera); return FReply::Handled(); }) ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Lidar", "LiDAR")).OnClicked_Lambda([this]() { SelectTargetKind(EVirtualSensorTargetKind::Lidar); return FReply::Handled(); }) ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Next", "다음 센서")).OnClicked_Lambda([this]() { SelectNextTarget(); return FReply::Handled(); }) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [
                    SNew(SEditableTextBox)
                    .Style(&FVirtualSensorUiStyle::EditableTextBoxStyle())
                    .HintText(LOCTEXT("SensorIdHint", "SensorId"))
                    .Text_Lambda([this]() { return FText::FromString(PendingState.SensorId); })
                    .OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { PendingState.SensorId = Text.ToString().TrimStartAndEnd(); ApplyPendingState(); })
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 2.0f)[ SNew(STextBlock).Text(LOCTEXT("ManipulationSection", "2. 위치·회전 조작")) ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SWrapBox).UseAllottedSize(true)
                    + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(bManipulationEnabled ? TEXT("조작 종료") : TEXT("센서 조작 시작")); }).OnClicked_Lambda([this]() { SetSensorManipulationEnabled(!bManipulationEnabled); return FReply::Handled(); }) ]
                    + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(GizmoMode == EVirtualSensorGizmoMode::Translate ? TEXT("모드: 이동") : TEXT("모드: 회전")); }).OnClicked_Lambda([this]() { SetSensorGizmoMode(GizmoMode == EVirtualSensorGizmoMode::Translate ? EVirtualSensorGizmoMode::Rotate : EVirtualSensorGizmoMode::Translate); return FReply::Handled(); }) ]
                    + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(CoordinateSpace == EVirtualSensorCoordinateSpace::Local ? TEXT("좌표: 로컬") : TEXT("좌표: 월드")); }).OnClicked_Lambda([this]() { SetSensorCoordinateSpace(CoordinateSpace == EVirtualSensorCoordinateSpace::Local ? EVirtualSensorCoordinateSpace::World : EVirtualSensorCoordinateSpace::Local); return FReply::Handled(); }) ]
                    + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(bGizmoVisible ? TEXT("기즈모 숨기기") : TEXT("기즈모 보이기")); }).OnClicked_Lambda([this]() { bGizmoVisible = !bGizmoVisible; if (GizmoActor) GizmoActor->SetGizmoVisible(bGizmoVisible); return FReply::Handled(); }) ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this]() { return bProjectionDebugEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetProjectionDebugEnabled(State == ECheckBoxState::Checked); })
                    [ SNew(STextBlock).Text(LOCTEXT("ProjectionDebug", "선택 센서 투사 범위 표시")) ]
                ]
                + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("LocationX", "위치 X (cm)"), [this]() { return PendingState.ActorTransform.GetLocation().X; }, [this](float V) { FVector L = PendingState.ActorTransform.GetLocation(); L.X = V; PendingState.ActorTransform.SetLocation(L); }, -100000.0f, 100000.0f) ]
                + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("LocationY", "위치 Y (cm)"), [this]() { return PendingState.ActorTransform.GetLocation().Y; }, [this](float V) { FVector L = PendingState.ActorTransform.GetLocation(); L.Y = V; PendingState.ActorTransform.SetLocation(L); }, -100000.0f, 100000.0f) ]
                + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("LocationZ", "위치 Z (cm)"), [this]() { return PendingState.ActorTransform.GetLocation().Z; }, [this](float V) { FVector L = PendingState.ActorTransform.GetLocation(); L.Z = V; PendingState.ActorTransform.SetLocation(L); }, -100000.0f, 100000.0f) ]
                + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("Pitch", "Pitch (도)"), [this]() { return PendingState.ActorTransform.Rotator().Pitch; }, [this](float V) { FRotator R = PendingState.ActorTransform.Rotator(); R.Pitch = V; PendingState.ActorTransform.SetRotation(R.Quaternion()); }, -180.0f, 180.0f) ]
                + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("Yaw", "Yaw (도)"), [this]() { return PendingState.ActorTransform.Rotator().Yaw; }, [this](float V) { FRotator R = PendingState.ActorTransform.Rotator(); R.Yaw = V; PendingState.ActorTransform.SetRotation(R.Quaternion()); }, -180.0f, 180.0f) ]
                + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("Roll", "Roll (도)"), [this]() { return PendingState.ActorTransform.Rotator().Roll; }, [this](float V) { FRotator R = PendingState.ActorTransform.Rotator(); R.Roll = V; PendingState.ActorTransform.SetRotation(R.Quaternion()); }, -180.0f, 180.0f) ]
                + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("MoveStep", "이동 단위 (cm)"), [this]() { return TranslationStepCm; }, [this](float V) { TranslationStepCm = V; }, 1.0f, 1000.0f, false) ]
                + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("RotateStep", "회전 단위 (도)"), [this]() { return RotationStepDegrees; }, [this](float V) { RotationStepDegrees = V; }, 0.1f, 90.0f, false) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(bKeyboardHelpExpanded ? TEXT("단축키 도움말 접기") : TEXT("단축키 도움말 펼치기")); }).OnClicked_Lambda([this]() { bKeyboardHelpExpanded = !bKeyboardHelpExpanded; SaveSettingsUiPreferences(); return FReply::Handled(); }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 0.0f, 4.0f, 6.0f)[ SNew(STextBlock).Visibility_Lambda([this]() { return bKeyboardHelpExpanded ? EVisibility::Visible : EVisibility::Collapsed; }).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text(LOCTEXT("KeyboardHelp", "W/S 전후 · A/D 좌우 · Q/E 높이 · 방향키 Pitch/Yaw · Z/C Roll\nShift 5배 · Ctrl 0.2배 · Esc 조작 종료\n기즈모: 빨강 X · 초록 Y · 파랑 Z")) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 2.0f)[ SNew(STextBlock).Text(LOCTEXT("BasicSection", "3. 기본 센서 설정 (입력 확정 즉시 반영)")) ]
                + SVerticalBox::Slot().AutoHeight()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("시뮬레이션 품질: %s"), *QualityText(PendingState.SimulationQuality))); }).OnClicked_Lambda([this]() { PendingState.SimulationQuality = NextQuality(PendingState.SimulationQuality); ApplySelectedProfileAndQualityPreset(); return FReply::Handled(); }) ]
                + SVerticalBox::Slot().AutoHeight()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(PendingState.TargetKind == EVirtualSensorTargetKind::Camera ? TEXT("장비 프로필: Intel RealSense D455 / Generic") : TEXT("장비 프로필: Livox Mid-360S / Generic")); }).OnClicked_Lambda([this]() { if (PendingState.TargetKind == EVirtualSensorTargetKind::Camera) PendingState.CameraProfile = PendingState.CameraProfile == EVirtualCameraDeviceProfile::IntelRealSenseD455 ? EVirtualCameraDeviceProfile::Generic : EVirtualCameraDeviceProfile::IntelRealSenseD455; else PendingState.LidarProfile = PendingState.LidarProfile == EVirtualLidarDeviceProfile::LivoxMid360S ? EVirtualLidarDeviceProfile::Generic : EVirtualLidarDeviceProfile::LivoxMid360S; ApplySelectedProfileAndQualityPreset(); return FReply::Handled(); }) ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox).Visibility_Lambda([this]() { return PendingState.TargetKind == EVirtualSensorTargetKind::Camera ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("CameraInterval", "카메라 캡처 주기 (초)"), [this]() { return PendingState.CameraCaptureInterval; }, [this](float V) { PendingState.CameraCaptureInterval = V; }, 0.033f, 60.0f) ]
                    + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("CameraWidth", "카메라 가로 해상도"), [this]() { return PendingState.CameraResolution.X; }, [this](int32 V) { PendingState.CameraResolution.X = V; }, 160, 4096) ]
                    + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("CameraHeight", "카메라 세로 해상도"), [this]() { return PendingState.CameraResolution.Y; }, [this](int32 V) { PendingState.CameraResolution.Y = V; }, 90, 2160) ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox).Visibility_Lambda([this]() { return PendingState.TargetKind == EVirtualSensorTargetKind::Lidar ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("LidarInterval", "LiDAR 스캔 주기 (초)"), [this]() { return PendingState.LidarScanInterval; }, [this](float V) { PendingState.LidarScanInterval = V; }, 0.033f, 60.0f) ]
                    + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("LidarRange", "LiDAR 최대 거리 (cm)"), [this]() { return PendingState.LidarMaxDistance; }, [this](float V) { PendingState.LidarMaxDistance = V; }, 10.0f, 10000.0f) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(bShowAdvanced ? TEXT("고급 설정 접기") : TEXT("고급 설정 펼치기")); }).OnClicked_Lambda([this]() { bShowAdvanced = !bShowAdvanced; return FReply::Handled(); }) ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox).Visibility_Lambda([this]() { return bShowAdvanced ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SVerticalBox).Visibility_Lambda([this]() { return PendingState.TargetKind == EVirtualSensorTargetKind::Camera ? EVisibility::Visible : EVisibility::Collapsed; })
                        + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("CameraFov", "카메라 FOV (도)"), [this]() { return PendingState.CameraFov; }, [this](float V) { PendingState.CameraFov = V; }, 5.0f, 170.0f) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("JpegQuality", "JPEG 품질"), [this]() { return PendingState.CameraJpegQuality; }, [this](int32 V) { PendingState.CameraJpegQuality = V; }, 1, 100) ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("카메라 출력 모드: %s"), *CaptureModeText(PendingState.CameraCaptureMode))); }).OnClicked_Lambda([this]() { PendingState.CameraCaptureMode = NextCaptureMode(PendingState.CameraCaptureMode); ApplyPendingState(); return FReply::Handled(); }) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SVerticalBox).Visibility_Lambda([this]() { return PendingState.TargetKind == EVirtualSensorTargetKind::Lidar ? EVisibility::Visible : EVisibility::Collapsed; })
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("HorizontalSamples", "LiDAR 수평 샘플 수"), [this]() { return PendingState.LidarHorizontalSamples; }, [this](int32 V) { PendingState.LidarHorizontalSamples = V; }, 1, 1440) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("VerticalChannels", "LiDAR 수직 채널 수"), [this]() { return PendingState.LidarVerticalChannels; }, [this](int32 V) { PendingState.LidarVerticalChannels = V; }, 1, 256) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("HorizontalFov", "LiDAR 수평 FOV (도)"), [this]() { return PendingState.LidarHorizontalFov; }, [this](float V) { PendingState.LidarHorizontalFov = V; }, 1.0f, 360.0f) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("VerticalMin", "LiDAR 수직 최소 각도"), [this]() { return PendingState.LidarMinVerticalAngle; }, [this](float V) { PendingState.LidarMinVerticalAngle = V; }, -90.0f, 89.0f) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("VerticalMax", "LiDAR 수직 최대 각도"), [this]() { return PendingState.LidarMaxVerticalAngle; }, [this](float V) { PendingState.LidarMaxVerticalAngle = V; }, -89.0f, 90.0f) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("ServerStride", "서버 Payload 간격"), [this]() { return PendingState.ServerPayloadStride; }, [this](int32 V) { PendingState.ServerPayloadStride = V; }, 1, 100) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("ServerMax", "서버 최대 점 수 (0=전체)"), [this]() { return PendingState.MaxServerPayloadPoints; }, [this](int32 V) { PendingState.MaxServerPayloadPoints = V; }, 0, 1000000) ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SCheckBox).IsChecked_Lambda([this]() { return PendingState.bIncludeMissPointsInServerPayload ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { PendingState.bIncludeMissPointsInServerPayload = V == ECheckBoxState::Checked; ApplyPendingState(); })[ SNew(STextBlock).Text(LOCTEXT("IncludeMisses", "서버 Payload에 미검출점 포함")) ] ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("PreviewStride", "미리보기 점 간격"), [this]() { return PendingState.PreviewPointStride; }, [this](int32 V) { PendingState.PreviewPointStride = V; }, 1, 100) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("PreviewMax", "미리보기 최대 점 수"), [this]() { return PendingState.MaxPreviewPoints; }, [this](int32 V) { PendingState.MaxPreviewPoints = V; }, 0, 1000000) ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SCheckBox).IsChecked_Lambda([this]() { return PendingState.bPreviewHitOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { PendingState.bPreviewHitOnly = V == ECheckBoxState::Checked; ApplyPendingState(); })[ SNew(STextBlock).Text(LOCTEXT("PreviewHitOnly", "미리보기에서 검출점만 표시")) ] ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SCheckBox).IsChecked_Lambda([this]() { return PendingState.bUseMultiHit ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { PendingState.bUseMultiHit = V == ECheckBoxState::Checked; ApplyPendingState(); })[ SNew(STextBlock).Text(LOCTEXT("MultiHit", "다중 검출 사용 (고부하)")) ] ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("MaxHits", "광선당 최대 검출 수"), [this]() { return PendingState.MaxHitsPerRay; }, [this](int32 V) { PendingState.MaxHitsPerRay = V; }, 1, 16) ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SCheckBox).IsChecked_Lambda([this]() { return PendingState.bExportCsvOnScan ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { PendingState.bExportCsvOnScan = V == ECheckBoxState::Checked; ApplyPendingState(); })[ SNew(STextBlock).Text(LOCTEXT("AutoCsv", "스캔 시 CSV 자동 내보내기")) ] ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SCheckBox).IsChecked_Lambda([this]() { return PendingState.bExportJsonLinesOnScan ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { PendingState.bExportJsonLinesOnScan = V == ECheckBoxState::Checked; ApplyPendingState(); })[ SNew(STextBlock).Text(LOCTEXT("AutoJsonl", "스캔 시 JSONL 자동 내보내기")) ] ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SCheckBox).IsChecked_Lambda([this]() { return PendingState.bExportPcdOnScan ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { PendingState.bExportPcdOnScan = V == ECheckBoxState::Checked; ApplyPendingState(); })[ SNew(STextBlock).Text(LOCTEXT("AutoPcd", "스캔 시 PCD 자동 내보내기")) ] ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[ SNew(STextBlock).Text(LOCTEXT("RealSource", "외부 센서 소스")) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SWrapBox).UseAllottedSize(true)
                        + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("StartSources", "전체 시작")).OnClicked_Lambda([this]() { StartAllRealSensorSources(); return FReply::Handled(); }) ]
                        + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("StopSources", "전체 중지")).OnClicked_Lambda([this]() { StopAllRealSensorSources(); return FReply::Handled(); }) ]
                        + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("PushSource", "선택 소스 1회 전송")).OnClicked_Lambda([this]() { PushSelectedRealSensorSource(); return FReply::Handled(); }) ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ResetValues", "PIE 시작값으로 되돌리기")).OnClicked_Lambda([this]() { ResetPendingStateToMapValue(); return FReply::Handled(); }) ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ApplyMap", "SensorTestMap에 저장 예약")).OnClicked_Lambda([this]() { QueuePendingStateForSensorTestMap(); return FReply::Handled(); }) ]
                ]
                + SVerticalBox::Slot().AutoHeight()[ SAssignNew(NativeStatusText, STextBlock).ColorAndOpacity_Lambda([this]() { return LastControlMessage.Contains(TEXT("실패")) ? FVirtualSensorUiStyle::Error : (LastControlMessage.Contains(TEXT("대기")) || LastControlMessage.Contains(TEXT("갱신")) ? FVirtualSensorUiStyle::Warning : FVirtualSensorUiStyle::Success); }).AutoWrapText(true).Text(FText::FromString(GetControlStatusText())) ]
            ]
        ]
    ];
}

bool UVirtualSensorSettingsWidget::ReadSelectedSensorState(FVirtualSensorEditableState& OutState) const
{
    if (!SensorManager)
    {
        return false;
    }
    OutState.TargetKind = PendingState.TargetKind;
    if (OutState.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        UVirtualCameraComp* Camera = SensorManager->GetSelectedCamera();
        AActor* Owner = Camera ? Camera->GetOwner() : nullptr;
        if (!Camera || !Owner) return false;
        OutState.PersistentActorTag = ResolvePersistentActorTag(Owner);
        OutState.OriginalSensorId = Camera->SensorId;
        OutState.SensorId = Camera->SensorId;
        OutState.ActorTransform = Owner->GetActorTransform();
        OutState.SimulationQuality = Camera->SimulationQuality;
        OutState.CameraProfile = Camera->DeviceProfile;
        OutState.CameraResolution = Camera->CaptureResolution;
        OutState.CameraCaptureInterval = Camera->CaptureInterval;
        OutState.CameraFov = Camera->FOVAngle;
        OutState.CameraJpegQuality = Camera->JpegQuality;
        OutState.CameraCaptureMode = Camera->CaptureMode;
        return true;
    }

    UVirtualLidarSensorComp* Lidar = SensorManager->GetSelectedLidar();
    AActor* Owner = Lidar ? Lidar->GetOwner() : nullptr;
    if (!Lidar || !Owner) return false;
    OutState.PersistentActorTag = ResolvePersistentActorTag(Owner);
    OutState.OriginalSensorId = Lidar->SensorId;
    OutState.SensorId = Lidar->SensorId;
    OutState.ActorTransform = Owner->GetActorTransform();
    OutState.SimulationQuality = Lidar->SimulationQuality;
    OutState.LidarProfile = Lidar->DeviceProfile;
    OutState.LidarScanInterval = Lidar->ScanInterval;
    OutState.LidarMaxDistance = Lidar->MaxDistance;
    OutState.LidarHorizontalSamples = Lidar->HorizontalSamples;
    OutState.LidarVerticalChannels = Lidar->VerticalChannels;
    OutState.LidarHorizontalFov = Lidar->HorizontalFov;
    OutState.LidarMinVerticalAngle = Lidar->MinVerticalAngle;
    OutState.LidarMaxVerticalAngle = Lidar->MaxVerticalAngle;
    OutState.ServerPayloadStride = Lidar->ServerPayloadStride;
    OutState.MaxServerPayloadPoints = Lidar->MaxServerPayloadPoints;
    OutState.bIncludeMissPointsInServerPayload = Lidar->bIncludeMissPointsInServerPayload;
    OutState.PreviewPointStride = Lidar->PreviewPointStride;
    OutState.MaxPreviewPoints = Lidar->MaxPreviewPoints;
    OutState.bPreviewHitOnly = Lidar->bPointCloudPreviewHitOnly;
    OutState.bUseMultiHit = Lidar->bUseMultiHit;
    OutState.MaxHitsPerRay = Lidar->MaxHitsPerRay;
    OutState.bExportCsvOnScan = Lidar->bExportCsvOnScan;
    OutState.bExportJsonLinesOnScan = Lidar->bExportJsonLinesOnScan;
    OutState.bExportPcdOnScan = Lidar->bExportPcdOnScan;
    return true;
}

void UVirtualSensorSettingsWidget::ApplySelectedProfileAndQualityPreset()
{
    if (!SensorManager)
    {
        LastControlMessage = TEXT("프리셋 적용 실패: SensorManager가 연결되지 않았습니다.");
        RefreshNativeText();
        return;
    }
    if (PendingState.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        if (UVirtualCameraComp* Camera = SensorManager->GetSelectedCamera())
        {
            Camera->StopCapture();
            Camera->ApplyDeviceProfile(PendingState.CameraProfile);
            Camera->ApplySimulationQuality(PendingState.SimulationQuality);
            Camera->StartCapture();
        }
    }
    else if (UVirtualLidarSensorComp* Lidar = SensorManager->GetSelectedLidar())
    {
        Lidar->StopScan();
        Lidar->ApplyDeviceProfile(PendingState.LidarProfile);
        Lidar->ApplySimulationQuality(PendingState.SimulationQuality);
        Lidar->StartScan();
    }
    RefreshPendingState(false);
    LastControlMessage = TEXT("장비 프로필과 시뮬레이션 품질을 PIE에 적용했습니다.");
    RefreshSelectedSensorNow(true);
    RefreshNativeText();
}

bool UVirtualSensorSettingsWidget::ApplyStateToRuntime(const FVirtualSensorEditableState& State, FString& OutError)
{
    if (!SensorManager) { OutError = TEXT("SensorManager가 연결되지 않았습니다"); return false; }
    if (State.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        UVirtualCameraComp* Camera = SensorManager->GetSelectedCamera();
        AActor* Owner = Camera ? Camera->GetOwner() : nullptr;
        if (!Camera || !Owner) { OutError = TEXT("선택한 카메라를 사용할 수 없습니다"); return false; }
        Owner->SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Camera->StopCapture();
        Camera->ApplyDeviceProfile(State.CameraProfile);
        Camera->ApplySimulationQuality(State.SimulationQuality);
        Camera->SensorId = State.SensorId;
        Camera->CaptureResolution = State.CameraResolution;
        Camera->CaptureInterval = State.CameraCaptureInterval;
        Camera->FOVAngle = State.CameraFov;
        Camera->JpegQuality = State.CameraJpegQuality;
        Camera->CaptureMode = State.CameraCaptureMode;
        Camera->StartCapture();
        return true;
    }

    UVirtualLidarSensorComp* Lidar = SensorManager->GetSelectedLidar();
    AActor* Owner = Lidar ? Lidar->GetOwner() : nullptr;
    if (!Lidar || !Owner) { OutError = TEXT("선택한 LiDAR를 사용할 수 없습니다"); return false; }
    Owner->SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
    Lidar->StopScan();
    Lidar->ApplyDeviceProfile(State.LidarProfile);
    Lidar->ApplySimulationQuality(State.SimulationQuality);
    Lidar->SensorId = State.SensorId;
    Lidar->ScanInterval = State.LidarScanInterval;
    Lidar->MaxDistance = State.LidarMaxDistance;
    Lidar->HorizontalSamples = State.LidarHorizontalSamples;
    Lidar->VerticalChannels = State.LidarVerticalChannels;
    Lidar->HorizontalFov = State.LidarHorizontalFov;
    Lidar->MinVerticalAngle = State.LidarMinVerticalAngle;
    Lidar->MaxVerticalAngle = State.LidarMaxVerticalAngle;
    Lidar->SetServerPayloadPolicy(State.ServerPayloadStride, State.MaxServerPayloadPoints, State.bIncludeMissPointsInServerPayload);
    Lidar->SetPreviewPolicy(State.PreviewPointStride, State.MaxPreviewPoints, State.bPreviewHitOnly);
    Lidar->bUseMultiHit = State.bUseMultiHit;
    Lidar->MaxHitsPerRay = State.MaxHitsPerRay;
    Lidar->bExportCsvOnScan = State.bExportCsvOnScan;
    Lidar->bExportJsonLinesOnScan = State.bExportJsonLinesOnScan;
    Lidar->bExportPcdOnScan = State.bExportPcdOnScan;
    Lidar->StartScan();
    return true;
}

bool UVirtualSensorSettingsWidget::ValidateState(const FVirtualSensorEditableState& State, FString& OutError) const
{
    if (!GetWorld()) { OutError = TEXT("World를 사용할 수 없습니다"); return false; }
    const UVirtualCameraComp* SelectedCamera = SensorManager ? SensorManager->GetSelectedCamera() : nullptr;
    const UVirtualLidarSensorComp* SelectedLidar = SensorManager ? SensorManager->GetSelectedLidar() : nullptr;
    TArray<FString> OtherSensorIds;
    for (TActorIterator<AVirtualCameraAct> It(GetWorld()); It; ++It)
    {
        const UVirtualCameraComp* Comp = It->VirtualCameraComp;
        if (Comp && Comp != SelectedCamera) OtherSensorIds.Add(Comp->SensorId);
    }
    for (TActorIterator<AVirtualSensorAct> It(GetWorld()); It; ++It)
    {
        const UVirtualLidarSensorComp* Comp = It->LidarSensorComp;
        if (Comp && Comp != SelectedLidar) OtherSensorIds.Add(Comp->SensorId);
    }
    return ValidateEditableStateValues(State, OtherSensorIds, OutError);
}

bool UVirtualSensorSettingsWidget::ValidateEditableStateValues(const FVirtualSensorEditableState& State, const TArray<FString>& OtherSensorIds, FString& OutError)
{
    if (State.SensorId.TrimStartAndEnd().IsEmpty()) { OutError = TEXT("SensorId가 비어 있습니다"); return false; }
    if (State.ActorTransform.ContainsNaN()) { OutError = TEXT("Transform에 유효하지 않은 숫자가 있습니다"); return false; }
    if (OtherSensorIds.Contains(State.SensorId)) { OutError = TEXT("이미 사용 중인 SensorId입니다"); return false; }
    if (State.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        if (State.CameraResolution.X < 160 || State.CameraResolution.X > 4096 || State.CameraResolution.Y < 90 || State.CameraResolution.Y > 2160 ||
            State.CameraCaptureInterval < 0.033f || State.CameraCaptureInterval > 60.0f || State.CameraFov < 5.0f || State.CameraFov > 170.0f ||
            State.CameraJpegQuality < 1 || State.CameraJpegQuality > 100)
        {
            OutError = TEXT("카메라 값이 지원 범위를 벗어났습니다");
            return false;
        }
    }
    else if (State.LidarScanInterval < 0.033f || State.LidarScanInterval > 60.0f || State.LidarMaxDistance < 10.0f || State.LidarMaxDistance > 10000.0f ||
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

void UVirtualSensorSettingsWidget::RefreshPendingState(bool bCaptureInitialValue)
{
    FVirtualSensorEditableState State;
    State.TargetKind = PendingState.TargetKind;
    if (ReadSelectedSensorState(State))
    {
        PendingState = State;
        if (bCaptureInitialValue && !InitialStates.Contains(State.PersistentActorTag))
        {
            InitialStates.Add(State.PersistentActorTag, State);
        }
        LastControlMessage = TEXT("현재 PIE 값을 불러왔습니다.");
    }
    else
    {
        LastControlMessage = TEXT("선택한 종류의 센서가 없습니다.");
    }
    RefreshNativeText();
}

void UVirtualSensorSettingsWidget::RefreshNativeText()
{
    if (NativeStatusText.IsValid()) NativeStatusText->SetText(FText::FromString(GetControlStatusText()));
}

void UVirtualSensorSettingsWidget::SpawnGizmoIfNeeded()
{
    if (GizmoActor || !GetWorld()) return;
    GizmoActor = GetWorld()->SpawnActor<AVirtualSensorTransformGizmoActor>();
    if (!GizmoActor) return;
    GizmoActor->OnTransformChanged.AddUObject(this, &UVirtualSensorSettingsWidget::HandleGizmoTransformChanged);
    GizmoActor->OnTransformCommitted.AddUObject(this, &UVirtualSensorSettingsWidget::HandleGizmoTransformCommitted);
    GizmoActor->SetGizmoMode(GizmoMode);
    GizmoActor->SetCoordinateSpace(CoordinateSpace);
    GizmoActor->SetGizmoVisible(bGizmoVisible);
    GizmoActor->SetManipulationEnabled(bManipulationEnabled);
    GizmoActor->SetProjectionDebugEnabled(bProjectionDebugEnabled);
    GizmoActor->SetStepSizes(TranslationStepCm, RotationStepDegrees);
}

void UVirtualSensorSettingsWidget::SyncGizmoTarget()
{
    if (!GizmoActor) return;
    AActor* SelectedActor = GetSelectedSensorActor();
    GizmoActor->BindTarget(SelectedActor);
    LastSyncedSensorActor = SelectedActor;
    GizmoActor->SetProjectionDebugEnabled(bProjectionDebugEnabled);
}

AActor* UVirtualSensorSettingsWidget::GetSelectedSensorActor() const
{
    if (!SensorManager) return nullptr;
    if (PendingState.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        const UVirtualCameraComp* Camera = SensorManager->GetSelectedCamera();
        return Camera ? Camera->GetOwner() : nullptr;
    }
    const UVirtualLidarSensorComp* Lidar = SensorManager->GetSelectedLidar();
    return Lidar ? Lidar->GetOwner() : nullptr;
}

void UVirtualSensorSettingsWidget::HandleGizmoTransformChanged(const FTransform& Transform)
{
    if (Transform.ContainsNaN()) return;
    PendingState.ActorTransform = Transform;
    LastControlMessage = TEXT("미리보기 갱신 중...");
    RefreshSelectedSensorNow(false);
    RefreshNativeText();
}

void UVirtualSensorSettingsWidget::HandleGizmoTransformCommitted(const FTransform& Transform)
{
    if (Transform.ContainsNaN()) return;
    PendingState.ActorTransform = Transform;
    RefreshSelectedSensorNow(true);
    LastControlMessage = FString::Printf(TEXT("PIE에 적용됨: %s"), *PendingState.SensorId);
    RefreshNativeText();
}

void UVirtualSensorSettingsWidget::RefreshSelectedSensorNow(bool bForce)
{
    if (!SensorManager || !GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bForce && LastPreviewRefreshTime >= 0.0 && Now - LastPreviewRefreshTime < 0.1) return;
    LastPreviewRefreshTime = Now;
    SensorManager->RefreshSelectedSensorOnce(PendingState.TargetKind == EVirtualSensorTargetKind::Lidar);
}

void UVirtualSensorSettingsWidget::StartAllRealSensorSources()
{
    const int32 Count = SensorManager ? SensorManager->StartAllRealSensorSources() : 0;
    LastControlMessage = FString::Printf(TEXT("외부 센서 소스 %d개를 시작했습니다."), Count);
    RefreshNativeText();
}

void UVirtualSensorSettingsWidget::StopAllRealSensorSources()
{
    if (SensorManager) SensorManager->StopAllRealSensorSources();
    LastControlMessage = SensorManager ? TEXT("외부 센서 소스를 모두 중지했습니다.") : TEXT("SensorManager가 연결되지 않았습니다.");
    RefreshNativeText();
}

void UVirtualSensorSettingsWidget::PushSelectedRealSensorSource()
{
    const bool bSucceeded = SensorManager && SensorManager->PushSelectedRealSensorSourceOnce(true);
    LastControlMessage = bSucceeded ? TEXT("선택한 외부 센서 프레임을 전송했습니다.") : TEXT("선택한 외부 센서 프레임 전송에 실패했습니다.");
    RefreshNativeText();
}

void UVirtualSensorSettingsWidget::RestoreSettingsUiPreferences()
{
    const UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
    if (Preferences)
    {
        bKeyboardHelpExpanded = Preferences->bSettingsKeyboardHelpExpanded;
    }
}

void UVirtualSensorSettingsWidget::SaveSettingsUiPreferences() const
{
    UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
    if (!Preferences) return;
    Preferences->bSettingsKeyboardHelpExpanded = bKeyboardHelpExpanded;
    UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
}

void UVirtualSensorSettingsWidget::ResetSettingsUiPreferencesToDefault()
{
    bKeyboardHelpExpanded = false;
    SaveSettingsUiPreferences();
}

FString UVirtualSensorSettingsWidget::BuildDeviceSpecText() const
{
    if (!SensorManager) return TEXT("장비 사양을 불러올 수 없습니다");
    if (PendingState.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        const UVirtualCameraComp* Camera = SensorManager->GetSelectedCamera();
        if (!Camera) return TEXT("D455를 사용할 수 없습니다");
        const FVirtualSensorDeviceSpec& Spec = Camera->GetDeviceSpec();
        return FString::Printf(TEXT("%s %s | Depth FOV %.0fx%.0f | 권장 %.2f-%.1fm | %dx%d @ %.0fHz\n주의: SceneCapture 미리보기는 보정된 Depth Stream이 아닙니다."), *Spec.Manufacturer, *Spec.Model, Spec.HorizontalFovDegrees, Spec.VerticalFovDegrees, Spec.MinRangeCm / 100.0f, Spec.MaxRangeCm / 100.0f, Spec.Width, Spec.Height, Spec.FrameRateHz);
    }
    const UVirtualLidarSensorComp* Lidar = SensorManager->GetSelectedLidar();
    if (!Lidar) return TEXT("Mid-360S를 사용할 수 없습니다");
    const FVirtualSensorDeviceSpec& Spec = Lidar->GetDeviceSpec();
    return FString::Printf(TEXT("%s %s | FOV %.0fx%.0f | %.1f-%.0fm | %d points/s @ %.0fHz\n런타임 광선 수는 시뮬레이션 품질에서 별도로 제어합니다."), *Spec.Manufacturer, *Spec.Model, Spec.HorizontalFovDegrees, Spec.VerticalFovDegrees, Spec.MinRangeCm / 100.0f, Spec.MaxRangeCm / 100.0f, Spec.PointRate, Spec.FrameRateHz);
}

FName UVirtualSensorSettingsWidget::ResolvePersistentActorTag(const AActor* Actor) const
{
    if (!Actor) return NAME_None;
    for (const FName Tag : Actor->Tags)
    {
        if (Tag.ToString().StartsWith(TEXT("SensorTestPersistent_"))) return Tag;
    }
    return Actor->GetFName();
}

TSharedRef<SWidget> UVirtualSensorSettingsWidget::MakeFloatRow(const FText& Label, TFunction<float()> Getter, TFunction<void(float)> Setter, float Min, float Max, bool bApplyOnCommit)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.55f)[ SNew(STextBlock).Text(Label) ]
        + SHorizontalBox::Slot().FillWidth(0.45f)
        [
            SNew(SSpinBox<float>)
            .MinValue(Min).MaxValue(Max)
            .Value_Lambda([Getter]() { return Getter(); })
            .OnValueChanged_Lambda([Setter](float Value) { Setter(Value); })
            .OnValueCommitted_Lambda([this, Setter, bApplyOnCommit](float Value, ETextCommit::Type) { Setter(Value); if (bApplyOnCommit) ApplyPendingState(); })
            .OnEndSliderMovement_Lambda([this, Setter, bApplyOnCommit](float Value) { Setter(Value); if (bApplyOnCommit) ApplyPendingState(); })
        ];
}

TSharedRef<SWidget> UVirtualSensorSettingsWidget::MakeIntRow(const FText& Label, TFunction<int32()> Getter, TFunction<void(int32)> Setter, int32 Min, int32 Max)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.55f)[ SNew(STextBlock).Text(Label) ]
        + SHorizontalBox::Slot().FillWidth(0.45f)
        [
            SNew(SSpinBox<int32>)
            .MinValue(Min).MaxValue(Max)
            .Value_Lambda([Getter]() { return Getter(); })
            .OnValueChanged_Lambda([Setter](int32 Value) { Setter(Value); })
            .OnValueCommitted_Lambda([this, Setter](int32 Value, ETextCommit::Type) { Setter(Value); ApplyPendingState(); })
            .OnEndSliderMovement_Lambda([this, Setter](int32 Value) { Setter(Value); ApplyPendingState(); })
        ];
}

#undef LOCTEXT_NAMESPACE
