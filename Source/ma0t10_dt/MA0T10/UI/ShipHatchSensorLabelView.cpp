#include "ShipHatchMonitorWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

namespace
{
class SHatchSensorLabels final:public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHatchSensorLabels){} SLATE_END_ARGS()
    void Construct(const FArguments&,UShipHatchMonitorWidget* InMonitor){Monitor=InMonitor;}
    FVector2D ComputeDesiredSize(float) const override{return FVector2D::ZeroVector;}
    int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto* M=Monitor.Get();if(!M)return Layer;
        const FLinearColor Colors[]={FLinearColor(.1,.9,1),FLinearColor(1,.6,.06),FLinearColor(.9,.2,.8)};
        auto Local=[&](FVector2D P){return FVector2f(G.AbsoluteToLocal(M->GetCachedGeometry().LocalToAbsolute(P)));};
        for(int I=0;I<3;++I)
        {
            FVector2D Anchor,End;if(!M->GetSensorBadgeLink(I,Anchor,End))continue;
            const auto A=Local(Anchor),E=Local(End);const auto C=M->GetSimulation()&&M->GetSimulation()->SensorEnabled(I)?Colors[I]:FSensorOwnedPresentationStyle::Muted();
            const auto Center=M->OverlayExclusionRect();const double Pad=6*M->GetFontScale();const FSlateRect Clear(Center.Left-Pad,Center.Top-Pad,Center.Right+Pad,Center.Bottom+Pad);
            const FVector2D Delta=End-Anchor;TArray<double,TInlineAllocator<6>> Cuts{0.,1.};
            if(FMath::Abs(Delta.X)>1.e-6)for(double X:{double(Clear.Left),double(Clear.Right)}){const double T=(X-Anchor.X)/Delta.X;if(T>0&&T<1)Cuts.Add(T);}
            if(FMath::Abs(Delta.Y)>1.e-6)for(double Y:{double(Clear.Top),double(Clear.Bottom)}){const double T=(Y-Anchor.Y)/Delta.Y;if(T>0&&T<1)Cuts.Add(T);}
            Cuts.Sort();for(int J=1;J<Cuts.Num();++J)if(!Clear.ContainsPoint(Anchor+Delta*((Cuts[J-1]+Cuts[J])*.5)))FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),TArray<FVector2f>{Local(Anchor+Delta*Cuts[J-1]),Local(Anchor+Delta*Cuts[J])},ESlateDrawEffect::None,C,true,FMath::Max(2.f,2.f/G.GetAccumulatedLayoutTransform().GetScale()));
            if(!M->IsOverlayAnchorVisible(Anchor))continue;
            const float Radius=4*M->GetFontScale();TArray<FVector2f,TInlineAllocator<13>> Ring;
            for(int J=0;J<=12;++J)Ring.Add(A+FVector2f(FMath::Cos(J*2*PI/12),FMath::Sin(J*2*PI/12))*Radius);
            // MakeLines owns its point payload; one tiny array per visible link.
            FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),TArray<FVector2f>(Ring),ESlateDrawEffect::None,C,true,1.5f);
            FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(FVector2D(4,4),FSlateLayoutTransform(FVector2D(A)-FVector2D(2,2))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C);
        }
        return Layer+1;
    }
private:
    TWeakObjectPtr<UShipHatchMonitorWidget> Monitor;
};
}
TSharedRef<SWidget> UShipHatchSensorLabelView::RebuildWidget(){return SNew(SHatchSensorLabels,Monitor.Get());}
bool UShipHatchMonitorWidget::GetSensorBadgeLink(int32 I,FVector2D& Anchor,FVector2D& End) const
{
    if(I<0||I>=3||!SensorBadgeLinkVisible[I]||!IsVisible()||bModelPresentation||bHudSuppressed||!(bSplit||ActiveView<2))return false;
    Anchor=SensorBadgeAnchors[I];End=SensorBadgeEnds[I];return true;
}
FSlateRect UShipHatchMonitorWidget::OverlayExclusionRect() const
{const auto W=SceneRect.Right-SceneRect.Left,H=SceneRect.Bottom-SceneRect.Top;return FSlateRect(SceneRect.Left+W*.2,SceneRect.Top+H*.2,SceneRect.Right-W*.2,SceneRect.Bottom-H*.2);}
bool UShipHatchMonitorWidget::IsOverlayAnchorVisible(FVector2D P) const
{const auto R=OverlayExclusionRect();const double Pad=6*FontScale;return !bModelPresentation&&!bHudSuppressed&&!FSlateRect(R.Left-Pad,R.Top-Pad,R.Right+Pad,R.Bottom+Pad).ContainsPoint(P);}
void UShipHatchMonitorWidget::UpdateOverlayControlVisibility()
{
    if(!PointPanel||!PointControlsToggle)return;
    const bool Available=ActiveView==1&&!bModelPresentation&&!bHudSuppressed&&!bSettings&&SceneRect.Right-SceneRect.Left>480;
    const bool Expanded=Available&&bPointControlsExpanded&&!bPresetHidesPointControls;
    PointPanel->SetVisibility(Expanded?ESlateVisibility::Visible:ESlateVisibility::Collapsed);PointControlsToggle->SetVisibility(Available?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    CastChecked<UTextBlock>(PointControlsToggle->GetContent())->SetText(FText::FromString(Expanded?TEXT("점군 설정 ∧"):TEXT("점군 설정 ▾")));
}
void UShipHatchMonitorWidget::OnPointControlsToggle()
{if(bPresetHidesPointControls){bPresetHidesPointControls=false;bPointControlsExpanded=true;}else bPointControlsExpanded=!bPointControlsExpanded;LastLayout=FVector2D::ZeroVector;Layout();}
void UShipHatchMonitorWidget::SetModelPresentation(bool Clean)
{
    if(bModelPresentation!=Clean)
    {
        bModelPresentation=Clean;if(auto* S=Simulation.Get()){S->SetSensorMarkersVisible(!Clean);if(Clean){bSavedModelRays=S->IsScanDisplayEnabled();S->SetScanDisplay(false);}else if(bSavedModelRays){S->SetScanDisplay(true);bSavedModelRays=false;}}
        bOverlayDirty=true;LastOverlayUpdate=0;UpdateOverlay();LastSensorBadgeUpdate=-1;UpdateSensorBadges();
    }
    UpdateOverlayControlVisibility();
}
