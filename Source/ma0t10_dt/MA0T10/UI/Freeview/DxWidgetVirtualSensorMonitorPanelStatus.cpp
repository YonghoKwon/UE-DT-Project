#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorMonitorPanel.h"
#include "DeviceUI/DeviceSensorMonitorBody.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "DeviceUI/SensorToolWidgetDecl.h"
#include "ma0t10_dt/MA0T10/UI/Common/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorPlacementPanel.h"
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
#include "Device/Sensor/VirtualLidarHeight.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Device/Camera/VirtualCameraCaptureComponent.h"
#include "Device/Sensor/RealSensorSourceComp.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarSensorTypes.h"
#include "Device/Sensor/VirtualLidarVisualizationComp.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "Device/Core/VirtualSensorSchedulerSubsystem.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"
#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorUiPreferences.h"
#include "DeviceUI/VirtualSensorUiStyle.h"

#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"

namespace
{


FString TransportModeToText(EVirtualSensorTransportMode Mode)
{
    switch (Mode)
    {
    case EVirtualSensorTransportMode::None:
        return TEXT("None");
    case EVirtualSensorTransportMode::LogOnly:
        return TEXT("LogOnly");
    case EVirtualSensorTransportMode::SaveToFile:
        return TEXT("SaveToFile");
    case EVirtualSensorTransportMode::HttpPost:
        return TEXT("HttpPost");
    default:
        return TEXT("Unknown");
    }
}

FString LidarViewModeDisplayText(EVirtualLidarViewMode ViewMode)
{
    if (ViewMode == EVirtualLidarViewMode::DepthGradient) return TEXT("거리 색상");
    if (ViewMode == EVirtualLidarViewMode::HitMask) return TEXT("검출 마스크");
    if (ViewMode == EVirtualLidarViewMode::ActorClassColor) return TEXT("의미 분류 색상");
    return TEXT("거리 회색조");
}
}


FString UDxWidgetVirtualSensorMonitorPanel::GetMonitorTitleText() const
{
    return BuildTitleText();
}

FString UDxWidgetVirtualSensorMonitorPanel::GetMonitorStatusText() const
{
    return BuildStatusText();
}

FString UDxWidgetVirtualSensorMonitorPanel::GetSelectedSensorIdText() const
{
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        return FString::Printf(TEXT("Sensor: %s"), *Status.SensorId);
    }
    if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        const FString DisplaySensorId = Status.SensorId.IsEmpty() ? CameraComp->SensorId : Status.SensorId;
        return FString::Printf(TEXT("Sensor: %s"), *DisplaySensorId);
    }
    return bShowingLidar ? TEXT("Sensor: LIDAR not bound") : TEXT("Sensor: Camera not bound");
}

FString UDxWidgetVirtualSensorMonitorPanel::GetFrameSummaryText() const
{ return FDeviceSensorMonitorBody::GetFrameSummaryText(*this); }

FString UDxWidgetVirtualSensorMonitorPanel::GetMeasurementSummaryText() const
{ return FDeviceSensorMonitorBody::GetMeasurementSummaryText(*this); }

FString UDxWidgetVirtualSensorMonitorPanel::GetPreviewPolicySummaryText() const
{ return FDeviceSensorMonitorBody::GetPreviewPolicySummaryText(*this); }

FString UDxWidgetVirtualSensorMonitorPanel::GetSlabAnalysisSummaryText() const
{
    if(const auto* Workspace=GetToolWorkspace())if(!Workspace->AreSlabToolsEnabled())return FString();
    if (!LidarComp)
    {
        return TEXT("Slab: unavailable");
    }

    const FVirtualLidarSlabAnalysisResult& Slab = LidarComp->GetRuntimeStatus().SlabAnalysis;
    return FString::Printf(TEXT("Slab: %s Points=%d Angle=%.2f Dev=%.2f Conf=%.2f"),
        Slab.bValid ? TEXT("Valid") : *Slab.StatusMessage,
        Slab.SlabHitPointCount,
        Slab.EstimatedYawDegrees,
        Slab.AngleDeviationDegrees,
        Slab.Confidence);
}

FString UDxWidgetVirtualSensorMonitorPanel::GetTransportWarningText() const
{ return FDeviceSensorMonitorBody::GetTransportWarningText(*this); }

FString UDxWidgetVirtualSensorMonitorPanel::GetTransportStatusSummaryText() const
{ return FDeviceSensorMonitorBody::GetTransportStatusSummaryText(*this); }

FString UDxWidgetVirtualSensorMonitorPanel::GetViewModeSummaryText() const
{ return FDeviceSensorMonitorBody::GetViewModeSummaryText(*this); }

FString UDxWidgetVirtualSensorMonitorPanel::GetAcceptanceGateSummaryText() const
{ return FDeviceSensorMonitorBody::GetAcceptanceGateSummaryText(*this); }

FString UDxWidgetVirtualSensorMonitorPanel::GetRealSensorDeploymentSummaryText() const
{ return FDeviceSensorMonitorBody::GetRealSensorDeploymentSummaryText(*this); }
void UDxWidgetVirtualSensorMonitorPanel::UpdateOwnedAcquisitionRateWarning()
{
    auto* Actor = GetOwnedMonitoredActor();
    if (OwnedRateWarningTarget.Get() != Actor)
    {
        OwnedAcquisitionRateWarning.Reset();
        OwnedRateWarningTarget = Actor;
    }
    const bool bEligible = IsValid(Actor) && bShowingLidar && Actor->IsSensorRunning() &&
        !Actor->IsInteractiveManipulationActive() && GetWorld() && !GetWorld()->IsPaused();
    const auto Status = bEligible ? Actor->GetSensorRuntimeStatus() : FVirtualSensorRuntimeStatus();
    OwnedAcquisitionRateWarning.Observe(FPlatformTime::Seconds(), Status.FrameId,
        Status.RequestedAcquisitionRateHz, bEligible);
}

FString UDxWidgetVirtualSensorMonitorPanel::BuildOwnedMeasurementWarning() const
{
    const auto* Actor=GetOwnedMonitoredActor();if(!IsValid(Actor)||!Actor->IsSensorRunning())return FString();
    if(const auto* Rate=GetSelectedRateDiagnostic())
    {
        if(Rate->State==EVirtualSensorRateState::Starved)return TEXT("측정 정체 · 마지막 측정 이후 새 데이터가 없습니다");
        if(Rate->State==EVirtualSensorRateState::Failed)return TEXT("측정 오류 · 개발자 정보에서 상세를 확인하세요");
    }
    if (OwnedAcquisitionRateWarning.IsBelowRequest())
        return FString::Printf(TEXT("요청 측정률 미달 · 요청 %.1fHz / 실제 %.1fHz · 측정 설정과 시스템 부하를 확인하세요"),
            OwnedAcquisitionRateWarning.GetRequestedHz(), OwnedAcquisitionRateWarning.GetObservedHz());
    const auto Status=Actor->GetSensorRuntimeStatus();
    if(!Status.PerformanceWarning.IsEmpty())return SensorToolUiText::UserReason(Status.PerformanceWarning);
    if(!bShowingLidar)
    {
        for(const auto* Word:{TEXT("실패"),TEXT("오류"),TEXT("차단"),TEXT("불가"),TEXT("failed"),TEXT("error"),TEXT("unavailable")})
            if(Status.LastMessage.Contains(Word,ESearchCase::IgnoreCase))return SensorToolUiText::UserReason(Status.LastMessage);
    }
    return FString();
}

void UDxWidgetVirtualSensorMonitorPanel::RefreshTitle()
{
    const FString Title = BuildTitleText();
    if (TitleText)
    {
        TitleText->SetText(FText::FromString(Title));
    }
    if (NativeTitleTextBlock.IsValid())
    {
        NativeTitleTextBlock->SetText(FText::FromString(Title));
    }
    if (ToggleButtonText)
    {
        ToggleButtonText->SetText(FText::FromString(bShowingLidar ? TEXT("Show Camera View") : TEXT("Show LIDAR View")));
    }
    RefreshLocalCaptureButtonText();
    RefreshLidarViewModeButtonText();
}

void UDxWidgetVirtualSensorMonitorPanel::RefreshStatusText()
{
    if(IsWorkspaceOwned()&&(!WidgetTree||!WidgetTree->RootWidget)){RefreshOwnedMonitorText();return;}
    const FString Text = BuildStatusText();
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Text));
    }
    if (NativeStatusTextBlock.IsValid())
    {
        NativeStatusTextBlock->SetText(FText::FromString(BuildCompactStatusText()));
    }
    if (bMonitorDetailsExpanded && NativeDetailedStatusTextBlock.IsValid() && (!IsWorkspaceOwned() || bOwnedAdvancedMenuOpen))
    {
        NativeDetailedStatusTextBlock->SetText(FText::FromString(Text));
    }
    if (NativeWarningTextBlock.IsValid())
    {
        const FString Warning=GetTransportWarningText();
        NativeWarningTextBlock->SetText(FText::FromString(IsWorkspaceOwned()?Warning.Replace(TEXT("\n"),TEXT(" ")).Replace(TEXT("\r"),TEXT(" ")):Warning));
        if(IsWorkspaceOwned())NativeWarningTextBlock->SetToolTipText(FText::FromString(Warning));
    }
}

void UDxWidgetVirtualSensorMonitorPanel::RefreshNativeFallbackText()
{
    if(IsWorkspaceOwned()&&(!WidgetTree||!WidgetTree->RootWidget)){RefreshOwnedMonitorText();return;}
    RefreshTitle();
    RefreshStatusText();
}

FString UDxWidgetVirtualSensorMonitorPanel::BuildTitleText() const
{
    const bool bPointCloudOnly = SensorManager && SensorManager->IsPointCloudOnlyModeEnabled();
    return bPointCloudOnly ? TEXT("LiDAR 포인트 클라우드 전용 보기")
        : (bShowingLidar ? TEXT("가상 LiDAR 모니터") : TEXT("가상 카메라 모니터"));
}

FText UDxWidgetVirtualSensorMonitorPanel::BuildPointCloudOnlyButtonText() const
{
    return FText::FromString(SensorManager && SensorManager->IsPointCloudOnlyModeEnabled()
        ? TEXT("포인트 클라우드 전용 끄기")
        : TEXT("포인트 클라우드 전용 켜기"));
}

FString UDxWidgetVirtualSensorMonitorPanel::GetPointCloudRendererStatusText() const
{ return FDeviceSensorMonitorBody::GetPointCloudRendererStatusText(*this); }

FString UDxWidgetVirtualSensorMonitorPanel::BuildCompactStatusText() const
{
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        if(IsWorkspaceOwned())return FormatOwnedSensorRate(TEXT("LiDAR"),LidarComp->SensorId,Status,LidarComp->IsScanRunning(),GetSelectedRateDiagnostic());
        return FString::Printf(TEXT("프레임 %lld · %.2f Hz\n광선 %d · 측정점 %d · 검출점 %d"),
            Status.FrameId,
            Status.MeasuredCompletionRateHz,
            LidarComp->HorizontalSamples * LidarComp->VerticalChannels,
            Status.TotalPointCount,
            Status.HitPointCount);
    }
    if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        if(IsWorkspaceOwned())return FormatOwnedSensorRate(TEXT("Camera"),CameraComp->SensorId,Status,CameraComp->IsCaptureRunning(),GetSelectedRateDiagnostic());
        return FString::Printf(TEXT("프레임 %lld · %.2f Hz\n해상도 %d × %d"),
            Status.FrameId, Status.MeasuredCompletionRateHz, CameraComp->CaptureResolution.X, CameraComp->CaptureResolution.Y);
    }
    return TEXT("센서 연결 대기 중");
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        return FString::Printf(TEXT("프레임 %lld · %.2f Hz\n광선 %d · 측정점 %d · 검출점 %d"),
            Status.FrameId,
            LidarComp->ScanInterval > KINDA_SMALL_NUMBER ? 1.0f / LidarComp->ScanInterval : 0.0f,
            LidarComp->HorizontalSamples * LidarComp->VerticalChannels,
            Status.TotalPointCount,
            Status.HitPointCount);
    }
    if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        return FString::Printf(TEXT("프레임 %lld · %.2f Hz\n해상도 %d × %d"),
            Status.FrameId,
            CameraComp->CaptureInterval > KINDA_SMALL_NUMBER ? 1.0f / CameraComp->CaptureInterval : 0.0f,
            CameraComp->CaptureResolution.X,
            CameraComp->CaptureResolution.Y);
    }
    return bShowingLidar ? TEXT("LiDAR가 연결되지 않았습니다.") : TEXT("카메라가 연결되지 않았습니다.");
}

void UDxWidgetVirtualSensorMonitorPanel::RefreshLidarViewModeButtonText()
{
    if (LidarViewModeButtonText)
    {
        LidarViewModeButtonText->SetText(FText::FromString(FString::Printf(TEXT("LiDAR 표시: %s"), *GetLidarViewModeDisplayText())));
    }
    if (PreviewHitOnlyButtonText)
    {
        const UVirtualLidarScanComp* TargetLidar = GetTargetLidarForPreview();
        PreviewHitOnlyButtonText->SetText(FText::FromString(TargetLidar && TargetLidar->bPointCloudPreviewHitOnly ? TEXT("미리보기: 검출점만") : TEXT("미리보기: 모든 점")));
    }
}

FString UDxWidgetVirtualSensorMonitorPanel::GetLidarViewModeDisplayText() const
{
    if (!LidarComp)
    {
        return TEXT("없음");
    }
    return LidarViewModeDisplayText(LidarComp->ViewMode);
}

FString UDxWidgetVirtualSensorMonitorPanel::GetLidarViewLegendText() const
{
    if (const UVirtualLidarVisualizationComp* Visualization = GetLidarVisualizationComponent())
    {
        return Visualization->GetLegendText();
    }
    if (!LidarComp)
    {
        return TEXT("LiDAR가 연결되지 않았습니다.");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::HitMask)
    {
        return TEXT("흰색=검출 · 검정=미검출 · 흰 윤곽선=깊이 경계");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        return BuildSemanticLegendText();
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::IntensityGray)
    {
        return TEXT("밝음=가까움 · 어두움=멀거나 미검출 · 흰 윤곽선=깊이 경계");
    }
    return TEXT("자홍/주황=가까움 · 노랑=중간 · 청록/파랑=멀음 · 검정=미검출");
}

FString UDxWidgetVirtualSensorMonitorPanel::GetLidarViewModeDescription() const
{
    if (const UVirtualLidarVisualizationComp* Visualization = GetLidarVisualizationComponent())
    {
        const FVirtualLidarVisualizationSettings& Settings = Visualization->GetVisualizationSettings();
        FString ProjectionDescription;
        switch (Settings.ProjectionMode)
        {
        case ELidarMonitorProjectionMode::TopDown:
            ProjectionDescription = TEXT("센서 로컬 조감도: 센서 기준 XY 평면에 점을 투영합니다. 수평 설치 센서의 전방·좌우 분포를 볼 때 적합합니다.");
            break;
		case ELidarMonitorProjectionMode::WorldTopDown:
			ProjectionDescription = TEXT("월드 XY 조감도: 센서 회전과 무관하게 월드 바닥 평면에 점을 투영합니다. 가로는 월드 Y, 세로는 월드 X이며 수직 설치 센서의 평면도에 적합합니다.");
			break;
        case ELidarMonitorProjectionMode::Elevation:
			ProjectionDescription = TEXT("방사 거리-높이 프로파일: X축은 sqrt(X²+Y²) 수평 방사거리, Y축은 센서 기준 높이입니다. 좌우 방향은 합쳐지며 바닥·천장·경사·물체 높이 분석에 적합합니다.");
			break;
		case ELidarMonitorProjectionMode::ForwardSlice:
			ProjectionDescription = TEXT("전방 수직 슬라이스: 선택한 방향과 두께 범위 안의 점만 센서 로컬 X-Z 단면으로 표시합니다. 우클릭 드래그로 슬라이스 방향을 회전합니다.");
            break;
        case ELidarMonitorProjectionMode::Split:
            ProjectionDescription = TEXT("분할: 거리 영상과 조감도를 동시에 표시합니다.");
            break;
        case ELidarMonitorProjectionMode::RangeImage:
        default:
            ProjectionDescription = TEXT("거리 영상: LiDAR 수직 채널과 수평 샘플 순서대로 측정 결과를 표시합니다.");
            break;
        }

        FString ColorDescription;
        switch (Settings.ColorMode)
        {
        case ELidarColorMode::DistanceViridis: ColorDescription = TEXT("Viridis 거리 색상은 색각 친화 팔레트로 거리를 구분합니다."); break;
        case ELidarColorMode::RelativeHeight:
            ColorDescription = VirtualLidarHeight::IsWorld(Settings)
                ? TEXT("월드 Z 높이에 따라 낮은 점부터 높은 점까지 색상을 매핑합니다.")
                : TEXT("센서 상대 높이에 따라 낮은 점부터 높은 점까지 색상을 매핑합니다.");
            break;
        case ELidarColorMode::SemanticLabel: ColorDescription = TEXT("SemanticLabel 규칙에 설정된 의미 분류 색상을 사용합니다."); break;
        case ELidarColorMode::GeometrySeparation: ColorDescription = TEXT("측정 XYZ만으로 주요 수평면과 돌출 부분을 구분합니다. 판을 찾지 못하면 월드 높이 색상으로 표시합니다. 태그·재질은 사용하지 않습니다."); break;
        case ELidarColorMode::VerticalChannel: ColorDescription = TEXT("각 점을 수직 채널(ring) 번호로 구분합니다."); break;
        case ELidarColorMode::ReturnIndex: ColorDescription = TEXT("MultiHit 측정의 return index를 서로 다른 색으로 구분합니다."); break;
        case ELidarColorMode::HitMask: ColorDescription = TEXT("검출 성공은 흰색, 미검출은 검정으로 표시합니다."); break;
        case ELidarColorMode::DistanceGray: ColorDescription = TEXT("가까운 점은 밝게, 먼 점은 어둡게 표시합니다."); break;
        case ELidarColorMode::DistanceTurbo:
        default: ColorDescription = TEXT("연속 Turbo 팔레트로 가까운 거리부터 먼 거리까지 표현합니다."); break;
        }
        return ProjectionDescription + TEXT("\n") + ColorDescription;
    }
    if (!LidarComp) return TEXT("LiDAR를 연결하면 표시 방식의 설명과 범례를 확인할 수 있습니다.");
    if (LidarComp->ViewMode == EVirtualLidarViewMode::HitMask)
    {
        return TEXT("거리와 분류를 무시하고 광선의 검출 성공 여부만 빠르게 확인합니다.");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        return TEXT("Actor 태그·클래스에서 계산한 SemanticLabel별 설정 색상을 표시합니다.");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::IntensityGray)
    {
        return TEXT("가까운 측정점을 밝게, 먼 측정점을 어둡게 표시해 깊이 구조를 단순하게 확인합니다.");
    }
    return bUseAdaptiveLidarDepthRange
        ? TEXT("현재 프레임의 최소·최대 검출 거리를 기준으로 색상 대비를 자동 조정합니다.")
        : TEXT("센서의 최대 측정 거리를 기준으로 가까운 점부터 먼 점까지 색상을 나눕니다.");
}

FString UDxWidgetVirtualSensorMonitorPanel::BuildSemanticLegendText() const
{ return FDeviceSensorMonitorBody::BuildSemanticLegendText(*this); }
#undef LOCTEXT_NAMESPACE
