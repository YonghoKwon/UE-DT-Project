#include "VirtualSensorMonitorWidget.h"

#include "Async/Async.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
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
#include "TextureResource.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorTypes.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"

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

FString BuildPointCloudExportDirectory(const UVirtualLidarSensorComp* LidarComp)
{
    const FString SensorId = LidarComp ? LidarComp->SensorId : TEXT("LIDAR");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

FString BuildServerPayloadExportDirectory(const UVirtualLidarSensorComp* LidarComp)
{
    const FString SensorId = LidarComp ? LidarComp->SensorId : TEXT("LIDAR");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("ServerPayload"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

FString BuildCameraPayloadExportDirectory(const UVirtualCameraComp* CameraComp)
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

FColor MakeMonitorLidarColor(EVirtualLidarViewMode ViewMode, const FVirtualLidarPoint& Point, float NormalizedDistance)
{
    const uint8 Intensity = Point.bHit ? static_cast<uint8>((1.0f - NormalizedDistance) * 255.0f) : 0;

    if (ViewMode == EVirtualLidarViewMode::HitMask)
    {
        return Point.bHit ? FColor::White : FColor::Black;
    }

    if (ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        return Point.bHit ? FColor(0, 255, 90, 255) : FColor(3, 8, 10, 255);
    }

    if (ViewMode == EVirtualLidarViewMode::IntensityGray)
    {
        return FColor(Intensity, Intensity, Intensity, 255);
    }

    if (!Point.bHit)
    {
        return FColor(4, 4, 12, 255);
    }

    if (NormalizedDistance < 0.20f)
    {
        return FColor(255, 0, 255, 255);
    }
    if (NormalizedDistance < 0.40f)
    {
        return FColor(255, 48, 0, 255);
    }
    if (NormalizedDistance < 0.60f)
    {
        return FColor(255, 235, 0, 255);
    }
    if (NormalizedDistance < 0.80f)
    {
        return FColor(0, 255, 255, 255);
    }
    return FColor(0, 80, 255, 255);
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

FString BuildRowColCsvText(const UVirtualLidarSensorComp* LidarComp, int32& OutExportedPointCount)
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

bool ExportRowColPointCloudCsv(const UVirtualLidarSensorComp* LidarComp, const FString& FileNamePrefix)
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

FString UVirtualSensorMonitorWidget::GetMonitorTitleText() const
{
    return BuildTitleText();
}

FString UVirtualSensorMonitorWidget::GetMonitorStatusText() const
{
    return BuildStatusText();
}

FVirtualSensorMonitorDisplayData UVirtualSensorMonitorWidget::GetMonitorDisplayData() const
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
    Data.WarningText = GetTransportWarningText();
    Data.ViewModeText = GetViewModeSummaryText();
    Data.FullStatusText = GetMonitorStatusText();
    return Data;
}

FString UVirtualSensorMonitorWidget::GetSelectedSensorIdText() const
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

FString UVirtualSensorMonitorWidget::GetFrameSummaryText() const
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

FString UVirtualSensorMonitorWidget::GetMeasurementSummaryText() const
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

FString UVirtualSensorMonitorWidget::GetServerPayloadSummaryText() const
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

FString UVirtualSensorMonitorWidget::GetPreviewPolicySummaryText() const
{
    const UVirtualLidarSensorComp* TargetLidar = GetTargetLidarForPreview();
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

FString UVirtualSensorMonitorWidget::GetSlabAnalysisSummaryText() const
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

FString UVirtualSensorMonitorWidget::GetTransportWarningText() const
{
    if (bShowingLidar && LidarComp)
    {
        const FString& Warning = LidarComp->GetRuntimeStatus().PerformanceWarning;
        return FString::Printf(TEXT("Transport/Warning: %s"), Warning.IsEmpty() ? TEXT("None") : *Warning);
    }
    if (!bShowingLidar && CameraComp)
    {
        const FString& Message = CameraComp->GetRuntimeStatus().LastMessage;
        return FString::Printf(TEXT("Transport/Warning: %s"), Message.IsEmpty() ? TEXT("None") : *Message);
    }
    return TEXT("Transport/Warning: unavailable");
}

FString UVirtualSensorMonitorWidget::GetViewModeSummaryText() const
{
    if (bShowingLidar)
    {
        return FString::Printf(TEXT("LiDAR View: %s"), *GetLidarViewModeDisplayText());
    }
    return TEXT("Camera View: Render Target");
}

TSharedRef<SWidget> UVirtualSensorMonitorWidget::RebuildWidget()
{
    if (!ShouldUseNativeFallbackWidget())
    {
        return Super::RebuildWidget();
    }

    return SNew(SBorder)
        .Padding(12.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SAssignNew(NativeTitleTextBlock, STextBlock)
                .Text(FText::FromString(BuildTitleText()))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(SWrapBox)
                .UseAllottedSize(true)
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Toggle View")))
                    .OnClicked_Lambda([this]()
                    {
                        HandleToggleButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Next Camera")))
                    .OnClicked_Lambda([this]()
                    {
                        HandleNextCameraButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Next LiDAR")))
                    .OnClicked_Lambda([this]()
                    {
                        HandleNextLidarButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("PointCloudOnly")))
                    .OnClicked_Lambda([this]()
                    {
                        HandlePointCloudOnlyButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("LiDAR View")))
                    .OnClicked_Lambda([this]()
                    {
                        HandleLidarViewModeButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Capture Once")))
                    .OnClicked_Lambda([this]()
                    {
                        HandleCaptureOnceButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Export Payload")))
                    .OnClicked_Lambda([this]()
                    {
                        HandleExportServerPayloadButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Hit Only")))
                    .OnClicked_Lambda([this]()
                    {
                        HandlePreviewHitOnlyButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Preview -")))
                    .OnClicked_Lambda([this]()
                    {
                        HandlePreviewLessButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
                + SWrapBox::Slot()
                .Padding(0.0f, 0.0f, 6.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Preview +")))
                    .OnClicked_Lambda([this]()
                    {
                        HandlePreviewMoreButtonClicked();
                        RefreshNativeFallbackText();
                        return FReply::Handled();
                    })
                ]
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    SAssignNew(NativeStatusTextBlock, STextBlock)
                    .AutoWrapText(true)
                    .Text(FText::FromString(BuildStatusText()))
                ]
            ]
        ];
}

void UVirtualSensorMonitorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ToggleButton)
    {
        ToggleButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleToggleButtonClicked);
        ToggleButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleToggleButtonClicked);
    }
    if (NextCameraButton)
    {
        NextCameraButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleNextCameraButtonClicked);
        NextCameraButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleNextCameraButtonClicked);
    }
    if (NextLidarButton)
    {
        NextLidarButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleNextLidarButtonClicked);
        NextLidarButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleNextLidarButtonClicked);
    }
    if (PointCloudOnlyButton)
    {
        PointCloudOnlyButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandlePointCloudOnlyButtonClicked);
        PointCloudOnlyButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandlePointCloudOnlyButtonClicked);
    }
    if (LidarViewModeButton)
    {
        LidarViewModeButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleLidarViewModeButtonClicked);
        LidarViewModeButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleLidarViewModeButtonClicked);
    }
    if (LogPointCloudButton)
    {
        LogPointCloudButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleLogPointCloudButtonClicked);
        LogPointCloudButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleLogPointCloudButtonClicked);
    }
    if (ExportPointCloudButton)
    {
        ExportPointCloudButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleExportPointCloudButtonClicked);
        ExportPointCloudButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleExportPointCloudButtonClicked);
    }
    if (LocalSensorCaptureButton)
    {
        LocalSensorCaptureButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleLocalSensorCaptureButtonClicked);
        LocalSensorCaptureButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleLocalSensorCaptureButtonClicked);
    }
    if (CaptureOnceButton)
    {
        CaptureOnceButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleCaptureOnceButtonClicked);
        CaptureOnceButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleCaptureOnceButtonClicked);
    }
    if (ExportServerPayloadButton)
    {
        ExportServerPayloadButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleExportServerPayloadButtonClicked);
        ExportServerPayloadButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleExportServerPayloadButtonClicked);
    }
    if (PreviewMoreButton)
    {
        PreviewMoreButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandlePreviewMoreButtonClicked);
        PreviewMoreButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandlePreviewMoreButtonClicked);
    }
    if (PreviewLessButton)
    {
        PreviewLessButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandlePreviewLessButtonClicked);
        PreviewLessButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandlePreviewLessButtonClicked);
    }
    if (PreviewHitOnlyButton)
    {
        PreviewHitOnlyButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandlePreviewHitOnlyButtonClicked);
        PreviewHitOnlyButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandlePreviewHitOnlyButtonClicked);
    }

    RefreshTitle();
    RefreshImageBrush();
    RefreshStatusText();
    RefreshLocalCaptureButtonText();
    RefreshLidarViewModeButtonText();
    if (PreviewHitOnlyButtonText)
    {
        const UVirtualLidarSensorComp* TargetLidar = GetTargetLidarForPreview();
        PreviewHitOnlyButtonText->SetText(FText::FromString(TargetLidar && TargetLidar->bPointCloudPreviewHitOnly ? TEXT("Preview: Hit Only") : TEXT("Preview: All Points")));
    }
}

void UVirtualSensorMonitorWidget::NativeDestruct()
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

void UVirtualSensorMonitorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    ProcessPendingCameraReadbacks();
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorWidget::BindVirtualCamera(UVirtualCameraComp* InCameraComp)
{
    CameraComp = InCameraComp;
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorWidget::BindVirtualLidar(UVirtualLidarSensorComp* InLidarComp)
{
    LidarComp = InLidarComp;
    if (LidarComp && LidarComp->ViewMode == EVirtualLidarViewMode::IntensityGray)
    {
        LidarComp->ViewMode = EVirtualLidarViewMode::DepthGradient;
    }
    InvalidateEnhancedLidarView();
    if (bShowingLidar && LidarComp && !LidarComp->GetLidarViewTexture())
    {
        RefreshLidarPreviewWithoutTransport();
    }
    RefreshImageBrush();
    RefreshStatusText();
    RefreshLidarViewModeButtonText();
}

void UVirtualSensorMonitorWidget::BindSensorManager(AVirtualSensorManager* InSensorManager)
{
    SensorManager = InSensorManager;
    if (SensorManager)
    {
        SensorManager->BindMonitorWidget(this);
    }
    RefreshStatusText();
}

void UVirtualSensorMonitorWidget::ShowCameraView()
{
    bShowingLidar = false;
    RefreshTitle();
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorWidget::ShowLidarView()
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

void UVirtualSensorMonitorWidget::ToggleView()
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

void UVirtualSensorMonitorWidget::CycleLidarViewMode()
{
    if (!LidarComp)
    {
        return;
    }

    if (LidarComp->ViewMode == EVirtualLidarViewMode::DepthGradient)
    {
        LidarComp->ViewMode = EVirtualLidarViewMode::HitMask;
    }
    else if (LidarComp->ViewMode == EVirtualLidarViewMode::HitMask)
    {
        LidarComp->ViewMode = EVirtualLidarViewMode::ActorClassColor;
    }
    else if (LidarComp->ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        LidarComp->ViewMode = EVirtualLidarViewMode::IntensityGray;
    }
    else
    {
        LidarComp->ViewMode = EVirtualLidarViewMode::DepthGradient;
    }

    InvalidateEnhancedLidarView();
    RefreshLidarPreviewWithoutTransport();
    RefreshLidarViewModeButtonText();
    RefreshStatusText();
    RefreshImageBrush();
}

void UVirtualSensorMonitorWidget::SetLidarPreviewBudget(int32 InStride, int32 InMaxPoints)
{
    UVirtualLidarSensorComp* TargetLidar = GetTargetLidarForPreview();
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

void UVirtualSensorMonitorWidget::IncreaseLidarPreviewBudget()
{
    const UVirtualLidarSensorComp* TargetLidar = GetTargetLidarForPreview();
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

void UVirtualSensorMonitorWidget::DecreaseLidarPreviewBudget()
{
    const UVirtualLidarSensorComp* TargetLidar = GetTargetLidarForPreview();
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

void UVirtualSensorMonitorWidget::ToggleLidarPreviewHitOnly()
{
    UVirtualLidarSensorComp* TargetLidar = GetTargetLidarForPreview();
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

void UVirtualSensorMonitorWidget::CaptureSelectedSensorsOnce()
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

bool UVirtualSensorMonitorWidget::ExportSelectedLidarServerPayload(const FString& FileNamePrefix)
{
    UVirtualLidarSensorComp* TargetLidar = GetTargetLidarForPreview();
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

bool UVirtualSensorMonitorWidget::ExportSelectedSensorServerPayload(const FString& FileNamePrefix)
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

void UVirtualSensorMonitorWidget::ToggleLocalSensorCapture()
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
        World->GetTimerManager().SetTimer(LocalSensorCaptureTimerHandle, this, &UVirtualSensorMonitorWidget::CaptureLocalSensorFrame, FMath::Max(0.05f, LocalCaptureIntervalSeconds), true);
        UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Local timed capture started. Interval=%.3fs Session=%s GpuReadback=%s MaxReadbacks=%d LidarFormats=[csv:%s las:%s laz:%s]"), LocalCaptureIntervalSeconds, *LocalCaptureSessionDirectory, bUseGpuAsyncCameraReadback ? TEXT("true") : TEXT("false"), MaxPendingCameraReadbacks, bLocalCaptureSaveLidarCsv ? TEXT("on") : TEXT("off"), bLocalCaptureSaveLidarLas ? TEXT("on") : TEXT("off"), bLocalCaptureSaveLidarLaz ? TEXT("on") : TEXT("off"));
    }
    RefreshLocalCaptureButtonText();
    RefreshStatusText();
}

void UVirtualSensorMonitorWidget::HandleToggleButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->ToggleSensorView();
        return;
    }
    ToggleView();
}

void UVirtualSensorMonitorWidget::HandleNextCameraButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->SelectNextCamera();
        SensorManager->SetViewMode(EVirtualSensorViewMode::Camera);
    }
}

void UVirtualSensorMonitorWidget::HandleNextLidarButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->SelectNextLidar();
        SensorManager->SetViewMode(EVirtualSensorViewMode::Lidar);
    }
}

void UVirtualSensorMonitorWidget::HandlePointCloudOnlyButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->TogglePointCloudOnlyView();
    }
}

void UVirtualSensorMonitorWidget::HandleLidarViewModeButtonClicked()
{
    CycleLidarViewMode();
}

void UVirtualSensorMonitorWidget::HandleLocalSensorCaptureButtonClicked()
{
    ToggleLocalSensorCapture();
}

void UVirtualSensorMonitorWidget::HandleCaptureOnceButtonClicked()
{
    CaptureSelectedSensorsOnce();
}

void UVirtualSensorMonitorWidget::HandleExportServerPayloadButtonClicked()
{
    ExportSelectedSensorServerPayload(TEXT("button_server_payload"));
}

void UVirtualSensorMonitorWidget::HandlePreviewMoreButtonClicked()
{
    IncreaseLidarPreviewBudget();
}

void UVirtualSensorMonitorWidget::HandlePreviewLessButtonClicked()
{
    DecreaseLidarPreviewBudget();
}

void UVirtualSensorMonitorWidget::HandlePreviewHitOnlyButtonClicked()
{
    ToggleLidarPreviewHitOnly();
}

void UVirtualSensorMonitorWidget::HandleLogPointCloudButtonClicked()
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

void UVirtualSensorMonitorWidget::HandleExportPointCloudButtonClicked()
{
    if (!LidarComp)
    {
        return;
    }

    ExportRowColPointCloudCsv(LidarComp, TEXT("button_export"));
    LidarComp->ExportLastPointCloudLas(TEXT("button_export"));
    LidarComp->ExportLastPointCloudLaz(TEXT("button_export"));
}

void UVirtualSensorMonitorWidget::RefreshImageBrush()
{
    if (!ViewImage)
    {
        return;
    }

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

    FSlateBrush Brush;
    Brush.SetResourceObject(Resource);
    Brush.ImageSize = FVector2D(640.0f, 360.0f);
    ViewImage->SetBrush(Brush);
}

void UVirtualSensorMonitorWidget::RefreshTitle()
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

void UVirtualSensorMonitorWidget::RefreshStatusText()
{
    const FString Text = BuildStatusText();
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Text));
    }
    if (NativeStatusTextBlock.IsValid())
    {
        NativeStatusTextBlock->SetText(FText::FromString(Text));
    }
}

void UVirtualSensorMonitorWidget::RefreshNativeFallbackText()
{
    RefreshTitle();
    RefreshStatusText();
}

FString UVirtualSensorMonitorWidget::BuildTitleText() const
{
    const bool bPointCloudOnly = SensorManager && SensorManager->IsPointCloudOnlyModeEnabled();
    return bPointCloudOnly ? TEXT("LiDAR Point Cloud Only") : (bShowingLidar ? TEXT("Virtual LIDAR View") : TEXT("Virtual Camera View"));
}

FString UVirtualSensorMonitorWidget::BuildStatusText() const
{
    FString Text;
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        const FVirtualLidarSlabAnalysisResult& Slab = Status.SlabAnalysis;
        Text = FString::Printf(TEXT("Sensor: %s\nFrame: %lld\nScan: %.3fs Rays=%d\nMeasured Points/Hits: %d/%d\nServer Payload: Points=%d Bytes=%d Stride=%d Max=%d IncludeMiss=%s\nPreview: %s Points=%d Stride=%d Max=%d HitOnly=%s\nSlab: %s Points=%d Angle=%.2f Ref=%.2f Dev=%.2f Conf=%.2f\nSlab Center: X=%.1f Y=%.1f Z=%.1f\nTransport/Warning: %s\nLiDAR View: %s\nEnhanced: %s Adaptive=%s Edge=%s Grid=%s\nControls: CaptureOnce optional, Preview +/- optional\nCSV: row,col,returnIndex,x,y,z\nMessage: %s"),
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
            LidarComp->bIncludeMissPointsInServerPayload ? TEXT("true") : TEXT("false"),
            LidarComp->IsPointCloudPreviewEnabled() ? TEXT("On") : TEXT("Off"),
            Status.PreviewPointCount,
            LidarComp->PreviewPointStride,
            LidarComp->MaxPreviewPoints,
            LidarComp->bPointCloudPreviewHitOnly ? TEXT("true") : TEXT("false"),
            Slab.bValid ? TEXT("Valid") : *Slab.StatusMessage,
            Slab.SlabHitPointCount,
            Slab.EstimatedYawDegrees,
            Slab.ReferenceYawDegrees,
            Slab.AngleDeviationDegrees,
            Slab.Confidence,
            Slab.Center.X,
            Slab.Center.Y,
            Slab.Center.Z,
            Status.PerformanceWarning.IsEmpty() ? TEXT("None") : *Status.PerformanceWarning,
            *GetLidarViewModeDisplayText(),
            bUseEnhancedLidarMonitorView ? TEXT("On") : TEXT("Off"),
            bUseAdaptiveLidarDepthRange ? TEXT("On") : TEXT("Off"),
            bOverlayLidarDepthEdges ? TEXT("On") : TEXT("Off"),
            bOverlayLidarMonitorGrid ? TEXT("On") : TEXT("Off"),
            *Status.LastMessage);
    }
    else if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        const FString DisplaySensorId = Status.SensorId.IsEmpty() ? CameraComp->SensorId : Status.SensorId;
        Text = FString::Printf(TEXT("Sensor: %s\nFrame: %lld\nSchema: virtual-camera.v1\nResolution: %dx%d\nCapture: Mode=%d Quality=%d Interval=%.3fs\nPayload: Bytes=%d Cached=%s\nRenderTarget: %s\nExport: Export Payload writes Saved/SensorCaptures/<SensorId>/ServerPayload\nMessage: %s"),
            *DisplaySensorId,
            Status.FrameId,
            CameraComp->CaptureResolution.X,
            CameraComp->CaptureResolution.Y,
            static_cast<int32>(CameraComp->CaptureMode),
            static_cast<int32>(CameraComp->GetSimulationQuality()),
            CameraComp->CaptureInterval,
            Status.LastPayloadLength,
            CameraComp->GetLastJsonPayload().IsEmpty() ? TEXT("false") : TEXT("true"),
            CameraComp->GetCameraRenderTarget() ? TEXT("Ready") : TEXT("None"),
            *Status.LastMessage);
    }
    else
    {
        Text = bShowingLidar ? TEXT("LIDAR sensor is not bound") : TEXT("Camera sensor is not bound");
    }

    if (SensorManager)
    {
        Text += FString::Printf(TEXT("\n\nHealth: %s"), *SensorManager->GetHealthSummary().Summary);
    }
    if (bLocalSensorCaptureActive || !LocalCaptureSessionDirectory.IsEmpty())
    {
        Text += FString::Printf(TEXT("\n\nLocal Capture: %s\nInterval: %.3fs Frames=%d\nCapture Pending: Camera=%s Lidar=%s Queue=%d/%d GPUReadback=%s\nFormats: CSV=%s LAS=%s LAZ=%s\nLocal Folder: %s"), bLocalSensorCaptureActive ? TEXT("Recording") : TEXT("Stopped"), LocalCaptureIntervalSeconds, LocalCaptureFrameIndex, bLocalCaptureCameraWritePending ? TEXT("true") : TEXT("false"), bLocalCaptureLidarWritePending ? TEXT("true") : TEXT("false"), PendingCameraReadbacks.Num(), MaxPendingCameraReadbacks, bUseGpuAsyncCameraReadback ? TEXT("On") : TEXT("Off"), bLocalCaptureSaveLidarCsv ? TEXT("On") : TEXT("Off"), bLocalCaptureSaveLidarLas ? TEXT("On") : TEXT("Off"), bLocalCaptureSaveLidarLaz ? TEXT("On") : TEXT("Off"), *LocalCaptureSessionDirectory);
    }
    if (!LastManualExportMessage.IsEmpty())
    {
        Text += FString::Printf(TEXT("\n\n%s"), *LastManualExportMessage);
    }
    return Text;
}

bool UVirtualSensorMonitorWidget::ShouldUseNativeFallbackWidget() const
{
    return GetClass() == UVirtualSensorMonitorWidget::StaticClass();
}

void UVirtualSensorMonitorWidget::RefreshLocalCaptureButtonText()
{
    if (LocalSensorCaptureButtonText)
    {
        LocalSensorCaptureButtonText->SetText(FText::FromString(bLocalSensorCaptureActive ? TEXT("Stop Local Capture") : TEXT("Start Local Capture")));
    }
}

void UVirtualSensorMonitorWidget::RefreshLidarViewModeButtonText()
{
    if (LidarViewModeButtonText)
    {
        LidarViewModeButtonText->SetText(FText::FromString(FString::Printf(TEXT("LiDAR View: %s"), *GetLidarViewModeDisplayText())));
    }
    if (PreviewHitOnlyButtonText)
    {
        const UVirtualLidarSensorComp* TargetLidar = GetTargetLidarForPreview();
        PreviewHitOnlyButtonText->SetText(FText::FromString(TargetLidar && TargetLidar->bPointCloudPreviewHitOnly ? TEXT("Preview: Hit Only") : TEXT("Preview: All Points")));
    }
}

FString UVirtualSensorMonitorWidget::GetLidarViewModeDisplayText() const
{
    if (!LidarComp)
    {
        return TEXT("None");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::DepthGradient)
    {
        return TEXT("Depth Color");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::HitMask)
    {
        return TEXT("Hit Mask");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        return TEXT("Actor Color");
    }
    return TEXT("Gray");
}

FString UVirtualSensorMonitorWidget::GetLidarViewLegendText() const
{
    if (!LidarComp)
    {
        return TEXT("None");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::HitMask)
    {
        return TEXT("Hit=White, Miss=Black, Edge=White Outline");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::ActorClassColor)
    {
        return TEXT("Hit=Bright Green, Miss=Dark, Edge=White Outline");
    }
    if (LidarComp->ViewMode == EVirtualLidarViewMode::IntensityGray)
    {
        return TEXT("Adaptive: Near=Bright, Far/Miss=Dark, Edge=White Outline");
    }
    return TEXT("Adaptive: Near=Magenta/Orange, Mid=Yellow, Far=Cyan/Blue, Miss=Dark, Edge=White Outline");
}

UObject* UVirtualSensorMonitorWidget::GetLidarBrushResource()
{
    if (!LidarComp)
    {
        return nullptr;
    }
    if (!bUseEnhancedLidarMonitorView)
    {
        return LidarComp->GetLidarViewTexture();
    }
    UTexture2D* EnhancedTexture = RebuildEnhancedLidarViewTexture();
    return EnhancedTexture ? EnhancedTexture : LidarComp->GetLidarViewTexture();
}

void UVirtualSensorMonitorWidget::InvalidateEnhancedLidarView()
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

UTexture2D* UVirtualSensorMonitorWidget::RebuildEnhancedLidarViewTexture()
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
        Pixels[PixelIndex] = ApplyMonitorGridOverlay(MakeMonitorLidarColor(LidarComp->ViewMode, Point, NormalizedDistance), Point.bHit, bGrid);
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

void UVirtualSensorMonitorWidget::CaptureLocalSensorFrame()
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

bool UVirtualSensorMonitorWidget::SaveCameraSnapshotToDisk(const FString& FramePrefix)
{
    return bUseGpuAsyncCameraReadback ? QueueCameraGpuReadbackToDisk(FramePrefix) : SaveCameraSnapshotToDiskSynchronous(FramePrefix);
}

bool UVirtualSensorMonitorWidget::QueueCameraGpuReadbackToDisk(const FString& FramePrefix)
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

bool UVirtualSensorMonitorWidget::SaveCameraSnapshotToDiskSynchronous(const FString& FramePrefix)
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

void UVirtualSensorMonitorWidget::ProcessPendingCameraReadbacks()
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

void UVirtualSensorMonitorWidget::StartAsyncCameraJpegWrite(TArray<FColor>&& RawPixels, int32 Width, int32 Height, const FString& Path)
{
    IImageWrapperModule* ImageWrapperModule = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TWeakObjectPtr<UVirtualSensorMonitorWidget> WeakThis(this);
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

void UVirtualSensorMonitorWidget::RefreshLocalCaptureCameraPendingState()
{
    bLocalCaptureCameraWritePending = PendingCameraReadbacks.Num() > 0 || LocalCaptureCameraAsyncWriteCount > 0;
}

bool UVirtualSensorMonitorWidget::SaveLidarPointCloudToDisk(const FString& FramePrefix)
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
    TWeakObjectPtr<UVirtualSensorMonitorWidget> WeakThis(this);
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

bool UVirtualSensorMonitorWidget::RefreshLidarPreviewWithoutTransport()
{
    if (!LidarComp)
    {
        return false;
    }

    const EVirtualLidarOutputMode SavedOutputMode = LidarComp->OutputMode;
    UVirtualSensorDataTransportComp* SavedTransport = LidarComp->TransportComponent;
    UVirtualSensorRecorderComp* SavedRecorder = LidarComp->RecorderComponent;
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

FString UVirtualSensorMonitorWidget::EnsureLocalCaptureSessionDirectory()
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

UVirtualLidarSensorComp* UVirtualSensorMonitorWidget::GetTargetLidarForPreview() const
{
    if (SensorManager && SensorManager->GetSelectedLidar())
    {
        return SensorManager->GetSelectedLidar();
    }
    return LidarComp;
}
