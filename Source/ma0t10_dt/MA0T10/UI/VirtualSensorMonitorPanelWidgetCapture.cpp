#include "VirtualSensorMonitorPanelWidget.h"
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


FString BuildPointCloudTimestamp()
{
    const FDateTime NowUtc = FDateTime::UtcNow();
    return FString::Printf(TEXT("%s_%03d_%lld"), *NowUtc.ToString(TEXT("%Y%m%d_%H%M%S")), NowUtc.GetMillisecond(), NowUtc.GetTicks());
}

FString BuildPointCloudExportDirectory(const UVirtualLidarScanComponent* LidarComp)
{
    const FString SensorId = LidarComp ? LidarComp->SensorId : TEXT("LIDAR");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), VirtualSensorDefaults::CaptureDirectory, SensorId, VirtualSensorDefaults::PointCloudSubdirectory);
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

FString BuildServerPayloadExportDirectory(const UVirtualLidarScanComponent* LidarComp)
{
    const FString SensorId = LidarComp ? LidarComp->SensorId : TEXT("LIDAR");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), VirtualSensorDefaults::CaptureDirectory, SensorId, VirtualSensorDefaults::ServerPayloadSubdirectory);
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

FString BuildCameraPayloadExportDirectory(const UVirtualCameraCaptureComponent* CameraComp)
{
    const FString SensorId = CameraComp ? CameraComp->SensorId : TEXT("CAMERA");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), VirtualSensorDefaults::CaptureDirectory, SensorId, VirtualSensorDefaults::ServerPayloadSubdirectory);
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

void UVirtualSensorMonitorPanelWidget::PumpPendingCaptureWork()
{
    ProcessPendingCameraReadbacks();
	if (bConfiguredOneShotPending)
	{
		const int64 CurrentFrameId = bConfiguredOneShotLidar
			? (LidarComp ? LidarComp->GetRuntimeStatus().FrameId : INDEX_NONE)
			: (CameraComp ? CameraComp->GetRuntimeStatus().FrameId : INDEX_NONE);
		const bool bNewFrameReady = CurrentFrameId > ConfiguredOneShotStartFrameId;
		const bool bTimedOut = FPlatformTime::Seconds() - ConfiguredOneShotStartedSeconds >= 2.0;
		if (bNewFrameReady || bTimedOut)
		{
			bConfiguredOneShotPending = false;
			CaptureConfiguredFrame();
		}
	}

}

void UVirtualSensorMonitorPanelWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    bPanningLidarProjection = false;
    bRotatingLidarProjection = false;
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UVirtualSensorMonitorPanelWidget::ConfigureLocalCapture(const FVirtualSensorCaptureSelection& Selection)
{
	LocalCaptureSelection = Selection;
	LocalCaptureSelection.IntervalSeconds = FMath::Clamp(Selection.IntervalSeconds, 0.05f, 3600.0f);
	LocalCaptureIntervalSeconds = LocalCaptureSelection.IntervalSeconds;
	bLocalCaptureSaveCameraFrames = LocalCaptureSelection.bCameraImage;
	bLocalCaptureSaveCameraPayload = LocalCaptureSelection.bCameraPayload;
	bLocalCaptureSaveLidarPayload = LocalCaptureSelection.bLidarPayload;
	bLocalCaptureSaveLidarPointCloud = LocalCaptureSelection.bPointCloud;
	bLocalCaptureSaveLidarCsv = LocalCaptureSelection.PointCloudFormat == EVirtualSensorExportKind::PointCloudCsv;
	bLocalCaptureSaveLidarJsonLines = LocalCaptureSelection.PointCloudFormat == EVirtualSensorExportKind::PointCloudJsonLines;
	bLocalCaptureSaveLidarPcd = LocalCaptureSelection.PointCloudFormat == EVirtualSensorExportKind::PointCloudPcd;
	bLocalCaptureSaveLidarLas = LocalCaptureSelection.PointCloudFormat == EVirtualSensorExportKind::PointCloudLas;
	bLocalCaptureSaveLidarLaz = LocalCaptureSelection.PointCloudFormat == EVirtualSensorExportKind::PointCloudLaz;

	if (bLocalSensorCaptureActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(LocalSensorCaptureTimerHandle, this, &UVirtualSensorMonitorPanelWidget::CaptureLocalSensorFrame, LocalCaptureIntervalSeconds, true);
		}
	}
}

void UVirtualSensorMonitorPanelWidget::CaptureConfiguredOutputsOnce()
{
	bConfiguredOneShotLidar = bShowingLidar;
	ConfiguredOneShotStartFrameId = bConfiguredOneShotLidar
		? (LidarComp ? LidarComp->GetRuntimeStatus().FrameId : INDEX_NONE)
		: (CameraComp ? CameraComp->GetRuntimeStatus().FrameId : INDEX_NONE);
	ConfiguredOneShotStartedSeconds = FPlatformTime::Seconds();
	bConfiguredOneShotPending = true;
	if (bConfiguredOneShotLidar)
	{
		if (LidarComp) LidarComp->RequestImmediateScheduledScan();
	}
	else if (CameraComp)
	{
		CameraComp->RequestImmediateScheduledCapture();
	}
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
        RefreshLidarPreviewWithoutTransport(true);
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

void UVirtualSensorMonitorPanelWidget::RefreshLocalCaptureButtonText()
{
    if (LocalSensorCaptureButtonText)
    {
        LocalSensorCaptureButtonText->SetText(FText::FromString(bLocalSensorCaptureActive ? TEXT("시간 캡처 중지") : TEXT("시간 캡처 시작")));
    }
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
	CaptureConfiguredFrame();
}

void UVirtualSensorMonitorPanelWidget::CaptureConfiguredFrame()
{
    ++LocalCaptureFrameIndex;
    const FString FramePrefix = FString::Printf(TEXT("frame_%06d_%s"), LocalCaptureFrameIndex, *BuildPointCloudTimestamp());
	const bool bCaptureLidar = bShowingLidar;
    const bool bCameraQueued = !bCaptureLidar && bLocalCaptureSaveCameraFrames && SaveCameraSnapshotToDisk(FramePrefix);
    const bool bLidarQueued = bCaptureLidar && bLocalCaptureSaveLidarPointCloud && SaveLidarPointCloudToDisk(FramePrefix);
	if ((!bCaptureLidar && bLocalCaptureSaveCameraPayload) || (bCaptureLidar && bLocalCaptureSaveLidarPayload))
	{
		ExportSelectedSensorServerPayload(FramePrefix + TEXT("_payload"));
	}
    UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Local capture frame=%d camera=%s lidar=%s folder=%s"), LocalCaptureFrameIndex, bCameraQueued ? TEXT("queued") : TEXT("skipped"), bLidarQueued ? TEXT("queued") : TEXT("skipped"), *LocalCaptureSessionDirectory);
}

bool UVirtualSensorMonitorPanelWidget::SaveCameraSnapshotToDisk(const FString& FramePrefix)
{
    return bUseGpuAsyncCameraReadback ? QueueCameraGpuReadbackToDisk(FramePrefix) : SaveCameraSnapshotToDiskSynchronous(FramePrefix);
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
    if (!bLocalCaptureSaveLidarCsv && !bLocalCaptureSaveLidarJsonLines && !bLocalCaptureSaveLidarPcd && !bLocalCaptureSaveLidarLas && !bLocalCaptureSaveLidarLaz)
    {
        return false;
    }
    if (!bLocalCaptureUseCachedSensorFrames || LidarComp->GetLastPoints().Num() <= 0)
    {
        RefreshLidarPreviewWithoutTransport(true);
    }

    const TSharedPtr<const FVirtualLidarFrameSnapshot, ESPMode::ThreadSafe> Snapshot = LidarComp->GetLastFrameSnapshot();
    if (!Snapshot.IsValid() || !Snapshot->Points.IsValid() || Snapshot->Points->IsEmpty())
    {
        return false;
    }

	FVirtualSensorFrameEnvelope Frame;
	Frame.SensorId = LidarComp->SensorId;
	Frame.SensorKind = EVirtualSensorKind::Lidar;
	Frame.FrameId = Snapshot->FrameId;
	Frame.TimestampUtc = FDateTime::UtcNow();
	Frame.PointSnapshot = Snapshot->Points;
	FVirtualSensorStreamConfig Config;
	Config.StreamKind = EVirtualSensorStreamKind::PointCloud;
	Config.PointCloudFormat = bLocalCaptureSaveLidarPcd ? EVirtualPointCloudStreamFormat::PCD
		: bLocalCaptureSaveLidarJsonLines ? EVirtualPointCloudStreamFormat::JSONL
		: bLocalCaptureSaveLidarLas ? EVirtualPointCloudStreamFormat::LAS
		: bLocalCaptureSaveLidarLaz ? EVirtualPointCloudStreamFormat::LAZ
		: EVirtualPointCloudStreamFormat::CSV;
	Config.LazCompressorPath = LidarComp->bUseExternalLazCompressor ? LidarComp->ExternalLazCompressorPath : FString();
	Config.LazCompressorArguments = LidarComp->ExternalLazCompressorArguments;
    const FString LidarDirectory = FPaths::Combine(EnsureLocalCaptureSessionDirectory(), TEXT("Lidar"));
    IFileManager::Get().MakeDirectory(*LidarDirectory, true);
	const FString SensorId = LidarComp->SensorId;
    TWeakObjectPtr<UVirtualSensorMonitorPanelWidget> WeakThis(this);
    bLocalCaptureLidarWritePending = true;

    Async(EAsyncExecution::ThreadPool, [WeakThis, Frame = MoveTemp(Frame), Config, LidarDirectory, FramePrefix, SensorId]() mutable
    {
		FString Extension;
		FString Error;
		TArray<uint8> Bytes;
		int32 PointCount = 0;
		const bool bSerialized = UVirtualSensorStreamPublisherComponent::SerializePointCloudFrame(Frame, Config, Extension, Bytes, PointCount, Error);
		const FString Path = bSerialized ? FPaths::Combine(LidarDirectory, FString::Printf(TEXT("%s_%s.%s"), *FramePrefix, *SensorId, *Extension)) : FString();
		const bool bSaved = bSerialized && FFileHelper::SaveArrayToFile(Bytes, *Path);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Path, Error = MoveTemp(Error), bSaved, PointCount]()
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            WeakThis->bLocalCaptureLidarWritePending = false;
            if (bSaved)
            {
				UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Async LiDAR capture saved: %s points=%d"), *Path, PointCount);
            }
            else
            {
				UE_LOG(LogTemp, Warning, TEXT("[SensorMonitor] Async LiDAR capture failed: %s %s"), *Path, *Error);
            }
        });
    });
    return true;
}

FString UVirtualSensorMonitorPanelWidget::EnsureLocalCaptureSessionDirectory()
{
    if (!LocalCaptureSessionDirectory.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*LocalCaptureSessionDirectory, true);
        return LocalCaptureSessionDirectory;
    }

    LocalCaptureSessionDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), VirtualSensorDefaults::CaptureDirectory, LocalCaptureFolderName, BuildPointCloudTimestamp());
    IFileManager::Get().MakeDirectory(*LocalCaptureSessionDirectory, true);
    return LocalCaptureSessionDirectory;
}
#undef LOCTEXT_NAMESPACE
