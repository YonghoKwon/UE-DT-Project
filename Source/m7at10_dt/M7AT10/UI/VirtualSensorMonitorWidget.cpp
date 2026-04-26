#include "UI/VirtualSensorMonitorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/VirtualCameraComp.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Sensor/VirtualLidarSensorComp.h"

void UVirtualSensorMonitorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildWidgetTreeIfNeeded();
    RefreshTitle();
}

void UVirtualSensorMonitorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshImageBrush();
}

void UVirtualSensorMonitorWidget::BindVirtualCamera(UVirtualCameraComp* InCameraComp)
{
    CameraComp = InCameraComp;
    RefreshImageBrush();
}

void UVirtualSensorMonitorWidget::BindVirtualLidar(UVirtualLidarSensorComp* InLidarComp)
{
    LidarComp = InLidarComp;
    RefreshImageBrush();
}

void UVirtualSensorMonitorWidget::ShowCameraView()
{
    bShowingLidar = false;
    RefreshTitle();
    RefreshImageBrush();
}

void UVirtualSensorMonitorWidget::ShowLidarView()
{
    bShowingLidar = true;
    RefreshTitle();
    RefreshImageBrush();
}

void UVirtualSensorMonitorWidget::ToggleView()
{
    bShowingLidar = !bShowingLidar;
    RefreshTitle();
    RefreshImageBrush();
}

void UVirtualSensorMonitorWidget::HandleToggleButtonClicked()
{
    ToggleView();
}

void UVirtualSensorMonitorWidget::BuildWidgetTreeIfNeeded()
{
    if (!WidgetTree || ViewImage)
    {
        return;
    }

    UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SensorMonitorRoot"));
    WidgetTree->RootWidget = RootBox;

    TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SensorMonitorTitle"));
    TitleText->SetText(FText::FromString(TEXT("Virtual Camera View")));
    if (UVerticalBoxSlot* TitleSlot = RootBox->AddChildToVerticalBox(TitleText))
    {
        TitleSlot->SetPadding(FMargin(8.0f));
    }

    ViewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SensorMonitorImage"));
    if (UVerticalBoxSlot* ImageSlot = RootBox->AddChildToVerticalBox(ViewImage))
    {
        ImageSlot->SetPadding(FMargin(8.0f));
        ImageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    ToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SensorMonitorToggleButton"));
    ToggleButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SensorMonitorToggleButtonText"));
    ToggleButtonText->SetText(FText::FromString(TEXT("Show LIDAR View")));
    ToggleButton->SetContent(ToggleButtonText);
    ToggleButton->OnClicked.AddDynamic(this, &UVirtualSensorMonitorWidget::HandleToggleButtonClicked);

    if (UVerticalBoxSlot* ButtonSlot = RootBox->AddChildToVerticalBox(ToggleButton))
    {
        ButtonSlot->SetPadding(FMargin(8.0f));
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
        TitleText->SetText(FText::FromString(bShowingLidar ? TEXT("Virtual LIDAR View") : TEXT("Virtual Camera View")));
    }

    if (ToggleButtonText)
    {
        ToggleButtonText->SetText(FText::FromString(bShowingLidar ? TEXT("Show Camera View") : TEXT("Show LIDAR View")));
    }
}
