#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "SensorToolToolbarWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"

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
