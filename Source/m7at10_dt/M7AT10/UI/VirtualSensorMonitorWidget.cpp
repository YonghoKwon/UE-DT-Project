#include "VirtualSensorMonitorWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualSensorManager.h"

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
        Text = FString::Printf(TEXT("Sensor: %s\nDevice: %s %s\nFrame: %lld\nPoints: %d\nHits: %d\nPayload: %d\nPointCloudPreview: %s\nMessage: %s"),
            *Status.SensorId,
            *LidarComp->GetDeviceSpec().Manufacturer,
            *LidarComp->GetDeviceSpec().Model,
            Status.FrameId,
            Status.TotalPointCount,
            Status.HitPointCount,
            Status.LastPayloadLength,
            LidarComp->IsPointCloudPreviewEnabled() ? TEXT("On") : TEXT("Off"),
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
