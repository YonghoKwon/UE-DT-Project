#include "VirtualSensorMonitorWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"
#include "m7at10_dt/M7AT10/Sensor/VirtualLidarSensorComp.h"

void UVirtualSensorMonitorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ToggleButton)
    {
        ToggleButton->OnClicked.RemoveDynamic(this, &UVirtualSensorMonitorWidget::HandleToggleButtonClicked);
        ToggleButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleToggleButtonClicked);
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
    ToggleView();
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
        TitleText->SetText(FText::FromString(bShowingLidar ? TEXT("Virtual LIDAR View") : TEXT("Virtual Camera View")));
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
        Text = FString::Printf(TEXT("Sensor: %s\nFrame: %lld\nPoints: %d\nHits: %d\nPayload: %d"),
            *Status.SensorId,
            Status.FrameId,
            Status.TotalPointCount,
            Status.HitPointCount,
            Status.LastPayloadLength);
    }
    else if (!bShowingLidar && CameraComp)
    {
        Text = FString::Printf(TEXT("Sensor: %s\nMode: Camera\nRenderTarget: %s"),
            *CameraComp->SensorId,
            CameraComp->GetCameraRenderTarget() ? TEXT("Ready") : TEXT("None"));
    }
    else
    {
        Text = bShowingLidar ? TEXT("LIDAR sensor is not bound") : TEXT("Camera sensor is not bound");
    }

    StatusText->SetText(FText::FromString(Text));
}
