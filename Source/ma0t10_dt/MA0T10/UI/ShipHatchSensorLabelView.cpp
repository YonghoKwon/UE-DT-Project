#include "ShipHatchMonitorWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

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
            FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),TArray<FVector2f>{A,E},ESlateDrawEffect::None,C,true,FMath::Max(2.f,2.f/G.GetAccumulatedLayoutTransform().GetScale()));
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
    if(I<0||I>=3||!SensorBadgeLinkVisible[I]||!IsVisible()||!(bSplit||ActiveView<2))return false;
    Anchor=SensorBadgeAnchors[I];End=SensorBadgeEnds[I];return true;
}
