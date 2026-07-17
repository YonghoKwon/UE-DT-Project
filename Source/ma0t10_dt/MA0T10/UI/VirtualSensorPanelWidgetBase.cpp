#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelHostComponent.h"

void UVirtualSensorPanelWidgetBase::SetPanelPersistenceKey(FName InPanelPersistenceKey)
{
    PanelPersistenceKey = InPanelPersistenceKey;
    if (bPanelLayoutConfigured)
    {
        RestorePanelUiState();
    }
}

void UVirtualSensorPanelWidgetBase::ConfigurePanelLayout(EVirtualSensorPanelPlacement InPlacement, FVector2D InDesiredSize, float InMargin)
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

void UVirtualSensorPanelWidgetBase::RefreshHostedPanelLayout()
{
    ApplyPanelSize();
    SetPanelPositionInternal(CurrentViewportPosition);
}

void UVirtualSensorPanelWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

void UVirtualSensorPanelWidgetBase::ApplyInitialPanelLayout(FVector2D ViewportSize)
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
    ApplyPanelSize();
    if (!Cast<UCanvasPanelSlot>(Slot))
    {
        SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
        SetAlignmentInViewport(FVector2D::ZeroVector);
    }
    bInitialLayoutPending = false;
    bPanelLayoutConfigured = true;
    ResetPanelPositionInternal(false);
    RestorePanelUiState();
}

void UVirtualSensorPanelWidgetBase::ResetPanelPosition()
{
    ResetPanelPositionInternal(true);
}

void UVirtualSensorPanelWidgetBase::ResetPanelPositionInternal(bool bPersist)
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

void UVirtualSensorPanelWidgetBase::TogglePanelCollapsed()
{
    bPanelCollapsed = !bPanelCollapsed;
    ApplyPanelSize();
    SetPanelPositionInternal(CurrentViewportPosition);
    SavePanelUiState();
}

void UVirtualSensorPanelWidgetBase::ResetPanelUiStateToDefault()
{
    bPanelCollapsed = false;
    ApplyPanelSize();
    ResetPanelPositionInternal(true);
}

FVector2D UVirtualSensorPanelWidgetBase::GetEffectivePanelSize() const
{
    return bPanelCollapsed
        ? FVector2D(DesiredPanelSize.X, FMath::Max(DragHandleHeight, CollapsedPanelHeight))
        : DesiredPanelSize;
}

FVector2D UVirtualSensorPanelWidgetBase::ClampPanelPosition(FVector2D Position, FVector2D PanelSize, FVector2D ViewportSize, float Margin)
{
    const float MaxX = FMath::Max(Margin, ViewportSize.X - PanelSize.X - Margin);
    const float MaxY = FMath::Max(Margin, ViewportSize.Y - FMath::Min(PanelSize.Y, 64.0f) - Margin);
    return FVector2D(FMath::Clamp(Position.X, Margin, MaxX), FMath::Clamp(Position.Y, Margin, MaxY));
}

FVector2D UVirtualSensorPanelWidgetBase::CalculateDraggedPanelPosition(FVector2D CurrentPosition, FVector2D CursorDelta, float DpiScale)
{
    return CurrentPosition + CursorDelta / FMath::Max(0.01f, DpiScale);
}

FReply UVirtualSensorPanelWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (PanelHostComponent) PanelHostComponent->BringPanelToFront(this);
    const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    if (bPanelDraggable && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && LocalPosition.Y <= DragHandleHeight)
    {
        bDraggingPanel = true;
        const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
        return CachedWidget.IsValid() ? FReply::Handled().CaptureMouse(CachedWidget.ToSharedRef()) : FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UVirtualSensorPanelWidgetBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDraggingPanel && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDraggingPanel = false;
        SavePanelUiState();
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UVirtualSensorPanelWidgetBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDraggingPanel && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        const float DpiScale = UWidgetLayoutLibrary::GetViewportScale(this);
        SetPanelPositionInternal(CalculateDraggedPanelPosition(CurrentViewportPosition, InMouseEvent.GetCursorDelta(), DpiScale));
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UVirtualSensorPanelWidgetBase::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    const bool bWasDragging = bDraggingPanel;
    bDraggingPanel = false;
    if (bWasDragging)
    {
        SavePanelUiState();
    }
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

EVisibility UVirtualSensorPanelWidgetBase::GetPanelBodyVisibility() const
{
    return bPanelCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
}

FVector2D UVirtualSensorPanelWidgetBase::ResolveLogicalViewportSize() const
{
    if (const UPanelWidget* ParentPanel = GetParent())
    {
        const FVector2D ParentSize = ParentPanel->GetCachedGeometry().GetLocalSize();
        if (ParentSize.X >= 320.0f && ParentSize.Y >= 200.0f)
        {
            return ParentSize;
        }
    }
    const float DpiScale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
    return UWidgetLayoutLibrary::GetViewportSize(const_cast<UVirtualSensorPanelWidgetBase*>(this)) / DpiScale;
}

void UVirtualSensorPanelWidgetBase::RestorePanelUiState()
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
    ApplyPanelSize();
    if (State->bHasSavedPosition)
    {
        SetPanelPositionInternal(FromNormalizedPanelPosition(State->NormalizedPosition, ResolveLogicalViewportSize()));
    }
}

void UVirtualSensorPanelWidgetBase::SavePanelUiState() const
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

FVector2D UVirtualSensorPanelWidgetBase::ToNormalizedPanelPosition(FVector2D Position, FVector2D ViewportSize) const
{
    const float MaxX = FMath::Max(ViewportMargin, ViewportSize.X - DesiredPanelSize.X - ViewportMargin);
    const float MaxY = FMath::Max(ViewportMargin, ViewportSize.Y - FMath::Min(DesiredPanelSize.Y, 64.0f) - ViewportMargin);
    const float RangeX = FMath::Max(KINDA_SMALL_NUMBER, MaxX - ViewportMargin);
    const float RangeY = FMath::Max(KINDA_SMALL_NUMBER, MaxY - ViewportMargin);
    return FVector2D(
        FMath::Clamp((Position.X - ViewportMargin) / RangeX, 0.0f, 1.0f),
        FMath::Clamp((Position.Y - ViewportMargin) / RangeY, 0.0f, 1.0f));
}

FVector2D UVirtualSensorPanelWidgetBase::FromNormalizedPanelPosition(FVector2D NormalizedPosition, FVector2D ViewportSize) const
{
    const float MaxX = FMath::Max(ViewportMargin, ViewportSize.X - DesiredPanelSize.X - ViewportMargin);
    const float MaxY = FMath::Max(ViewportMargin, ViewportSize.Y - FMath::Min(DesiredPanelSize.Y, 64.0f) - ViewportMargin);
    return FVector2D(
        FMath::Lerp(ViewportMargin, MaxX, FMath::Clamp(NormalizedPosition.X, 0.0f, 1.0f)),
        FMath::Lerp(ViewportMargin, MaxY, FMath::Clamp(NormalizedPosition.Y, 0.0f, 1.0f)));
}

void UVirtualSensorPanelWidgetBase::SetPanelPositionInternal(FVector2D NewPosition)
{
    const FVector2D EffectiveSize = GetEffectivePanelSize();
    if (bClampPanelToViewport)
    {
        NewPosition = ClampPanelPosition(NewPosition, EffectiveSize, ResolveLogicalViewportSize(), ViewportMargin);
    }
    CurrentViewportPosition = NewPosition;
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        CanvasSlot->SetPosition(CurrentViewportPosition);
    }
    else
    {
        SetPositionInViewport(CurrentViewportPosition, false);
    }
}

void UVirtualSensorPanelWidgetBase::ApplyPanelSize()
{
    const FVector2D EffectiveSize = GetEffectivePanelSize();
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        CanvasSlot->SetAutoSize(false);
        CanvasSlot->SetSize(EffectiveSize);
    }
    else
    {
        SetDesiredSizeInViewport(EffectiveSize);
    }
}
