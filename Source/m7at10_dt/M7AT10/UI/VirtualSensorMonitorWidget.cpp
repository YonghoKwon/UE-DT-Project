#include "VirtualSensorMonitorWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"

namespace
{
FString BuildPointCloudExportDirectory(const UVirtualLidarSensorComp* LidarComp)
{
    const FString SensorId = LidarComp ? LidarComp->SensorId : TEXT("LIDAR");
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures"), SensorId, TEXT("PointCloud"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    return Directory;
}

FString BuildPointCloudTimestamp()
{
    const FDateTime NowUtc = FDateTime::UtcNow();
    return FString::Printf(TEXT("%s_%lld"), *NowUtc.ToString(TEXT("%Y%m%d_%H%M%S")), NowUtc.GetTicks());
}

FString BuildRowColCsvText(const UVirtualLidarSensorComp* LidarComp, int32& OutExportedPointCount)
{
    OutExportedPointCount = 0;
    FString Text = TEXT("row,col,x,y,z\n");

    if (!LidarComp)
    {
        return Text;
    }

    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    const int32 SafeHorizontalSamples = FMath::Max(1, LidarComp->HorizontalSamples);

    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (LidarComp->bExportHitOnlyPointCloud && !Point.bHit)
        {
            continue;
        }

        const int32 Row = PointIndex / SafeHorizontalSamples;
        const int32 Col = PointIndex % SafeHorizontalSamples;
        Text += FString::Printf(TEXT("%d,%d,%f,%f,%f\n"),
            Row,
            Col,
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
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar] row/col CSV export failed: LidarComp is null."));
        return false;
    }

    int32 ExportedPointCount = 0;
    const FString Text = BuildRowColCsvText(LidarComp, ExportedPointCount);
    const FString Directory = BuildPointCloudExportDirectory(LidarComp);
    const FString Prefix = FileNamePrefix.IsEmpty() ? LidarComp->SensorId : FileNamePrefix;
    const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s.csv"), *Prefix, *BuildPointCloudTimestamp()));
    const bool bSaved = FFileHelper::SaveStringToFile(Text, *Path);

    if (bSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("[VirtualLidar:%s] row/col CSV export saved: %s points=%d format=row,col,x,y,z"), *LidarComp->SensorId, *Path, ExportedPointCount);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] row/col CSV export failed: %s points=%d"), *LidarComp->SensorId, *Path, ExportedPointCount);
    }

    return bSaved;
}
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

    RefreshTitle();
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
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
    RefreshImageBrush();
    RefreshStatusText();
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
    RefreshTitle();
    RefreshImageBrush();
    RefreshStatusText();
}

void UVirtualSensorMonitorWidget::ToggleView()
{
    bShowingLidar = !bShowingLidar;
    RefreshTitle();
    RefreshImageBrush();
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

void UVirtualSensorMonitorWidget::HandleLogPointCloudButtonClicked()
{
    if (!LidarComp)
    {
        return;
    }

    const TArray<FVirtualLidarPoint>& Points = LidarComp->GetLastPoints();
    const int32 SafeHorizontalSamples = FMath::Max(1, LidarComp->HorizontalSamples);
    int32 Logged = 0;
    int32 CandidateCount = 0;
    const int32 MaxPointsToLog = 200;

    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] RowCol PointCloud log start. format=row,col,x,y,z hitOnly=%s maxLog=%d"),
        *LidarComp->SensorId,
        LidarComp->bExportHitOnlyPointCloud ? TEXT("true") : TEXT("false"),
        MaxPointsToLog);

    for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
    {
        const FVirtualLidarPoint& Point = Points[PointIndex];
        if (LidarComp->bExportHitOnlyPointCloud && !Point.bHit)
        {
            continue;
        }

        ++CandidateCount;
        if (Logged >= MaxPointsToLog)
        {
            continue;
        }

        const int32 Row = PointIndex / SafeHorizontalSamples;
        const int32 Col = PointIndex % SafeHorizontalSamples;
        UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] row=%d col=%d x=%.3f y=%.3f z=%.3f"),
            *LidarComp->SensorId,
            Row,
            Col,
            Point.WorldLocation.X,
            Point.WorldLocation.Y,
            Point.WorldLocation.Z);
        ++Logged;
    }

    UE_LOG(LogTemp, Warning, TEXT("[VirtualLidar:%s] RowCol PointCloud log complete. candidates=%d logged=%d"), *LidarComp->SensorId, CandidateCount, Logged);
}

void UVirtualSensorMonitorWidget::HandleExportPointCloudButtonClicked()
{
    if (LidarComp)
    {
        ExportRowColPointCloudCsv(LidarComp, TEXT("button_export"));
        LidarComp->ExportLastPointCloudLas(TEXT("button_export"));
        LidarComp->ExportLastPointCloudLaz(TEXT("button_export"));
    }
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
        Resource = LidarComp ? LidarComp->GetLidarViewTexture() : nullptr;
    }
    else
    {
        Resource = CameraComp ? CameraComp->GetCameraRenderTarget() : nullptr;
    }

    FSlateBrush Brush;
    Brush.SetResourceObject(Resource);
    Brush.ImageSize = FVector2D(640.0f, 360.0f);
    ViewImage->SetBrush(Brush);
}

void UVirtualSensorMonitorWidget::RefreshTitle()
{
    if (TitleText)
    {
        const bool bPointCloudOnly = SensorManager && SensorManager->IsPointCloudOnlyModeEnabled();
        TitleText->SetText(FText::FromString(bPointCloudOnly ? TEXT("LiDAR Point Cloud Only") : (bShowingLidar ? TEXT("Virtual LIDAR View") : TEXT("Virtual Camera View"))));
    }

    if (ToggleButtonText)
    {
        ToggleButtonText->SetText(FText::FromString(bShowingLidar ? TEXT("Show Camera View") : TEXT("Show LIDAR View")));
    }
}

void UVirtualSensorMonitorWidget::RefreshStatusText()
{
    if (!StatusText)
    {
        return;
    }

    FString Text;
    if (bShowingLidar && LidarComp)
    {
        const FVirtualSensorRuntimeStatus& Status = LidarComp->GetRuntimeStatus();
        Text = FString::Printf(TEXT("Sensor: %s\nDevice: %s %s\nFrame: %lld\nPoints: %d\nHits: %d\nPayload: %d\nPointCloudPreview: %s\nColor: %s\nExports: Saved/SensorCaptures/%s/PointCloud\nCSV Format: row,col,x,y,z\nMessage: %s"),
            *Status.SensorId,
            *LidarComp->GetDeviceSpec().Manufacturer,
            *LidarComp->GetDeviceSpec().Model,
            Status.FrameId,
            Status.TotalPointCount,
            Status.HitPointCount,
            Status.LastPayloadLength,
            LidarComp->IsPointCloudPreviewEnabled() ? TEXT("On") : TEXT("Off"),
            *LidarComp->PointCloudPreviewColor.ToString(),
            *LidarComp->SensorId,
            *Status.LastMessage);
    }
    else if (!bShowingLidar && CameraComp)
    {
        const FVirtualSensorRuntimeStatus& Status = CameraComp->GetRuntimeStatus();
        Text = FString::Printf(TEXT("Sensor: %s\nDevice: %s %s\nFrame: %lld\nPayload: %d\nMessage: %s\nRenderTarget: %s"),
            *Status.SensorId,
            *CameraComp->GetDeviceSpec().Manufacturer,
            *CameraComp->GetDeviceSpec().Model,
            Status.FrameId,
            Status.LastPayloadLength,
            *Status.LastMessage,
            CameraComp->GetCameraRenderTarget() ? TEXT("Ready") : TEXT("None"));
    }
    else
    {
        Text = bShowingLidar ? TEXT("LIDAR sensor is not bound") : TEXT("Camera sensor is not bound");
    }

    if (SensorManager)
    {
        const FVirtualSensorHealthSummary Health = SensorManager->GetHealthSummary();
        Text += FString::Printf(TEXT("\n\nHealth: %s"), *Health.Summary);
    }

    StatusText->SetText(FText::FromString(Text));
}
