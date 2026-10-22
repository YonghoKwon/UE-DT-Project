#include "ShipHatchMonitorWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"

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
        if(M->IsSiteVisualization())
        {
            const auto* S=M->GetSimulation();auto* PC=M->GetOwningPlayer();if(!S||!PC)return Layer;
            int32 W,H;PC->GetViewportSize(W,H);if(W<=0||H<=0)return Layer;
            const auto Root=M->GetCachedGeometry();const auto Size=Root.GetLocalSize();
            const auto Project=[&](FVector World,FVector2f& Pixel){FVector2D P;if(!PC->ProjectWorldLocationToScreen(World,P))return false;Pixel=FVector2f(G.AbsoluteToLocal(Root.LocalToAbsolute(P*Size/FVector2D(W,H))));return true;};
            const auto Line=[&](FVector A,FVector B,FLinearColor C){FVector2f P,Q;if(Project(A,P)&&Project(B,Q))FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),{P,Q},ESlateDrawEffect::None,C,true,2);};
            const FTransform Pose=S->GetActorTransform();const auto Frame=[&](const FString& Name,FTransform F)
            {
                const auto O=F.GetLocation();Line(O,O+F.GetUnitAxis(EAxis::X)*180,FLinearColor::Red);Line(O,O+F.GetUnitAxis(EAxis::Y)*180,FLinearColor::Green);Line(O,O+F.GetUnitAxis(EAxis::Z)*180,FLinearColor::Blue);
                FVector2f P;if(Project(O,P))FSlateDrawElement::MakeText(Out,Layer+2,G.ToPaintGeometry(FVector2D(220,28),FSlateLayoutTransform(FVector2D(P)+FVector2D(5,-22))),Name,FSensorOwnedPresentationStyle::Font(12,M->GetFontScale()),ESlateDrawEffect::None,FLinearColor::White);
            };
            static const FString Names[]={TEXT("Mesh_origin"),TEXT("Crane"),TEXT("Trolley"),TEXT("Cabin"),TEXT("os_lidar_boom"),TEXT("os_lidar_cabin_01"),TEXT("os_lidar_cabin_02"),TEXT("Grab")};
            Frame(Names[0],Pose);const auto At=[&](FVector P){return FTransform(Pose.GetRotation(),Pose.TransformPosition(P*100));};
            Frame(Names[1],At({0,S->SeaRailY(),S->BoomSoffitZ()}));Frame(Names[2],At({0,S->GrabLip().Y,S->BoomSoffitZ()+.75}));Frame(Names[3],At({S->CabX,S->CabY,S->HoldDepth+S->CabHeight}));
            for(int I=0;I<3;++I)if(const auto* A=S->SensorAt(I))Frame(Names[I+4],A->GetActorTransform());
            Frame(Names[7],At(S->GrabLip()+FVector(0,0,3)));
            // When the physical grab is off this is a display-only pose envelope;
            // switching visualization must not add an acquisition obstruction.
            {
                const double Angle=FMath::DegreesToRadians(S->GrabOpening()*65),HalfX=FMath::Sqrt(FMath::Square(2.1*FMath::Cos(Angle))+FMath::Square(1.55*FMath::Sin(Angle)));
                const FVector Min=S->GrabLip()+FVector(-HalfX,-2.15,-.05),Max=S->GrabLip()+FVector(HalfX,2.15,6.1);const auto C=S->IsGrabWorkActive()?FLinearColor::Red:FLinearColor::Green;
                for(int Axis=0;Axis<3;++Axis)for(int A=0;A<2;++A)for(int B=0;B<2;++B){FVector P=Min,Q=Min;P[(Axis+1)%3]=Q[(Axis+1)%3]=A?Max[(Axis+1)%3]:Min[(Axis+1)%3];P[(Axis+2)%3]=Q[(Axis+2)%3]=B?Max[(Axis+2)%3]:Min[(Axis+2)%3];Q[Axis]=Max[Axis];Line(Pose.TransformPosition(P*100),Pose.TransformPosition(Q*100),C);}
            }
            return Layer+2;
        }
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
