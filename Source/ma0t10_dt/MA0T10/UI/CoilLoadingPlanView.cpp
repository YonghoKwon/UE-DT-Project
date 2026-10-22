#include "CoilLoadingMonitor.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

namespace
{
class SCoilPlan final:public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCoilPlan){}SLATE_END_ARGS()
    void Construct(const FArguments&,UCoilLoadingPlanView* View){Owner=View;}
    FVector2D ComputeDesiredSize(float) const override{return FVector2D(400,240);}
    int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const override
    {
        auto* O=Owner.Get();auto* M=O?O->Monitor.Get():nullptr;auto* S=M?M->Simulation():nullptr;if(!S)return L;const FVector2D Size=G.GetLocalSize();const bool Section=S->View()==2;
        const double SpanX=Section?S->Settings.HoldWidth:S->Settings.HoldLength,SpanY=Section?FMath::Max(S->Settings.HoldHeight,S->PreviewPlacement().Center.Z+2):S->Settings.HoldWidth;
        const double Scale=FMath::Min((Size.X-50)/SpanX,(Size.Y-70)/SpanY);const FVector2D Start((Size.X-SpanX*Scale)*.5,35+(Size.Y-70-SpanY*Scale)*.5);
        const auto Point=[&](double X,double Y){return FVector2f(Start.X+(X+SpanX*.5)*Scale,Start.Y+(Section?SpanY-Y:Y+SpanY*.5)*Scale);};
        const auto Line=[&](TArray<FVector2f> P,FLinearColor C,float W=1){FSlateDrawElement::MakeLines(Out,L+1,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,W);};
        const auto Text=[&](FVector2D P,const FString& T){FSlateDrawElement::MakeText(Out,L+2,G.ToPaintGeometry(FVector2D(800,25),FSlateLayoutTransform(P)),T,FSensorOwnedPresentationStyle::Font(12),ESlateDrawEffect::None,FSensorOwnedPresentationStyle::Text());};
        Text({8,0},Section?FString::Printf(TEXT("횡단면 · %d열 · 눈(eye) 선수미 방향 / 초록=고스트"),S->NextRow):TEXT("적재도 · X 선수 → / Y 좌현 ↓ · 초록=다음 배치"));
        Line({Point(-SpanX*.5,Section?0:-SpanY*.5),Point(SpanX*.5,Section?0:-SpanY*.5),Point(SpanX*.5,SpanY*(Section?1:.5)),Point(-SpanX*.5,SpanY*(Section?1:.5)),Point(-SpanX*.5,Section?0:-SpanY*.5)},FSensorOwnedPresentationStyle::Accent(),2);
        const auto Draw=[&](const CoilLoading::FPlacement& P,bool Ghost,int32 Index)
        {
            if(Section&&P.Row!=S->NextRow)return;const FLinearColor Color=S->IsHighlighted(Index)?FLinearColor(0,1,1):Ghost?FLinearColor(.2,1,.45):FLinearColor(.68,.73,.8);
            if(Section)
            {
                for(double R:{P.Spec.OuterDiameter()*.5,P.Spec.ClearBore()*.5}){TArray<FVector2f> Ring;for(int I=0;I<=32;++I){const double A=I*2*PI/32;Ring.Add(Point(P.Center.Y+R*FMath::Cos(A),P.Center.Z+R*FMath::Sin(A)));}Line(Ring,Color,Ghost?3:2);}
            }
            else
            {
                const double A=P.Spec.OverallWidth()*.5,B=P.Spec.OuterDiameter()*.5;Line({Point(P.Center.X-A,P.Center.Y-B),Point(P.Center.X+A,P.Center.Y-B),Point(P.Center.X+A,P.Center.Y+B),Point(P.Center.X-A,P.Center.Y+B),Point(P.Center.X-A,P.Center.Y-B)},Color,Ghost?3:1);
            }
        };
        for(int32 I=0;I<S->Plan().Num();++I)Draw(S->Plan()[I],false,I);Draw(S->PreviewPlacement(),true,S->Plan().Num());Text({8,Size.Y-24},TEXT("해석상의 계획 위치 · Chaos 실제 위치/센서 실측은 비교 수치에 별도 표시"));return L+2;
    }
private:TWeakObjectPtr<UCoilLoadingPlanView> Owner;
};
}
TSharedRef<SWidget> UCoilLoadingPlanView::RebuildWidget(){return SNew(SCoilPlan,this);}
