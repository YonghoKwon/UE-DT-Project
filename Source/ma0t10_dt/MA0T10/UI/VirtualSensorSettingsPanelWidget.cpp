#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"

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
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSchedulerSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"

#define LOCTEXT_NAMESPACE "VirtualSensorSettingsPanelWidget"

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

FVirtualSensorSettingHelpDescriptor MakeSettingHelp(const TCHAR* Key, const TCHAR* Label, const TCHAR* Description, const TCHAR* Unit, const TCHAR* Impact, const TCHAR* Recommended, const TCHAR* Caution = TEXT(""))
{
    FVirtualSensorSettingHelpDescriptor Result;
    Result.Key = FName(Key);
    Result.Label = FText::FromString(Label);
    Result.Description = FText::FromString(Description);
    Result.Unit = FText::FromString(Unit);
    Result.PerformanceImpact = FText::FromString(Impact);
    Result.RecommendedValue = FText::FromString(Recommended);
    Result.Caution = FText::FromString(Caution);
    return Result;
}

const TArray<FVirtualSensorSettingHelpDescriptor>& SettingHelpDescriptors()
{
    static const TArray<FVirtualSensorSettingHelpDescriptor> Descriptors = {
        MakeSettingHelp(TEXT("SimulationQuality"), TEXT("시뮬레이션 품질"), TEXT("센서별 해상도·광선 수·명목 갱신 주기를 한 번에 적용하는 실행 예산입니다."), TEXT("Preset"), TEXT("FullSpec일수록 높음"), TEXT("일반 조작은 실시간 미리보기, 최종 검증은 FullSpec")),
        MakeSettingHelp(TEXT("CameraInterval"), TEXT("카메라 캡처 주기"), TEXT("장면을 다시 렌더링하는 간격입니다. 값이 작을수록 초당 캡처 횟수가 증가합니다."), TEXT("초"), TEXT("렌더링·GPU readback 빈도에 반비례"), TEXT("FullSpec 0.033초(30Hz), 실시간 미리보기 0.1초")),
        MakeSettingHelp(TEXT("CameraWidth"), TEXT("카메라 가로 해상도"), TEXT("렌더 타깃의 가로 픽셀 수입니다."), TEXT("px"), TEXT("세로 해상도와 곱한 픽셀 수에 비례"), TEXT("D455 FullSpec 1280")),
        MakeSettingHelp(TEXT("CameraHeight"), TEXT("카메라 세로 해상도"), TEXT("렌더 타깃의 세로 픽셀 수입니다."), TEXT("px"), TEXT("가로 해상도와 곱한 픽셀 수에 비례"), TEXT("D455 FullSpec 720")),
        MakeSettingHelp(TEXT("CameraFov"), TEXT("카메라 FOV"), TEXT("카메라의 수평 시야각입니다. 넓을수록 더 넓은 영역이 보이지만 물체가 작게 표현됩니다."), TEXT("도"), TEXT("동일 해상도에서는 영향 낮음"), TEXT("D455 87°")),
        MakeSettingHelp(TEXT("JpegQuality"), TEXT("JPEG 품질"), TEXT("카메라 Payload의 JPEG 압축 품질입니다. 높을수록 화질과 파일 크기가 증가합니다."), TEXT("1~100"), TEXT("CPU 압축 시간·Payload 크기 증가"), TEXT("FullSpec 80, 실시간 미리보기 70")),
        MakeSettingHelp(TEXT("CaptureMode"), TEXT("카메라 출력 모드"), TEXT("미리보기만은 readback을 생략하고, Payload만은 JSON을 캐시하며, Payload 및 출력은 전송까지 수행합니다."), TEXT("Mode"), TEXT("미리보기만 < Payload만 < Payload 및 출력"), TEXT("화면 확인은 미리보기만, 연동 검증은 Payload 및 출력")),
        MakeSettingHelp(TEXT("LidarInterval"), TEXT("LiDAR 스캔 주기"), TEXT("새 스캔을 시작하는 명목 간격입니다."), TEXT("초"), TEXT("광선/s에 반비례"), TEXT("Mid-360S FullSpec 0.1초(10Hz)")),
        MakeSettingHelp(TEXT("LidarRange"), TEXT("LiDAR 최대 거리"), TEXT("각 광선이 충돌을 검사하는 최대 거리입니다."), TEXT("cm"), TEXT("장면 복잡도에 따라 trace 비용 증가 가능"), TEXT("일반 검출 4000cm, 장비 cutoff 10000cm")),
        MakeSettingHelp(TEXT("HorizontalSamples"), TEXT("LiDAR 수평 샘플 수"), TEXT("한 수직 채널에서 발사하는 수평 광선 수입니다."), TEXT("개/행"), TEXT("수직 채널과 곱해 선형 증가"), TEXT("FullSpec 360")),
        MakeSettingHelp(TEXT("VerticalChannels"), TEXT("LiDAR 수직 채널 수"), TEXT("수직 방향의 광선 행 수입니다."), TEXT("채널"), TEXT("수평 샘플과 곱해 선형 증가"), TEXT("FullSpec 60")),
        MakeSettingHelp(TEXT("HorizontalFov"), TEXT("LiDAR 수평 FOV"), TEXT("수평으로 스캔하는 전체 각도입니다. 광선 수가 같으면 값이 클수록 각도 간격이 넓어집니다."), TEXT("도"), TEXT("동일 광선 수에서는 영향 낮음"), TEXT("Mid-360S 360°")),
        MakeSettingHelp(TEXT("VerticalMin"), TEXT("LiDAR 수직 최소 각도"), TEXT("수직 스캔 범위의 아래쪽 경계입니다."), TEXT("도"), TEXT("동일 채널 수에서는 영향 낮음"), TEXT("Mid-360S -7°")),
        MakeSettingHelp(TEXT("VerticalMax"), TEXT("LiDAR 수직 최대 각도"), TEXT("수직 스캔 범위의 위쪽 경계입니다."), TEXT("도"), TEXT("동일 채널 수에서는 영향 낮음"), TEXT("Mid-360S 52°")),
        MakeSettingHelp(TEXT("ServerStride"), TEXT("서버 Payload 간격"), TEXT("측정 배열에서 N번째 점마다 서버 Payload에 포함합니다."), TEXT("N개당 1점"), TEXT("값이 클수록 JSON·네트워크 부하 감소"), TEXT("정밀 전송 1, 실시간 연동은 2~8")),
        MakeSettingHelp(TEXT("ServerMax"), TEXT("서버 최대 점 수"), TEXT("한 Payload에 넣을 최대 점 수입니다. 0은 제한 없음입니다."), TEXT("점"), TEXT("0은 큰 JSON과 메모리 사용 가능"), TEXT("FullSpec 원본 검증은 0, 실시간 서버는 수용량에 맞게 제한"), TEXT("서버 판정에 필요한 측정 밀도를 먼저 확인하세요.")),
        MakeSettingHelp(TEXT("IncludeMisses"), TEXT("서버 Payload 미검출점 포함"), TEXT("충돌하지 않은 광선도 최대 거리의 miss 점으로 전송합니다."), TEXT("On/Off"), TEXT("Payload 점 수와 JSON 크기 증가"), TEXT("일반적으로 끔")),
        MakeSettingHelp(TEXT("PreviewStride"), TEXT("미리보기 점 간격"), TEXT("화면에 표시할 점을 N개마다 선택합니다. 서버 Payload에는 영향을 주지 않습니다."), TEXT("N개당 1점"), TEXT("값이 클수록 ISM 갱신 비용 감소"), TEXT("FullSpec 6")),
        MakeSettingHelp(TEXT("PreviewMax"), TEXT("미리보기 최대 점 수"), TEXT("화면에 그릴 최대 점 수입니다. 0은 제한 없음입니다."), TEXT("점"), TEXT("점 수에 따라 렌더·instance 갱신 증가"), TEXT("FullSpec 5000"), TEXT("FullSpec에서 0은 권장하지 않습니다.")),
        MakeSettingHelp(TEXT("PreviewHitOnly"), TEXT("미리보기 검출점만 표시"), TEXT("miss 점을 제외하고 실제 충돌점만 화면에 표시합니다."), TEXT("On/Off"), TEXT("켜면 Preview 부하 감소"), TEXT("켬")),
        MakeSettingHelp(TEXT("MultiHit"), TEXT("다중 검출"), TEXT("광선 하나가 통과하며 만나는 여러 충돌을 모두 수집합니다."), TEXT("On/Off"), TEXT("매우 높음"), TEXT("기본 끔"), TEXT("FullSpec과 함께 사용하면 완료 주기가 낮아질 수 있습니다.")),
        MakeSettingHelp(TEXT("MaxHits"), TEXT("광선당 최대 검출 수"), TEXT("MultiHit에서 광선 하나가 저장할 최대 return 수입니다."), TEXT("점/광선"), TEXT("메모리·Payload가 값에 비례해 증가"), TEXT("3")),
        MakeSettingHelp(TEXT("AutoExport"), TEXT("스캔 시 자동 내보내기"), TEXT("각 스캔 완료 후 CSV·JSONL·PCD를 디스크에 기록합니다."), TEXT("On/Off"), TEXT("직렬화·디스크 부하 매우 높음"), TEXT("FullSpec에서는 끄고 Capture/Export 패널 사용"))
    };
    return Descriptors;
}

FText SettingHelpTooltip(FName Key)
{
    const FVirtualSensorSettingHelpDescriptor* Help = SettingHelpDescriptors().FindByPredicate([Key](const FVirtualSensorSettingHelpDescriptor& Item) { return Item.Key == Key; });
    return Help ? FText::Format(NSLOCTEXT("VirtualSensorSettingsPanelWidget", "SharedSettingTip", "{0}\n성능 영향: {1}\n권장값: {2}"), Help->Description, Help->PerformanceImpact, Help->RecommendedValue) : FText::GetEmpty();
}
}

void UVirtualSensorSettingsPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RestoreSettingsUiPreferences();
    SpawnGizmoIfNeeded();
    SyncGizmoTarget();
}

void UVirtualSensorSettingsPanelWidget::NativeDestruct()
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

void UVirtualSensorSettingsPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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
    if (bManipulationEnabled && bMonitorAutoFollowingManipulation && SensorManager &&
        static_cast<uint8>(SensorManager->GetViewMode()) != AutoFollowMonitorView)
    {
        // A direct monitor selection during manipulation is an explicit user override.
        bMonitorAutoFollowingManipulation = false;
        bRestoreMonitorViewAfterManipulation = false;
    }
}

void UVirtualSensorSettingsPanelWidget::BindSensorManager(AVirtualSensorCoordinator* InSensorManager)
{
    SensorManager = InSensorManager;
    RefreshPendingState(true);
    SpawnGizmoIfNeeded();
    SyncGizmoTarget();
}

void UVirtualSensorSettingsPanelWidget::BindHostActor(AVirtualSensorUiHostActor* InHostActor)
{
    HostActor = InHostActor;
}

TArray<FVirtualSensorSettingHelpDescriptor> UVirtualSensorSettingsPanelWidget::GetAllSettingHelpDescriptors() const
{
    return SettingHelpDescriptors();
}

const FVirtualSensorSettingHelpDescriptor* UVirtualSensorSettingsPanelWidget::FindSettingHelp(FName SettingKey) const
{
    return SettingHelpDescriptors().FindByPredicate([SettingKey](const FVirtualSensorSettingHelpDescriptor& Item) { return Item.Key == SettingKey; });
}

bool UVirtualSensorSettingsPanelWidget::SelectSettingHelp(FName SettingKey)
{
    if (!FindSettingHelp(SettingKey)) return false;
    SelectedSettingHelpKey = SettingKey;
    RefreshNativeText();
    return true;
}

FString UVirtualSensorSettingsPanelWidget::GetSelectedSettingHelpText() const
{
    const FVirtualSensorSettingHelpDescriptor* Help = FindSettingHelp(SelectedSettingHelpKey);
    if (!Help) return TEXT("설정 항목의 ⓘ 버튼을 누르면 의미와 성능 영향을 확인할 수 있습니다.");
    FString Result = FString::Printf(TEXT("%s  [%s]\n%s\n성능 영향: %s\n권장값: %s"),
        *Help->Label.ToString(), *Help->Unit.ToString(), *Help->Description.ToString(), *Help->PerformanceImpact.ToString(), *Help->RecommendedValue.ToString());
    if (!Help->Caution.IsEmpty()) Result += FString::Printf(TEXT("\n주의: %s"), *Help->Caution.ToString());
    return Result;
}

FString UVirtualSensorSettingsPanelWidget::GetCurrentLoadSummaryText() const
{
    FString LoadText;
    if (PendingState.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        const double RateHz = 1.0 / FMath::Max(0.001, static_cast<double>(PendingState.CameraCaptureInterval));
        const double MegaPixelsPerSecond = CalculateCameraMegaPixelsPerSecond(PendingState);
        const TCHAR* Level = MegaPixelsPerSecond < 10.0 ? TEXT("낮음") : (MegaPixelsPerSecond < 25.0 ? TEXT("보통") : TEXT("높음"));
        LoadText = FString::Printf(TEXT("예상 부하: %.1f MP/s · %.1fHz · %s"), MegaPixelsPerSecond, RateHz, Level);
    }
    else
    {
        const int64 RaysPerScan = static_cast<int64>(PendingState.LidarHorizontalSamples) * PendingState.LidarVerticalChannels;
        const double RateHz = 1.0 / FMath::Max(0.001, static_cast<double>(PendingState.LidarScanInterval));
        const double RaysPerSecond = CalculateLidarRaysPerSecond(PendingState);
        const int32 PayloadUpper = PendingState.MaxServerPayloadPoints > 0
            ? FMath::Min(PendingState.MaxServerPayloadPoints, static_cast<int32>(RaysPerScan / FMath::Max(1, PendingState.ServerPayloadStride)))
            : static_cast<int32>(RaysPerScan / FMath::Max(1, PendingState.ServerPayloadStride));
        const int32 PreviewUpper = PendingState.MaxPreviewPoints > 0
            ? FMath::Min(PendingState.MaxPreviewPoints, static_cast<int32>(RaysPerScan / FMath::Max(1, PendingState.PreviewPointStride)))
            : static_cast<int32>(RaysPerScan / FMath::Max(1, PendingState.PreviewPointStride));
        const TCHAR* Level = RaysPerSecond < 20000.0 ? TEXT("낮음") : (RaysPerSecond < 100000.0 ? TEXT("보통") : TEXT("높음"));
        LoadText = FString::Printf(TEXT("예상 부하: %lld 광선/스캔 · %.0f 광선/s · Payload≤%d · Preview≤%d · %s"), RaysPerScan, RaysPerSecond, PayloadUpper, PreviewUpper, Level);
    }

    if (GetWorld())
    {
        if (const UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>())
        {
            LoadText += TEXT("\n") + Subsystem->GetTelemetrySummaryText();
        }
    }
    return LoadText;
}

double UVirtualSensorSettingsPanelWidget::CalculateCameraMegaPixelsPerSecond(const FVirtualSensorEditableState& State)
{
    if (State.CameraCaptureInterval <= SMALL_NUMBER) return 0.0;
    const double RateHz = 1.0 / static_cast<double>(State.CameraCaptureInterval);
    return static_cast<double>(State.CameraResolution.X) * State.CameraResolution.Y * RateHz / 1000000.0;
}

double UVirtualSensorSettingsPanelWidget::CalculateLidarRaysPerSecond(const FVirtualSensorEditableState& State)
{
    if (State.LidarScanInterval <= SMALL_NUMBER) return 0.0;
    const double RateHz = 1.0 / static_cast<double>(State.LidarScanInterval);
    return static_cast<double>(State.LidarHorizontalSamples) * State.LidarVerticalChannels * RateHz;
}

void UVirtualSensorSettingsPanelWidget::SelectTargetKind(EVirtualSensorTargetKind InTargetKind)
{
    AVirtualSensorActorBase* PreviousActor = Cast<AVirtualSensorActorBase>(GetSelectedSensorActor());
    if (bManipulationEnabled && PreviousActor) PreviousActor->EndInteractiveManipulation();
    PendingState.TargetKind = InTargetKind;
    RefreshPendingState(true);
    SyncGizmoTarget();
    if (bManipulationEnabled)
    {
        if (AVirtualSensorActorBase* SensorActor = Cast<AVirtualSensorActorBase>(GetSelectedSensorActor()))
        {
            SensorActor->BeginInteractiveManipulation(InteractionRequest);
        }
        BeginMonitorFollowForManipulation();
    }
}

void UVirtualSensorSettingsPanelWidget::SelectNextTarget()
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

bool UVirtualSensorSettingsPanelWidget::ApplyPendingState()
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

void UVirtualSensorSettingsPanelWidget::ResetPendingStateToMapValue()
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

bool UVirtualSensorSettingsPanelWidget::QueuePendingStateForSensorTestMap()
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

void UVirtualSensorSettingsPanelWidget::NudgeSelectedSensor(FVector TranslationDelta, FRotator RotationDelta)
{
    PendingState.ActorTransform = AVirtualSensorTransformGizmoActor::ApplyKeyboardDelta(PendingState.ActorTransform, CoordinateSpace, TranslationDelta, RotationDelta);
	if (AVirtualSensorActorBase* SensorActor = Cast<AVirtualSensorActorBase>(GetSelectedSensorActor()))
	{
		if (!SensorActor->IsInteractiveManipulationActive()) SensorActor->BeginInteractiveManipulation(InteractionRequest);
		SensorActor->UpdateInteractiveTransform(PendingState.ActorTransform);
	}
	LastControlMessage = TEXT("조작 중: 경량 미리보기");
	RefreshNativeText();
}

void UVirtualSensorSettingsPanelWidget::SetSensorManipulationEnabled(bool bEnabled)
{
	if (bManipulationEnabled == bEnabled) return;
    bManipulationEnabled = bEnabled;
    SpawnGizmoIfNeeded();
    if (GizmoActor) GizmoActor->SetManipulationEnabled(bEnabled);
	if (AVirtualSensorActorBase* SensorActor = Cast<AVirtualSensorActorBase>(GetSelectedSensorActor()))
	{
		if (bEnabled) SensorActor->BeginInteractiveManipulation(InteractionRequest);
		else SensorActor->EndInteractiveManipulation();
	}
    if (bEnabled) BeginMonitorFollowForManipulation();
    else EndMonitorFollowForManipulation();
    LastControlMessage = bEnabled
        ? TEXT("센서 조작 모드가 켜졌습니다. WASD/QE와 방향키, Z/C를 사용할 수 있습니다.")
        : TEXT("센서 조작 모드가 꺼졌습니다.");
	LastControlMessage = bEnabled
		? TEXT("조작 중: 경량 미리보기 (WASD/QE 이동, 방향키/Z/C 회전)")
		: TEXT("FullSpec 최종 갱신 중");
	RefreshNativeText();
}

void UVirtualSensorSettingsPanelWidget::BeginMonitorFollowForManipulation()
{
    if (!SensorManager) return;
    const EVirtualSensorViewMode DesiredMode = PendingState.TargetKind == EVirtualSensorTargetKind::Lidar
        ? EVirtualSensorViewMode::Lidar
        : EVirtualSensorViewMode::Camera;
    if (!bMonitorAutoFollowingManipulation)
    {
        MonitorViewBeforeManipulation = static_cast<uint8>(SensorManager->GetViewMode());
    }
    AutoFollowMonitorView = static_cast<uint8>(DesiredMode);
    bMonitorAutoFollowingManipulation = true;
    bRestoreMonitorViewAfterManipulation = true;
    SensorManager->SetViewMode(DesiredMode);
}

void UVirtualSensorSettingsPanelWidget::EndMonitorFollowForManipulation()
{
    if (SensorManager && bRestoreMonitorViewAfterManipulation && bMonitorAutoFollowingManipulation &&
        static_cast<uint8>(SensorManager->GetViewMode()) == AutoFollowMonitorView)
    {
        SensorManager->SetViewMode(static_cast<EVirtualSensorViewMode>(MonitorViewBeforeManipulation));
    }
    bMonitorAutoFollowingManipulation = false;
    bRestoreMonitorViewAfterManipulation = false;
}

void UVirtualSensorSettingsPanelWidget::SetSensorGizmoMode(EVirtualSensorGizmoMode InMode)
{
    GizmoMode = InMode;
    if (GizmoActor) GizmoActor->SetGizmoMode(InMode);
}

void UVirtualSensorSettingsPanelWidget::SetSensorCoordinateSpace(EVirtualSensorCoordinateSpace InSpace)
{
    CoordinateSpace = InSpace;
    if (GizmoActor) GizmoActor->SetCoordinateSpace(InSpace);
}

void UVirtualSensorSettingsPanelWidget::SetProjectionDebugEnabled(bool bEnabled)
{
    bProjectionDebugEnabled = bEnabled;
    SpawnGizmoIfNeeded();
    if (GizmoActor) GizmoActor->SetProjectionDebugEnabled(bEnabled);
    LastControlMessage = bEnabled ? TEXT("선택 센서의 투사 범위를 표시합니다.") : TEXT("투사 범위 표시를 껐습니다.");
    RefreshNativeText();
}

FString UVirtualSensorSettingsPanelWidget::GetControlStatusText() const
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

TSharedRef<SWidget> UVirtualSensorSettingsPanelWidget::RebuildWidget()
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
                + SVerticalBox::Slot().AutoHeight()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).ToolTipText(SettingHelpTooltip(TEXT("SimulationQuality"))).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("시뮬레이션 품질: %s"), *QualityText(PendingState.SimulationQuality))); }).OnClicked_Lambda([this]() { SelectSettingHelp(TEXT("SimulationQuality")); SetSelectedSimulationQuality(NextQuality(PendingState.SimulationQuality)); return FReply::Handled(); }) ]
                + SVerticalBox::Slot().AutoHeight()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(PendingState.TargetKind == EVirtualSensorTargetKind::Camera ? TEXT("장비 프로필: Intel RealSense D455 / Generic") : TEXT("장비 프로필: Livox Mid-360S / Generic")); }).OnClicked_Lambda([this]() { if (PendingState.TargetKind == EVirtualSensorTargetKind::Camera) PendingState.CameraProfile = PendingState.CameraProfile == EVirtualCameraDeviceProfile::IntelRealSenseD455 ? EVirtualCameraDeviceProfile::Generic : EVirtualCameraDeviceProfile::IntelRealSenseD455; else PendingState.LidarProfile = PendingState.LidarProfile == EVirtualLidarDeviceProfile::LivoxMid360S ? EVirtualLidarDeviceProfile::Generic : EVirtualLidarDeviceProfile::LivoxMid360S; ApplySelectedProfileAndQualityPreset(); return FReply::Handled(); }) ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox).Visibility_Lambda([this]() { return PendingState.TargetKind == EVirtualSensorTargetKind::Camera ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("CameraInterval", "카메라 캡처 주기 (초)"), [this]() { return PendingState.CameraCaptureInterval; }, [this](float V) { PendingState.CameraCaptureInterval = V; }, 0.033f, 60.0f, true, TEXT("CameraInterval")) ]
                    + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("CameraWidth", "카메라 가로 해상도"), [this]() { return PendingState.CameraResolution.X; }, [this](int32 V) { PendingState.CameraResolution.X = V; }, 160, 4096, TEXT("CameraWidth")) ]
                    + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("CameraHeight", "카메라 세로 해상도"), [this]() { return PendingState.CameraResolution.Y; }, [this](int32 V) { PendingState.CameraResolution.Y = V; }, 90, 2160, TEXT("CameraHeight")) ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox).Visibility_Lambda([this]() { return PendingState.TargetKind == EVirtualSensorTargetKind::Lidar ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("LidarInterval", "LiDAR 스캔 주기 (초)"), [this]() { return PendingState.LidarScanInterval; }, [this](float V) { PendingState.LidarScanInterval = V; }, 0.033f, 60.0f, true, TEXT("LidarInterval")) ]
                    + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("LidarRange", "LiDAR 최대 거리 (cm)"), [this]() { return PendingState.LidarMaxDistance; }, [this](float V) { PendingState.LidarMaxDistance = V; }, 10.0f, 10000.0f, true, TEXT("LidarRange")) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(bShowAdvanced ? TEXT("고급 설정 접기") : TEXT("고급 설정 펼치기")); }).OnClicked_Lambda([this]() { bShowAdvanced = !bShowAdvanced; return FReply::Handled(); }) ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox).Visibility_Lambda([this]() { return bShowAdvanced ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SVerticalBox).Visibility_Lambda([this]() { return PendingState.TargetKind == EVirtualSensorTargetKind::Camera ? EVisibility::Visible : EVisibility::Collapsed; })
                        + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("CameraFov", "카메라 FOV (도)"), [this]() { return PendingState.CameraFov; }, [this](float V) { PendingState.CameraFov = V; }, 5.0f, 170.0f, true, TEXT("CameraFov")) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("JpegQuality", "JPEG 품질"), [this]() { return PendingState.CameraJpegQuality; }, [this](int32 V) { PendingState.CameraJpegQuality = V; }, 1, 100, TEXT("JpegQuality")) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SVerticalBox).Visibility_Lambda([this]() { return PendingState.TargetKind == EVirtualSensorTargetKind::Lidar ? EVisibility::Visible : EVisibility::Collapsed; })
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("HorizontalSamples", "LiDAR 수평 샘플 수"), [this]() { return PendingState.LidarHorizontalSamples; }, [this](int32 V) { PendingState.LidarHorizontalSamples = V; }, 1, 1440, TEXT("HorizontalSamples")) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("VerticalChannels", "LiDAR 수직 채널 수"), [this]() { return PendingState.LidarVerticalChannels; }, [this](int32 V) { PendingState.LidarVerticalChannels = V; }, 1, 256, TEXT("VerticalChannels")) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("HorizontalFov", "LiDAR 수평 FOV (도)"), [this]() { return PendingState.LidarHorizontalFov; }, [this](float V) { PendingState.LidarHorizontalFov = V; }, 1.0f, 360.0f, true, TEXT("HorizontalFov")) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("VerticalMin", "LiDAR 수직 최소 각도"), [this]() { return PendingState.LidarMinVerticalAngle; }, [this](float V) { PendingState.LidarMinVerticalAngle = V; }, -90.0f, 89.0f, true, TEXT("VerticalMin")) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeFloatRow(LOCTEXT("VerticalMax", "LiDAR 수직 최대 각도"), [this]() { return PendingState.LidarMaxVerticalAngle; }, [this](float V) { PendingState.LidarMaxVerticalAngle = V; }, -89.0f, 90.0f, true, TEXT("VerticalMax")) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("PreviewStride", "미리보기 점 간격"), [this]() { return PendingState.PreviewPointStride; }, [this](int32 V) { PendingState.PreviewPointStride = V; }, 1, 100, TEXT("PreviewStride")) ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("PreviewMax", "미리보기 최대 점 수"), [this]() { return PendingState.MaxPreviewPoints; }, [this](int32 V) { PendingState.MaxPreviewPoints = V; }, 0, 1000000, TEXT("PreviewMax")) ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SCheckBox).ToolTipText(SettingHelpTooltip(TEXT("PreviewHitOnly"))).IsChecked_Lambda([this]() { return PendingState.bPreviewHitOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SelectSettingHelp(TEXT("PreviewHitOnly")); PendingState.bPreviewHitOnly = V == ECheckBoxState::Checked; ApplyPendingState(); })[ SNew(STextBlock).Text(LOCTEXT("PreviewHitOnly", "미리보기에서 검출점만 표시")) ] ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(SCheckBox).ToolTipText(SettingHelpTooltip(TEXT("MultiHit"))).IsChecked_Lambda([this]() { return PendingState.bUseMultiHit ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SelectSettingHelp(TEXT("MultiHit")); PendingState.bUseMultiHit = V == ECheckBoxState::Checked; ApplyPendingState(); })[ SNew(STextBlock).Text(LOCTEXT("MultiHit", "다중 검출 사용 (고부하)")) ] ]
                        + SVerticalBox::Slot().AutoHeight()[ MakeIntRow(LOCTEXT("MaxHits", "광선당 최대 검출 수"), [this]() { return PendingState.MaxHitsPerRay; }, [this](int32 V) { PendingState.MaxHitsPerRay = V; }, 1, 16, TEXT("MaxHits")) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[ SNew(STextBlock).Text(LOCTEXT("RealSource", "외부 센서 소스")) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SWrapBox).UseAllottedSize(true)
                        + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("StartSources", "전체 시작")).OnClicked_Lambda([this]() { StartAllRealSensorSources(); return FReply::Handled(); }) ]
                        + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("StopSources", "전체 중지")).OnClicked_Lambda([this]() { StopAllRealSensorSources(); return FReply::Handled(); }) ]
                        + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("PushSource", "선택 Source 1회 주입")).ToolTipText(LOCTEXT("PushSourceTip", "입력 프레임을 선택 센서에 주입합니다. 외부 서버 전송은 캡처/내보내기 패널에서 실행합니다.")).OnClicked_Lambda([this]() { PushSelectedRealSensorSource(); return FReply::Handled(); }) ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FVirtualSensorUiStyle::SectionBackground)
                    .Padding(8.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text(LOCTEXT("LoadSummaryTitle", "현재 설정 예상 부하")) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 7.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(GetCurrentLoadSummaryText()); }) ]
                        + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text(LOCTEXT("SettingHelpTitle", "선택한 설정 도움말")) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(GetSelectedSettingHelpText()); }) ]
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

bool UVirtualSensorSettingsPanelWidget::ReadSelectedSensorState(FVirtualSensorEditableState& OutState) const
{
    const EVirtualSensorKind RequestedKind = OutState.TargetKind == EVirtualSensorTargetKind::Lidar
        ? EVirtualSensorKind::Lidar
        : EVirtualSensorKind::Camera;
    AVirtualSensorActorBase* SensorActor = SensorManager ? SensorManager->GetSelectedSensorActorByKind(RequestedKind) : nullptr;
    if (!SensorActor || !SensorActor->ReadEditableState(OutState)) return false;
    OutState.PersistentActorTag = ResolvePersistentActorTag(SensorActor);
    return true;
}

bool UVirtualSensorSettingsPanelWidget::SetSelectedSimulationQuality(EVirtualSensorSimulationQuality NewQuality)
{
    PendingState.SimulationQuality = NewQuality;
    return ApplySelectedProfileAndQualityPreset();
}

bool UVirtualSensorSettingsPanelWidget::ApplySelectedProfileAndQualityPreset()
{
    const EVirtualSensorKind RequestedKind = PendingState.TargetKind == EVirtualSensorTargetKind::Lidar
        ? EVirtualSensorKind::Lidar
        : EVirtualSensorKind::Camera;
    AVirtualSensorActorBase* SensorActor = SensorManager ? SensorManager->GetSelectedSensorActorByKind(RequestedKind) : nullptr;
    FString Error;
    FVirtualSensorEditableState AppliedState;
    if (!SensorActor || !SensorActor->ApplyProfileAndSimulationQuality(PendingState, AppliedState, Error))
    {
        if (!SensorActor && Error.IsEmpty()) Error = TEXT("선택한 V2 센서 Actor를 사용할 수 없습니다.");
        LastControlMessage = FString::Printf(TEXT("프리셋 적용 실패: %s"), *Error);
        RefreshNativeText();
        return false;
    }
    AppliedState.PersistentActorTag = ResolvePersistentActorTag(SensorActor);
    PendingState = AppliedState;
    LastControlMessage = TEXT("장비 프로필과 시뮬레이션 품질을 PIE에 적용했습니다.");
    RefreshSelectedSensorNow(true);
    RefreshNativeText();
    return true;
}

bool UVirtualSensorSettingsPanelWidget::ApplyStateToRuntime(const FVirtualSensorEditableState& State, FString& OutError)
{
    const EVirtualSensorKind RequestedKind = State.TargetKind == EVirtualSensorTargetKind::Lidar
        ? EVirtualSensorKind::Lidar
        : EVirtualSensorKind::Camera;
    AVirtualSensorActorBase* SensorActor = SensorManager ? SensorManager->GetSelectedSensorActorByKind(RequestedKind) : nullptr;
    if (!SensorActor)
    {
        OutError = TEXT("선택한 V2 센서 Actor를 사용할 수 없습니다.");
        return false;
    }
    return SensorActor->ApplyEditableState(State, OutError);
}

bool UVirtualSensorSettingsPanelWidget::ValidateState(const FVirtualSensorEditableState& State, FString& OutError) const
{
    if (!GetWorld()) { OutError = TEXT("World를 사용할 수 없습니다"); return false; }
    const UVirtualCameraCaptureComponent* SelectedCamera = State.TargetKind == EVirtualSensorTargetKind::Camera && SensorManager ? SensorManager->GetSelectedCamera() : nullptr;
    const UVirtualLidarScanComponent* SelectedLidar = State.TargetKind == EVirtualSensorTargetKind::Lidar && SensorManager ? SensorManager->GetSelectedLidar() : nullptr;
    TArray<FString> OtherSensorIds;
    for (TActorIterator<AVirtualCameraSensorActor> It(GetWorld()); It; ++It)
    {
        const UVirtualCameraCaptureComponent* Comp = It->CaptureComponent;
        if (Comp && Comp != SelectedCamera) OtherSensorIds.Add(Comp->SensorId);
    }
    for (TActorIterator<AVirtualLidarSensorActor> It(GetWorld()); It; ++It)
    {
        const UVirtualLidarScanComponent* Comp = It->ScanComponent;
        if (Comp && Comp != SelectedLidar) OtherSensorIds.Add(Comp->SensorId);
    }
    if (!ValidateEditableStateValues(State, OtherSensorIds, OutError)) return false;
    const EVirtualSensorKind RequestedKind = State.TargetKind == EVirtualSensorTargetKind::Lidar
        ? EVirtualSensorKind::Lidar
        : EVirtualSensorKind::Camera;
    AVirtualSensorActorBase* SensorActor = SensorManager ? SensorManager->GetSelectedSensorActorByKind(RequestedKind) : nullptr;
    return SensorActor ? SensorActor->ValidateEditableState(State, OutError) : false;
}

bool UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(const FVirtualSensorEditableState& State, const TArray<FString>& OtherSensorIds, FString& OutError)
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

void UVirtualSensorSettingsPanelWidget::RefreshPendingState(bool bCaptureInitialValue)
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

void UVirtualSensorSettingsPanelWidget::RefreshNativeText()
{
    if (NativeStatusText.IsValid()) NativeStatusText->SetText(FText::FromString(GetControlStatusText()));
}

void UVirtualSensorSettingsPanelWidget::SpawnGizmoIfNeeded()
{
    if (GizmoActor || !GetWorld()) return;
    GizmoActor = GetWorld()->SpawnActor<AVirtualSensorTransformGizmoActor>();
    if (!GizmoActor) return;
    GizmoActor->OnTransformChanged.AddUObject(this, &UVirtualSensorSettingsPanelWidget::HandleGizmoTransformChanged);
    GizmoActor->OnTransformCommitted.AddUObject(this, &UVirtualSensorSettingsPanelWidget::HandleGizmoTransformCommitted);
    GizmoActor->SetGizmoMode(GizmoMode);
    GizmoActor->SetCoordinateSpace(CoordinateSpace);
    GizmoActor->SetGizmoVisible(bGizmoVisible);
    GizmoActor->SetManipulationEnabled(bManipulationEnabled);
    GizmoActor->SetProjectionDebugEnabled(bProjectionDebugEnabled);
    GizmoActor->SetStepSizes(TranslationStepCm, RotationStepDegrees);
}

void UVirtualSensorSettingsPanelWidget::SyncGizmoTarget()
{
    if (!GizmoActor) return;
    AActor* SelectedActor = GetSelectedSensorActor();
    GizmoActor->BindTarget(SelectedActor);
    LastSyncedSensorActor = SelectedActor;
    GizmoActor->SetProjectionDebugEnabled(bProjectionDebugEnabled);
}

AActor* UVirtualSensorSettingsPanelWidget::GetSelectedSensorActor() const
{
    if (!SensorManager) return nullptr;
    const EVirtualSensorKind RequestedKind = PendingState.TargetKind == EVirtualSensorTargetKind::Lidar
        ? EVirtualSensorKind::Lidar
        : EVirtualSensorKind::Camera;
    return SensorManager->GetSelectedSensorActorByKind(RequestedKind);
}

void UVirtualSensorSettingsPanelWidget::HandleGizmoTransformChanged(const FTransform& Transform)
{
    if (Transform.ContainsNaN()) return;
    PendingState.ActorTransform = Transform;
    LastControlMessage = TEXT("미리보기 갱신 중...");
	if (AVirtualSensorActorBase* SensorActor = Cast<AVirtualSensorActorBase>(GetSelectedSensorActor()))
	{
		if (!SensorActor->IsInteractiveManipulationActive()) SensorActor->BeginInteractiveManipulation(InteractionRequest);
		SensorActor->UpdateInteractiveTransform(Transform);
	}
	LastControlMessage = TEXT("조작 중: 경량 미리보기");
    RefreshNativeText();
}

void UVirtualSensorSettingsPanelWidget::HandleGizmoTransformCommitted(const FTransform& Transform)
{
    if (Transform.ContainsNaN()) return;
    PendingState.ActorTransform = Transform;
	if (AVirtualSensorActorBase* SensorActor = Cast<AVirtualSensorActorBase>(GetSelectedSensorActor()))
	{
		SensorActor->UpdateInteractiveTransform(Transform);
	}
	LastControlMessage = TEXT("조작 위치가 PIE에 반영됨");
    LastControlMessage = FString::Printf(TEXT("PIE에 적용됨: %s"), *PendingState.SensorId);
    RefreshNativeText();
}

void UVirtualSensorSettingsPanelWidget::RefreshSelectedSensorNow(bool bForce)
{
    if (!SensorManager || !GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bForce && LastPreviewRefreshTime >= 0.0 && Now - LastPreviewRefreshTime < 0.1) return;
    LastPreviewRefreshTime = Now;
    SensorManager->RefreshSelectedSensorOnce(PendingState.TargetKind == EVirtualSensorTargetKind::Lidar);
}

void UVirtualSensorSettingsPanelWidget::StartAllRealSensorSources()
{
    const int32 Count = SensorManager ? SensorManager->StartAllRealSensorSources() : 0;
    LastControlMessage = FString::Printf(TEXT("외부 센서 소스 %d개를 시작했습니다."), Count);
    RefreshNativeText();
}

void UVirtualSensorSettingsPanelWidget::StopAllRealSensorSources()
{
    if (SensorManager) SensorManager->StopAllRealSensorSources();
    LastControlMessage = SensorManager ? TEXT("외부 센서 소스를 모두 중지했습니다.") : TEXT("SensorManager가 연결되지 않았습니다.");
    RefreshNativeText();
}

void UVirtualSensorSettingsPanelWidget::PushSelectedRealSensorSource()
{
    const bool bSucceeded = SensorManager && SensorManager->PushSelectedRealSensorSourceOnce(false);
    LastControlMessage = bSucceeded
        ? TEXT("선택한 외부 Source 프레임을 센서에 1회 주입했습니다. 외부 전송은 캡처/내보내기 패널에서 선택하세요.")
        : TEXT("선택한 외부 Source 프레임 주입에 실패했습니다. Source 선택과 입력 데이터를 확인하세요.");
    RefreshNativeText();
}

void UVirtualSensorSettingsPanelWidget::RestoreSettingsUiPreferences()
{
    const UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
    if (Preferences)
    {
        bKeyboardHelpExpanded = Preferences->bSettingsKeyboardHelpExpanded;
    }
}

void UVirtualSensorSettingsPanelWidget::SaveSettingsUiPreferences() const
{
    UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
    if (!Preferences) return;
    Preferences->bSettingsKeyboardHelpExpanded = bKeyboardHelpExpanded;
    UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
}

void UVirtualSensorSettingsPanelWidget::ResetSettingsUiPreferencesToDefault()
{
    bKeyboardHelpExpanded = false;
    SaveSettingsUiPreferences();
}

FString UVirtualSensorSettingsPanelWidget::BuildDeviceSpecText() const
{
    if (!SensorManager) return TEXT("장비 사양을 불러올 수 없습니다");
    if (PendingState.TargetKind == EVirtualSensorTargetKind::Camera)
    {
        const UVirtualCameraCaptureComponent* Camera = SensorManager->GetSelectedCamera();
        if (!Camera) return TEXT("D455를 사용할 수 없습니다");
        const FVirtualSensorDeviceSpec& Spec = Camera->GetDeviceSpec();
        return FString::Printf(TEXT("%s %s | Depth FOV %.0fx%.0f | 권장 %.2f-%.1fm | %dx%d @ %.0fHz\n주의: SceneCapture 미리보기는 보정된 Depth Stream이 아닙니다."), *Spec.Manufacturer, *Spec.Model, Spec.HorizontalFovDegrees, Spec.VerticalFovDegrees, Spec.MinRangeCm / 100.0f, Spec.MaxRangeCm / 100.0f, Spec.Width, Spec.Height, Spec.FrameRateHz);
    }
    const UVirtualLidarScanComponent* Lidar = SensorManager->GetSelectedLidar();
    if (!Lidar) return TEXT("Mid-360S를 사용할 수 없습니다");
    const FVirtualSensorDeviceSpec& Spec = Lidar->GetDeviceSpec();
    return FString::Printf(TEXT("%s %s | FOV %.0fx%.0f | %.1f-%.0fm | %d points/s @ %.0fHz\n런타임 광선 수는 시뮬레이션 품질에서 별도로 제어합니다."), *Spec.Manufacturer, *Spec.Model, Spec.HorizontalFovDegrees, Spec.VerticalFovDegrees, Spec.MinRangeCm / 100.0f, Spec.MaxRangeCm / 100.0f, Spec.PointRate, Spec.FrameRateHz);
}

FName UVirtualSensorSettingsPanelWidget::ResolvePersistentActorTag(const AActor* Actor) const
{
    if (!Actor) return NAME_None;
    for (const FName Tag : Actor->Tags)
    {
        if (Tag.ToString().StartsWith(TEXT("SensorTestPersistent_"))) return Tag;
    }
    return Actor->GetFName();
}

TSharedRef<SWidget> UVirtualSensorSettingsPanelWidget::MakeFloatRow(const FText& Label, TFunction<float()> Getter, TFunction<void(float)> Setter, float Min, float Max, bool bApplyOnCommit, FName HelpKey)
{
    const FVirtualSensorSettingHelpDescriptor* Help = FindSettingHelp(HelpKey);
    const FText ToolTip = Help ? FText::Format(LOCTEXT("SettingTipFormat", "{0}\n성능 영향: {1}\n권장값: {2}"), Help->Description, Help->PerformanceImpact, Help->RecommendedValue) : FText::GetEmpty();
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.47f)[ SNew(STextBlock).Text(Label).ToolTipText(ToolTip) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
        [
            SNew(SButton)
            .Visibility(Help ? EVisibility::Visible : EVisibility::Collapsed)
            .ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
            .ForegroundColor(FVirtualSensorUiStyle::Accent)
            .Text(LOCTEXT("InfoButton", "ⓘ"))
            .ToolTipText(ToolTip)
            .OnClicked_Lambda([this, HelpKey]() { SelectSettingHelp(HelpKey); return FReply::Handled(); })
        ]
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

TSharedRef<SWidget> UVirtualSensorSettingsPanelWidget::MakeIntRow(const FText& Label, TFunction<int32()> Getter, TFunction<void(int32)> Setter, int32 Min, int32 Max, FName HelpKey)
{
    const FVirtualSensorSettingHelpDescriptor* Help = FindSettingHelp(HelpKey);
    const FText ToolTip = Help ? FText::Format(LOCTEXT("SettingIntTipFormat", "{0}\n성능 영향: {1}\n권장값: {2}"), Help->Description, Help->PerformanceImpact, Help->RecommendedValue) : FText::GetEmpty();
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.47f)[ SNew(STextBlock).Text(Label).ToolTipText(ToolTip) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
        [
            SNew(SButton)
            .Visibility(Help ? EVisibility::Visible : EVisibility::Collapsed)
            .ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
            .ForegroundColor(FVirtualSensorUiStyle::Accent)
            .Text(LOCTEXT("InfoIntButton", "ⓘ"))
            .ToolTipText(ToolTip)
            .OnClicked_Lambda([this, HelpKey]() { SelectSettingHelp(HelpKey); return FReply::Handled(); })
        ]
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
