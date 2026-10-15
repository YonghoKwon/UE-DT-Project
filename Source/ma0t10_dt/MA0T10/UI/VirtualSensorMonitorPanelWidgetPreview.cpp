#include "VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
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

bool UVirtualSensorMonitorPanelWidget::SetLidarHeightDisplay(ELidarHeightReference Reference, bool bAutoRange, float MinMeters, float MaxMeters)
{
    auto* V = GetLidarVisualizationComponent();
    if (!V || !FMath::IsFinite(MinMeters) || !FMath::IsFinite(MaxMeters) || MaxMeters <= MinMeters) return false;
    auto S = V->GetVisualizationSettings();
    S.HeightReference = Reference; S.bAutoHeightRange = bAutoRange;
    S.HeightMinMeters = MinMeters; S.HeightMaxMeters = MaxMeters;
    V->SetVisualizationSettings(S);
    SaveMonitorUiPreferences();
    return true;
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

void UVirtualSensorMonitorPanelWidget::SetLidarWorldTopDownAutoFit(bool bEnabled)
{
    if (UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        Visualization->SetWorldTopDownAutoFit(bEnabled);
        RefreshImageBrush();
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

void UVirtualSensorMonitorPanelWidget::HandleNextLidarButtonClicked()
{
    if (SensorManager)
    {
        SensorManager->SelectNextLidar();
        SensorManager->SetViewMode(EVirtualSensorViewMode::Lidar);
    }
}

void UVirtualSensorMonitorPanelWidget::HandleLidarViewModeButtonClicked()
{
    CycleLidarViewMode();
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

void UVirtualSensorMonitorPanelWidget::RefreshImageBrush()
{
    if(IsWorkspaceOwned()&&(!WidgetTree||!WidgetTree->RootWidget))
    {
        const auto* Actor=GetOwnedMonitoredActor();
        if(!IsVisible()||IsPanelCollapsed()||!IsValid(Actor)||(bShowingLidar&&!Actor->IsSensorRunning()))return;
    }
    UObject* Resource = nullptr;
	bool bPrimaryFrameChanged = true;
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
        if(CameraTruthDisplayMode&&CameraComp&&CameraComp->bGroundTruthEnabled)Resource=CameraComp->GetGroundTruth()?CameraComp->GetGroundTruth()->Preview(CameraTruthDisplayMode):nullptr;
		const int64 CurrentFrameId = CameraComp ? CameraComp->GetRuntimeStatus().FrameId : INDEX_NONE;
		bPrimaryFrameChanged = CurrentFrameId != LastPrimaryCameraDisplayFrameId;
		LastPrimaryCameraDisplayFrameId = CurrentFrameId;
    }

    if (!Resource)
    {
        if(CameraTruthDisplayMode&&NativeViewImage){NativeViewBrush.SetResourceObject(nullptr);NativeViewImage->Invalidate(EInvalidateWidgetReason::Paint);}
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
		if (bShowingLidar || bPrimaryFrameChanged || NativeViewBrush.GetResourceObject() != Resource)
		{
			NativeViewBrush.SetResourceObject(Resource);
			NativeViewBrush.ImageSize = FVector2D(640.0f, 360.0f);
			NativeViewImage->Invalidate(EInvalidateWidgetReason::Paint);
		}
    }

    if (NativeSecondaryViewImage)
    {
        NativeSecondaryViewBrush.SetResourceObject(bShowingLidar ? GetSecondaryLidarBrushResource() : nullptr);
        NativeSecondaryViewBrush.ImageSize = FVector2D(640.0f, 360.0f);
        NativeSecondaryViewImage->Invalidate(EInvalidateWidgetReason::Paint);
    }
	if (NativeSecondaryCameraImage)
	{
		UObject* SecondaryResource = !bShowingLidar && bDualCameraModeEnabled && SecondaryCameraComp ? SecondaryCameraComp->GetCameraRenderTarget() : nullptr;
		const int64 SecondaryFrameId = SecondaryCameraComp ? SecondaryCameraComp->GetRuntimeStatus().FrameId : INDEX_NONE;
		if (SecondaryFrameId != LastSecondaryCameraDisplayFrameId || NativeSecondaryCameraBrush.GetResourceObject() != SecondaryResource)
		{
			LastSecondaryCameraDisplayFrameId = SecondaryFrameId;
			NativeSecondaryCameraBrush.SetResourceObject(SecondaryResource);
			NativeSecondaryCameraBrush.ImageSize = FVector2D(640.0f, 360.0f);
			NativeSecondaryCameraImage->Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
}

FLinearColor UVirtualSensorMonitorPanelWidget::GetLidarLegendSwatchColor(int32 SwatchIndex) const
{
    if (const UVirtualLidarVisualizationComponent* Visualization = GetLidarVisualizationComponent())
    {
        if (Visualization->Settings.ColorMode == ELidarColorMode::GeometrySeparation)
            return FLinearColor(VirtualLidarGeometry::Color(SwatchIndex==0?ELidarGeometryClass::Plane:SwatchIndex==1?ELidarGeometryClass::Protrusion:ELidarGeometryClass::Unknown));
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

bool UVirtualSensorMonitorPanelWidget::RefreshLidarPreviewWithoutTransport(bool bExplicitCompatibilityCapture)
{
    if (!LidarComp)
    {
        return false;
    }
    if(!bExplicitCompatibilityCapture&&IsWorkspaceOwned()&&(!WidgetTree||!WidgetTree->RootWidget))
    {
        // Owned preview consumes existing frames. Empty/view controls cannot
        // invoke the legacy synchronous scan or edit acquisition/output fields.
        InvalidateEnhancedLidarView();
        return LidarComp->GetLidarViewTexture()!=nullptr;
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
