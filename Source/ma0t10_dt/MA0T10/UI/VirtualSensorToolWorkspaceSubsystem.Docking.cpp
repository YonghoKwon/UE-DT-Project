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
{return FSensorOwnedPresentationStyle::SidebarWidth*GetOwnedPanelFontScale()*GetOwnedPixelScale();}
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
void UVirtualSensorToolWorkspaceSubsystem::RefreshDockedLayout()
{
    if(!bSidebarMode||!Canvas)return;
    const FVector2D V=Canvas->GetCachedGeometry().GetLocalSize();if(V.X<320||V.Y<200)return;
    const float Scale=GetOwnedPanelFontScale()*GetOwnedPixelScale(),Rail=FSensorOwnedPresentationStyle::RailWidth*Scale,Bottom=FSensorOwnedPresentationStyle::StatusHeight*Scale;
    const float Width=FMath::Min(GetSidebarWidth(),float(V.X*.48));
    for(auto& Pair:Panels)if(auto* P=Pair.Value.Get())if(P->SupportsOwnedDocking())
    {
        if(Pair.Key==ESensorToolPanelRole::Monitor)
        {
            const float CardWidth=FMath::Min(400.f*Scale,float(V.X-Width-32));
            const float CardHeight=FMath::Min(288.f*Scale,float(V.Y*.43));
            P->ApplyWorkspaceDockRect(FVector2D(V.X-CardWidth-16,V.Y-Bottom-CardHeight-16),FVector2D(CardWidth,CardHeight));
        }
        else P->ApplyWorkspaceDockRect(FVector2D(Rail,0),FVector2D(Width-Rail,V.Y-Bottom));
        PendingInitialLayouts.Remove(Pair.Key);
    }
}
