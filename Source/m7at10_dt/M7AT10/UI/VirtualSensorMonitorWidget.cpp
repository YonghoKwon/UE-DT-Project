#include "VirtualSensorMonitorWidget.h"

#include "Async/Async.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
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
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"

namespace
{
struct FLocalLidarCsvPoint { int32 Row = 0; int32 Col = 0; FVector Point = FVector::ZeroVector; };

FString BuildPointCloudTimestamp()
{
    const FDateTime NowUtc = FDateTime::UtcNow();
    return FString::Printf(TEXT("%s_%lld"), *NowUtc.ToString(TEXT("%Y%m%d_%H%M%S")), NowUtc.GetTicks());
}

FString BuildPointCloudExportDirectory(const UVirtualLidarSensorComp* LidarComp)
{
    const FString SensorId = LidarComp ? LidarComp->SensorId : TEXT("LIDAR");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

FString BuildRowColCsvText(const UVirtualLidarSensorComp* LidarComp, int32& OutExportedPointCount)
{
    OutExportedPointCount = 0;
    FString Text;
    Text.Reserve(LidarComp ? FMath::Max(128, LidarComp->GetLastPoints().Num() * 64) : 128);
    Text += TEXT("row,col,x,y,z\n");
    if (!LidarComp) return Text;

    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    const int32 SafeHorizontalSamples = FMath::Max(1, LidarComp->HorizontalSamples);
    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (LidarComp->bExportHitOnlyPointCloud && !Point.bHit) continue;
        Text += FString::Printf(TEXT("%d,%d,%f,%f,%f\n"), PointIndex / SafeHorizontalSamples, PointIndex % SafeHorizontalSamples, Point.WorldLocation.X, Point.WorldLocation.Y, Point.WorldLocation.Z);
        ++OutExportedPointCount;
    }
    return Text;
}

bool ExportRowColPointCloudCsv(const UVirtualLidarSensorComp* LidarComp, const FString& FileNamePrefix)
{
    if (!LidarComp) return false;
    int32 ExportedPointCount = 0;
    const FString Text = BuildRowColCsvText(LidarComp, ExportedPointCount);
    const FString Prefix = FileNamePrefix.IsEmpty() ? LidarComp->SensorId : FileNamePrefix;
    const FString Path = FPaths::Combine(BuildPointCloudExportDirectory(LidarComp), FString::Printf(TEXT("%s_%s.csv"), *Prefix, *BuildPointCloudTimestamp()));
    const bool bSaved = FFileHelper::SaveStringToFile(Text, *Path);
    if (bSaved) UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] row/col CSV export saved: %s points=%d"), *LidarComp->SensorId, *Path, ExportedPointCount);
    else UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] row/col CSV export failed: %s points=%d"), *LidarComp->SensorId, *Path, ExportedPointCount);
    return bSaved;
}
}

void UVirtualSensorMonitorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (ToggleButton) { ToggleButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleToggleButtonClicked); ToggleButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleToggleButtonClicked); }
    if (NextCameraButton) { NextCameraButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleNextCameraButtonClicked); NextCameraButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleNextCameraButtonClicked); }
    if (NextLidarButton) { NextLidarButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleNextLidarButtonClicked); NextLidarButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleNextLidarButtonClicked); }
    if (PointCloudOnlyButton) { PointCloudOnlyButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandlePointCloudOnlyButtonClicked); PointCloudOnlyButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandlePointCloudOnlyButtonClicked); }
    if (LogPointCloudButton) { LogPointCloudButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleLogPointCloudButtonClicked); LogPointCloudButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleLogPointCloudButtonClicked); }
    if (ExportPointCloudButton) { ExportPointCloudButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleExportPointCloudButtonClicked); ExportPointCloudButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleExportPointCloudButtonClicked); }
    if (LocalSensorCaptureButton) { LocalSensorCaptureButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleLocalSensorCaptureButtonClicked); LocalSensorCaptureButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleLocalSensorCaptureButtonClicked); }
    RefreshTitle(); RefreshImageBrush(); RefreshStatusText(); RefreshLocalCaptureButtonText();
}

void UVirtualSensorMonitorWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(LocalSensorCaptureTimerHandle);
    bLocalSensorCaptureActive = false;
    PendingCameraReadbacks.Reset();
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

void UVirtualSensorMonitorWidget::BindVirtualCamera(UVirtualCameraComp* InCameraComp) { CameraComp = InCameraComp; RefreshImageBrush(); RefreshStatusText(); }
void UVirtualSensorMonitorWidget::BindVirtualLidar(UVirtualLidarSensorComp* InLidarComp) { LidarComp = InLidarComp; if (bShowingLidar && LidarComp && !LidarComp->GetLidarViewTexture()) RefreshLidarPreviewWithoutTransport(); RefreshImageBrush(); RefreshStatusText(); }
void UVirtualSensorMonitorWidget::BindSensorManager(AVirtualSensorManager* InSensorManager) { SensorManager = InSensorManager; if (SensorManager) SensorManager->BindMonitorWidget(this); RefreshStatusText(); }
void UVirtualSensorMonitorWidget::ShowCameraView() { bShowingLidar = false; RefreshTitle(); RefreshImageBrush(); RefreshStatusText(); }
void UVirtualSensorMonitorWidget::ShowLidarView() { bShowingLidar = true; if (LidarComp && !LidarComp->GetLidarViewTexture()) RefreshLidarPreviewWithoutTransport(); RefreshTitle(); RefreshImageBrush(); RefreshStatusText(); }
void UVirtualSensorMonitorWidget::ToggleView() { bShowingLidar = !bShowingLidar; if (bShowingLidar && LidarComp && !LidarComp->GetLidarViewTexture()) RefreshLidarPreviewWithoutTransport(); RefreshTitle(); RefreshImageBrush(); RefreshStatusText(); }

void UVirtualSensorMonitorWidget::ToggleLocalSensorCapture()
{
    UWorld* World = GetWorld(); if (!World) return;
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
        UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Local timed capture started. Interval=%.3fs Session=%s GpuReadback=%s MaxReadbacks=%d"), LocalCaptureIntervalSeconds, *LocalCaptureSessionDirectory, bUseGpuAsyncCameraReadback ? TEXT("true") : TEXT("false"), MaxPendingCameraReadbacks);
    }
    RefreshLocalCaptureButtonText(); RefreshStatusText();
}

void UVirtualSensorMonitorWidget::HandleToggleButtonClicked() { if (SensorManager) { SensorManager->ToggleSensorView(); return; } ToggleView(); }
void UVirtualSensorMonitorWidget::HandleNextCameraButtonClicked() { if (SensorManager) { SensorManager->SelectNextCamera(); SensorManager->SetViewMode(EVirtualSensorViewMode::Camera); } }
void UVirtualSensorMonitorWidget::HandleNextLidarButtonClicked() { if (SensorManager) { SensorManager->SelectNextLidar(); SensorManager->SetViewMode(EVirtualSensorViewMode::Lidar); } }
void UVirtualSensorMonitorWidget::HandlePointCloudOnlyButtonClicked() { if (SensorManager) SensorManager->TogglePointCloudOnlyView(); }
void UVirtualSensorMonitorWidget::HandleLocalSensorCaptureButtonClicked() { ToggleLocalSensorCapture(); }

void UVirtualSensorMonitorWidget::HandleLogPointCloudButtonClicked()
{
    if (!LidarComp) return;
    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    const int32 SafeHorizontalSamples = FMath::Max(1, LidarComp->HorizontalSamples);
    int32 Logged = 0, CandidateCount = 0;
    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] RowCol PointCloud log start. format=row,col,x,y,z"), *LidarComp->SensorId);
    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (LidarComp->bExportHitOnlyPointCloud && !Point.bHit) continue;
        ++CandidateCount;
        if (Logged++ >= 200) continue;
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] row=%d col=%d x=%.3f y=%.3f z=%.3f"), *LidarComp->SensorId, PointIndex / SafeHorizontalSamples, PointIndex % SafeHorizontalSamples, Point.WorldLocation.X, Point.WorldLocation.Y, Point.WorldLocation.Z);
    }
    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] RowCol PointCloud log complete. candidates=%d"), *LidarComp->SensorId, CandidateCount);
}

void UVirtualSensorMonitorWidget::HandleExportPointCloudButtonClicked()
{
    if (!LidarComp) return;
    ExportRowColPointCloudCsv(LidarComp, TEXT("button_export"));
    LidarComp->ExportLastPointCloudLas(TEXT("button_export"));
    LidarComp->ExportLastPointCloudLaz(TEXT("button_export"));
}

void UVirtualSensorMonitorWidget::RefreshImageBrush()
{
    if (!ViewImage) return;
    UObject* Resource = nullptr;
    if (bShowingLidar)
    {
        if (LidarComp && !LidarComp->GetLidarViewTexture()) RefreshLidarPreviewWithoutTransport();
        Resource = LidarComp ? LidarComp->GetLidarViewTexture() : nullptr;
    }
    else Resource = CameraComp ? CameraComp->GetCameraRenderTarget() : nullptr;
    if (!Resource) return;
    FSlateBrush Brush; Brush.SetResourceObject(Resource); Brush.ImageSize = FVector2D(640.0f, 360.0f); ViewImage->SetBrush(Brush);
}

void UVirtualSensorMonitorWidget::RefreshTitle()
{
    if (TitleText)
    {
        const bool bPointCloudOnly = SensorManager && SensorManager->IsPointCloudOnlyModeEnabled();
        TitleText->SetText(FText::FromString(bPointCloudOnly ? TEXT("LiDAR Point Cloud Only") : (bShowingLidar ? TEXT("Virtual LIDAR View") : TEXT("Virtual Camera View"))));
    }
    if (ToggleButtonText) ToggleButtonText->SetText(FText::FromString(bShowingLidar ? TEXT("Show Camera View") : TEXT("Show LIDAR View")));
    RefreshLocalCaptureButtonText();
}

void UVirtualSensorMonitorWidget::RefreshStatusText()
{
    if (!StatusText) return;
    FString Text;
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        Text = FString::Printf(TEXT("Sensor: %s\nFrame: %lld\nPoints: %d\nHits: %d\nPointCloudPreview: %s\nCSV Format: row,col,x,y,z\nMessage: %s"), *Status.SensorId, Status.FrameId, Status.TotalPointCount, Status.HitPointCount, LidarComp->IsPointCloudPreviewEnabled() ? TEXT("On") : TEXT("Off"), *Status.LastMessage);
    }
    else if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        Text = FString::Printf(TEXT("Sensor: %s\nFrame: %lld\nRenderTarget: %s\nMessage: %s"), *Status.SensorId, Status.FrameId, CameraComp->GetCameraRenderTarget() ? TEXT("Ready") : TEXT("None"), *Status.LastMessage);
    }
    else Text = bShowingLidar ? TEXT("LIDAR sensor is not bound") : TEXT("Camera sensor is not bound");

    if (SensorManager) Text += FString::Printf(TEXT("\n\nHealth: %s"), *SensorManager->GetHealthSummary().Summary);
    if (bLocalSensorCaptureActive || !LocalCaptureSessionDirectory.IsEmpty())
    {
        Text += FString::Printf(TEXT("\n\nLocal Capture: %s\nInterval: %.3fs\nFrames: %d\nCamera Pending=%s GPUReadback=%s Queue=%d/%d\nLidar Pending=%s\nFolder: %s"), bLocalSensorCaptureActive ? TEXT("Recording") : TEXT("Stopped"), LocalCaptureIntervalSeconds, LocalCaptureFrameIndex, bLocalCaptureCameraWritePending ? TEXT("true") : TEXT("false"), bUseGpuAsyncCameraReadback ? TEXT("On") : TEXT("Off"), PendingCameraReadbacks.Num(), MaxPendingCameraReadbacks, bLocalCaptureLidarWritePending ? TEXT("true") : TEXT("false"), *LocalCaptureSessionDirectory);
    }
    StatusText->SetText(FText::FromString(Text));
}

void UVirtualSensorMonitorWidget::RefreshLocalCaptureButtonText() { if (LocalSensorCaptureButtonText) LocalSensorCaptureButtonText->SetText(FText::FromString(bLocalSensorCaptureActive ? TEXT("Stop Local Capture") : TEXT("Start Local Capture"))); }

void UVirtualSensorMonitorWidget::CaptureLocalSensorFrame()
{
    if (!bLocalSensorCaptureActive) return;
    if (bSkipLocalCaptureWhenWritePending && (bLocalCaptureCameraWritePending || bLocalCaptureLidarWritePending)) return;
    ++LocalCaptureFrameIndex;
    const FString FramePrefix = FString::Printf(TEXT("frame_%06d"), LocalCaptureFrameIndex);
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
    if (!CameraComp || PendingCameraReadbacks.Num() >= FMath::Max(1, MaxPendingCameraReadbacks)) return false;
    UTextureRenderTarget2D* RenderTarget = CameraComp->GetCameraRenderTarget();
    if (!RenderTarget) return false;
    if (!bLocalCaptureUseCachedSensorFrames) CameraComp->CaptureScene();
    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource) return false;
    FTextureRHIRef Texture = Resource->GetRenderTargetTexture();
    if (!Texture.IsValid()) return false;

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
        if (Readback.IsValid() && Texture.IsValid()) Readback->EnqueueCopy(RHICmdList, Texture);
    });
    PendingCameraReadbacks.Add(MoveTemp(Pending));
    bLocalCaptureCameraWritePending = true;
    return true;
}

bool UVirtualSensorMonitorWidget::SaveCameraSnapshotToDiskSynchronous(const FString& FramePrefix)
{
    if (!CameraComp || bLocalCaptureCameraWritePending) return false;
    UTextureRenderTarget2D* RenderTarget = CameraComp->GetCameraRenderTarget();
    if (!RenderTarget) return false;
    if (!bLocalCaptureUseCachedSensorFrames) CameraComp->CaptureScene();
    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource) return false;
    TArray<FColor> RawPixels;
    if (!Resource->ReadPixels(RawPixels) || RawPixels.Num() == 0) return false;
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
        if (!Pending.Readback.IsValid()) { PendingCameraReadbacks.RemoveAtSwap(Index); continue; }
        if (!Pending.Readback->IsReady()) continue;
        int32 RowPitchInPixels = 0;
        void* LockedData = Pending.Readback->Lock(RowPitchInPixels);
        if (!LockedData || RowPitchInPixels <= 0)
        {
            Pending.Readback->Unlock();
            PendingCameraReadbacks.RemoveAtSwap(Index);
            continue;
        }
        TArray<FColor> RawPixels; RawPixels.SetNumUninitialized(Pending.Width * Pending.Height);
        const FColor* SourcePixels = static_cast<const FColor*>(LockedData);
        for (int32 Y = 0; Y < Pending.Height; ++Y) FMemory::Memcpy(RawPixels.GetData() + Y * Pending.Width, SourcePixels + Y * RowPitchInPixels, Pending.Width * sizeof(FColor));
        Pending.Readback->Unlock();
        const FString Path = Pending.OutputPath; const int32 Width = Pending.Width; const int32 Height = Pending.Height;
        PendingCameraReadbacks.RemoveAtSwap(Index);
        StartAsyncCameraJpegWrite(MoveTemp(RawPixels), Width, Height, Path);
    }
    if (PendingCameraReadbacks.Num() == 0 && !bLocalCaptureCameraWritePending) bLocalCaptureCameraWritePending = false;
}

void UVirtualSensorMonitorWidget::StartAsyncCameraJpegWrite(TArray<FColor>&& RawPixels, int32 Width, int32 Height, const FString& Path)
{
    IImageWrapperModule* ImageWrapperModule = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TWeakObjectPtr<UVirtualSensorMonitorWidget> WeakThis(this);
    bLocalCaptureCameraWritePending = true;
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
                    TArray<uint8> Bytes32; Bytes32.Append(Bytes64.GetData(), static_cast<int32>(Bytes64.Num()));
                    bSaved = FFileHelper::SaveArrayToFile(Bytes32, *Path);
                }
            }
        }
        AsyncTask(ENamedThreads::GameThread, [WeakThis, Path, bSaved]()
        {
            if (!WeakThis.IsValid()) return;
            WeakThis->bLocalCaptureCameraWritePending = WeakThis->PendingCameraReadbacks.Num() > 0;
            if (bSaved) UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Async camera save saved: %s"), *Path);
            else UE_LOG(LogTemp, Warning, TEXT("[SensorMonitor] Async camera save failed: %s"), *Path);
        });
    });
}

bool UVirtualSensorMonitorWidget::SaveLidarPointCloudToDisk(const FString& FramePrefix)
{
    if (!LidarComp || bLocalCaptureLidarWritePending) return false;
    if (!bLocalCaptureUseCachedSensorFrames || LidarComp->GetLastPoints().Num() <= 0) RefreshLidarPreviewWithoutTransport();
    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    if (Points.Num() <= 0) return false;

    const int32 SafeHorizontalSamples = FMath::Max(1, LidarComp->HorizontalSamples);
    TArray<FLocalLidarCsvPoint> CsvPoints; CsvPoints.Reserve(Points.Num());
    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (LidarComp->bExportHitOnlyPointCloud && !Point.bHit) continue;
        FLocalLidarCsvPoint CsvPoint; CsvPoint.Row = PointIndex / SafeHorizontalSamples; CsvPoint.Col = PointIndex % SafeHorizontalSamples; CsvPoint.Point = Point.WorldLocation; CsvPoints.Add(CsvPoint);
    }
    if (CsvPoints.Num() <= 0) return false;

    const FString LidarDirectory = FPaths::Combine(EnsureLocalCaptureSessionDirectory(), TEXT("Lidar"));
    IFileManager::Get().MakeDirectory(*LidarDirectory, true);
    const FString Path = FPaths::Combine(LidarDirectory, FString::Printf(TEXT("%s_%s.csv"), *FramePrefix, *LidarComp->SensorId));
    TWeakObjectPtr<UVirtualSensorMonitorWidget> WeakThis(this);
    bLocalCaptureLidarWritePending = true;
    Async(EAsyncExecution::ThreadPool, [WeakThis, CsvPoints = MoveTemp(CsvPoints), Path]() mutable
    {
        FString Text; Text.Reserve(FMath::Max(128, CsvPoints.Num() * 64)); Text += TEXT("row,col,x,y,z\n");
        for (const FLocalLidarCsvPoint& CsvPoint : CsvPoints) Text += FString::Printf(TEXT("%d,%d,%f,%f,%f\n"), CsvPoint.Row, CsvPoint.Col, CsvPoint.Point.X, CsvPoint.Point.Y, CsvPoint.Point.Z);
        const bool bSaved = FFileHelper::SaveStringToFile(Text, *Path);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, Path, bSaved]()
        {
            if (!WeakThis.IsValid()) return;
            WeakThis->bLocalCaptureLidarWritePending = false;
            if (bSaved) UE_LOG(LogTemp, Log, TEXT("[SensorMonitor] Async lidar CSV save saved: %s"), *Path);
            else UE_LOG(LogTemp, Warning, TEXT("[SensorMonitor] Async lidar CSV save failed: %s"), *Path);
        });
    });
    return true;
}

bool UVirtualSensorMonitorWidget::RefreshLidarPreviewWithoutTransport()
{
    if (!LidarComp) return false;
    const EVirtualLidarOutputMode SavedOutputMode = LidarComp->OutputMode;
    UVirtualSensorDataTransportComp* SavedTransport = LidarComp->TransportComponent;
    UVirtualSensorRecorderComp* SavedRecorder = LidarComp->RecorderComponent;
    const bool bSavedExportCsv = LidarComp->bExportCsvOnScan;
    const bool bSavedExportJsonLines = LidarComp->bExportJsonLinesOnScan;
    const bool bSavedExportPcd = LidarComp->bExportPcdOnScan;
    LidarComp->OutputMode = EVirtualLidarOutputMode::None; LidarComp->TransportComponent = nullptr; LidarComp->RecorderComponent = nullptr; LidarComp->bExportCsvOnScan = false; LidarComp->bExportJsonLinesOnScan = false; LidarComp->bExportPcdOnScan = false;
    LidarComp->ScanAndSend();
    LidarComp->OutputMode = SavedOutputMode; LidarComp->TransportComponent = SavedTransport; LidarComp->RecorderComponent = SavedRecorder; LidarComp->bExportCsvOnScan = bSavedExportCsv; LidarComp->bExportJsonLinesOnScan = bSavedExportJsonLines; LidarComp->bExportPcdOnScan = bSavedExportPcd;
    return LidarComp->GetLidarViewTexture() != nullptr;
}

FString UVirtualSensorMonitorWidget::EnsureLocalCaptureSessionDirectory()
{
    if (!LocalCaptureSessionDirectory.IsEmpty()) { IFileManager::Get().MakeDirectory(*LocalCaptureSessionDirectory, true); return LocalCaptureSessionDirectory; }
    LocalCaptureSessionDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), LocalCaptureFolderName, BuildPointCloudTimestamp());
    IFileManager::Get().MakeDirectory(*LocalCaptureSessionDirectory, true);
    return LocalCaptureSessionDirectory;
}
