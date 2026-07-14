#include "ma0t10_dt/MA0T10/UI/VirtualSensorDraggableWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"

void UVirtualSensorDraggableWidgetBase::SetPanelPersistenceKey(FName InPanelPersistenceKey)
{
    PanelPersistenceKey = InPanelPersistenceKey;
    if (bPanelLayoutConfigured)
    {
        RestorePanelUiState();
    }
}

void UVirtualSensorDraggableWidgetBase::ConfigurePanelLayout(EVirtualSensorPanelPlacement InPlacement, FVector2D InDesiredSize, float InMargin)
{
    DefaultPlacement = InPlacement;
    RequestedPanelSize = InDesiredSize;
    ViewportMargin = FMath::Max(0.0f, InMargin);
    const FVector2D ViewportSize = ResolveLogicalViewportSize();
    if (ViewportSize.X >= 320.0f && ViewportSize.Y >= 200.0f)
    {
        ApplyInitialPanelLayout(ViewportSize);
        return;
    }
    bInitialLayoutPending = true;
}

void UVirtualSensorDraggableWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bInitialLayoutPending)
    {
        const FVector2D ViewportSize = ResolveLogicalViewportSize();
        if (ViewportSize.X >= 320.0f && ViewportSize.Y >= 200.0f)
        {
            ApplyInitialPanelLayout(ViewportSize);
        }
    }
}

void UVirtualSensorDraggableWidgetBase::ApplyInitialPanelLayout(FVector2D ViewportSize)
{
    FVector2D ResolvedSize = RequestedPanelSize;
    if (ViewportSize.X <= 1400.0f)
    {
        // Keep all three default panels independently reachable at 1280x720.
        // Their scroll boxes preserve the full control surface at these compact sizes.
        if (DefaultPlacement == EVirtualSensorPanelPlacement::RightCenter)
        {
            ResolvedSize.X = FMath::Min(ResolvedSize.X, 620.0f);
            ResolvedSize.Y = FMath::Min(ResolvedSize.Y, 340.0f);
        }
        else if (DefaultPlacement == EVirtualSensorPanelPlacement::LeftCenter)
        {
            ResolvedSize.X = FMath::Min(ResolvedSize.X, 340.0f);
            ResolvedSize.Y = FMath::Min(ResolvedSize.Y, 600.0f);
        }
        else if (DefaultPlacement == EVirtualSensorPanelPlacement::BottomCenter)
        {
            ResolvedSize.X = FMath::Min(ResolvedSize.X, 500.0f);
            ResolvedSize.Y = FMath::Min(ResolvedSize.Y, 160.0f);
        }
    }
    DesiredPanelSize.X = FMath::Max(160.0f, ResolvedSize.X);
    DesiredPanelSize.Y = FMath::Max(80.0f, ResolvedSize.Y);
    SetDesiredSizeInViewport(DesiredPanelSize);
    SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
    SetAlignmentInViewport(FVector2D::ZeroVector);
    bInitialLayoutPending = false;
    bPanelLayoutConfigured = true;
    ResetPanelPositionInternal(false);
    RestorePanelUiState();
}

void UVirtualSensorDraggableWidgetBase::ResetPanelPosition()
{
    ResetPanelPositionInternal(true);
}

void UVirtualSensorDraggableWidgetBase::ResetPanelPositionInternal(bool bPersist)
{
    const FVector2D ViewportSize = ResolveLogicalViewportSize();
    FVector2D Position(ViewportMargin, ViewportMargin);
    if (DefaultPlacement == EVirtualSensorPanelPlacement::RightCenter)
    {
        Position = FVector2D(ViewportSize.X - DesiredPanelSize.X - ViewportMargin, (ViewportSize.Y - DesiredPanelSize.Y) * 0.5f);
    }
    else if (DefaultPlacement == EVirtualSensorPanelPlacement::LeftCenter)
    {
        Position = FVector2D(ViewportMargin, (ViewportSize.Y - DesiredPanelSize.Y) * 0.5f);
    }
    else if (DefaultPlacement == EVirtualSensorPanelPlacement::BottomCenter)
    {
        Position = FVector2D((ViewportSize.X - DesiredPanelSize.X) * 0.5f, ViewportSize.Y - DesiredPanelSize.Y - ViewportMargin);
    }
    SetPanelPositionInternal(Position);
    if (bPersist)
    {
        SavePanelUiState();
    }
}

void UVirtualSensorDraggableWidgetBase::TogglePanelCollapsed()
{
    bPanelCollapsed = !bPanelCollapsed;
    SavePanelUiState();
}

void UVirtualSensorDraggableWidgetBase::ResetPanelUiStateToDefault()
{
    bPanelCollapsed = false;
    ResetPanelPositionInternal(true);
}

FVector2D UVirtualSensorDraggableWidgetBase::ClampPanelPosition(FVector2D Position, FVector2D PanelSize, FVector2D ViewportSize, float Margin)
{
    const float MaxX = FMath::Max(Margin, ViewportSize.X - PanelSize.X - Margin);
    const float MaxY = FMath::Max(Margin, ViewportSize.Y - FMath::Min(PanelSize.Y, 64.0f) - Margin);
    return FVector2D(FMath::Clamp(Position.X, Margin, MaxX), FMath::Clamp(Position.Y, Margin, MaxY));
}

FReply UVirtualSensorDraggableWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    if (bPanelDraggable && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && LocalPosition.Y <= DragHandleHeight)
    {
        bDraggingPanel = true;
        const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
        return CachedWidget.IsValid() ? FReply::Handled().CaptureMouse(CachedWidget.ToSharedRef()) : FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UVirtualSensorDraggableWidgetBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDraggingPanel && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDraggingPanel = false;
        SavePanelUiState();
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UVirtualSensorDraggableWidgetBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDraggingPanel && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        const float DpiScale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
        SetPanelPositionInternal(CurrentViewportPosition + InMouseEvent.GetCursorDelta() / DpiScale);
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UVirtualSensorDraggableWidgetBase::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    const bool bWasDragging = bDraggingPanel;
    bDraggingPanel = false;
    if (bWasDragging)
    {
        SavePanelUiState();
    }
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

EVisibility UVirtualSensorDraggableWidgetBase::GetPanelBodyVisibility() const
{
    return bPanelCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
}

FVector2D UVirtualSensorDraggableWidgetBase::ResolveLogicalViewportSize() const
{
    const float DpiScale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
    return UWidgetLayoutLibrary::GetViewportSize(const_cast<UVirtualSensorDraggableWidgetBase*>(this)) / DpiScale;
}

void UVirtualSensorDraggableWidgetBase::RestorePanelUiState()
{
    if (PanelPersistenceKey.IsNone() || !bPanelLayoutConfigured)
    {
        return;
    }
    const UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
    const FVirtualSensorPanelUiState* State = Preferences ? Preferences->PanelStates.Find(PanelPersistenceKey) : nullptr;
    if (!State)
    {
        return;
    }
    bPanelCollapsed = State->bCollapsed;
    if (State->bHasSavedPosition)
    {
        SetPanelPositionInternal(FromNormalizedPanelPosition(State->NormalizedPosition, ResolveLogicalViewportSize()));
    }
}

void UVirtualSensorDraggableWidgetBase::SavePanelUiState() const
{
    if (PanelPersistenceKey.IsNone() || !bPanelLayoutConfigured)
    {
        return;
    }
    UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
    if (!Preferences)
    {
        return;
    }
    FVirtualSensorPanelUiState& State = Preferences->PanelStates.FindOrAdd(PanelPersistenceKey);
    State.NormalizedPosition = ToNormalizedPanelPosition(CurrentViewportPosition, ResolveLogicalViewportSize());
    State.bHasSavedPosition = true;
    State.bCollapsed = bPanelCollapsed;
    UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
}

FVector2D UVirtualSensorDraggableWidgetBase::ToNormalizedPanelPosition(FVector2D Position, FVector2D ViewportSize) const
{
    const float MaxX = FMath::Max(ViewportMargin, ViewportSize.X - DesiredPanelSize.X - ViewportMargin);
    const float MaxY = FMath::Max(ViewportMargin, ViewportSize.Y - FMath::Min(DesiredPanelSize.Y, 64.0f) - ViewportMargin);
    const float RangeX = FMath::Max(KINDA_SMALL_NUMBER, MaxX - ViewportMargin);
    const float RangeY = FMath::Max(KINDA_SMALL_NUMBER, MaxY - ViewportMargin);
    return FVector2D(
        FMath::Clamp((Position.X - ViewportMargin) / RangeX, 0.0f, 1.0f),
        FMath::Clamp((Position.Y - ViewportMargin) / RangeY, 0.0f, 1.0f));
}

FVector2D UVirtualSensorDraggableWidgetBase::FromNormalizedPanelPosition(FVector2D NormalizedPosition, FVector2D ViewportSize) const
{
    const float MaxX = FMath::Max(ViewportMargin, ViewportSize.X - DesiredPanelSize.X - ViewportMargin);
    const float MaxY = FMath::Max(ViewportMargin, ViewportSize.Y - FMath::Min(DesiredPanelSize.Y, 64.0f) - ViewportMargin);
    return FVector2D(
        FMath::Lerp(ViewportMargin, MaxX, FMath::Clamp(NormalizedPosition.X, 0.0f, 1.0f)),
        FMath::Lerp(ViewportMargin, MaxY, FMath::Clamp(NormalizedPosition.Y, 0.0f, 1.0f)));
}

void UVirtualSensorDraggableWidgetBase::SetPanelPositionInternal(FVector2D NewPosition)
{
    if (bClampPanelToViewport)
    {
        NewPosition = ClampPanelPosition(NewPosition, DesiredPanelSize, ResolveLogicalViewportSize(), ViewportMargin);
    }
    CurrentViewportPosition = NewPosition;
    SetPositionInViewport(CurrentViewportPosition, false);
}
