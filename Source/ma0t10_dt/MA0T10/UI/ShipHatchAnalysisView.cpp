#include "ShipHatchMonitorWidget.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchModel.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
using Style=FSensorOwnedPresentationStyle;
class SHatchGrid final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHatchGrid){} SLATE_END_ARGS()
    void Construct(const FArguments&,UShipHatchAnalysisView* InOwner){Owner=InOwner;}
    virtual FVector2D ComputeDesiredSize(float) const override
    {const auto* O=Owner.Get();const auto* M=O?O->Monitor.Get():nullptr;return FVector2D(240,O&&O->bLegend?64*(M?M->GetFontScale():1):160);}
    FSlateRect Plot(const FGeometry& G) const
    {
        const auto* O=Owner.Get();const auto* M=O?O->Monitor.Get():nullptr;
        const auto* S=M?M->GetAnalysisSettings():nullptr;
        const double Width=G.GetLocalSize().X-(O&&O->bHeatmap?60:24),Height=G.GetLocalSize().Y-24;
        const double Aspect=S?S->OpeningWidth/S->OpeningLength:23./20.;
        const double W=FMath::Max(1.,FMath::Min(Width,Height*Aspect)),H=W/Aspect;
        const double X=(Width-W)*.5+12,Y=(G.GetLocalSize().Y-H)*.5;
        return FSlateRect(X,Y,X+W,Y+H);
    }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto* O=Owner.Get();const auto* M=O?O->Monitor.Get():nullptr;
        const auto* A=M?M->GetDisplayAnalysis():nullptr;const auto* S=M?M->GetAnalysisSettings():nullptr;
        if(!A||!S||A->Cells.IsEmpty())return Layer;
        if(O->bLegend)
        {
            const double Width=G.GetLocalSize().X,Scale=M->GetFontScale();
            auto Box=[&](double X,double Y,double W,double H,FLinearColor Color){FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(W,H),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);};
            auto Text=[&](double X,double Y,const FString& Value){FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(Width,24*Scale),FSlateLayoutTransform(FVector2D(X,Y))),Value,Style::Font(Style::LabelSize,Scale),ESlateDrawEffect::None,Style::Text());};
            if(M->GetColor()==1)
            {
                const TCHAR* Names[]={TEXT("원료"),TEXT("해치 코밍"),TEXT("홀드 구조"),TEXT("바깥")};const FLinearColor Colors[]={FLinearColor(1,.6,.05),FLinearColor(.05,1,.2),FLinearColor(.1,.55,1),FLinearColor(1,.05,.05)};
                for(int32 I=0;I<4;++I){double X=(I%2)*Width*.5,Y=(I/2)*24*Scale;Box(X,Y+4*Scale,10*Scale,10*Scale,Colors[I]);Text(X+18*Scale,Y,Names[I]);}
            }
            else if(M->GetColor()==0)
            {
                for(int32 I=0;I<64;++I)Box(Width*I/64,8*Scale,Width/64+1,8*Scale,FLinearColor::FromSRGBColor(ShipHatch::Jet(double(I)/63)));
                Text(0,22*Scale,TEXT("0 m"));Text(Width-65*Scale,22*Scale,FString::Printf(TEXT("%.1f m"),S->Depth));
            }
            return Layer+1;
        }
        const auto R=Plot(G);const double W=R.Right-R.Left,H=R.Bottom-R.Top;
        auto Rect=[&](double X,double Y,double DX,double DY,FLinearColor C)
        {FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(DX,DY),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C);};
        Rect(R.Left-3,R.Top-3,W+6,H+6,Style::Border());
        // Fit the aperture with a 5% masked ship border on each side. The grid
        // retains full-hold SI indexing and full-hold capacity denominators.
        const double ExtW=S->OpeningWidth*1.1,ExtL=S->OpeningLength*1.1;
        const double OX=R.Left+W/22,OY=R.Top+H/22,OW=W/1.1,OH=H/1.1;
        Rect(R.Left,R.Top,W,H,Style::Color(48,57,65));
        for(int32 Y=0;Y<A->NX;++Y)for(int32 X=0;X<A->NY;++X)
        {
            const auto& Cell=A->Cells[X*A->NX+Y];
            const double L0=FMath::Max(Y*A->DX-S->Length*.5,-S->OpeningLength*.5),L1=FMath::Min((Y+1)*A->DX-S->Length*.5,S->OpeningLength*.5);
            const double B0=FMath::Max(X*A->DY-S->Width*.5,-S->OpeningWidth*.5),B1=FMath::Min((X+1)*A->DY-S->Width*.5,S->OpeningWidth*.5);
            if(L1<=L0||B1<=B0)continue;
            const double CW=(B1-B0)/ExtW*W,CH=(L1-L0)/ExtL*H;
            const FLinearColor Color=!Cell.Count?Style::Border():O->bHeatmap?FLinearColor::FromSRGBColor(ShipHatch::Jet(Cell.Mean()/S->Depth)):
                S->Cargo==ShipHatch::ECargo::Coal?Style::Color(82,92,103):Style::Color(172,111,75);
            Rect(R.Left+(B0/ExtW+.5)*W,R.Top+(L0/ExtL+.5)*H,CW-(O->bHeatmap?FMath::Min(.65,CW*.12):0),CH-(O->bHeatmap?FMath::Min(.65,CH*.12):0),Color);
        }
        // Same SI axes as analysis: screen horizontal Y/breadth, vertical X/length.
        const double T=O->bHeatmap?1:2;
        Rect(OX,OY,OW,T,Style::Accent());Rect(OX,OY+OH-T,OW,T,Style::Accent());
        Rect(OX,OY,T,OH,Style::Accent());Rect(OX+OW-T,OY,T,OH,Style::Accent());
        if(O->bHeatmap)
        {
            for(int32 I=0;I<64;++I)Rect(R.Right+16,R.Top+H*I/64,10,H/64+1,FLinearColor::FromSRGBColor(ShipHatch::Jet(1.-double(I)/63)));
            auto Text=[&](double Y,const FString& Value){FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(52,20),FSlateLayoutTransform(FVector2D(R.Right+12,Y))),Value,Style::Font(Style::LabelSize,M->GetFontScale()),ESlateDrawEffect::None,Style::Text());};
            Text(R.Top-20,FString::Printf(TEXT("%.1f m"),S->Depth));Text(R.Bottom,TEXT("0 m"));
        }
        return Layer+1;
    }
    virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E) override
    {
        const auto* O=Owner.Get();const auto* M=O?O->Monitor.Get():nullptr;
        if(!O||!M||!O->bHeatmap)return FReply::Unhandled();
        const auto R=Plot(G);const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
        const auto* A=M->GetDisplayAnalysis();const auto* S=M->GetAnalysisSettings();
        int32 Cell=-1;
        if(A&&S&&P.X>=R.Left&&P.X<R.Right&&P.Y>=R.Top&&P.Y<R.Bottom)
        {const double B=((P.X-R.Left)/(R.Right-R.Left)-.5)*S->OpeningWidth*1.1,L=((P.Y-R.Top)/(R.Bottom-R.Top)-.5)*S->OpeningLength*1.1;
         if(FMath::Abs(B)<=S->OpeningWidth*.5&&FMath::Abs(L)<=S->OpeningLength*.5){const int32 X=FMath::FloorToInt((B+S->Width*.5)/A->DY),Y=FMath::FloorToInt((L+S->Length*.5)/A->DX);Cell=X*A->NX+Y;}}
        if(Cell!=HoveredCell||TooltipBuild!=M->AnalysisBuilds){HoveredCell=Cell;TooltipBuild=M->AnalysisBuilds;SetToolTipText(M->CellTooltip(Cell));}
        return FReply::Handled();
    }
private:
    TWeakObjectPtr<UShipHatchAnalysisView> Owner;
    int32 HoveredCell=MIN_int32;
    uint64 TooltipBuild=0;
};
}
TSharedRef<SWidget> UShipHatchAnalysisView::RebuildWidget(){return SNew(SHatchGrid,this);}
