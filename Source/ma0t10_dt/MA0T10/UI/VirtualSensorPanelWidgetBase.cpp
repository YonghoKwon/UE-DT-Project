#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelHostComponent.h"
#include "VirtualSensorUiHostActor.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "SensorToolWorkspaceStyle.h"
#include "SensorToolUiText.h"
#include "SensorToolWidgetDecl.h"
#include "VirtualSensorUiStyle.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolIcon.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Blueprint/WidgetTree.h"

bool UVirtualSensorPanelWidgetBase::SupportsOwnedDocking() const
{return IsWorkspaceOwned()&&(!WidgetTree||!WidgetTree->RootWidget);}
float UVirtualSensorPanelWidgetBase::GetSensorToolVisualScale() const
{return SensorToolFontScale*(SupportsOwnedDocking()&&ToolWorkspace.IsValid()?ToolWorkspace->GetOwnedPixelScale()*.75f:1.f);} // Slate point sizes -> design pixels; stored scale/API is unchanged.
void UVirtualSensorPanelWidgetBase::ApplyWorkspaceDockRect(FVector2D Position,FVector2D Size)
{
    if(!SupportsOwnedDocking())return;
	if(!bWorkspaceDocked)CancelPanelPointerGesture();
    if(!bWorkspaceDocked){bFloatingDraggable=bPanelDraggable;bFloatingResizable=bPanelResizable;bFloatingClamp=bClampPanelToViewport;}
    bWorkspaceDocked=true;bPanelDraggable=false;bPanelResizable=false;bClampPanelToViewport=false;
    bInitialLayoutPending=false;bPanelLayoutConfigured=true;DesiredPanelSize=Size;CurrentViewportPosition=Position;
    if(auto* S=Cast<UCanvasPanelSlot>(Slot)){if(S->GetAutoSize())S->SetAutoSize(false);if(!S->GetPosition().Equals(Position,.1))S->SetPosition(Position);const auto Effective=GetEffectivePanelSize();if(!S->GetSize().Equals(Effective,.1))S->SetSize(Effective);}
}
void UVirtualSensorPanelWidgetBase::LeaveWorkspaceDock()
{
    if(!bWorkspaceDocked)return;bWorkspaceDocked=false;bPanelDraggable=bFloatingDraggable;bPanelResizable=bFloatingResizable;bClampPanelToViewport=bFloatingClamp;
}

const FSlateBrush* UVirtualSensorPanelWidgetBase::GetToolPanelBrush() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::PanelBrush():FCoreStyle::Get().GetBrush("WhiteBrush");}
const FSlateBrush* UVirtualSensorPanelWidgetBase::GetToolSectionBrush() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::SectionBrush():FCoreStyle::Get().GetBrush("WhiteBrush");}
FLinearColor UVirtualSensorPanelWidgetBase::GetToolPanelColor() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::Panel():FVirtualSensorUiStyle::PanelBackground;}
FLinearColor UVirtualSensorPanelWidgetBase::GetToolHeaderColor() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::Header():FVirtualSensorUiStyle::HeaderBackground;}
FLinearColor UVirtualSensorPanelWidgetBase::GetToolSectionColor() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::Section():FVirtualSensorUiStyle::SectionBackground;}
FLinearColor UVirtualSensorPanelWidgetBase::GetToolTextColor() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::Text():FVirtualSensorUiStyle::PrimaryText;}
FLinearColor UVirtualSensorPanelWidgetBase::GetToolMutedColor() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::Muted():FVirtualSensorUiStyle::SecondaryText;}
FLinearColor UVirtualSensorPanelWidgetBase::GetToolAccentColor() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::Accent():FVirtualSensorUiStyle::Accent;}
const FButtonStyle& UVirtualSensorPanelWidgetBase::GetToolButtonStyle(bool Danger) const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::Button(Danger):FVirtualSensorUiStyle::ButtonStyle();}
const FEditableTextBoxStyle& UVirtualSensorPanelWidgetBase::GetToolInputStyle() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::Input():FVirtualSensorUiStyle::EditableTextBoxStyle();}
const FComboBoxStyle& UVirtualSensorPanelWidgetBase::GetToolComboBoxStyle() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::ComboBox():FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");}
const FSpinBoxStyle& UVirtualSensorPanelWidgetBase::GetToolSpinBoxStyle() const {return IsWorkspaceOwned()?FSensorOwnedPresentationStyle::SpinBox():FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox");}
TSharedRef<SWidget> UVirtualSensorPanelWidgetBase::BuildToolFormRow(const FText& Label,TSharedRef<SWidget> Control,TSharedRef<SWidget> Detail)
{
    return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,FSensorOwnedPresentationStyle::Gap)
        [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNewSensorTool(STextBlock).Text(Label)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Detail]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,FSensorOwnedPresentationStyle::Gap)
        [SNew(SBox).HeightOverride_Lambda([this](){return FSensorOwnedPresentationStyle::ControlHeight*GetSensorToolVisualScale()/.75f;})[Control]];
}
void UVirtualSensorPanelWidgetBase::PruneFontRegistrations()
{
    for(int32 I=SensorFontLifetimes.Num()-1;I>=0;--I)if(!SensorFontLifetimes[I]()){SensorFontLifetimes.RemoveAt(I);SensorFontSetters.RemoveAt(I);}
    for(auto It=RegisteredSensorUmg.CreateIterator();It;++It)if(!It->IsValid())It.RemoveCurrent();
}
void UVirtualSensorPanelWidgetBase::AddFontRegistration(TFunction<void(float)> Setter,TFunction<bool()> IsAlive)
{PruneFontRegistrations();SensorFontSetters.Add(MoveTemp(Setter));SensorFontLifetimes.Add(MoveTemp(IsAlive));}

void UVirtualSensorPanelWidgetBase::SetGlobalSensorUiFontScale(float InScale)
{
	SetSensorToolFontScale(InScale);
}

float UVirtualSensorPanelWidgetBase::GetGlobalSensorUiFontScale() const
{
	if(ToolWorkspace.IsValid())return ToolWorkspace->GetOwnedPanelFontScale();
	return SensorAppearanceOwner.IsValid() ? SensorAppearanceOwner->GetSensorToolFontScale() : 1.0f;
}

void UVirtualSensorPanelWidgetBase::ResetGlobalSensorUiFontScale()
{
	SetSensorToolFontScale(1.0f);
}

int32 UVirtualSensorPanelWidgetBase::CalculateScaledFontSize(int32 BaseSize, float Scale)
{
	return FMath::Max(8, FMath::RoundToInt(BaseSize * (FMath::IsFinite(Scale) ? FMath::Clamp(Scale, 0.85f, 1.5f) : 1.0f)));
}

void UVirtualSensorPanelWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	bSensorPanelConstructed = true;
	// Initial Slate construction / synchronization may replace the constructor-time
	// font attributes. Reapply only our explicit native controls after it finishes.
	if(SupportsOwnedDocking())bPendingOwnedFontRefresh=true;
	if (SensorAppearanceOwner.IsValid()) OnSensorUiFontScaleChanged(SensorToolFontScale);
}

void UVirtualSensorPanelWidgetBase::NativeDestruct()
{
	CancelPanelPointerGesture();
	if(ToolWorkspace.IsValid())ToolWorkspace->UnregisterPanel(this);
	bSensorPanelConstructed = false;
	Super::NativeDestruct();
}

void UVirtualSensorPanelWidgetBase::SetSensorAppearanceOwner(AVirtualSensorUiHostActor* Host)
{
	SensorAppearanceOwner = Host;
	if (Host) ApplySensorToolFontScale(ToolWorkspace.IsValid()?ToolWorkspace->GetOwnedPanelFontScale():Host->GetSensorToolFontScale());
}
void UVirtualSensorPanelWidgetBase::SetSensorToolFontScale(float Scale)
{
	if(ToolWorkspace.IsValid()){ToolWorkspace->SetOwnedPanelFontScale(Scale);return;}
	if (SensorAppearanceOwner.IsValid()) SensorAppearanceOwner->SetSensorToolFontScale(Scale);
}
void UVirtualSensorPanelWidgetBase::ApplySensorToolFontScale(float Scale)
{
	if ((!SensorAppearanceOwner.IsValid()&&!ToolWorkspace.IsValid()) || !FMath::IsFinite(Scale)) return;
	const float PreviousScale = SensorToolFontScale;
	SensorToolFontScale = FMath::Clamp(Scale, 0.85f, 1.5f);
	if(IsWorkspaceOwned()){DragHandleHeight=FMath::Max(38.0f,26.0f*SensorToolFontScale+20.0f);SetPanelExpandedSize(DesiredPanelSize,false);}
	PruneFontRegistrations();
	for (auto& Setter : SensorFontSetters) Setter(GetSensorToolVisualScale());
	InvalidateLayoutAndVolatility();
	if (bSensorPanelConstructed && !FMath::IsNearlyEqual(PreviousScale, SensorToolFontScale))
		OnSensorUiFontScaleChanged(SensorToolFontScale);
}
void UVirtualSensorPanelWidgetBase::RegisterSensorNativeFont(TSharedRef<STextBlock> Widget, const STextBlock::FArguments& Args)
{
	FSlateFontInfo Base = Widget->GetFont();
	// Respect the local control's wrapping contract. Forcing every caption to wrap
	// makes short buttons/checkboxes split into syllables at larger font scales.
    const auto Color=Widget->GetColorAndOpacity().GetSpecifiedColor();const bool Secondary=Color.Equals(FVirtualSensorUiStyle::SecondaryText)||Color.Equals(GetToolMutedColor());
	AddFontRegistration([Weak=TWeakPtr<STextBlock>(Widget), Owner=TWeakObjectPtr<UVirtualSensorPanelWidgetBase>(this),Base,Secondary](float Scale)
    {
        if(auto W=Weak.Pin())
        {
            if(!Owner.IsValid()||!Owner->SupportsOwnedDocking()){auto Font=Base;Font.Size=FMath::Max(8,FMath::RoundToInt(Base.Size*Scale));W->SetFont(Font);return;}
            W->SetFont(TAttribute<FSlateFontInfo>::CreateLambda([Owner,Base,Secondary,Scale](){auto Font=Base;const bool Owned=Owner.IsValid()&&Owner->SupportsOwnedDocking();const int32 Size=Owned?(Secondary?FSensorOwnedPresentationStyle::LabelSize:FSensorOwnedPresentationStyle::BodySize):Base.Size;Font.Size=FMath::Max(8,FMath::RoundToInt(Size*(Owned?Owner->GetSensorToolVisualScale():Scale)));return Font;}));
        }
    },[Weak=TWeakPtr<STextBlock>(Widget)](){return Weak.IsValid();});
	if (SensorAppearanceOwner.IsValid()||IsWorkspaceOwned()) SensorFontSetters.Last()(GetSensorToolVisualScale());
}
void UVirtualSensorPanelWidgetBase::RegisterSensorNativeFont(TSharedRef<SButton> Widget, const SButton::FArguments& Args)
{
	if(IsWorkspaceOwned())
	{
		if(Args._ButtonStyle==&FVirtualSensorUiStyle::ButtonStyle()||Args._ButtonStyle==&FSensorToolWorkspaceStyle::Button()||Args._ButtonStyle==&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))Widget->SetButtonStyle(&GetToolButtonStyle());
		if(!Args._ForegroundColor.IsBound())Widget->SetForegroundColor(GetToolTextColor());
	}
	// Only SButton's own default caption, never user-supplied children.
	if (Args._Content.Widget == SNullWidget::NullWidget && Widget->GetContent()->GetType() == FName(TEXT("STextBlock")))
		RegisterSensorNativeFont(StaticCastSharedRef<STextBlock>(Widget->GetContent()), STextBlock::FArguments());
}
void UVirtualSensorPanelWidgetBase::RegisterSensorNativeFont(TSharedRef<SEditableTextBox> Widget, const SEditableTextBox::FArguments& Args)
{
	FSlateFontInfo Base = Args._Font.Get(Args._Style->TextStyle.Font);
	if(IsWorkspaceOwned())Widget->SetStyle(&GetToolInputStyle());
	AddFontRegistration([Weak=TWeakPtr<SEditableTextBox>(Widget), Owner=TWeakObjectPtr<UVirtualSensorPanelWidgetBase>(this), Base](float Scale)
    {
        if(auto W=Weak.Pin())
        {
            if(!Owner.IsValid()||!Owner->SupportsOwnedDocking()){auto Font=Base;Font.Size=FMath::Max(8,FMath::RoundToInt(Base.Size*Scale));W->SetFont(Font);return;}
            W->SetFont(TAttribute<FSlateFontInfo>::CreateLambda([Owner,Base,Scale](){auto Font=Base;const bool Owned=Owner.IsValid()&&Owner->SupportsOwnedDocking();const int32 Size=Owned?FSensorOwnedPresentationStyle::BodySize:Base.Size;Font.Size=FMath::Max(8,FMath::RoundToInt(Size*(Owned?Owner->GetSensorToolVisualScale():Scale)));return Font;}));
        }
    },[Weak=TWeakPtr<SEditableTextBox>(Widget)](){return Weak.IsValid();});
	if (SensorAppearanceOwner.IsValid()||IsWorkspaceOwned()) SensorFontSetters.Last()(GetSensorToolVisualScale());
}
void UVirtualSensorPanelWidgetBase::RegisterSensorTextControl(UTextBlock* Text)
{
	if (!Text || Text->GetTypedOuter<UUserWidget>() != this || RegisteredSensorUmg.Contains(Text)) return;
	RegisteredSensorUmg.Add(Text);
	const FSlateFontInfo Base = Text->GetFont();
	AddFontRegistration([Weak=TWeakObjectPtr<UTextBlock>(Text), Base](float Scale) { if (Weak.IsValid()) { auto Font=Base; Font.Size=FMath::Max(8, FMath::RoundToInt(Base.Size * Scale)); Weak->SetFont(Font); } },[Weak=TWeakObjectPtr<UTextBlock>(Text)](){return Weak.IsValid();});
	if (SensorAppearanceOwner.IsValid()||IsWorkspaceOwned()) SensorFontSetters.Last()(SensorToolFontScale);
}
void UVirtualSensorPanelWidgetBase::RegisterSensorInputControl(UEditableTextBox* Input)
{
	if (!Input || Input->GetTypedOuter<UUserWidget>() != this || RegisteredSensorUmg.Contains(Input)) return;
	RegisteredSensorUmg.Add(Input);
	const FEditableTextBoxStyle Base = Input->WidgetStyle;
	AddFontRegistration([Weak=TWeakObjectPtr<UEditableTextBox>(Input), Base](float Scale) { if (Weak.IsValid()) { auto Style=Base; Style.TextStyle.Font.Size=FMath::Max(8, FMath::RoundToInt(Base.TextStyle.Font.Size * Scale)); Weak->WidgetStyle=Style; Weak->SynchronizeProperties(); } },[Weak=TWeakObjectPtr<UEditableTextBox>(Input)](){return Weak.IsValid();});
	if (SensorAppearanceOwner.IsValid()||IsWorkspaceOwned()) SensorFontSetters.Last()(SensorToolFontScale);
}

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

void UVirtualSensorPanelWidgetBase::SetPanelResizeLimits(FVector2D InMinimumSize, FVector2D InMaximumSize)
{
    MinimumPanelSize.X = FMath::Max(160.0f, InMinimumSize.X);
    MinimumPanelSize.Y = FMath::Max(80.0f, InMinimumSize.Y);
    MaximumPanelSize.X = FMath::Max(0.0f, InMaximumSize.X);
    MaximumPanelSize.Y = FMath::Max(0.0f, InMaximumSize.Y);
    SetPanelExpandedSize(DesiredPanelSize, false);
}

void UVirtualSensorPanelWidgetBase::SetPanelExpandedSize(FVector2D InExpandedSize, bool bPersist)
{
    DesiredPanelSize = CalculateResizedPanelSize(
        FVector2D::ZeroVector,
        InExpandedSize,
        1.0f,
        GetResolvedPanelMinimum(),
        ResolveMaximumPanelSize());
    ApplyPanelSize();
    SetPanelPositionInternal(CurrentViewportPosition);
    if (bPersist)
    {
        SavePanelUiState();
    }
}

void UVirtualSensorPanelWidgetBase::ResetPanelSize()
{
    SetPanelExpandedSize(DefaultResolvedPanelSize, true);
}

void UVirtualSensorPanelWidgetBase::RefreshHostedPanelLayout()
{
    ApplyPanelSize();
    SetPanelPositionInternal(CurrentViewportPosition);
}

void UVirtualSensorPanelWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if(bPendingOwnedFontRefresh&&SupportsOwnedDocking()&&MyGeometry.GetLocalSize().X>0)
    {bPendingOwnedFontRefresh=false;ApplySensorToolFontScale(SensorToolFontScale);}
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
	if(ToolWorkspace.IsValid()) { bInitialLayoutPending=false;bPanelLayoutConfigured=true;ToolWorkspace->RestorePanel(ToolRole);return; }
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
    DefaultResolvedPanelSize.X = FMath::Max(160.0f, ResolvedSize.X);
    DefaultResolvedPanelSize.Y = FMath::Max(80.0f, ResolvedSize.Y);
    DesiredPanelSize = DefaultResolvedPanelSize;
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
    DesiredPanelSize = DefaultResolvedPanelSize;
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

FVector2D UVirtualSensorPanelWidgetBase::CalculateResizedPanelSize(
    FVector2D CurrentSize,
    FVector2D CursorDelta,
    float DpiScale,
    FVector2D MinimumSize,
    FVector2D MaximumSize)
{
    const FVector2D Candidate = CurrentSize + CursorDelta / FMath::Max(0.01f, DpiScale);
    return FVector2D(
        FMath::Clamp(Candidate.X, MinimumSize.X, FMath::Max(MinimumSize.X, MaximumSize.X)),
        FMath::Clamp(Candidate.Y, MinimumSize.Y, FMath::Max(MinimumSize.Y, MaximumSize.Y)));
}

FReply UVirtualSensorPanelWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ToolWorkspace.IsValid())ToolWorkspace->BringOwnedPanelToFront(ToolRole);
	else if (PanelHostComponent) PanelHostComponent->BringPanelToFront(this);
    const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    if (bPanelResizable && !bPanelCollapsed && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
        IsInResizeHandle(InGeometry, InMouseEvent.GetScreenSpacePosition()))
    {
        bResizingPanel = true;
        bDraggingPanel = false;
        return CapturePanelPointer(InMouseEvent);
    }
    if (bPanelDraggable && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && LocalPosition.Y <= DragHandleHeight)
    {
        bDraggingPanel = true;
        bResizingPanel = false;
        return CapturePanelPointer(InMouseEvent);
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UVirtualSensorPanelWidgetBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && FinishPanelPointerGesture())
    {
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UVirtualSensorPanelWidgetBase::CapturePanelPointer(const FPointerEvent& Event)
{
    PanelPointerUser=Event.GetUserIndex();
    const auto Widget=GetCachedWidget();if(!Widget.IsValid())return FReply::Handled();
    auto Reply=FReply::Handled().CaptureMouse(Widget.ToSharedRef());
    // Without an explicit focus recipient Slate falls back to SViewport after
    // the title bubbles. Permanent game capture then overwrites panel capture.
    if(SupportsOwnedDocking())Reply.SetUserFocus(Widget.ToSharedRef(),EFocusCause::Mouse);
    return Reply;
}

FReply UVirtualSensorPanelWidgetBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bResizingPanel && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        const FVector2D LocalDelta=InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()+InMouseEvent.GetCursorDelta())-
            InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        SetPanelExpandedSize(CalculateResizedPanelSize(
            DesiredPanelSize,
            LocalDelta,
            1.0f,
            GetResolvedPanelMinimum(),
            ResolveMaximumPanelSize()), false);
        return FReply::Handled();
    }
    if (bDraggingPanel && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        const FVector2D LocalDelta=InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()+InMouseEvent.GetCursorDelta())-
            InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        SetPanelPositionInternal(CalculateDraggedPanelPosition(CurrentViewportPosition, LocalDelta, 1.0f));
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UVirtualSensorPanelWidgetBase::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    FinishPanelPointerGesture();
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

bool UVirtualSensorPanelWidgetBase::FinishPanelPointerGesture()
{
    const bool bWasDragging=bDraggingPanel||bResizingPanel;
    bDraggingPanel = false;
    bResizingPanel = false;
    if (bWasDragging)
    {
        SavePanelUiState();
    }
    return bWasDragging;
}

void UVirtualSensorPanelWidgetBase::CancelPanelPointerGesture()
{
    if(!FinishPanelPointerGesture()||!FSlateApplication::IsInitialized())return;
    const auto Widget=GetCachedWidget();auto& App=FSlateApplication::Get();
    if(Widget.IsValid()&&Widget->HasMouseCaptureByUser(PanelPointerUser))
    {
        FWidgetPath Path;App.GeneratePathToWidgetUnchecked(Widget.ToSharedRef(),Path);
        App.ProcessReply(Path,FReply::Handled().ReleaseMouseCapture(),nullptr,nullptr,PanelPointerUser);
    }
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
	if(ToolWorkspace.IsValid()){ToolWorkspace->RestorePanel(ToolRole);return;}
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
    if (State->bHasSavedSize)
    {
        DesiredPanelSize = CalculateResizedPanelSize(
            FVector2D::ZeroVector,
            State->ExpandedSize,
            1.0f,
            MinimumPanelSize,
            ResolveMaximumPanelSize());
    }
    ApplyPanelSize();
    if (State->bHasSavedPosition)
    {
        SetPanelPositionInternal(FromNormalizedPanelPosition(State->NormalizedPosition, ResolveLogicalViewportSize()));
    }
}

void UVirtualSensorPanelWidgetBase::SavePanelUiState() const
{
	if(ToolWorkspace.IsValid()){ToolWorkspace->SavePanel(ToolRole);return;}
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
    State.ExpandedSize = DesiredPanelSize;
    State.bHasSavedSize = true;
    UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
}

FVector2D UVirtualSensorPanelWidgetBase::ResolveMaximumPanelSize() const
{
    const FVector2D ViewportSize = ResolveLogicalViewportSize();
    if(IsWorkspaceOwned()&&ViewportSize.X>=320&&ViewportSize.Y>=200)
    {
        const FVector2D Available(FMath::Max(160.0,ViewportSize.X-ViewportMargin*2),FMath::Max(80.0,ViewportSize.Y-ToolWorkspace->GetToolbarReservedTop()-ViewportMargin));
        return FVector2D(MaximumPanelSize.X>0?FMath::Min(MaximumPanelSize.X,Available.X):Available.X,MaximumPanelSize.Y>0?FMath::Min(MaximumPanelSize.Y,Available.Y):Available.Y);
    }
    const FVector2D ViewportMaximum(
        FMath::Max(MinimumPanelSize.X, ViewportSize.X - ViewportMargin * 2.0f),
        FMath::Max(MinimumPanelSize.Y, ViewportSize.Y - ViewportMargin * 2.0f));
    return FVector2D(
        MaximumPanelSize.X > 0.0f ? FMath::Min(MaximumPanelSize.X, ViewportMaximum.X) : ViewportMaximum.X,
        MaximumPanelSize.Y > 0.0f ? FMath::Min(MaximumPanelSize.Y, ViewportMaximum.Y) : ViewportMaximum.Y);
}

FVector2D UVirtualSensorPanelWidgetBase::GetResolvedPanelMinimum() const
{
    if(bWorkspaceDocked)return FVector2D(160,80);
    if(!IsWorkspaceOwned())return MinimumPanelSize;
    const FVector2D Maximum=ResolveMaximumPanelSize();
    const float HeaderWidthScale=FMath::Max(1.0f,SensorToolFontScale);
    return FVector2D(FMath::Min(MinimumPanelSize.X*HeaderWidthScale,Maximum.X),FMath::Min(MinimumPanelSize.Y,Maximum.Y));
}

bool UVirtualSensorPanelWidgetBase::IsInResizeHandle(const FGeometry& Geometry, const FVector2D& ScreenPosition) const
{
    const FVector2D LocalPosition = Geometry.AbsoluteToLocal(ScreenPosition);
    const FVector2D LocalSize = Geometry.GetLocalSize();
    return LocalPosition.X >= LocalSize.X - ResizeHandleSize &&
        LocalPosition.Y >= LocalSize.Y - ResizeHandleSize;
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
		if(IsWorkspaceOwned()){
			const FVector2D V=ResolveLogicalViewportSize();
			const double Top=ToolWorkspace->GetToolbarReservedTop();
			NewPosition.Y=FMath::Clamp(NewPosition.Y,Top,FMath::Max(Top,V.Y-EffectiveSize.Y-ViewportMargin));
		}
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

void UVirtualSensorPanelWidgetBase::SetToolWorkspace(UVirtualSensorToolWorkspaceSubsystem* Owner,ESensorToolPanelRole Role)
{
    if(ToolWorkspace.Get()!=Owner)CancelPanelPointerGesture();
    if(bOwnedPointerFocus){SetIsFocusable(bPreviousFocusable);bOwnedPointerFocus=false;}
    ToolWorkspace=Owner;ToolRole=Role;bPendingOwnedFontRefresh=SupportsOwnedDocking();
    if(SupportsOwnedDocking()){bPreviousFocusable=IsFocusable();SetIsFocusable(true);bOwnedPointerFocus=true;}
}
FVirtualSensorPanelUiState UVirtualSensorPanelWidgetBase::CaptureWorkspaceLayout() const
{
	FVirtualSensorPanelUiState S;S.bHasSavedPosition=true;S.bHasSavedSize=true;S.bCollapsed=bPanelCollapsed;S.ExpandedSize=DesiredPanelSize;
	S.NormalizedPosition=ToNormalizedPanelPosition(CurrentViewportPosition,ResolveLogicalViewportSize());return S;
}
void UVirtualSensorPanelWidgetBase::ApplyWorkspaceLayout(const FVirtualSensorPanelUiState& S)
{
	if(!IsWorkspaceOwned())return;bPanelLayoutConfigured=true;bInitialLayoutPending=false;
	if(S.bHasSavedSize&&FMath::IsFinite(S.ExpandedSize.X)&&FMath::IsFinite(S.ExpandedSize.Y))SetPanelExpandedSize(S.ExpandedSize,false);
	bPanelCollapsed=S.bCollapsed;ApplyPanelSize();
	if(S.bHasSavedPosition&&FMath::IsFinite(S.NormalizedPosition.X)&&FMath::IsFinite(S.NormalizedPosition.Y))SetPanelPositionInternal(FromNormalizedPanelPosition(S.NormalizedPosition,ResolveLogicalViewportSize()));
}
FReply UVirtualSensorPanelWidgetBase::NativeOnPreviewMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
	if(ToolWorkspace.IsValid())
	{
		ToolWorkspace->BringOwnedPanelToFront(ToolRole);
		if(bPanelResizable&&!bPanelCollapsed&&E.GetEffectingButton()==EKeys::LeftMouseButton&&IsInResizeHandle(G,E.GetScreenSpacePosition()))
		{
			bResizingPanel=true;bDraggingPanel=false;
			return CapturePanelPointer(E);
		}
		// DTCore's unrelated OpenWidgets list does not contain these owned panels.
		// Its BringToFront would immediately overwrite our local z-order with 1.
		return FReply::Unhandled();
	}
	return Super::NativeOnPreviewMouseButtonDown(G,E);
}
TSharedRef<SWidget> UVirtualSensorPanelWidgetBase::BuildToolPanelHeader(const FText& Title)
{
	if(IsWorkspaceOwned())
    {
        // Keep the title filling the remaining width: floating title drag uses
        // the same panel hit region as UXR-11, while actions stay compact.
        return SNew(SBox).MinDesiredHeight_Lambda([this](){return FSensorOwnedPresentationStyle::ControlHeight*GetSensorToolVisualScale();})
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,FSensorOwnedPresentationStyle::Gap,0)
         [SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::HeaderButton())
          .Visibility_Lambda([this](){return SupportsOwnedDocking()&&!GetPanelBackLabel().IsEmpty()?EVisibility::Visible:EVisibility::Collapsed;})
          .IsEnabled_Lambda([this](){return CanNavigatePanelBack();}).Text(FText::FromString(TEXT("←")))
          .ToolTipText_Lambda([this](){return GetPanelBackLabel();})
          .OnClicked_Lambda([this](){NavigatePanelBack();return FReply::Handled();})]
         +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
         [SNew(STextBlock).Font_Lambda([this](){return FSensorOwnedPresentationStyle::Font(FSensorOwnedPresentationStyle::TitleSize,GetSensorToolVisualScale(),true);})
          .ColorAndOpacity(GetToolTextColor()).OverflowPolicy(ETextOverflowPolicy::Ellipsis).Text(Title)
          .ToolTipText_Lambda([this](){return FText::FromString(bWorkspaceDocked?TEXT("작업 사이드바"):TEXT("제목을 드래그해 이동 · 우하단 모서리에서 크기 조절"));})]
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
         [SNew(SComboButton).Visibility_Lambda([this](){return bWorkspaceDocked&&ToolRole==ESensorToolPanelRole::Monitor?EVisibility::Collapsed:EVisibility::Visible;})
          .ComboButtonStyle(&FSensorOwnedPresentationStyle::ComboButton()).ButtonStyle(&FSensorOwnedPresentationStyle::HeaderButton()).ContentPadding(FMargin(0)).HasDownArrow(false)
          .ToolTipText(NSLOCTEXT("SensorToolPanel","MoreTip","이 창의 배치 설정"))
          .OnGetMenuContent_Lambda([this]()
          {
              auto Menu=SNew(SVerticalBox);
              Menu->AddSlot().AutoHeight()[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton())
                .Text(FText::FromString(IsPanelCollapsed()?TEXT("펼치기"):TEXT("접기")))
                .OnClicked_Lambda([this](){FSlateApplication::Get().DismissAllMenus();if(bWorkspaceDocked&&ToolRole!=ESensorToolPanelRole::Monitor&&ToolWorkspace.IsValid())ToolWorkspace->CollapseSidebar();else TogglePanelCollapsed();return FReply::Handled();})];
              if(bWorkspaceDocked&&ToolRole!=ESensorToolPanelRole::Monitor)
                  Menu->AddSlot().AutoHeight()[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton())
                    .Text(ToolWorkspace.IsValid()&&ToolWorkspace->IsSidebarWide()?SensorToolUiText::DefaultWidth():SensorToolUiText::WideWidth())
                    .ToolTipText(SensorToolUiText::WidthHint())
                    .OnClicked_Lambda([this](){FSlateApplication::Get().DismissAllMenus();if(ToolWorkspace.IsValid())ToolWorkspace->ToggleSidebarWidth();return FReply::Handled();})];
              Menu->AddSlot().AutoHeight()[SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::QuietButton())
                .Text(NSLOCTEXT("SensorToolPanel","ResetPlacement","이 창 위치·크기 초기화"))
                .OnClicked_Lambda([this](){FSlateApplication::Get().DismissAllMenus();if(ToolWorkspace.IsValid())ToolWorkspace->ResetOwnedPanelLayout(ToolRole);return FReply::Handled();})];
              return Menu;
          })
          .ButtonContent()[SNew(SBox).WidthOverride_Lambda([this](){return 18.f*GetSensorToolVisualScale();}).HeightOverride_Lambda([this](){return 18.f*GetSensorToolVisualScale();})[SNew(SSensorToolIcon).Kind(5).Color(FSensorOwnedPresentationStyle::Muted())]]]
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
         [SNewSensorTool(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::HeaderButton()).Text(FText::FromString(TEXT("×")))
          .ToolTipText(SensorToolUiText::HidePanelHint()).OnClicked_Lambda([this](){if(ToolWorkspace.IsValid())ToolWorkspace->SetPanelOpen(ToolRole,false);return FReply::Handled();})]];
    }
	return SNew(SHorizontalBox)
	+SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).ColorAndOpacity(FSensorToolWorkspaceStyle::Text()).Font_Lambda([this](){return FSensorToolWorkspaceStyle::Font(18,GetSensorToolFontScale());}).Text(Title).ToolTipText(FText::FromString(TEXT("제목을 드래그해 이동 · 우하단에서 크기 조절")))]
	+SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNewSensorTool(SButton).ButtonStyle(&FSensorToolWorkspaceStyle::Button()).Text_Lambda([this](){return FText::FromString(IsPanelCollapsed()?TEXT("펼치기"):TEXT("접기"));}).OnClicked_Lambda([this](){TogglePanelCollapsed();return FReply::Handled();})]
	+SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNewSensorTool(SButton).ButtonStyle(&FSensorToolWorkspaceStyle::Button()).Text(FText::FromString(TEXT("숨김"))).IsEnabled_Lambda([this](){return IsWorkspaceOwned();}).OnClicked_Lambda([this](){if(ToolWorkspace.IsValid())ToolWorkspace->SetPanelOpen(ToolRole,false);return FReply::Handled();})]
	+SHorizontalBox::Slot().AutoWidth()[SNewSensorTool(SButton).ButtonStyle(&FSensorToolWorkspaceStyle::Button()).Text(FText::FromString(TEXT("초기화"))).OnClicked_Lambda([this](){if(ToolWorkspace.IsValid())ToolWorkspace->ResetOwnedPanelLayout(ToolRole);else{ResetPanelSize();ResetPanelPosition();}return FReply::Handled();})];
}

FText UVirtualSensorPanelWidgetBase::GetPanelBackLabel() const
{return ToolWorkspace.IsValid()?ToolWorkspace->GetPanelBackLabel(ToolRole):FText::GetEmpty();}
bool UVirtualSensorPanelWidgetBase::CanNavigatePanelBack() const
{return ToolWorkspace.IsValid()&&ToolWorkspace->CanNavigatePanelBack(ToolRole);}
bool UVirtualSensorPanelWidgetBase::NavigatePanelBack()
{return ToolWorkspace.IsValid()&&ToolWorkspace->NavigatePanelBack(ToolRole);}

TSharedRef<SWidget> UVirtualSensorPanelWidgetBase::BuildToolPanelFrame(TSharedRef<SWidget> Content)
{
    return SNew(SOverlay)
        +SOverlay::Slot()[Content]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
        [SNew(SBox).WidthOverride(ResizeHandleSize).HeightOverride(ResizeHandleSize)
         .Visibility_Lambda([this](){return SupportsOwnedDocking()&&!bWorkspaceDocked&&bPanelResizable&&!bPanelCollapsed?EVisibility::Visible:EVisibility::Collapsed;})
         [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBorder")).Padding(4)
          .Cursor(EMouseCursor::ResizeSouthEast).ToolTipText(SensorToolUiText::ResizeHint())
          [SNewSensorTool(STextBlock).Text(NSLOCTEXT("SensorNavigation","ResizeMark","↘")).ColorAndOpacity(GetToolMutedColor())]]];
}
