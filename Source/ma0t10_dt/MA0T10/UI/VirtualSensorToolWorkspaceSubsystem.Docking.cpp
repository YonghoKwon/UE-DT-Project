#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "SensorToolToolbarWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"
#include "SensorToolUiText.h"
#include "SensorPlacementInteraction.h"

TArray<FBox2D> UVirtualSensorToolWorkspaceSubsystem::GetOwnedDiagnosticExclusionRects() const
{
    TArray<FBox2D> Rects;Rects.Reserve(10);
    const auto* View=GetWorld()?GetWorld()->GetGameViewport():nullptr;
    if(!View||!View->Viewport||!Canvas||!View->GetGameViewportWidget().IsValid())return Rects;
    const auto& Geometry=View->GetGameViewportWidget()->GetCachedGeometry();
    const FVector2D Local=Geometry.GetLocalSize();if(Local.GetMin()<=0)return Rects;
    const FIntPoint Pixels=View->Viewport->GetSizeXY();const FVector2D Scale(Pixels.X/Local.X,Pixels.Y/Local.Y);
    auto Add=[&](const FGeometry& G)
    {
        if(G.GetLocalSize().GetMin()<=0)return;
        const FVector2D A=Geometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector))*Scale;
        const FVector2D B=Geometry.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()))*Scale;
        Rects.Add(FBox2D(A-FVector2D(6,6),B+FVector2D(6,6)));
    };
    if(bSidebarMode)
    {
        const auto& G=Canvas->GetCachedGeometry();const FVector2D V=G.GetLocalSize();
        const double Width=bSidebarExpanded?FMath::Min(double(GetSidebarWidth()),V.X*.48):FSensorOwnedPresentationStyle::RailWidth*GetOwnedPanelFontScale()*GetOwnedPixelScale();
        const FVector2D A=Geometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector))*Scale;
        const FVector2D B=Geometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2D(Width,V.Y)))*Scale;
        Rects.Add(FBox2D(A-FVector2D(6,6),B+FVector2D(6,6)));
        const double Bottom=FSensorOwnedPresentationStyle::StatusHeight*GetOwnedPanelFontScale()*GetOwnedPixelScale();
        const FVector2D C=Geometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2D(0,V.Y-Bottom)))*Scale;
        Rects.Add(FBox2D(C,FVector2D(Pixels.X,Pixels.Y)));
        if(const auto* Bar=Cast<USensorToolToolbarWidget>(Toolbar))if(auto Card=Bar->GetSlabSummaryCard())Add(Card->GetCachedGeometry());
    }
    for(const auto& Pair:Panels)if(Pair.Value&&IsPanelOpen(Pair.Key))Add(Pair.Value->GetCachedGeometry());
    return Rects;
}

float UVirtualSensorToolWorkspaceSubsystem::GetOwnedPixelScale() const
{return LastOwnedPixelScale>0?LastOwnedPixelScale:1.f;}
float UVirtualSensorToolWorkspaceSubsystem::MeasureOwnedPixelScale() const
{
    const auto* View=GetWorld()?GetWorld()->GetGameViewport():nullptr;
    if(!Canvas||!View||!View->Viewport||View->Viewport->GetSizeXY().X<=0||Canvas->GetCachedGeometry().GetLocalSize().X<320)return 1;
    return FMath::Clamp(float(Canvas->GetCachedGeometry().GetLocalSize().X/View->Viewport->GetSizeXY().X),.5f,3.f);
}
float UVirtualSensorToolWorkspaceSubsystem::GetSidebarWidth() const
{return (bSidebarWide?FSensorOwnedPresentationStyle::WideSidebarWidth:FSensorOwnedPresentationStyle::SidebarWidth)*GetOwnedPanelFontScale()*GetOwnedPixelScale();}
void UVirtualSensorToolWorkspaceSubsystem::ToggleSidebarWidth()
{
    if(!bSidebarMode)return;
    bSidebarWide=!bSidebarWide;RefreshDockedLayout();
    if(Toolbar)Toolbar->InvalidateLayoutAndVolatility();
}
void UVirtualSensorToolWorkspaceSubsystem::ClearPanelBackTarget(ESensorToolPanelRole Role)
{PanelBackTargets.Remove(Role);}
void UVirtualSensorToolWorkspaceSubsystem::SetPanelBackToTools(ESensorToolPanelRole Role)
{
    FPanelBackTarget Target;Target.bTools=true;
    Target.Label=SensorToolUiText::BackTools();PanelBackTargets.Add(Role,Target);
}
FText UVirtualSensorToolWorkspaceSubsystem::GetPanelBackLabel(ESensorToolPanelRole Role) const
{
    const auto* Panel=GetOwnedPanel(Role);
    if(!bSidebarMode||!Panel||!Panel->SupportsOwnedDocking())return FText::GetEmpty();
    const auto* Target=PanelBackTargets.Find(Role);return Target?Target->Label:FText::GetEmpty();
}
bool UVirtualSensorToolWorkspaceSubsystem::CanNavigatePanelBack(ESensorToolPanelRole Role) const
{
    // Hiding a settings/placement owner ends manipulation by contract. Do not let
    // a presentation-only Back action silently end an active interaction.
    return !GetPanelBackLabel(Role).IsEmpty()&&(!PlacementInteraction||!PlacementInteraction->IsManipulating());
}
bool UVirtualSensorToolWorkspaceSubsystem::NavigatePanelBack(ESensorToolPanelRole Role)
{
    if(!CanNavigatePanelBack(Role))return false;
    const auto Target=PanelBackTargets.FindChecked(Role);
    TGuardValue<bool> Guard(bNavigatingBack,true);
    if(Target.bTools)
    {
        if(auto* Bar=Cast<USensorToolToolbarWidget>(Toolbar))if(Bar->ShowOwnedToolsMenu()){SetPanelOpen(Role,false);PanelBackTargets.Remove(Role);return true;}
        return false;
    }
    if(!GetOwnedPanel(Target.Role))return false;
    SetPanelOpen(Target.Role,true);
    if(!IsPanelOpen(Target.Role))return false;
    PanelBackTargets.Remove(Role);
    if(Target.ParentBack)PanelBackTargets.Add(Target.Role,*Target.ParentBack);else PanelBackTargets.Remove(Target.Role);
    return true;
}
void UVirtualSensorToolWorkspaceSubsystem::CollapseSidebar()
{if(bSidebarMode&&bSidebarExpanded)SetPanelOpen(SidebarRole,false);}
void UVirtualSensorToolWorkspaceSubsystem::SetSidebarMode(bool Enabled)
{
    if(bSidebarMode==Enabled)return;
    if(!Enabled)for(auto& Pair:Panels)if(Pair.Value)Pair.Value->LeaveWorkspaceDock();
    bSidebarMode=Enabled;LastCanvasSize=FVector2D::ZeroVector;
    if(Enabled){SidebarOpen.Reset();SidebarRole=ESensorToolPanelRole::Placement;bSidebarExpanded=true;SidebarOpen.Add(SidebarRole);SidebarOpen.Add(ESensorToolPanelRole::Monitor);ToolbarReservedTop=0;}
    for(auto& Pair:Panels)if(Pair.Value){Pair.Value->SetVisibility(IsPanelOpen(Pair.Key)?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);RestorePanel(Pair.Key);}
    RefreshDockedLayout();
}
FVector2D UVirtualSensorToolWorkspaceSubsystem::GetDockedCardMinimum() const
{const float Scale=GetOwnedPanelFontScale()*GetOwnedPixelScale();return FVector2D(260.f,180.f)*FMath::Max(.5f,Scale);}
FBox2D UVirtualSensorToolWorkspaceSubsystem::GetDockedCardBounds() const
{
    if(!Canvas)return FBox2D(FVector2D::ZeroVector,FVector2D(100000,100000));const FVector2D V=Canvas->GetCachedGeometry().GetLocalSize();
    const float Scale=GetOwnedPanelFontScale()*GetOwnedPixelScale(),Bottom=FSensorOwnedPresentationStyle::StatusHeight*Scale;
    // Right of the sidebar, above the status bar and inside the 3D view.
    return FBox2D(FVector2D(FMath::Min(GetSidebarWidth(),float(V.X*.48))+8.f,8.f),FVector2D(float(V.X)-8.f,float(V.Y)-Bottom-8.f));
}
void UVirtualSensorToolWorkspaceSubsystem::ClampDockedCard(FVector2D& Position,FVector2D& Size) const
{
    if(!Canvas)return;const FBox2D B=GetDockedCardBounds();
    const float Left=B.Min.X,Top=B.Min.Y,Right=B.Max.X,Lower=B.Max.Y;
    const FVector2D Min=GetDockedCardMinimum();
    Size.X=FMath::Clamp(Size.X,FMath::Min(Min.X,Right-Left),FMath::Max(1.f,Right-Left));Size.Y=FMath::Clamp(Size.Y,FMath::Min(Min.Y,Lower-Top),FMath::Max(1.f,Lower-Top));
    Position.X=FMath::Clamp(Position.X,Left,FMath::Max(Left,Right-Size.X));Position.Y=FMath::Clamp(Position.Y,Top,FMath::Max(Top,Lower-Size.Y));
}
void UVirtualSensorToolWorkspaceSubsystem::SetDockedCardRect(FVector2D Position,FVector2D Size)
{
    ClampDockedCard(Position,Size);bDockedCardCustom=true;DockedCardPosition=Position;DockedCardSize=Size;
    if(auto* P=GetOwnedPanel(ESensorToolPanelRole::Monitor))P->ApplyWorkspaceDockRect(Position,Size);
}
void UVirtualSensorToolWorkspaceSubsystem::RefreshDockedLayout()
{
    if(!bSidebarMode||!Canvas)return;
    const FVector2D V=Canvas->GetCachedGeometry().GetLocalSize();if(V.X<320||V.Y<200)return;
    const float Scale=GetOwnedPanelFontScale()*GetOwnedPixelScale(),Rail=FSensorOwnedPresentationStyle::RailWidth*Scale,Bottom=FSensorOwnedPresentationStyle::StatusHeight*Scale;
    const float Width=FMath::Min(GetSidebarWidth(),float(V.X*.48));
    for(auto& Pair:Panels)if(Pair.Key!=ESensorToolPanelRole::TestSet)if(auto* P=Pair.Value.Get())if(P->SupportsOwnedDocking())
    {
        if(Pair.Key==ESensorToolPanelRole::Monitor)
        {
            const float CardWidth=FMath::Min(400.f*Scale,float(V.X-Width-32));
            const float CardHeight=FMath::Min(288.f*Scale,float(V.Y*.43));
            FVector2D Position(V.X-CardWidth-16,V.Y-Bottom-CardHeight-16),Size(CardWidth,CardHeight);
            if(bDockedCardCustom){Position=DockedCardPosition;Size=DockedCardSize;ClampDockedCard(Position,Size);}
            P->ApplyWorkspaceDockRect(Position,Size);
        }
        else P->ApplyWorkspaceDockRect(FVector2D(Rail,0),FVector2D(Width-Rail,V.Y-Bottom));
        PendingInitialLayouts.Remove(Pair.Key);
    }
}
