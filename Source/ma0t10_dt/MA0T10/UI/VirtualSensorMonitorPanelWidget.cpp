#include "VirtualSensorMonitorPanelWidget.h"

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
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
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
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"

#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"

namespace
{
struct FLocalLidarCsvPoint
{
    int32 Row = 0;
    int32 Col = 0;
    int32 ReturnIndex = 0;
    FVector Point = FVector::ZeroVector;
};

FString BuildPointCloudTimestamp()
{
    const FDateTime NowUtc = FDateTime::UtcNow();
    return FString::Printf(TEXT("%s_%03d_%lld"), *NowUtc.ToString(TEXT("%Y%m%d_%H%M%S")), NowUtc.GetMillisecond(), NowUtc.GetTicks());
}

FString BuildPointCloudExportDirectory(const UVirtualLidarScanComponent* LidarComp)
{
    const FString SensorId = LidarComp ? LidarComp->SensorId : TEXT("LIDAR");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

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

FString BuildServerPayloadExportDirectory(const UVirtualLidarScanComponent* LidarComp)
{
    const FString SensorId = LidarComp ? LidarComp->SensorId : TEXT("LIDAR");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("ServerPayload"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

FString BuildCameraPayloadExportDirectory(const UVirtualCameraCaptureComponent* CameraComp)
{
    const FString SensorId = CameraComp ? CameraComp->SensorId : TEXT("CAMERA");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("ServerPayload"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

bool SaveServerPayloadJsonFile(const FString& Directory, const FString& Prefix, const FString& SensorId, const FString& Payload, FString& OutPath)
{
    const FString SafePrefix = FPaths::MakeValidFileName(Prefix.IsEmpty() ? TEXT("manual_server_payload") : Prefix);
    const FString SafeSensorId = FPaths::MakeValidFileName(SensorId.IsEmpty() ? TEXT("UNKNOWN_SENSOR") : SensorId);
    OutPath = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s_%s.json"), *SafePrefix, *SafeSensorId, *BuildPointCloudTimestamp()));
    return FFileHelper::SaveStringToFile(Payload, *OutPath);
}

float NormalizeMonitorLidarDistance(const FVirtualLidarPoint& Point, float MaxDistance, float MinHitDistance, float MaxHitDistance, bool bUseAdaptiveDepthRange)
{
    if (!Point.bHit)
    {
        return 1.0f;
    }

    if (bUseAdaptiveDepthRange && MaxHitDistance > MinHitDistance + KINDA_SMALL_NUMBER)
    {
        return FMath::Clamp((Point.Distance - MinHitDistance) / (MaxHitDistance - MinHitDistance), 0.0f, 1.0f);
    }

    return FMath::Clamp(Point.Distance / FMath::Max(1.0f, MaxDistance), 0.0f, 1.0f);
}

FColor MakeMonitorLidarColor(const UVirtualLidarScanComponent* LidarComp, EVirtualLidarViewMode ViewMode, const FVirtualLidarPoint& Point, float NormalizedDistance)
{
    return UVirtualSensorMonitorPanelWidget::ResolveLidarPointDisplayColor(LidarComp, ViewMode, Point, NormalizedDistance);
}

FString LidarViewModeDisplayText(EVirtualLidarViewMode ViewMode)
{
    if (ViewMode == EVirtualLidarViewMode::DepthGradient) return TEXT("거리 색상");
    if (ViewMode == EVirtualLidarViewMode::HitMask) return TEXT("검출 마스크");
    if (ViewMode == EVirtualLidarViewMode::ActorClassColor) return TEXT("의미 분류 색상");
    return TEXT("거리 회색조");
    if (ViewMode == EVirtualLidarViewMode::DepthGradient) return TEXT("거리 색상");
    if (ViewMode == EVirtualLidarViewMode::HitMask) return TEXT("검출 마스크");
    if (ViewMode == EVirtualLidarViewMode::ActorClassColor) return TEXT("의미 분류 색상");
    return TEXT("거리 회색조");
}

FString LidarProjectionDisplayText(ELidarMonitorProjectionMode Mode)
{
    if (Mode == ELidarMonitorProjectionMode::TopDown) return TEXT("조감도");
	if (Mode == ELidarMonitorProjectionMode::Elevation) return TEXT("방사 거리-높이 프로파일");
	if (Mode == ELidarMonitorProjectionMode::ForwardSlice) return TEXT("전방 수직 슬라이스");
    if (Mode == ELidarMonitorProjectionMode::Split) return TEXT("거리 영상 + 조감도");
    return TEXT("거리 영상");
    switch (Mode)
    {
    case ELidarMonitorProjectionMode::TopDown: return TEXT("조감도");
    case ELidarMonitorProjectionMode::Elevation: return TEXT("거리-높이 단면");
    case ELidarMonitorProjectionMode::Split: return TEXT("거리 영상 + 조감도");
    default: return TEXT("거리 영상");
    }
}

FString LidarColorDisplayText(ELidarColorMode Mode)
{
    if (Mode == ELidarColorMode::DistanceViridis) return TEXT("거리 Viridis (색각 친화)");
    if (Mode == ELidarColorMode::RelativeHeight) return TEXT("센서 상대 높이");
    if (Mode == ELidarColorMode::SemanticLabel) return TEXT("의미 분류 색상");
    if (Mode == ELidarColorMode::VerticalChannel) return TEXT("수직 채널 / Ring");
    if (Mode == ELidarColorMode::ReturnIndex) return TEXT("MultiHit Return 번호");
    if (Mode == ELidarColorMode::HitMask) return TEXT("검출 마스크");
    if (Mode == ELidarColorMode::DistanceGray) return TEXT("거리 회색조");
    return TEXT("거리 Turbo");
    switch (Mode)
    {
    case ELidarColorMode::DistanceViridis: return TEXT("거리 Viridis (색각 친화)");
    case ELidarColorMode::RelativeHeight: return TEXT("센서 상대 높이");
    case ELidarColorMode::SemanticLabel: return TEXT("의미 분류 색상");
    case ELidarColorMode::VerticalChannel: return TEXT("수직 채널 / Ring");
    case ELidarColorMode::ReturnIndex: return TEXT("MultiHit Return 번호");
    case ELidarColorMode::HitMask: return TEXT("검출 마스크");
    case ELidarColorMode::DistanceGray: return TEXT("거리 회색조");
    default: return TEXT("거리 Turbo");
    }
}

FColor ApplyMonitorGridOverlay(const FColor& InColor, bool bHit, bool bIsGrid)
{
    if (!bIsGrid)
    {
        return InColor;
    }

    if (!bHit)
    {
        return FColor(36, 36, 42, 255);
    }

    return FColor(
        static_cast<uint8>(FMath::Clamp(static_cast<int32>(InColor.R) + 70, 0, 255)),
        static_cast<uint8>(FMath::Clamp(static_cast<int32>(InColor.G) + 70, 0, 255)),
        static_cast<uint8>(FMath::Clamp(static_cast<int32>(InColor.B) + 70, 0, 255)),
        255);
}

FIntPoint ResolveLidarGridCoord(const FVirtualLidarPoint& Point, int32 PointIndex, int32 HorizontalSamples)
{
    if (Point.bHasGridCoord)
    {
        return FIntPoint(Point.Row, Point.Col);
    }

    const int32 SafeHorizontalSamples = FMath::Max(1, HorizontalSamples);
    return FIntPoint(PointIndex / SafeHorizontalSamples, PointIndex % SafeHorizontalSamples);
}

FString BuildRowColCsvText(const UVirtualLidarScanComponent* LidarComp, int32& OutExportedPointCount)
{
    OutExportedPointCount = 0;
    FString Text;
    Text.Reserve(LidarComp ? FMath::Max(128, LidarComp->GetLastPoints().Num() * 64) : 128);
    Text += TEXT("row,col,returnIndex,x,y,z\n");
    if (!LidarComp)
    {
        return Text;
    }

    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (LidarComp->bExportHitOnlyPointCloud && !Point.bHit)
        {
            continue;
        }

        const FIntPoint GridCoord = ResolveLidarGridCoord(Point, PointIndex, LidarComp->HorizontalSamples);
        Text += FString::Printf(TEXT("%d,%d,%d,%f,%f,%f\n"),
            GridCoord.X,
            GridCoord.Y,
            Point.ReturnIndex,
            Point.WorldLocation.X,
            Point.WorldLocation.Y,
            Point.WorldLocation.Z);
        ++OutExportedPointCount;
    }
    return Text;
}

bool ExportRowColPointCloudCsv(const UVirtualLidarScanComponent* LidarComp, const FString& FileNamePrefix)
{
    if (!LidarComp)
    {
        return false;
    }

    int32 ExportedPointCount = 0;
    const FString Text = BuildRowColCsvText(LidarComp, ExportedPointCount);
    const FString Prefix = FileNamePrefix.IsEmpty() ? LidarComp->SensorId : FileNamePrefix;
    const FString Path = FPaths::Combine(BuildPointCloudExportDirectory(LidarComp), FString::Printf(TEXT("%s_%s.csv"), *Prefix, *BuildPointCloudTimestamp()));
    const bool bSaved = FFileHelper::SaveStringToFile(Text, *Path);
    if (bSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] row/col CSV export saved: %s points=%d"), *LidarComp->SensorId, *Path, ExportedPointCount);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] row/col CSV export failed: %s points=%d"), *LidarComp->SensorId, *Path, ExportedPointCount);
    }
    return bSaved;
}
}

FString UVirtualSensorMonitorPanelWidget::GetMonitorTitleText() const
{
    return BuildTitleText();
}

FString UVirtualSensorMonitorPanelWidget::GetMonitorStatusText() const
{
    return BuildStatusText();
}

FVirtualSensorMonitorDisplayData UVirtualSensorMonitorPanelWidget::GetMonitorDisplayData() const
{
    FVirtualSensorMonitorDisplayData Data;
    Data.bShowingLidar = bShowingLidar;
    Data.TitleText = GetMonitorTitleText();
    Data.SelectedSensorText = GetSelectedSensorIdText();
    Data.FrameText = GetFrameSummaryText();
    Data.MeasurementText = GetMeasurementSummaryText();
    Data.ServerPayloadText = GetServerPayloadSummaryText();
    Data.PreviewText = GetPreviewPolicySummaryText();
    Data.SlabText = GetSlabAnalysisSummaryText();
    Data.LazExportText = GetLazExportSummaryText();
    Data.TransportText = GetTransportStatusSummaryText();
    Data.WarningText = GetTransportWarningText();
    Data.ViewModeText = GetViewModeSummaryText();
    Data.AcceptanceGateText = GetAcceptanceGateSummaryText();
    Data.RealSensorText = GetRealSensorDeploymentSummaryText();
    Data.FullStatusText = GetMonitorStatusText();
    return Data;
}

FString UVirtualSensorMonitorPanelWidget::GetSelectedSensorIdText() const
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

FString UVirtualSensorMonitorPanelWidget::GetFrameSummaryText() const
{
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        return FString::Printf(TEXT("Frame: %lld | Scan: %.3fs"), Status.FrameId, LidarComp->ScanInterval);
    }
    if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        return FString::Printf(TEXT("Frame: %lld | Interval: %.3fs"), Status.FrameId, CameraComp->CaptureInterval);
    }
    return TEXT("Frame: unavailable");
}

FString UVirtualSensorMonitorPanelWidget::GetMeasurementSummaryText() const
{
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        return FString::Printf(TEXT("Rays: %d | Points/Hits: %d/%d"),
            LidarComp->HorizontalSamples * LidarComp->VerticalChannels,
            Status.TotalPointCount,
            Status.HitPointCount);
    }
    if (!bShowingLidar && CameraComp)
    {
        return FString::Printf(TEXT("Resolution: %dx%d"), CameraComp->CaptureResolution.X, CameraComp->CaptureResolution.Y);
    }
    return TEXT("Measurements: unavailable");
}

FString UVirtualSensorMonitorPanelWidget::GetServerPayloadSummaryText() const
{
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        return FString::Printf(TEXT("Server Payload: Points=%d Bytes=%d Stride=%d Max=%d IncludeMiss=%s"),
            Status.ServerPayloadPointCount,
            Status.LastPayloadLength,
            LidarComp->ServerPayloadStride,
            LidarComp->MaxServerPayloadPoints,
            LidarComp->bIncludeMissPointsInServerPayload ? TEXT("true") : TEXT("false"));
    }
    if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        return FString::Printf(TEXT("Payload: Bytes=%d Cached=%s"),
            Status.LastPayloadLength,
            CameraComp->GetLastJsonPayload().IsEmpty() ? TEXT("false") : TEXT("true"));
    }
    return TEXT("Server Payload: unavailable");
}

FString UVirtualSensorMonitorPanelWidget::GetPreviewPolicySummaryText() const
{
    const UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
    if (!TargetLidar)
    {
        return TEXT("Preview: unavailable");
    }

    const FVirtualSensorRuntimeStatus& Status = TargetLidar->GetRuntimeStatus();
    return FString::Printf(TEXT("Preview: %s Points=%d Stride=%d Max=%d HitOnly=%s"),
        TargetLidar->IsPointCloudPreviewEnabled() ? TEXT("On") : TEXT("Off"),
        Status.PreviewPointCount,
        TargetLidar->PreviewPointStride,
        TargetLidar->MaxPreviewPoints,
        TargetLidar->bPointCloudPreviewHitOnly ? TEXT("true") : TEXT("false"));
}

FString UVirtualSensorMonitorPanelWidget::GetSlabAnalysisSummaryText() const
{
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

FString UVirtualSensorMonitorPanelWidget::GetLazExportSummaryText() const
{
    const UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
    if (!TargetLidar)
    {
        return TEXT("LAZ Export: unavailable");
    }

    const FString LazStatusText = TargetLidar->GetLastLazExportStatusText();
    const FString LazWarningText = TargetLidar->GetLastLazExportWarningText();
    return FString::Printf(TEXT("LAZ Export: Attempted=%s Success=%s Placeholder=%s External=%s/%s/%s Produced=%s TrueValidated=%s Points=%d ReturnCode=%d Size=%lld Status=%s Warning=%s"),
        TargetLidar->WasLastLazExportAttempted() ? TEXT("true") : TEXT("false"),
        TargetLidar->DidLastLazExportSucceed() ? TEXT("true") : TEXT("false"),
        TargetLidar->WasLastLazExportPlaceholderOnly() ? TEXT("true") : TEXT("false"),
        TargetLidar->WasLastLazExternalCompressorRequested() ? TEXT("requested") : TEXT("not-requested"),
        TargetLidar->WasLastLazExternalCompressorAttempted() ? TEXT("attempted") : TEXT("not-attempted"),
        TargetLidar->DidLastLazExternalCompressorSucceed() ? TEXT("succeeded") : TEXT("not-succeeded"),
        TargetLidar->DidLastLazProduceOutputFile() ? TEXT("true") : TEXT("false"),
        TargetLidar->WasLastLazTrueCompressionValidated() ? TEXT("true") : TEXT("false"),
        TargetLidar->GetLastLazExportedPointCount(),
        TargetLidar->GetLastLazExternalCompressorReturnCode(),
        TargetLidar->GetLastLazOutputSizeBytes(),
        LazStatusText.IsEmpty() ? TEXT("None") : *LazStatusText,
        LazWarningText.IsEmpty() ? TEXT("None") : *LazWarningText);
}

FString UVirtualSensorMonitorPanelWidget::GetTransportWarningText() const
{
    if (bShowingLidar && LidarComp)
    {
        const FString& Warning = LidarComp->GetRuntimeStatus().PerformanceWarning;
        return FString::Printf(TEXT("상태/경고: %s"), Warning.IsEmpty() ? TEXT("없음") : *Warning);
    }
    if (!bShowingLidar && CameraComp)
    {
        const FString& Message = CameraComp->GetRuntimeStatus().LastMessage;
        return FString::Printf(TEXT("상태/경고: %s"), Message.IsEmpty() ? TEXT("없음") : *Message);
    }
    return TEXT("상태/경고: 센서 연결 없음");
}

FString UVirtualSensorMonitorPanelWidget::GetTransportStatusSummaryText() const
{
    const UVirtualSensorTransportComponent* TransportComp = SensorManager
        ? SensorManager->SharedTransportComponent.Get()
        : (bShowingLidar && LidarComp
            ? LidarComp->TransportComponent.Get()
            : (!bShowingLidar && CameraComp ? CameraComp->TransportComponent.Get() : nullptr));
    FString SchedulerText;
    if (GetWorld()) if (const UVirtualSensorSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>()) SchedulerText = TEXT("\n성능: ") + Scheduler->GetTelemetrySummaryText();
    if (!TransportComp) return TEXT("Transport: unavailable") + SchedulerText;

    const FVirtualSensorTransportResult& Result = TransportComp->LastResult;
    return FString::Printf(TEXT("Transport: Mode=%s InFlight=%d/%d BackpressureRejected=%d Retries=%d Failed=%d RetryExhausted=%d LastRetries=%d LastSubmitted=%s LastAccepted=%s LastCode=%d%s"),
        *TransportModeToText(TransportComp->TransportMode),
        TransportComp->InFlightHttpRequestCount,
        FMath::Max(1, TransportComp->MaxInFlightHttpRequests),
        TransportComp->BackpressureRejectedRequestCount,
        TransportComp->TotalHttpRetryAttemptCount,
        TransportComp->FailedHttpRequestCount,
        TransportComp->RetryExhaustedRequestCount,
        Result.RetryAttemptCount,
        Result.bSubmitted ? TEXT("true") : TEXT("false"),
        Result.bAccepted ? TEXT("true") : TEXT("false"),
        Result.HttpStatusCode,
        *SchedulerText);
}

FString UVirtualSensorMonitorPanelWidget::GetViewModeSummaryText() const
{
    if (bShowingLidar)
    {
        return FString::Printf(TEXT("LiDAR View: %s"), *GetLidarViewModeDisplayText());
    }
    return TEXT("Camera View: Render Target");
}

FString UVirtualSensorMonitorPanelWidget::GetAcceptanceGateSummaryText() const
{
    const UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
    const bool bTrueLazValidated = TargetLidar && TargetLidar->WasLastLazTrueCompressionValidated();
    const bool bHasServerPayload = bShowingLidar
        ? (TargetLidar && !TargetLidar->GetLastJsonPayload().IsEmpty())
        : (CameraComp && !CameraComp->GetLastJsonPayload().IsEmpty());

    return FString::Printf(TEXT("Acceptance Gates: WBP=ManualPIEPending Server=%s LAZ=%s RealSensor=DeploymentEvidencePending"),
        bHasServerPayload ? TEXT("PayloadCached") : TEXT("PayloadPending"),
        bTrueLazValidated ? TEXT("TrueValidated") : TEXT("TrueCompressionPending"));
}

FString UVirtualSensorMonitorPanelWidget::GetRealSensorDeploymentSummaryText() const
{
    if (RealSensorSourceComponent)
    {
        return RealSensorSourceComponent->GetDeploymentReadinessSummaryText();
    }
    return TEXT("Real Sensor Deployment: source not bound; live adapter evidence pending");
}

TSharedRef<SWidget> UVirtualSensorMonitorPanelWidget::RebuildWidget()
{
    if (!ShouldUseNativeFallbackWidget())
    {
        return Super::RebuildWidget();
    }

    RestoreMonitorUiPreferences();
    NativeLidarProjectionOptions.Reset();
    NativeLidarProjectionOptions.Add(MakeShared<ELidarMonitorProjectionMode>(ELidarMonitorProjectionMode::RangeImage));
    NativeLidarProjectionOptions.Add(MakeShared<ELidarMonitorProjectionMode>(ELidarMonitorProjectionMode::TopDown));
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

    TSharedPtr<ELidarMonitorProjectionMode> InitialProjection = NativeLidarProjectionOptions[0];
    TSharedPtr<ELidarColorMode> InitialColor = NativeLidarColorOptions[0];
    if (const UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate())
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
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SAssignNew(NativeTitleTextBlock, STextBlock)
                        .ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText)
                        .Text(FText::FromString(BuildTitleText()))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
                    [
                        SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
                        .ForegroundColor(FVirtualSensorUiStyle::PrimaryText)
                        .Text_Lambda([this]() { return FText::FromString(IsPanelCollapsed() ? TEXT("펼치기") : TEXT("접기")); })
                        .OnClicked_Lambda([this]() { TogglePanelCollapsed(); return FReply::Handled(); })
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
                        .ForegroundColor(FVirtualSensorUiStyle::PrimaryText)
                        .Text(LOCTEXT("ResetUi", "위치 초기화"))
                        .ToolTipText(LOCTEXT("ResetUiTip", "이 패널을 기본 위치로 되돌립니다."))
                        .OnClicked_Lambda([this]() { ResetPanelPosition(); return FReply::Handled(); })
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
                        .ForegroundColor(FVirtualSensorUiStyle::PrimaryText)
                        .Text(LOCTEXT("ResetMonitorSize", "크기 초기화"))
                        .ToolTipText(LOCTEXT("ResetMonitorSizeTip", "모니터를 현재 화면 해상도의 기본 크기로 되돌립니다."))
                        .OnClicked_Lambda([this]() { ResetPanelSize(); return FReply::Handled(); })
                    ]
                ]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 8.0f, 0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                .Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
                + SHorizontalBox::Slot().FillWidth(0.61f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.025f, 1.0f))
                    .Padding(2.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [ SAssignNew(NativeViewImage, SImage).Image(&NativeViewBrush) ]
                        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 2.0f, 0.0f, 0.0f)
                        [
                            SAssignNew(NativeSecondaryViewImage, SImage)
                            .Visibility_Lambda([this]() { return bShowingLidar && GetLidarProjectionMode() == ELidarMonitorProjectionMode::Split ? EVisibility::Visible : EVisibility::Collapsed; })
                            .Image(&NativeSecondaryViewBrush)
                        ]
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(0.39f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text_Lambda([this]() { return FText::FromString(GetSelectedSensorIdText()); }) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                        [ SAssignNew(NativeStatusTextBlock, STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).AutoWrapText(true).Text(FText::FromString(BuildCompactStatusText())) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 5.0f)
                        [ SAssignNew(NativeWarningTextBlock, STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Warning).AutoWrapText(true).Text(FText::FromString(GetTransportWarningText())) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
                            .ForegroundColor(FVirtualSensorUiStyle::PrimaryText)
                            .Text_Lambda([this]() { return FText::FromString(bMonitorDetailsExpanded ? TEXT("상세 진단 접기") : TEXT("상세 진단 펼치기")); })
                            .OnClicked_Lambda([this]() { bMonitorDetailsExpanded = !bMonitorDetailsExpanded; SaveMonitorUiPreferences(); return FReply::Handled(); })
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                        [
                            SAssignNew(NativeDetailedStatusTextBlock, STextBlock)
                            .Visibility_Lambda([this]() { return bMonitorDetailsExpanded ? EVisibility::Visible : EVisibility::Collapsed; })
                            .ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText)
                            .AutoWrapText(true)
                            .Text(FText::FromString(BuildStatusText()))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
                        [
                            SNew(STextBlock)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            .ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText)
                            .Text(LOCTEXT("LidarModeLabel", "LiDAR 표시 방식"))
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SComboBox<TSharedPtr<ELidarMonitorProjectionMode>>)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            .OptionsSource(&NativeLidarProjectionOptions)
                            .InitiallySelectedItem(InitialProjection)
                            .OnGenerateWidget_Lambda([](TSharedPtr<ELidarMonitorProjectionMode> Item)
                            {
                                return SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(FText::FromString(Item.IsValid() ? LidarProjectionDisplayText(*Item) : TEXT("없음")));
                            })
                            .OnSelectionChanged_Lambda([this](TSharedPtr<ELidarMonitorProjectionMode> Item, ESelectInfo::Type)
                            {
                                if (Item.IsValid()) SetLidarProjectionMode(*Item);
                            })
                            [ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(LidarProjectionDisplayText(GetLidarProjectionMode())); }) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 2.0f)
                        [
                            SNew(STextBlock).Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            .ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).Text(LOCTEXT("LidarColorLabel", "LiDAR 색상 방식"))
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SComboBox<TSharedPtr<ELidarColorMode>>)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            .OptionsSource(&NativeLidarColorOptions).InitiallySelectedItem(InitialColor)
                            .OnGenerateWidget_Lambda([](TSharedPtr<ELidarColorMode> Item)
                            {
                                return SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(FText::FromString(Item.IsValid() ? LidarColorDisplayText(*Item) : TEXT("없음")));
                            })
                            .OnSelectionChanged_Lambda([this](TSharedPtr<ELidarColorMode> Item, ESelectInfo::Type)
                            {
                                if (Item.IsValid()) SetLidarColorMode(*Item);
                            })
                            [ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(LidarColorDisplayText(GetLidarColorMode())); }) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 2.0f)
                        [
                            SNew(SHorizontalBox)
                            .Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ SNew(SBox).WidthOverride(28.0f).HeightOverride(12.0f)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this]() { return GetLidarLegendSwatchColor(0); }) ] ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ SNew(SBox).WidthOverride(28.0f).HeightOverride(12.0f)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this]() { return GetLidarLegendSwatchColor(1); }) ] ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)[ SNew(SBox).WidthOverride(28.0f).HeightOverride(12.0f)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this]() { return GetLidarLegendSwatchColor(2); }) ] ]
                            + SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(28.0f).HeightOverride(12.0f)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this]() { return GetLidarLegendSwatchColor(3); }) ] ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 4.0f)
                        [ SNew(STextBlock).Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; }).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(GetLidarViewModeDescription() + TEXT("\n") + GetLidarViewLegendText()); }) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 4.0f)
                        [
                            SNew(SHorizontalBox).Visibility_Lambda([this]() { const auto Mode = GetLidarProjectionMode(); return bShowingLidar && Mode != ELidarMonitorProjectionMode::RangeImage ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text(LOCTEXT("ProjectionNavigationHelp", "보기에서 좌클릭 드래그=이동 · 우클릭 드래그=회전 · 휠=확대/축소")) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
                            [ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("ResetProjectionView", "보기 초기화")).OnClicked_Lambda([this]() { if (auto* V = GetLidarVisualizationComponent()) V->ResetProjectionView(GetLidarProjectionMode()); return FReply::Handled(); }) ]
                        ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(SHorizontalBox)
							.Visibility_Lambda([this]() { return bShowingLidar && GetLidarProjectionMode() == ELidarMonitorProjectionMode::ForwardSlice ? EVisibility::Visible : EVisibility::Collapsed; })
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).Text_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); return FText::FromString(FString::Printf(TEXT("슬라이스 두께 %.2fm"), V ? V->GetVisualizationSettings().ForwardSliceThicknessCm * 0.01f : 1.0f)); }) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)
							[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("SliceThinner", "두께 -")).OnClicked_Lambda([this]() { if (auto* V = GetLidarVisualizationComponent()) { auto S = V->GetVisualizationSettings(); S.ForwardSliceThicknessCm = FMath::Max(10.0f, S.ForwardSliceThicknessCm - 25.0f); V->SetVisualizationSettings(S); } return FReply::Handled(); }) ]
							+ SHorizontalBox::Slot().AutoWidth()
							[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("SliceThicker", "두께 +")).OnClicked_Lambda([this]() { if (auto* V = GetLidarVisualizationComponent()) { auto S = V->GetVisualizationSettings(); S.ForwardSliceThicknessCm = FMath::Min(10000.0f, S.ForwardSliceThicknessCm + 25.0f); V->SetVisualizationSettings(S); } return FReply::Handled(); }) ]
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
                                [ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("WorldPoints", "월드 3D 포인트 표시")) ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)
                            [ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("PointSmaller", "포인트 -")).OnClicked_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); SetLidarPointSize(V ? V->GetVisualizationSettings().PointSize - 0.5f : 1.5f); return FReply::Handled(); }) ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("PointLarger", "포인트 +")).OnClicked_Lambda([this]() { const auto* V = GetLidarVisualizationComponent(); SetLidarPointSize(V ? V->GetVisualizationSettings().PointSize + 0.5f : 2.5f); return FReply::Handled(); }) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SWrapBox).UseAllottedSize(true).Visibility_Lambda([this]() { return bShowingLidar ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SWrapBox::Slot()[ SNew(SCheckBox).ToolTipText(LOCTEXT("AdaptiveTip", "현재 프레임의 검출 거리 범위를 기준으로 색상 대비를 자동 조정합니다.")).IsChecked_Lambda([this]() { return bUseAdaptiveLidarDepthRange ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SetLidarOverlayOptions(V == ECheckBoxState::Checked, bOverlayLidarMonitorGrid, bOverlayLidarDepthEdges); })[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Adaptive", "적응형 거리")) ] ]
                            + SWrapBox::Slot()[ SNew(SCheckBox).ToolTipText(LOCTEXT("EdgeTip", "인접 측정점의 거리 차이가 큰 경계를 흰색 선으로 강조합니다.")).IsChecked_Lambda([this]() { return bOverlayLidarDepthEdges ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SetLidarOverlayOptions(bUseAdaptiveLidarDepthRange, bOverlayLidarMonitorGrid, V == ECheckBoxState::Checked); })[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Edges", "깊이 경계")) ] ]
                            + SWrapBox::Slot()[ SNew(SCheckBox).ToolTipText(LOCTEXT("GridTip", "LiDAR 행과 열의 간격을 확인할 수 있도록 기준 격자를 겹쳐 표시합니다.")).IsChecked_Lambda([this]() { return bOverlayLidarMonitorGrid ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState V) { SetLidarOverlayOptions(bUseAdaptiveLidarDepthRange, V == ECheckBoxState::Checked, bOverlayLidarDepthEdges); })[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Grid", "격자")) ] ]
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SWrapBox).UseAllottedSize(true).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
                + SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ToggleView", "카메라/LiDAR 전환")).OnClicked_Lambda([this]() { HandleToggleButtonClicked(); RefreshNativeFallbackText(); return FReply::Handled(); }) ]
                + SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("NextCamera", "다음 카메라")).OnClicked_Lambda([this]() { HandleNextCameraButtonClicked(); RefreshNativeFallbackText(); return FReply::Handled(); }) ]
                + SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("NextLidar", "다음 LiDAR")).OnClicked_Lambda([this]() { HandleNextLidarButtonClicked(); RefreshNativeFallbackText(); return FReply::Handled(); }) ]
                + SWrapBox::Slot()
                [
                    SNew(SButton)
                    .ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle())
                    .ForegroundColor(FVirtualSensorUiStyle::PrimaryText)
                    .OnClicked_Lambda([this]() { HandlePointCloudOnlyButtonClicked(); RefreshNativeFallbackText(); return FReply::Handled(); })
                    [
                        SNew(STextBlock)
                        .ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText)
                        .Text_Lambda([this]() { return BuildPointCloudOnlyButtonText(); })
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
            [
                SNew(STextBlock)
                .Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
                .ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText)
                .Text(LOCTEXT("MonitorResizeHint", "↘ 드래그: 모니터 크기 조절"))
                .ToolTipText(LOCTEXT("MonitorResizeHintTip", "패널 오른쪽 아래를 드래그해 가로와 세로 크기를 자유롭게 조절합니다."))
            ]
        ];
}

FColor UVirtualSensorMonitorPanelWidget::ResolveLidarPointDisplayColor(const UVirtualLidarScanComponent* InLidarComp, EVirtualLidarViewMode ViewMode, const FVirtualLidarPoint& Point, float NormalizedDistance)
{
    const uint8 Intensity = Point.bHit ? static_cast<uint8>((1.0f - FMath::Clamp(NormalizedDistance, 0.0f, 1.0f)) * 255.0f) : 0;
    if (ViewMode == EVirtualLidarViewMode::HitMask)
    {
        return Point.bHit ? FColor::White : FColor::Black;
    }
    if (ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        if (!Point.bHit)
        {
            return FColor(3, 8, 10, 255);
        }
        const FLinearColor SemanticColor = InLidarComp
            ? InLidarComp->GetSemanticColorForLabel(Point.SemanticLabel)
            : FLinearColor(0.55f, 0.55f, 0.55f, 1.0f);
        return SemanticColor.ToFColor(true);
    }
    if (ViewMode == EVirtualLidarViewMode::IntensityGray)
    {
        return FColor(Intensity, Intensity, Intensity, 255);
    }
    if (!Point.bHit)
    {
        return FColor(4, 4, 12, 255);
    }
    if (NormalizedDistance < 0.20f) return FColor(255, 0, 255, 255);
    if (NormalizedDistance < 0.40f) return FColor(255, 48, 0, 255);
    if (NormalizedDistance < 0.60f) return FColor(255, 235, 0, 255);
    if (NormalizedDistance < 0.80f) return FColor(0, 255, 255, 255);
    return FColor(0, 80, 255, 255);
}

void UVirtualSensorMonitorPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RestoreMonitorUiPreferences();

    if (ToggleButton)
    {
        ToggleButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleToggleButtonClicked);
        ToggleButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleToggleButtonClicked);
    }
    if (NextCameraButton)
    {
        NextCameraButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleNextCameraButtonClicked);
        NextCameraButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleNextCameraButtonClicked);
    }
    if (NextLidarButton)
    {
        NextLidarButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleNextLidarButtonClicked);
        NextLidarButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleNextLidarButtonClicked);
    }
    if (PointCloudOnlyButton)
    {
        PointCloudOnlyButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePointCloudOnlyButtonClicked);
        PointCloudOnlyButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePointCloudOnlyButtonClicked);
    }
    if (LidarViewModeButton)
    {
        LidarViewModeButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleLidarViewModeButtonClicked);
        LidarViewModeButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleLidarViewModeButtonClicked);
    }
    if (LogPointCloudButton)
    {
        LogPointCloudButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleLogPointCloudButtonClicked);
        LogPointCloudButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleLogPointCloudButtonClicked);
    }
    if (ExportPointCloudButton)
    {
        ExportPointCloudButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleExportPointCloudButtonClicked);
        ExportPointCloudButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleExportPointCloudButtonClicked);
    }
    if (LocalSensorCaptureButton)
    {
        LocalSensorCaptureButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleLocalSensorCaptureButtonClicked);
        LocalSensorCaptureButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleLocalSensorCaptureButtonClicked);
    }
    if (CaptureOnceButton)
    {
        CaptureOnceButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleCaptureOnceButtonClicked);
        CaptureOnceButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleCaptureOnceButtonClicked);
    }
    if (ExportServerPayloadButton)
    {
        ExportServerPayloadButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleExportServerPayloadButtonClicked);
        ExportServerPayloadButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleExportServerPayloadButtonClicked);
    }
    if (PreviewMoreButton)
    {
        PreviewMoreButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePreviewMoreButtonClicked);
        PreviewMoreButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePreviewMoreButtonClicked);
    }
    if (PreviewLessButton)
    {
        PreviewLessButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePreviewLessButtonClicked);
        PreviewLessButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePreviewLessButtonClicked);
    }
    if (PreviewHitOnlyButton)
    {
        PreviewHitOnlyButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePreviewHitOnlyButtonClicked);
        PreviewHitOnlyButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePreviewHitOnlyButtonClicked);
    }
    if (StartRealSensorSourcesButton)
    {
        StartRealSensorSourcesButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleStartRealSensorSourcesButtonClicked);
        StartRealSensorSourcesButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleStartRealSensorSourcesButtonClicked);
    }
    if (StopRealSensorSourcesButton)
    {
        StopRealSensorSourcesButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleStopRealSensorSourcesButtonClicked);
        StopRealSensorSourcesButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandleStopRealSensorSourcesButtonClicked);
    }
    if (PushRealSensorSourceButton)
    {
        PushRealSensorSourceButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePushRealSensorSourceButtonClicked);
        PushRealSensorSourceButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorPanelWidget::HandlePushRealSensorSourceButtonClicked);
    }

    RefreshTitle();
    RefreshImageBrush();
    RefreshStatusText();
    RefreshLocalCaptureButtonText();
    RefreshLidarViewModeButtonText();
    if (PreviewHitOnlyButtonText)
    {
        const UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
        PreviewHitOnlyButtonText->SetText(FText::FromString(TargetLidar && TargetLidar->bPointCloudPreviewHitOnly ? TEXT("미리보기: 검출점만") : TEXT("미리보기: 모든 점")));
    }
}

void UVirtualSensorMonitorPanelWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LocalSensorCaptureTimerHandle);
    }
    bLocalSensorCaptureActive = false;
    PendingCameraReadbacks.Reset();
    LocalCaptureCameraAsyncWriteCount = 0;
    bLocalCaptureCameraWritePending = false;
    Super::NativeDestruct();
}

void UVirtualSensorMonitorPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    ProcessPendingCameraReadbacks();
    RefreshImageBrush();
    StatusRefreshAccumulator += InDeltaTime;
    if (StatusRefreshAccumulator >= 0.2)
    {
        StatusRefreshAccumulator = 0.0;
        RefreshStatusText();
    }
}

bool UVirtualSensorMonitorPanelWidget::ResolveInteractiveProjection(const FVector2D& ScreenPosition, ELidarMonitorProjectionMode& OutProjection, FVector2D& OutViewportSize) const
{
    if (!bShowingLidar) return false;
    const ELidarMonitorProjectionMode Mode = GetLidarProjectionMode();
    auto IsUnder = [&ScreenPosition, &OutViewportSize](const FGeometry& Geometry)
    {
        if (!Geometry.IsUnderLocation(ScreenPosition)) return false;
        OutViewportSize = Geometry.GetLocalSize();
        return true;
    };

    if (Mode == ELidarMonitorProjectionMode::Split && NativeSecondaryViewImage.IsValid() && IsUnder(NativeSecondaryViewImage->GetCachedGeometry()))
    {
        OutProjection = ELidarMonitorProjectionMode::TopDown;
        return true;
    }
    if (Mode != ELidarMonitorProjectionMode::RangeImage && Mode != ELidarMonitorProjectionMode::Split)
    {
        if (ViewImage && IsUnder(ViewImage->GetCachedGeometry())) { OutProjection = Mode; return true; }
        if (NativeViewImage.IsValid() && IsUnder(NativeViewImage->GetCachedGeometry())) { OutProjection = Mode; return true; }
    }
    return false;
}

FReply UVirtualSensorMonitorPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    ELidarMonitorProjectionMode Projection;
    FVector2D ViewportSize;
    const FKey Button = InMouseEvent.GetEffectingButton();
    if ((Button == EKeys::LeftMouseButton || Button == EKeys::RightMouseButton) &&
        ResolveInteractiveProjection(InMouseEvent.GetScreenSpacePosition(), Projection, ViewportSize))
    {
        ActiveProjectionInteraction = Projection;
        ActiveProjectionViewportSize = ViewportSize;
        bPanningLidarProjection = Button == EKeys::LeftMouseButton;
        bRotatingLidarProjection = Button == EKeys::RightMouseButton;
        const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
        return CachedWidget.IsValid() ? FReply::Handled().CaptureMouse(CachedWidget.ToSharedRef()) : FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UVirtualSensorMonitorPanelWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bPanningLidarProjection || bRotatingLidarProjection)
    {
        bPanningLidarProjection = false;
        bRotatingLidarProjection = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UVirtualSensorMonitorPanelWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        if (bPanningLidarProjection && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
        {
            Visualization->PanProjectionView(ActiveProjectionInteraction, InMouseEvent.GetCursorDelta(), ActiveProjectionViewportSize);
            return FReply::Handled();
        }
        if (bRotatingLidarProjection && InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
        {
            Visualization->RotateProjectionView(ActiveProjectionInteraction, InMouseEvent.GetCursorDelta().X * 0.25f);
            return FReply::Handled();
        }
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UVirtualSensorMonitorPanelWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    ELidarMonitorProjectionMode Projection;
    FVector2D ViewportSize;
    if (ResolveInteractiveProjection(InMouseEvent.GetScreenSpacePosition(), Projection, ViewportSize))
    {
        if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
        {
            Visualization->ZoomProjectionView(Projection, FMath::Pow(1.15f, InMouseEvent.GetWheelDelta()));
            return FReply::Handled();
        }
    }
    return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void UVirtualSensorMonitorPanelWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    bPanningLidarProjection = false;
    bRotatingLidarProjection = false;
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UVirtualSensorMonitorPanelWidget::BindVirtualCamera(UVirtualCameraCaptureComponent* InCameraComp)
{
    CameraComp = InCameraComp;
    if (GetWorld())
    {
        if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>()) Subsystem->SetPreferredCamera(InCameraComp);
    }
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorPanelWidget::BindVirtualLidar(UVirtualLidarScanComponent* InLidarComp)
{
    if (LidarComp && LidarComp != InLidarComp)
    {
        if (AVirtualLidarSensorActor* PreviousActor = Cast<AVirtualLidarSensorActor>(LidarComp->GetOwner()))
        {
            if (PreviousActor->VisualizationComponent) PreviousActor->VisualizationComponent->SetWorldPointCloudEnabled(false);
        }
    }
    LidarComp = InLidarComp;
    if (GetWorld())
    {
        if (UVirtualSensorSchedulerSubsystem* Subsystem = GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>()) Subsystem->SetPreferredLidar(InLidarComp);
    }
    RestoreMonitorUiPreferences();
    InvalidateEnhancedLidarView();
    if (bShowingLidar && LidarComp && !LidarComp->GetLidarViewTexture())
    {
        RefreshLidarPreviewWithoutTransport();
    }
    RefreshImageBrush();
    RefreshStatusText();
    RefreshLidarViewModeButtonText();
}

void UVirtualSensorMonitorPanelWidget::BindSensorManager(AVirtualSensorCoordinator* InSensorManager)
{
    SensorManager = InSensorManager;
    if (SensorManager)
    {
        SensorManager->BindMonitorWidget(this);
    }
    RefreshStatusText();
}

void UVirtualSensorMonitorPanelWidget::BindRealSensorSource(URealSensorSourceComponent* InRealSensorSourceComponent)
{
    RealSensorSourceComponent = InRealSensorSourceComponent;
    RefreshStatusText();
}

void UVirtualSensorMonitorPanelWidget::ShowCameraView()
{
    bShowingLidar = false;
    RefreshTitle();
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorPanelWidget::ShowLidarView()
{
    bShowingLidar = true;
    if (LidarComp && !LidarComp->GetLidarViewTexture())
    {
        RefreshLidarPreviewWithoutTransport();
    }
    RefreshTitle();
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorPanelWidget::ToggleView()
{
    bShowingLidar = !bShowingLidar;
    if (bShowingLidar && LidarComp && !LidarComp->GetLidarViewTexture())
    {
        RefreshLidarPreviewWithoutTransport();
    }
    RefreshTitle();
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorPanelWidget::CycleLidarViewMode()
{
    if (!LidarComp)
    {
        return;
    }

    EVirtualLidarViewMode NextMode = EVirtualLidarViewMode::DepthGradient;
    if (LidarComp->ViewMode == EVirtualLidarViewMode::DepthGradient)
    {
        NextMode = EVirtualLidarViewMode::HitMask;
    }
    else if (LidarComp->ViewMode == EVirtualLidarViewMode::HitMask)
    {
        NextMode = EVirtualLidarViewMode::ActorClassColor;
    }
    else if (LidarComp->ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        NextMode = EVirtualLidarViewMode::IntensityGray;
    }
    SetLidarViewMode(NextMode);
}

void UVirtualSensorMonitorPanelWidget::SetLidarViewMode(EVirtualLidarViewMode InViewMode)
{
    if (!LidarComp)
    {
        return;
    }
    LidarComp->ViewMode = InViewMode;
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        Visualization->SetColorMode(UVirtualLidarVisualizationComponent::MapLegacyViewMode(InViewMode));
    }
    InvalidateEnhancedLidarView();
    RefreshLidarPreviewWithoutTransport();
    RefreshLidarViewModeButtonText();
    RefreshStatusText();
    RefreshImageBrush();
    SaveMonitorUiPreferences();
}

void UVirtualSensorMonitorPanelWidget::SetLidarProjectionMode(ELidarMonitorProjectionMode InProjectionMode)
{
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        Visualization->SetProjectionMode(InProjectionMode);
        RefreshImageBrush();
        SaveMonitorUiPreferences();
    }
}

void UVirtualSensorMonitorPanelWidget::SetLidarColorMode(ELidarColorMode InColorMode)
{
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        Visualization->SetColorMode(InColorMode);
        RefreshImageBrush();
        RefreshStatusText();
        SaveMonitorUiPreferences();
    }
}

void UVirtualSensorMonitorPanelWidget::SetLidarWorldPointCloudEnabled(bool bEnabled)
{
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        Visualization->SetWorldPointCloudEnabled(bEnabled);
        SaveMonitorUiPreferences();
    }
}

void UVirtualSensorMonitorPanelWidget::SetLidarPointSize(float InPointSize)
{
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        FVirtualLidarVisualizationSettings Settings = Visualization->GetVisualizationSettings();
        Settings.PointSize = FMath::Clamp(InPointSize, 0.25f, 12.0f);
        Visualization->SetVisualizationSettings(Settings);
        SaveMonitorUiPreferences();
    }
}

ELidarMonitorProjectionMode UVirtualSensorMonitorPanelWidget::GetLidarProjectionMode() const
{
    const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent();
    return Visualization ? Visualization->GetVisualizationSettings().ProjectionMode : ELidarMonitorProjectionMode::RangeImage;
}

ELidarColorMode UVirtualSensorMonitorPanelWidget::GetLidarColorMode() const
{
    const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent();
    return Visualization ? Visualization->GetVisualizationSettings().ColorMode : ELidarColorMode::DistanceTurbo;
}

void UVirtualSensorMonitorPanelWidget::SetLidarPreviewBudget(int32 InStride, int32 InMaxPoints)
{
    UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
    if (!TargetLidar)
    {
        return;
    }

    const int32 SafeStrideMin = FMath::Max(1, PreviewStrideMin);
    const int32 SafeStrideMax = FMath::Max(SafeStrideMin, PreviewStrideMax);
    const int32 SafeMinPoints = FMath::Max(0, PreviewBudgetMinPoints);
    const int32 SafeMaxPoints = FMath::Max(SafeMinPoints, PreviewBudgetMaxPoints);
    const int32 ClampedStride = FMath::Clamp(InStride, SafeStrideMin, SafeStrideMax);
    const int32 ClampedMaxPoints = FMath::Clamp(InMaxPoints, SafeMinPoints, SafeMaxPoints);

    if (SensorManager && TargetLidar == SensorManager->GetSelectedLidar())
    {
        SensorManager->SetSelectedLidarPreviewPolicy(ClampedStride, ClampedMaxPoints, TargetLidar->bPointCloudPreviewHitOnly);
    }
    else
    {
        TargetLidar->SetPreviewPolicy(ClampedStride, ClampedMaxPoints, TargetLidar->bPointCloudPreviewHitOnly);
    }
    InvalidateEnhancedLidarView();
    RefreshLidarPreviewWithoutTransport();
    RefreshStatusText();
    RefreshImageBrush();

    UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] LiDAR preview budget changed. Sensor=%s Stride=%d MaxPoints=%d"),
        *TargetLidar->SensorId,
        TargetLidar->PreviewPointStride,
        TargetLidar->MaxPreviewPoints);
}

void UVirtualSensorMonitorPanelWidget::IncreaseLidarPreviewBudget()
{
    const UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
    if (!TargetLidar)
    {
        return;
    }

    const int32 NextStride = FMath::Max(FMath::Max(1, PreviewStrideMin), TargetLidar->PreviewPointStride - 1);
    const int32 NextMaxPoints = TargetLidar->MaxPreviewPoints <= 0
        ? PreviewBudgetMinPoints
        : TargetLidar->MaxPreviewPoints + FMath::Max(100, PreviewBudgetStepPoints);
    SetLidarPreviewBudget(NextStride, NextMaxPoints);
}

void UVirtualSensorMonitorPanelWidget::DecreaseLidarPreviewBudget()
{
    const UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
    if (!TargetLidar)
    {
        return;
    }

    const int32 NextStride = FMath::Min(FMath::Max(PreviewStrideMin, PreviewStrideMax), TargetLidar->PreviewPointStride + 1);
    const int32 NextMaxPoints = TargetLidar->MaxPreviewPoints <= 0
        ? PreviewBudgetMaxPoints
        : TargetLidar->MaxPreviewPoints - FMath::Max(100, PreviewBudgetStepPoints);
    SetLidarPreviewBudget(NextStride, NextMaxPoints);
}

void UVirtualSensorMonitorPanelWidget::ToggleLidarPreviewHitOnly()
{
    UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
    if (!TargetLidar)
    {
        return;
    }

    const bool bNextHitOnly = !TargetLidar->bPointCloudPreviewHitOnly;
    if (SensorManager && TargetLidar == SensorManager->GetSelectedLidar())
    {
        SensorManager->SetSelectedLidarPreviewPolicy(TargetLidar->PreviewPointStride, TargetLidar->MaxPreviewPoints, bNextHitOnly);
    }
    else
    {
        TargetLidar->SetPreviewPolicy(TargetLidar->PreviewPointStride, TargetLidar->MaxPreviewPoints, bNextHitOnly);
    }

    InvalidateEnhancedLidarView();
    RefreshLidarPreviewWithoutTransport();
    RefreshTitle();
    RefreshStatusText();
    RefreshImageBrush();

    UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] LiDAR preview hit-only changed. Sensor=%s HitOnly=%s"),
        *TargetLidar->SensorId,
        TargetLidar->bPointCloudPreviewHitOnly ? TEXT("true") : TEXT("false"));
}

void UVirtualSensorMonitorPanelWidget::CaptureSelectedSensorsOnce()
{
    if (SensorManager)
    {
        SensorManager->CaptureSelectedOnce();
    }
    else
    {
        if (CameraComp)
        {
            CameraComp->CaptureAndSendImage();
        }
        if (LidarComp)
        {
            LidarComp->ScanAndSend();
        }
    }

    InvalidateEnhancedLidarView();
    RefreshImageBrush();
    RefreshStatusText();
}

int32 UVirtualSensorMonitorPanelWidget::StartRealSensorSources()
{
    const int32 StartedCount = SensorManager
        ? SensorManager->StartAllRealSensorSources()
        : (RealSensorSourceComponent && RealSensorSourceComponent->StartSource() ? 1 : 0);
    LastRealSensorControlMessage = FString::Printf(TEXT("Real Sensor Control: started %d source(s)"), StartedCount);
    RefreshStatusText();
    return StartedCount;
}

void UVirtualSensorMonitorPanelWidget::StopRealSensorSources()
{
    if (SensorManager)
    {
        SensorManager->StopAllRealSensorSources();
    }
    else if (RealSensorSourceComponent)
    {
        RealSensorSourceComponent->StopSource();
    }
    LastRealSensorControlMessage = TEXT("Real Sensor Control: stop requested");
    RefreshStatusText();
}

bool UVirtualSensorMonitorPanelWidget::PushSelectedRealSensorSourceOnce(bool bSendTransport)
{
    const bool bPushed = SensorManager
        ? SensorManager->PushSelectedRealSensorSourceOnce(bSendTransport)
        : (RealSensorSourceComponent && RealSensorSourceComponent->PushFrameOnce(bSendTransport));
    LastRealSensorControlMessage = FString::Printf(TEXT("Real Sensor Control: push %s transport=%s"),
        bPushed ? TEXT("succeeded") : TEXT("failed"),
        bSendTransport ? TEXT("true") : TEXT("false"));
    InvalidateEnhancedLidarView();
    RefreshImageBrush();
    RefreshStatusText();
    return bPushed;
}

bool UVirtualSensorMonitorPanelWidget::ExportSelectedLidarServerPayload(const FString& FileNamePrefix)
{
    UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
    if (!TargetLidar)
    {
        LastManualExportPath.Reset();
        LastManualExportMessage = TEXT("Server Payload Export: failed, LiDAR sensor is not bound");
        RefreshStatusText();
        return false;
    }

    if (TargetLidar->GetLastJsonPayload().IsEmpty())
    {
        RefreshLidarPreviewWithoutTransport();
    }

    const FString& Payload = TargetLidar->GetLastJsonPayload();
    if (Payload.IsEmpty())
    {
        LastManualExportPath.Reset();
        LastManualExportMessage = FString::Printf(TEXT("Server Payload Export: failed, no payload for %s"), *TargetLidar->SensorId);
        RefreshStatusText();
        return false;
    }

    const FString Directory = BuildServerPayloadExportDirectory(TargetLidar);
    const bool bSaved = SaveServerPayloadJsonFile(Directory, FileNamePrefix, TargetLidar->SensorId, Payload, LastManualExportPath);
    LastManualExportMessage = bSaved
        ? FString::Printf(TEXT("LiDAR Server Payload Export: saved %s bytes=%d"), *LastManualExportPath, Payload.Len())
        : FString::Printf(TEXT("LiDAR Server Payload Export: failed %s bytes=%d"), *LastManualExportPath, Payload.Len());

    if (bSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] %s"), *LastManualExportMessage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SensorMonitor] %s"), *LastManualExportMessage);
    }

    RefreshStatusText();
    return bSaved;
}

bool UVirtualSensorMonitorPanelWidget::ExportSelectedSensorServerPayload(const FString& FileNamePrefix)
{
    if (bShowingLidar)
    {
        return ExportSelectedLidarServerPayload(FileNamePrefix);
    }

    if (!CameraComp)
    {
        LastManualExportPath.Reset();
        LastManualExportMessage = TEXT("Camera Server Payload Export: failed, camera sensor is not bound");
        RefreshStatusText();
        return false;
    }

    if (CameraComp->GetLastJsonPayload().IsEmpty())
    {
        CameraComp->CaptureAndSendImage();
    }

    const FString& Payload = CameraComp->GetLastJsonPayload();
    if (Payload.IsEmpty())
    {
        LastManualExportPath.Reset();
        LastManualExportMessage = FString::Printf(TEXT("Camera Server Payload Export: failed, no payload for %s"), *CameraComp->SensorId);
        RefreshStatusText();
        return false;
    }

    const FString Directory = BuildCameraPayloadExportDirectory(CameraComp);
    const bool bSaved = SaveServerPayloadJsonFile(Directory, FileNamePrefix, CameraComp->SensorId, Payload, LastManualExportPath);
    LastManualExportMessage = bSaved
        ? FString::Printf(TEXT("Camera Server Payload Export: saved %s bytes=%d"), *LastManualExportPath, Payload.Len())
        : FString::Printf(TEXT("Camera Server Payload Export: failed %s bytes=%d"), *LastManualExportPath, Payload.Len());

    if (bSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] %s"), *LastManualExportMessage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SensorMonitor] %s"), *LastManualExportMessage);
    }

    RefreshStatusText();
    return bSaved;
}

void UVirtualSensorMonitorPanelWidget::ToggleLocalSensorCapture()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (bLocalSensorCaptureActive)
    {
        World->GetTimerManager().ClearTimer(LocalSensorCaptureTimerHandle);
        bLocalSensorCaptureActive = false;
        UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Local timed capture stopped. Session=%s frames=%d pendingReadbacks=%d"), *LocalCaptureSessionDirectory, LocalCaptureFrameIndex, PendingCameraReadbacks.Num());
    }
    else
    {
        LocalCaptureSessionDirectory = EnsureLocalCaptureSessionDirectory();
        LocalCaptureFrameIndex = 0;
        bLocalSensorCaptureActive = true;
        CaptureLocalSensorFrame();
        World->GetTimerManager().SetTimer(LocalSensorCaptureTimerHandle, this, &UVirtualSensorMonitorPanelWidget::CaptureLocalSensorFrame, FMath::Max(0.05f, LocalCaptureIntervalSeconds), true);
        UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Local timed capture started. Interval=%.3fs Session=%s GpuReadback=%s MaxReadbacks=%d LidarFormats=[csv:%s las:%s laz:%s]"), LocalCaptureIntervalSeconds, *LocalCaptureSessionDirectory, bUseGpuAsyncCameraReadback ? TEXT("true") : TEXT("false"), MaxPendingCameraReadbacks, bLocalCaptureSaveLidarCsv ? TEXT("on") : TEXT("off"), bLocalCaptureSaveLidarLas ? TEXT("on") : TEXT("off"), bLocalCaptureSaveLidarLaz ? TEXT("on") : TEXT("off"));
    }
    RefreshLocalCaptureButtonText();
    RefreshStatusText();
}

void UVirtualSensorMonitorPanelWidget::HandleToggleButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->ToggleSensorView();
        return;
    }
    ToggleView();
}

void UVirtualSensorMonitorPanelWidget::HandleNextCameraButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->SelectNextCamera();
        SensorManager->SetViewMode(EVirtualSensorViewMode::Camera);
    }
}

void UVirtualSensorMonitorPanelWidget::HandleNextLidarButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->SelectNextLidar();
        SensorManager->SetViewMode(EVirtualSensorViewMode::Lidar);
    }
}

void UVirtualSensorMonitorPanelWidget::HandlePointCloudOnlyButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->TogglePointCloudOnlyView();
    }
}

void UVirtualSensorMonitorPanelWidget::HandleLidarViewModeButtonClicked()
{
    CycleLidarViewMode();
}

void UVirtualSensorMonitorPanelWidget::HandleLocalSensorCaptureButtonClicked()
{
    ToggleLocalSensorCapture();
}

void UVirtualSensorMonitorPanelWidget::HandleCaptureOnceButtonClicked()
{
    CaptureSelectedSensorsOnce();
}

void UVirtualSensorMonitorPanelWidget::HandleExportServerPayloadButtonClicked()
{
    ExportSelectedSensorServerPayload(TEXT("button_server_payload"));
}

void UVirtualSensorMonitorPanelWidget::HandlePreviewMoreButtonClicked()
{
    IncreaseLidarPreviewBudget();
}

void UVirtualSensorMonitorPanelWidget::HandlePreviewLessButtonClicked()
{
    DecreaseLidarPreviewBudget();
}

void UVirtualSensorMonitorPanelWidget::HandlePreviewHitOnlyButtonClicked()
{
    ToggleLidarPreviewHitOnly();
}

void UVirtualSensorMonitorPanelWidget::HandleStartRealSensorSourcesButtonClicked()
{
    StartRealSensorSources();
}

void UVirtualSensorMonitorPanelWidget::HandleStopRealSensorSourcesButtonClicked()
{
    StopRealSensorSources();
}

void UVirtualSensorMonitorPanelWidget::HandlePushRealSensorSourceButtonClicked()
{
    PushSelectedRealSensorSourceOnce(true);
}

void UVirtualSensorMonitorPanelWidget::HandleLogPointCloudButtonClicked()
{
    if (!LidarComp)
    {
        return;
    }

    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    int32 Logged = 0;
    int32 CandidateCount = 0;
    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] RowCol PointCloud log start. format=row,col,returnIndex,x,y,z"), *LidarComp->SensorId);
    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (LidarComp->bExportHitOnlyPointCloud && !Point.bHit)
        {
            continue;
        }
        ++CandidateCount;
        if (Logged++ >= 200)
        {
            continue;
        }
        const FIntPoint GridCoord = ResolveLidarGridCoord(Point, PointIndex, LidarComp->HorizontalSamples);
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] row=%d col=%d returnIndex=%d x=%.3f y=%.3f z=%.3f"), *LidarComp->SensorId, GridCoord.X, GridCoord.Y, Point.ReturnIndex, Point.WorldLocation.X, Point.WorldLocation.Y, Point.WorldLocation.Z);
    }
    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] RowCol PointCloud log complete. candidates=%d"), *LidarComp->SensorId, CandidateCount);
}

void UVirtualSensorMonitorPanelWidget::HandleExportPointCloudButtonClicked()
{
    if (!LidarComp)
    {
        return;
    }

    ExportRowColPointCloudCsv(LidarComp, TEXT("button_export"));
    LidarComp->ExportLastPointCloudLas(TEXT("button_export"));
    LidarComp->ExportLastPointCloudLaz(TEXT("button_export"));
}

void UVirtualSensorMonitorPanelWidget::RefreshImageBrush()
{
    UObject* Resource = nullptr;
    if (bShowingLidar)
    {
        if (LidarComp && !LidarComp->GetLidarViewTexture())
        {
            RefreshLidarPreviewWithoutTransport();
        }
        Resource = GetLidarBrushResource();
    }
    else
    {
        Resource = CameraComp ? CameraComp->GetCameraRenderTarget() : nullptr;
    }

    if (!Resource)
    {
        return;
    }

    if (ViewImage)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(Resource);
        Brush.ImageSize = FVector2D(640.0f, 360.0f);
        ViewImage->SetBrush(Brush);
    }

    if (NativeViewImage)
    {
        NativeViewBrush.SetResourceObject(Resource);
        NativeViewBrush.ImageSize = FVector2D(640.0f, 360.0f);
        NativeViewImage->Invalidate(EInvalidateWidgetReason::Paint);
    }

    if (NativeSecondaryViewImage)
    {
        NativeSecondaryViewBrush.SetResourceObject(bShowingLidar ? GetSecondaryLidarBrushResource() : nullptr);
        NativeSecondaryViewBrush.ImageSize = FVector2D(640.0f, 360.0f);
        NativeSecondaryViewImage->Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void UVirtualSensorMonitorPanelWidget::RefreshTitle()
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

void UVirtualSensorMonitorPanelWidget::RefreshStatusText()
{
    const FString Text = BuildStatusText();
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Text));
    }
    if (NativeStatusTextBlock.IsValid())
    {
        NativeStatusTextBlock->SetText(FText::FromString(BuildCompactStatusText()));
    }
    if (NativeDetailedStatusTextBlock.IsValid())
    {
        NativeDetailedStatusTextBlock->SetText(FText::FromString(Text));
    }
    if (NativeWarningTextBlock.IsValid())
    {
        NativeWarningTextBlock->SetText(FText::FromString(GetTransportWarningText()));
    }
}

void UVirtualSensorMonitorPanelWidget::RefreshNativeFallbackText()
{
    RefreshTitle();
    RefreshStatusText();
}

FString UVirtualSensorMonitorPanelWidget::BuildTitleText() const
{
    const bool bPointCloudOnly = SensorManager && SensorManager->IsPointCloudOnlyModeEnabled();
    return bPointCloudOnly ? TEXT("LiDAR 포인트 클라우드 전용 보기")
        : (bShowingLidar ? TEXT("가상 LiDAR 모니터") : TEXT("가상 카메라 모니터"));
}

FText UVirtualSensorMonitorPanelWidget::BuildPointCloudOnlyButtonText() const
{
    return FText::FromString(SensorManager && SensorManager->IsPointCloudOnlyModeEnabled()
        ? TEXT("포인트 클라우드 전용 끄기")
        : TEXT("포인트 클라우드 전용 켜기"));
}

FString UVirtualSensorMonitorPanelWidget::GetPointCloudRendererStatusText() const
{
    const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent();
    if (!Visualization) return TEXT("3D 포인트 렌더러: 사용할 수 없음");
    const FVirtualLidarRendererTelemetry& Render = Visualization->GetRendererTelemetry();
    const TCHAR* StateText = TEXT("꺼짐");
    switch (Render.State)
    {
    case ELidarPointCloudRendererState::Starting: StateText = TEXT("시작 중"); break;
    case ELidarPointCloudRendererState::NiagaraActive: StateText = TEXT("Niagara 활성"); break;
    case ELidarPointCloudRendererState::CpuFallback: StateText = TEXT("CPU fallback"); break;
    case ELidarPointCloudRendererState::Error: StateText = TEXT("오류"); break;
    case ELidarPointCloudRendererState::Disabled:
    default: break;
    }
    return FString::Printf(
        TEXT("3D 포인트 렌더러: %s | 측정=%d 검출=%d 업로드=%d 표시=%d | %s"),
        StateText,
        Render.MeasuredPointCount,
        Render.HitPointCount,
        Render.UploadedPointCount,
        Render.VisiblePointCount,
        Render.Message.IsEmpty() ? TEXT("상태 메시지 없음") : *Render.Message);
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
            TEXT("완료 공정성(Camera/LiDAR): %.2f / %.2f · 파생 프레임 생략: %d\n")
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
            Telemetry ? Telemetry->CameraCompletionFairnessRatio : 1.0f,
            Telemetry ? Telemetry->LidarCompletionFairnessRatio : 1.0f,
            Telemetry ? Telemetry->DroppedDerivedFrameCount : Status.DroppedDerivedFrameCount,
            Status.PerformanceWarning.IsEmpty() ? TEXT("없음") : *Status.PerformanceWarning,
            *Status.LastMessage);
        Text += TEXT("\n") + GetPointCloudRendererStatusText();
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

bool UVirtualSensorMonitorPanelWidget::ShouldUseNativeFallbackWidget() const
{
    return GetClass() == UVirtualSensorMonitorPanelWidget::StaticClass() ||
        !WidgetTree || !WidgetTree->RootWidget;
}

FString UVirtualSensorMonitorPanelWidget::BuildCompactStatusText() const
{
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
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

void UVirtualSensorMonitorPanelWidget::RefreshLocalCaptureButtonText()
{
    if (LocalSensorCaptureButtonText)
    {
        LocalSensorCaptureButtonText->SetText(FText::FromString(bLocalSensorCaptureActive ? TEXT("시간 캡처 중지") : TEXT("시간 캡처 시작")));
    }
}

void UVirtualSensorMonitorPanelWidget::RefreshLidarViewModeButtonText()
{
    if (LidarViewModeButtonText)
    {
        LidarViewModeButtonText->SetText(FText::FromString(FString::Printf(TEXT("LiDAR 표시: %s"), *GetLidarViewModeDisplayText())));
    }
    if (PreviewHitOnlyButtonText)
    {
        const UVirtualLidarScanComponent* TargetLidar = GetTargetLidarForPreview();
        PreviewHitOnlyButtonText->SetText(FText::FromString(TargetLidar && TargetLidar->bPointCloudPreviewHitOnly ? TEXT("미리보기: 검출점만") : TEXT("미리보기: 모든 점")));
    }
}

FString UVirtualSensorMonitorPanelWidget::GetLidarViewModeDisplayText() const
{
    if (!LidarComp)
    {
        return TEXT("없음");
    }
    return LidarViewModeDisplayText(LidarComp->ViewMode);
}

FString UVirtualSensorMonitorPanelWidget::GetLidarViewLegendText() const
{
    if (const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
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

FString UVirtualSensorMonitorPanelWidget::GetLidarViewModeDescription() const
{
    if (const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        const FVirtualLidarVisualizationSettings& Settings = Visualization->GetVisualizationSettings();
        FString ProjectionDescription;
        switch (Settings.ProjectionMode)
        {
        case ELidarMonitorProjectionMode::TopDown:
            ProjectionDescription = TEXT("조감도: 센서 기준 XY 평면에 점을 투영하며 거리 원과 센서 전방 방향을 함께 표시합니다.");
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
        case ELidarColorMode::RelativeHeight: ColorDescription = TEXT("센서 상대 높이에 따라 낮은 점부터 높은 점까지 색상을 매핑합니다."); break;
        case ELidarColorMode::SemanticLabel: ColorDescription = TEXT("SemanticLabel 규칙에 설정된 의미 분류 색상을 사용합니다."); break;
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

FString UVirtualSensorMonitorPanelWidget::BuildSemanticLegendText() const
{
    if (!LidarComp) return TEXT("의미 분류 규칙 없음");
    FString Text = TEXT("분류: ");
    const int32 Count = FMath::Min(6, LidarComp->SemanticClassRules.Num());
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Index > 0) Text += TEXT(" · ");
        const FVirtualLidarSemanticClassRule& Rule = LidarComp->SemanticClassRules[Index];
        Text += Rule.Label.IsNone() ? TEXT("이름 없음") : Rule.Label.ToString();
    }
    if (LidarComp->SemanticClassRules.Num() > Count)
    {
        Text += FString::Printf(TEXT(" · 외 %d개"), LidarComp->SemanticClassRules.Num() - Count);
    }
    if (Count == 0) Text += FString::Printf(TEXT("미분류=%s"), *LidarComp->DefaultSemanticLabel.ToString());
    return Text;
}

FLinearColor UVirtualSensorMonitorPanelWidget::GetLidarLegendSwatchColor(int32 SwatchIndex) const
{
    if (const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        const int32 Index = FMath::Clamp(SwatchIndex, 0, 3);
        FVirtualLidarPoint Sample;
        Sample.bHit = true;
        Sample.Row = LidarComp ? FMath::RoundToInt((LidarComp->VerticalChannels - 1) * (Index / 3.0f)) : Index;
        Sample.ReturnIndex = Index;
        Sample.SemanticLabel = LidarComp && LidarComp->SemanticClassRules.IsValidIndex(Index)
            ? LidarComp->SemanticClassRules[Index].Label : NAME_None;
        const float Normalized = Index / 3.0f;
        return FLinearColor(UVirtualLidarVisualizationComponent::ResolveDisplayColor(
            LidarComp, Visualization->GetVisualizationSettings().ColorMode, Sample, Normalized, Normalized));
    }
    const EVirtualLidarViewMode ViewMode = LidarComp ? LidarComp->ViewMode : EVirtualLidarViewMode::DepthGradient;
    const int32 Index = FMath::Clamp(SwatchIndex, 0, 3);
    if (ViewMode == EVirtualLidarViewMode::HitMask)
    {
        static const FLinearColor Colors[] = { FLinearColor::White, FLinearColor::Black, FLinearColor(0.15f, 0.15f, 0.17f), FLinearColor::White };
        return Colors[Index];
    }
    if (ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        if (LidarComp && LidarComp->SemanticClassRules.IsValidIndex(Index)) return LidarComp->SemanticClassRules[Index].DisplayColor;
        return LidarComp ? LidarComp->DefaultSemanticColor : FLinearColor(0.55f, 0.55f, 0.55f);
    }
    if (ViewMode == EVirtualLidarViewMode::IntensityGray)
    {
        const float Value = 1.0f - Index / 3.0f;
        return FLinearColor(Value, Value, Value, 1.0f);
    }
    static const FLinearColor DepthColors[] = {
        FLinearColor::FromSRGBColor(FColor(255, 0, 255)), FLinearColor::FromSRGBColor(FColor(255, 235, 0)),
        FLinearColor::FromSRGBColor(FColor(0, 255, 255)), FLinearColor::FromSRGBColor(FColor(0, 80, 255)) };
    return DepthColors[Index];
}

void UVirtualSensorMonitorPanelWidget::SetLidarOverlayOptions(bool bAdaptiveDepth, bool bGrid, bool bDepthEdges)
{
    bUseAdaptiveLidarDepthRange = bAdaptiveDepth;
    bOverlayLidarMonitorGrid = bGrid;
    bOverlayLidarDepthEdges = bDepthEdges;
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        FVirtualLidarVisualizationSettings Settings = Visualization->GetVisualizationSettings();
        Settings.bUseAdaptiveDistance = bAdaptiveDepth;
        Settings.bShowGrid = bGrid;
        Settings.bShowDepthEdges = bDepthEdges;
        Visualization->SetVisualizationSettings(Settings);
    }
    InvalidateEnhancedLidarView();
    RefreshImageBrush();
    RefreshStatusText();
    SaveMonitorUiPreferences();
}

void UVirtualSensorMonitorPanelWidget::RestoreMonitorUiPreferences()
{
    const UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
    if (!Preferences) return;
    bUseAdaptiveLidarDepthRange = Preferences->bUseAdaptiveLidarDepthRange;
    bOverlayLidarMonitorGrid = Preferences->bOverlayLidarMonitorGrid;
    bOverlayLidarDepthEdges = Preferences->bOverlayLidarDepthEdges;
    bMonitorDetailsExpanded = Preferences->bMonitorDetailsExpanded;
    if (LidarComp && Preferences->LidarViewMode <= static_cast<uint8>(EVirtualLidarViewMode::ActorClassColor))
    {
        LidarComp->ViewMode = static_cast<EVirtualLidarViewMode>(Preferences->LidarViewMode);
    }
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        FVirtualLidarVisualizationSettings Settings = Visualization->GetVisualizationSettings();
        Settings.ProjectionMode = Preferences->LidarProjectionMode;
        Settings.ColorMode = Preferences->LidarColorMode;
        Settings.bShowWorldPointCloud = Preferences->bShowWorldLidarPointCloud;
        Settings.PointSize = Preferences->LidarPointSize;
        Settings.bUseAdaptiveDistance = bUseAdaptiveLidarDepthRange;
        Settings.bShowGrid = bOverlayLidarMonitorGrid;
        Settings.bShowDepthEdges = bOverlayLidarDepthEdges;
        Visualization->SetVisualizationSettings(Settings);
    }
}

void UVirtualSensorMonitorPanelWidget::SaveMonitorUiPreferences() const
{
    UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
    if (!Preferences) return;
    if (LidarComp) Preferences->LidarViewMode = static_cast<uint8>(LidarComp->ViewMode);
    if (const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        const FVirtualLidarVisualizationSettings& Settings = Visualization->GetVisualizationSettings();
        Preferences->LidarProjectionMode = Settings.ProjectionMode;
        Preferences->LidarColorMode = Settings.ColorMode;
        Preferences->bShowWorldLidarPointCloud = Settings.bShowWorldPointCloud;
        Preferences->LidarPointSize = Settings.PointSize;
    }
    Preferences->bUseAdaptiveLidarDepthRange = bUseAdaptiveLidarDepthRange;
    Preferences->bOverlayLidarMonitorGrid = bOverlayLidarMonitorGrid;
    Preferences->bOverlayLidarDepthEdges = bOverlayLidarDepthEdges;
    Preferences->bMonitorDetailsExpanded = bMonitorDetailsExpanded;
    UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
}

void UVirtualSensorMonitorPanelWidget::ResetMonitorUiPreferencesToDefault()
{
    bMonitorDetailsExpanded = false;
    bUseAdaptiveLidarDepthRange = true;
    bOverlayLidarMonitorGrid = true;
    bOverlayLidarDepthEdges = true;
    if (LidarComp) LidarComp->ViewMode = EVirtualLidarViewMode::DepthGradient;
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        Visualization->SetVisualizationSettings(FVirtualLidarVisualizationSettings());
    }
    InvalidateEnhancedLidarView();
    SaveMonitorUiPreferences();
    RefreshNativeFallbackText();
}

UObject* UVirtualSensorMonitorPanelWidget::GetLidarBrushResource()
{
    if (!LidarComp)
    {
        return nullptr;
    }
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        if (UTexture2D* Texture = Visualization->GetPreviewTexture()) return Texture;
    }
    if (!bUseEnhancedLidarMonitorView)
    {
        return LidarComp->GetLidarViewTexture();
    }
    UTexture2D* EnhancedTexture = RebuildEnhancedLidarViewTexture();
    return EnhancedTexture ? EnhancedTexture : LidarComp->GetLidarViewTexture();
}

void UVirtualSensorMonitorPanelWidget::InvalidateEnhancedLidarView()
{
    LastEnhancedLidarFrameId = INDEX_NONE;
    LastEnhancedLidarWidth = 0;
    LastEnhancedLidarHeight = 0;
    LastEnhancedLidarViewMode = 255;
    bLastEnhancedAdaptiveDepth = false;
    bLastEnhancedGrid = false;
    bLastEnhancedEdges = false;
    LastEnhancedEdgeThreshold = -1.0f;
    LastEnhancedEdgeColor = FLinearColor::Transparent;
}

UTexture2D* UVirtualSensorMonitorPanelWidget::RebuildEnhancedLidarViewTexture()
{
    if (!LidarComp)
    {
        return nullptr;
    }

    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    if (Points.Num() <= 0)
    {
        return LidarComp->GetLidarViewTexture();
    }

    const int32 Width = FMath::Max(1, LidarComp->HorizontalSamples);
    const int32 Height = FMath::Max(1, LidarComp->VerticalChannels);
    const int64 CurrentFrameId = LidarComp->GetRuntimeStatus().FrameId;
    const uint8 CurrentViewMode = static_cast<uint8>(LidarComp->ViewMode);
    const bool bCurrentAdaptiveDepth = bUseAdaptiveLidarDepthRange;
    const bool bCurrentGrid = bOverlayLidarMonitorGrid;
    const bool bCurrentEdges = bOverlayLidarDepthEdges;
    const float CurrentEdgeThreshold = LidarDepthEdgeThreshold;
    const FLinearColor CurrentEdgeColor = LidarDepthEdgeColor;

    if (EnhancedLidarViewTexture &&
        LastEnhancedLidarFrameId == CurrentFrameId &&
        LastEnhancedLidarWidth == Width &&
        LastEnhancedLidarHeight == Height &&
        LastEnhancedLidarViewMode == CurrentViewMode &&
        bLastEnhancedAdaptiveDepth == bCurrentAdaptiveDepth &&
        bLastEnhancedGrid == bCurrentGrid &&
        bLastEnhancedEdges == bCurrentEdges &&
        FMath::IsNearlyEqual(LastEnhancedEdgeThreshold, CurrentEdgeThreshold) &&
        LastEnhancedEdgeColor.Equals(CurrentEdgeColor))
    {
        return EnhancedLidarViewTexture;
    }

    TArray<FColor> Pixels;
    Pixels.Init(FColor(4, 4, 12, 255), Width * Height);

    TArray<float> Depths;
    Depths.Init(0.0f, Width * Height);
    TArray<uint8> Hits;
    Hits.Init(0, Width * Height);

    const int32 MaxRenderablePoints = FMath::Min(Points.Num(), Width * Height);
    float MinHitDistance = TNumericLimits<float>::Max();
    float MaxHitDistance = 0.0f;

    for (int32 PointIndex = 0; PointIndex < MaxRenderablePoints; ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (Point.bHit)
        {
            MinHitDistance = FMath::Min(MinHitDistance, Point.Distance);
            MaxHitDistance = FMath::Max(MaxHitDistance, Point.Distance);
        }
    }

    if (MinHitDistance == TNumericLimits<float>::Max())
    {
        MinHitDistance = 0.0f;
        MaxHitDistance = FMath::Max(1.0f, LidarComp->MaxDistance);
    }

    const int32 SafeGridColumnStep = FMath::Max(1, LidarMonitorGridColumnStep);
    const int32 SafeGridRowStep = FMath::Max(1, LidarMonitorGridRowStep);

    for (int32 PointIndex = 0; PointIndex < MaxRenderablePoints; ++PointIndex)
    {
        const int32 H = PointIndex % Width;
        const int32 V = PointIndex / Width;
        const int32 DrawH = LidarComp->bFlipLidarViewHorizontal ? Width - 1 - H : H;
        const int32 DrawV = LidarComp->bFlipLidarViewVertical ? Height - 1 - V : V;
        const int32 PixelIndex = DrawV * Width + DrawH;
        if (!Pixels.IsValidIndex(PixelIndex))
        {
            continue;
        }

        const FVirtualLidarPoint& Point = Points[PointIndex];
        const float NormalizedDistance = NormalizeMonitorLidarDistance(Point, LidarComp->MaxDistance, MinHitDistance, MaxHitDistance, bUseAdaptiveLidarDepthRange);
        const bool bGrid = bOverlayLidarMonitorGrid && ((H % SafeGridColumnStep) == 0 || (V % SafeGridRowStep) == 0);
        Pixels[PixelIndex] = ApplyMonitorGridOverlay(MakeMonitorLidarColor(LidarComp, LidarComp->ViewMode, Point, NormalizedDistance), Point.bHit, bGrid);
        Depths[PixelIndex] = Point.Distance;
        Hits[PixelIndex] = Point.bHit ? 1 : 0;
    }

    if (bOverlayLidarDepthEdges)
    {
        const FColor EdgeColor = LidarDepthEdgeColor.ToFColor(true);
        const float SafeThreshold = FMath::Max(0.1f, LidarDepthEdgeThreshold);
        TArray<FColor> EdgePixels = Pixels;

        for (int32 Y = 0; Y < Height; ++Y)
        {
            for (int32 X = 0; X < Width; ++X)
            {
                const int32 PixelIndex = Y * Width + X;
                if (!Hits.IsValidIndex(PixelIndex) || Hits[PixelIndex] == 0)
                {
                    continue;
                }

                bool bEdge = false;
                if (X + 1 < Width)
                {
                    const int32 RightIndex = Y * Width + (X + 1);
                    bEdge |= Hits[RightIndex] != 0 && FMath::Abs(Depths[PixelIndex] - Depths[RightIndex]) >= SafeThreshold;
                }
                if (Y + 1 < Height)
                {
                    const int32 DownIndex = (Y + 1) * Width + X;
                    bEdge |= Hits[DownIndex] != 0 && FMath::Abs(Depths[PixelIndex] - Depths[DownIndex]) >= SafeThreshold;
                }
                if (X > 0)
                {
                    const int32 LeftIndex = Y * Width + (X - 1);
                    bEdge |= Hits[LeftIndex] != 0 && FMath::Abs(Depths[PixelIndex] - Depths[LeftIndex]) >= SafeThreshold;
                }
                if (Y > 0)
                {
                    const int32 UpIndex = (Y - 1) * Width + X;
                    bEdge |= Hits[UpIndex] != 0 && FMath::Abs(Depths[PixelIndex] - Depths[UpIndex]) >= SafeThreshold;
                }

                if (bEdge)
                {
                    EdgePixels[PixelIndex] = EdgeColor;
                }
            }
        }

        Pixels = MoveTemp(EdgePixels);
    }

    if (!EnhancedLidarViewTexture || LastEnhancedLidarWidth != Width || LastEnhancedLidarHeight != Height)
    {
        EnhancedLidarViewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
        if (EnhancedLidarViewTexture)
        {
            EnhancedLidarViewTexture->SRGB = true;
            EnhancedLidarViewTexture->CompressionSettings = TC_VectorDisplacementmap;
        }
    }

    if (!EnhancedLidarViewTexture || !EnhancedLidarViewTexture->GetPlatformData() || EnhancedLidarViewTexture->GetPlatformData()->Mips.Num() <= 0)
    {
        return LidarComp->GetLidarViewTexture();
    }

    FTexture2DMipMap& Mip = EnhancedLidarViewTexture->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    if (!Data)
    {
        Mip.BulkData.Unlock();
        return LidarComp->GetLidarViewTexture();
    }

    FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
    Mip.BulkData.Unlock();
    EnhancedLidarViewTexture->UpdateResource();

    LastEnhancedLidarFrameId = CurrentFrameId;
    LastEnhancedLidarWidth = Width;
    LastEnhancedLidarHeight = Height;
    LastEnhancedLidarViewMode = CurrentViewMode;
    bLastEnhancedAdaptiveDepth = bCurrentAdaptiveDepth;
    bLastEnhancedGrid = bCurrentGrid;
    bLastEnhancedEdges = bCurrentEdges;
    LastEnhancedEdgeThreshold = CurrentEdgeThreshold;
    LastEnhancedEdgeColor = CurrentEdgeColor;
    return EnhancedLidarViewTexture;
}

void UVirtualSensorMonitorPanelWidget::CaptureLocalSensorFrame()
{
    if (!bLocalSensorCaptureActive)
    {
        return;
    }
    if (bSkipLocalCaptureWhenWritePending && (bLocalCaptureCameraWritePending || bLocalCaptureLidarWritePending))
    {
        return;
    }

    ++LocalCaptureFrameIndex;
    const FString FramePrefix = FString::Printf(TEXT("frame_%06d_%s"), LocalCaptureFrameIndex, *BuildPointCloudTimestamp());
    const bool bCameraQueued = bLocalCaptureSaveCameraFrames && SaveCameraSnapshotToDisk(FramePrefix);
    const bool bLidarQueued = bLocalCaptureSaveLidarPointCloud && SaveLidarPointCloudToDisk(FramePrefix);
    UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Local capture frame=%d camera=%s lidar=%s folder=%s"), LocalCaptureFrameIndex, bCameraQueued ? TEXT("queued") : TEXT("skipped"), bLidarQueued ? TEXT("queued") : TEXT("skipped"), *LocalCaptureSessionDirectory);
}

bool UVirtualSensorMonitorPanelWidget::SaveCameraSnapshotToDisk(const FString& FramePrefix)
{
    return bUseGpuAsyncCameraReadback ? QueueCameraGpuReadbackToDisk(FramePrefix) : SaveCameraSnapshotToDiskSynchronous(FramePrefix);
}

bool UVirtualSensorMonitorPanelWidget::QueueCameraGpuReadbackToDisk(const FString& FramePrefix)
{
    if (!CameraComp || PendingCameraReadbacks.Num() >= FMath::Max(1, MaxPendingCameraReadbacks))
    {
        return false;
    }

    UTextureRenderTarget2D* RenderTarget = CameraComp->GetCameraRenderTarget();
    if (!RenderTarget)
    {
        return false;
    }

    if (!bLocalCaptureUseCachedSensorFrames)
    {
        CameraComp->CaptureScene();
    }

    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return false;
    }

    FTextureRHIRef Texture = Resource->GetRenderTargetTexture();
    if (!Texture.IsValid())
    {
        return false;
    }

    const FString CameraDirectory = FPaths::Combine(EnsureLocalCaptureSessionDirectory(), TEXT("Camera"));
    IFileManager::Get().MakeDirectory(*CameraDirectory, true);

    FVirtualSensorPendingCameraReadback Pending;
    Pending.OutputPath = FPaths::Combine(CameraDirectory, FString::Printf(TEXT("%s_%s.jpg"), *FramePrefix, *CameraComp->SensorId));
    Pending.Width = RenderTarget->SizeX;
    Pending.Height = RenderTarget->SizeY;
    Pending.Readback = MakeShared<FRHIGPUTextureReadback, ESPMode::ThreadSafe>(TEXT("VirtualSensorCameraReadback"));

    TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback = Pending.Readback;
    ENQUEUE_RENDER_COMMAND(VirtualSensorQueueCameraReadback)([Readback, Texture](FRHICommandListImmediate& RHICmdList)
    {
        if (Readback.IsValid() && Texture.IsValid())
        {
            Readback->EnqueueCopy(RHICmdList, Texture);
        }
    });

    PendingCameraReadbacks.Add(MoveTemp(Pending));
    RefreshLocalCaptureCameraPendingState();
    return true;
}

bool UVirtualSensorMonitorPanelWidget::SaveCameraSnapshotToDiskSynchronous(const FString& FramePrefix)
{
    if (!CameraComp || bLocalCaptureCameraWritePending)
    {
        return false;
    }

    UTextureRenderTarget2D* RenderTarget = CameraComp->GetCameraRenderTarget();
    if (!RenderTarget)
    {
        return false;
    }

    if (!bLocalCaptureUseCachedSensorFrames)
    {
        CameraComp->CaptureScene();
    }

    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return false;
    }

    TArray<FColor> RawPixels;
    if (!Resource->ReadPixels(RawPixels) || RawPixels.Num() == 0)
    {
        return false;
    }

    const FString CameraDirectory = FPaths::Combine(EnsureLocalCaptureSessionDirectory(), TEXT("Camera"));
    IFileManager::Get().MakeDirectory(*CameraDirectory, true);
    const FString Path = FPaths::Combine(CameraDirectory, FString::Printf(TEXT("%s_%s.jpg"), *FramePrefix, *CameraComp->SensorId));
    StartAsyncCameraJpegWrite(MoveTemp(RawPixels), RenderTarget->SizeX, RenderTarget->SizeY, Path);
    return true;
}

void UVirtualSensorMonitorPanelWidget::ProcessPendingCameraReadbacks()
{
    for (int32 Index = PendingCameraReadbacks.Num() - 1; Index >= 0; --Index)
    {
        FVirtualSensorPendingCameraReadback& Pending = PendingCameraReadbacks[Index];
        if (!Pending.Readback.IsValid())
        {
            PendingCameraReadbacks.RemoveAtSwap(Index);
            RefreshLocalCaptureCameraPendingState();
            continue;
        }

        if (!Pending.Readback->IsReady())
        {
            continue;
        }

        int32 RowPitchInPixels = 0;
        void* LockedData = Pending.Readback->Lock(RowPitchInPixels);
        if (!LockedData || RowPitchInPixels < Pending.Width || Pending.Width <= 0 || Pending.Height <= 0)
        {
            if (LockedData)
            {
                Pending.Readback->Unlock();
            }
            PendingCameraReadbacks.RemoveAtSwap(Index);
            RefreshLocalCaptureCameraPendingState();
            continue;
        }

        TArray<FColor> RawPixels;
        RawPixels.SetNumUninitialized(Pending.Width * Pending.Height);
        const FColor* SourcePixels = static_cast<const FColor*>(LockedData);
        for (int32 Y = 0; Y < Pending.Height; ++Y)
        {
            FMemory::Memcpy(RawPixels.GetData() + Y * Pending.Width, SourcePixels + Y * RowPitchInPixels, Pending.Width * sizeof(FColor));
        }
        Pending.Readback->Unlock();

        const FString Path = Pending.OutputPath;
        const int32 Width = Pending.Width;
        const int32 Height = Pending.Height;
        PendingCameraReadbacks.RemoveAtSwap(Index);
        RefreshLocalCaptureCameraPendingState();
        StartAsyncCameraJpegWrite(MoveTemp(RawPixels), Width, Height, Path);
    }

    RefreshLocalCaptureCameraPendingState();
}

void UVirtualSensorMonitorPanelWidget::StartAsyncCameraJpegWrite(TArray<FColor>&& RawPixels, int32 Width, int32 Height, const FString& Path)
{
    IImageWrapperModule* ImageWrapperModule = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TWeakObjectPtr<UVirtualSensorMonitorPanelWidget> WeakThis(this);
    ++LocalCaptureCameraAsyncWriteCount;
    RefreshLocalCaptureCameraPendingState();

    Async(EAsyncExecution::ThreadPool, [WeakThis, ImageWrapperModule, RawPixels = MoveTemp(RawPixels), Path, Width, Height]() mutable
    {
        bool bSaved = false;
        if (ImageWrapperModule)
        {
            TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::JPEG);
            if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
            {
                const TArray64<uint8>& Bytes64 = ImageWrapper->GetCompressed(80);
                if (Bytes64.Num() > 0)
                {
                    TArray<uint8> Bytes32;
                    Bytes32.Append(Bytes64.GetData(), static_cast<int32>(Bytes64.Num()));
                    bSaved = FFileHelper::SaveArrayToFile(Bytes32, *Path);
                }
            }
        }

        AsyncTask(ENamedThreads::GameThread, [WeakThis, Path, bSaved]()
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            WeakThis->LocalCaptureCameraAsyncWriteCount = FMath::Max(0, WeakThis->LocalCaptureCameraAsyncWriteCount - 1);
            WeakThis->RefreshLocalCaptureCameraPendingState();
            if (bSaved)
            {
                UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Async camera save saved: %s"), *Path);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[SensorMonitor] Async camera save failed: %s"), *Path);
            }
        });
    });
}

void UVirtualSensorMonitorPanelWidget::RefreshLocalCaptureCameraPendingState()
{
    bLocalCaptureCameraWritePending = PendingCameraReadbacks.Num() > 0 || LocalCaptureCameraAsyncWriteCount > 0;
}

bool UVirtualSensorMonitorPanelWidget::SaveLidarPointCloudToDisk(const FString& FramePrefix)
{
    if (!LidarComp || bLocalCaptureLidarWritePending)
    {
        return false;
    }
    if (!bLocalCaptureSaveLidarCsv && !bLocalCaptureSaveLidarLas && !bLocalCaptureSaveLidarLaz)
    {
        return false;
    }
    if (!bLocalCaptureUseCachedSensorFrames || LidarComp->GetLastPoints().Num() <= 0)
    {
        RefreshLidarPreviewWithoutTransport();
    }

    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    if (Points.Num() <= 0)
    {
        return false;
    }

    const bool bSyncLasSaved = bLocalCaptureSaveLidarLas ? LidarComp->ExportLastPointCloudLas(FramePrefix) : false;
    const bool bSyncLazSaved = bLocalCaptureSaveLidarLaz ? LidarComp->ExportLastPointCloudLaz(FramePrefix) : false;

    if (!bLocalCaptureSaveLidarCsv)
    {
        return bSyncLasSaved || bSyncLazSaved;
    }

    TArray<FLocalLidarCsvPoint> CsvPoints;
    CsvPoints.Reserve(Points.Num());
    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (LidarComp->bExportHitOnlyPointCloud && !Point.bHit)
        {
            continue;
        }

        const FIntPoint GridCoord = ResolveLidarGridCoord(Point, PointIndex, LidarComp->HorizontalSamples);
        FLocalLidarCsvPoint CsvPoint;
        CsvPoint.Row = GridCoord.X;
        CsvPoint.Col = GridCoord.Y;
        CsvPoint.ReturnIndex = Point.ReturnIndex;
        CsvPoint.Point = Point.WorldLocation;
        CsvPoints.Add(CsvPoint);
    }
    if (CsvPoints.Num() <= 0)
    {
        return bSyncLasSaved || bSyncLazSaved;
    }

    const FString LidarDirectory = FPaths::Combine(EnsureLocalCaptureSessionDirectory(), TEXT("Lidar"));
    IFileManager::Get().MakeDirectory(*LidarDirectory, true);
    const FString Path = FPaths::Combine(LidarDirectory, FString::Printf(TEXT("%s_%s.csv"), *FramePrefix, *LidarComp->SensorId));
    TWeakObjectPtr<UVirtualSensorMonitorPanelWidget> WeakThis(this);
    bLocalCaptureLidarWritePending = true;

    Async(EAsyncExecution::ThreadPool, [WeakThis, CsvPoints = MoveTemp(CsvPoints), Path]() mutable
    {
        FString Text;
        Text.Reserve(FMath::Max(128, CsvPoints.Num() * 64));
        Text += TEXT("row,col,returnIndex,x,y,z\n");
        for (const FLocalLidarCsvPoint& CsvPoint : CsvPoints)
        {
            Text += FString::Printf(TEXT("%d,%d,%d,%f,%f,%f\n"), CsvPoint.Row, CsvPoint.Col, CsvPoint.ReturnIndex, CsvPoint.Point.X, CsvPoint.Point.Y, CsvPoint.Point.Z);
        }
        const bool bSaved = FFileHelper::SaveStringToFile(Text, *Path);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, Path, bSaved]()
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            WeakThis->bLocalCaptureLidarWritePending = false;
            if (bSaved)
            {
                UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Async lidar CSV save saved: %s"), *Path);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[SensorMonitor] Async lidar CSV save failed: %s"), *Path);
            }
        });
    });
    return true;
}

bool UVirtualSensorMonitorPanelWidget::RefreshLidarPreviewWithoutTransport()
{
    if (!LidarComp)
    {
        return false;
    }

    const EVirtualLidarOutputMode SavedOutputMode = LidarComp->OutputMode;
    UVirtualSensorTransportComponent* SavedTransport = LidarComp->TransportComponent;
    UVirtualSensorRecorderComponent* SavedRecorder = LidarComp->RecorderComponent;
    const bool bSavedExportCsv = LidarComp->bExportCsvOnScan;
    const bool bSavedExportJsonLines = LidarComp->bExportJsonLinesOnScan;
    const bool bSavedExportPcd = LidarComp->bExportPcdOnScan;

    LidarComp->OutputMode = EVirtualLidarOutputMode::None;
    LidarComp->TransportComponent = nullptr;
    LidarComp->RecorderComponent = nullptr;
    LidarComp->bExportCsvOnScan = false;
    LidarComp->bExportJsonLinesOnScan = false;
    LidarComp->bExportPcdOnScan = false;
    LidarComp->ScanAndSend();
    LidarComp->OutputMode = SavedOutputMode;
    LidarComp->TransportComponent = SavedTransport;
    LidarComp->RecorderComponent = SavedRecorder;
    LidarComp->bExportCsvOnScan = bSavedExportCsv;
    LidarComp->bExportJsonLinesOnScan = bSavedExportJsonLines;
    LidarComp->bExportPcdOnScan = bSavedExportPcd;
    InvalidateEnhancedLidarView();
    return LidarComp->GetLidarViewTexture() != nullptr;
}

FString UVirtualSensorMonitorPanelWidget::EnsureLocalCaptureSessionDirectory()
{
    if (!LocalCaptureSessionDirectory.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*LocalCaptureSessionDirectory, true);
        return LocalCaptureSessionDirectory;
    }

    LocalCaptureSessionDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), LocalCaptureFolderName, BuildPointCloudTimestamp());
    IFileManager::Get().MakeDirectory(*LocalCaptureSessionDirectory, true);
    return LocalCaptureSessionDirectory;
}

UVirtualLidarScanComponent* UVirtualSensorMonitorPanelWidget::GetTargetLidarForPreview() const
{
    if (SensorManager && SensorManager->GetSelectedLidar())
    {
        return SensorManager->GetSelectedLidar();
    }
    return LidarComp;
}

UObject* UVirtualSensorMonitorPanelWidget::GetSecondaryLidarBrushResource()
{
    UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent();
    return Visualization ? Visualization->GetSecondaryPreviewTexture() : nullptr;
}

UVirtualLidarVisualizationComponent* UVirtualSensorMonitorPanelWidget::GetLidarVisualizationComponent() const
{
    const AVirtualLidarSensorActor* Actor = LidarComp ? Cast<AVirtualLidarSensorActor>(LidarComp->GetOwner()) : nullptr;
    return Actor ? Actor->VisualizationComponent : nullptr;
}

#undef LOCTEXT_NAMESPACE
