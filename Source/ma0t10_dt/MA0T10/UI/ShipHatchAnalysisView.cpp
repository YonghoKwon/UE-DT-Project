#include "ShipHatchMonitorWidget.h"
#include "ShipHatchViewTransform.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchModel.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

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
        return ShipHatch::FViewTransform::Top(S?*S:ShipHatch::FSettings()).Fit(G.GetLocalSize(),O&&O->bHeatmap,M?M->GetFontScale():1);
    }
    void Polygon(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer,const TArray<FVector2f>& P,FLinearColor Color) const
    {
        TArray<FSlateVertex> V;TArray<SlateIndex> I;
        for(auto XY:P)V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),XY,FVector2f::ZeroVector,Color.ToFColorSRGB()));
        for(int32 J=1;J+1<P.Num();++J)I.Append({SlateIndex(0),SlateIndex(J),SlateIndex(J+1)});
        FSlateDrawElement::MakeCustomVerts(Out,Layer,FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush")),V,I,nullptr,0,0);
    }
    int32 SectionPaint(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer,const UShipHatchMonitorWidget& M) const
    {
        const auto* Surface=M.GetSurface();if(!Surface)return Layer;const auto& S=Surface->Settings;const auto& Samples=M.GetSectionSamples();
        const double Scale=M.GetFontScale(),W=G.GetLocalSize().X,H=G.GetLocalSize().Y;
        const double Span=M.SectionAlongLength()?S.Length:S.Width,Opening=M.SectionAlongLength()?S.OpeningLength:S.OpeningWidth;
        double Top=S.Depth+S.Coaming+2;if(const auto* Sim=M.GetSimulation())for(int32 I=0;I<3;++I)if(Sim->SensorEnabled(I))Top=FMath::Max(Top,Sim->SensorOrigin(I).Z+2);
        const double L=46*Scale,R=FMath::Max(L+1,W-16),T=42*Scale,B=FMath::Max(T+1,H-22*Scale);
        const auto Mapping=ShipHatch::FViewTransform::Section(M.SectionAlongLength(),Span,Top);const FSlateRect SectionRect(L,T,R,B);
        auto P=[&](double X,double Z){return FVector2f(Mapping.ToPixel(M.SectionAlongLength()?FVector(X,0,Z):FVector(0,X,Z),SectionRect));};
        auto Line=[&](FVector2f A,FVector2f C,FLinearColor Color,float Thickness=1){FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),TArray<FVector2f>{A,C},ESlateDrawEffect::None,Color,true,Thickness);};
        auto Text=[&](double X,double Y,const FString& Value){FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(W,24*Scale),FSlateLayoutTransform(FVector2D(X,Y))),Value,Style::Font(Style::LabelSize,Scale),ESlateDrawEffect::None,Style::Text());};
        Text(L,0,M.SectionAlongLength()?TEXT("길이 · 육측 −Y → +Y 시점"):TEXT("폭 · +X → −X 시점"));
        Text(L,20*Scale,M.SectionAlongLength()?FString::Printf(TEXT("절단 Y %+.1f m · +X ← → −X"),M.SectionOffset()):TEXT("해측 +Y ← → 육측 −Y"));
        const int32 TickStep=5*FMath::Max(1,FMath::CeilToInt(Top*22*Scale/(FMath::Max(1.,B-T)*5)));
        for(int32 I=0;I<=int32(Top);I+=TickStep){const auto Y=P(0,I).Y;Line(FVector2f(L,Y),FVector2f(R,Y),Style::Border());Text(0,Y-7*Scale,FString::Printf(TEXT("%d m"),I));}
        // One connected stroke per nonempty run avoids antialiased end-cap
        // gaps at every triangle intersection and replaces per-segment arrays.
        TArray<FVector2f> Profile;Profile.Reserve(Samples.Num());
        auto FlushProfile=[&](){if(Profile.Num()>1)FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Profile,ESlateDrawEffect::None,Style::Warning(),true,FMath::Max(2.f,2.f/G.GetAccumulatedLayoutTransform().GetScale()));Profile.Reset();};
        for(int32 I=1;I<Samples.Num();++I)
        {
            const auto& A=Samples[I-1];const auto& C=Samples[I];if(A.Height<=1.e-4&&C.Height<=1.e-4){FlushProfile();continue;}
            Polygon(G,Out,Layer,TArray<FVector2f>{P(A.Position,0),P(C.Position,0),P(C.Position,C.Height),P(A.Position,A.Height)},S.Cargo==ShipHatch::ECargo::Coal?Style::Color(82,92,103):Style::Color(172,111,75));
            if(Profile.IsEmpty())Profile.Add(P(A.Position,A.Height));Profile.Add(P(C.Position,C.Height));
        }
        FlushProfile();
        Line(P(-Span*.5,S.Depth),P(-Span*.5,0),Style::Accent(),3);Line(P(-Span*.5,0),P(Span*.5,0),Style::Accent(),3);Line(P(Span*.5,0),P(Span*.5,S.Depth),Style::Accent(),3);
        const double Across=M.SectionAlongLength()?S.OpeningWidth:S.OpeningLength;
        if(FMath::Abs(M.SectionOffset())<=Across*.5)
        {
            for(int Sign:{-1,1}){Line(P(Sign*Span*.5,S.Depth),P(Sign*Opening*.5,S.Depth),Style::Muted(),3);Line(P(Sign*Opening*.5,S.Depth),P(Sign*Opening*.5,S.Depth+S.Coaming),Style::Success(),3);}
        }
        else Line(P(-Span*.5,S.Depth),P(Span*.5,S.Depth),Style::Muted(),3);
        Text(L,B+3,FString::Printf(TEXT("%+.1f m"),Span*.5));Text(R-65*Scale,B+3,FString::Printf(TEXT("%+.1f m"),-Span*.5));
        FVector O,D;double Fov;
        if(const auto* Sim=M.GetSimulation())for(int32 I=0;I<3;++I)if(Sim->SensorEnabled(I))
        {
            const auto Sensor=Sim->SensorOrigin(I);const double X=M.SectionAlongLength()?Sensor.X:Sensor.Y;
            const auto Icon=P(X,FMath::Min(Sensor.Z,Top));const FLinearColor C=I==0?FLinearColor(.1,.9,1):I==1?FLinearColor(1,.6,.06):FLinearColor(.9,.2,.8);
            Polygon(G,Out,Layer+3,{Icon+FVector2f(-5,-5),Icon+FVector2f(5,-5),Icon+FVector2f(0,5)},C);Line(Icon,Icon+FVector2f(-18,35),C);Line(Icon,Icon+FVector2f(18,35),C);
        }
        if(M.CameraInHold(O,D,Fov))
        {
            const double X=M.SectionAlongLength()?O.X:O.Y;
            const auto Camera=P(FMath::Clamp(X,-Span*.5,Span*.5),FMath::Clamp(O.Z,0.,Top));
            Line(Camera,Camera+FVector2f(Mapping.DirectionToPixel(D,SectionRect)).GetSafeNormal()*28,Style::Success(),2);
            Polygon(G,Out,Layer+3,{Camera+FVector2f(-4,-4),Camera+FVector2f(4,-4),Camera+FVector2f(4,4),Camera+FVector2f(-4,4)},Style::Success());
        }
        return Layer+3;
    }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto* O=Owner.Get();const auto* M=O?O->Monitor.Get():nullptr;
        if(O&&M&&O->bSection)return SectionPaint(G,Out,Layer,*M);
        const auto* A=M?M->GetDisplayAnalysis():nullptr;const auto* S=M?M->GetAnalysisSettings():nullptr;
        if(!A||!S||A->Cells.IsEmpty())return Layer;
        if(O->bLegend)
        {
            const double Width=G.GetLocalSize().X,Scale=M->GetFontScale();
            auto Box=[&](double X,double Y,double W,double H,FLinearColor Color){FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(W,H),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);};
            auto Text=[&](double X,double Y,const FString& Value){FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(Width,24*Scale),FSlateLayoutTransform(FVector2D(X,Y))),Value,Style::Font(Style::LabelSize,Scale),ESlateDrawEffect::None,Style::Text());};
            if(M->IsSensorColoring())
            {
                const TCHAR* Names[]={TEXT("붐 끝"),TEXT("운전실 좌"),TEXT("운전실 우")};const FLinearColor Colors[]={FLinearColor(.1,.9,1),FLinearColor(1,.6,.06),FLinearColor(.9,.2,.8)};
                for(int32 I=0;I<3;++I){const double X=(I%2)*Width*.5,Y=(I/2)*24*Scale;Box(X,Y+4*Scale,10*Scale,10*Scale,Colors[I]);Text(X+18*Scale,Y,Names[I]);}
            }
            else if(M->GetColor()==1)
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
        auto DirectionLabel=[&](double Y,const TCHAR* Text){FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(G.GetLocalSize().X,22*M->GetFontScale()),FSlateLayoutTransform(FVector2D(12,Y))),Text,Style::Font(Style::LabelSize,M->GetFontScale()),ESlateDrawEffect::None,Style::Text());};
        DirectionLabel(R.Top-22*M->GetFontScale(),TEXT("해측 +Y ↑ · +X ← → −X"));DirectionLabel(R.Bottom+2,TEXT("육측 −Y ↓"));
        auto Rect=[&](double X,double Y,double DX,double DY,FLinearColor C)
        {FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(DX,DY),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C);};
        Rect(R.Left-3,R.Top-3,W+6,H+6,Style::Border());
        // Fit the aperture with a 5% masked ship border on each side. The grid
        // retains full-hold SI indexing and full-hold capacity denominators.
        const auto Mapping=ShipHatch::FViewTransform::Top(*S);
        const double OX=R.Left+W/22,OY=R.Top+H/22,OW=W/1.1,OH=H/1.1;
        Rect(R.Left,R.Top,W,H,Style::Color(48,57,65));
        for(int32 Y=0;Y<A->NY;++Y)for(int32 X=0;X<A->NX;++X)
        {
            const auto& Cell=A->Cells[Y*A->NX+X];
            const double L0=FMath::Max(X*A->DX-S->Length*.5,-S->OpeningLength*.5),L1=FMath::Min((X+1)*A->DX-S->Length*.5,S->OpeningLength*.5);
            const double B0=FMath::Max(Y*A->DY-S->Width*.5,-S->OpeningWidth*.5),B1=FMath::Min((Y+1)*A->DY-S->Width*.5,S->OpeningWidth*.5);
            if(L1<=L0||B1<=B0)continue;
            const auto CellRect=Mapping.Bounds(FVector(L0,B0,0),FVector(L1,B1,0),R);const double CW=CellRect.Right-CellRect.Left,CH=CellRect.Bottom-CellRect.Top;
            const FLinearColor Color=!Cell.Count?Style::Border():O->bHeatmap?FLinearColor::FromSRGBColor(ShipHatch::Jet(Cell.Mean()/S->Depth)):
                S->Cargo==ShipHatch::ECargo::Coal?Style::Color(82,92,103):Style::Color(172,111,75);
            Rect(CellRect.Left,CellRect.Top,CW-(O->bHeatmap?FMath::Min(.65,CW*.12):0),CH-(O->bHeatmap?FMath::Min(.65,CH*.12):0),Color);
        }
        // Coverage storage is row=length, column=breadth; project its SI bounds.
        if(M->ShowCoverage())
        {
            const auto& Masks=M->GetCoverageMask();if(Masks.Num()==1600)for(int32 Y=0;Y<40;++Y)for(int32 X=0;X<40;++X)
            {
                const uint8 Mask=Masks[Y*40+X];const FLinearColor C=Mask?FLinearColor((Mask&2)?.9:0,(Mask&1)?.85:.1,(Mask&4)?.9:.1,.33):FLinearColor(.9,.08,.08,.55);
                const auto Cell=Mapping.Bounds(FVector((double(Y)/40-.5)*S->OpeningLength,(double(X)/40-.5)*S->OpeningWidth,0),FVector((double(Y+1)/40-.5)*S->OpeningLength,(double(X+1)/40-.5)*S->OpeningWidth,0),R);
                Rect(Cell.Left,Cell.Top,Cell.Right-Cell.Left+.1,Cell.Bottom-Cell.Top+.1,C);
            }
        }
        const double T=O->bHeatmap?1:2;
        Rect(OX,OY,OW,T,Style::Accent());Rect(OX,OY+OH-T,OW,T,Style::Accent());
        Rect(OX,OY,T,OH,Style::Accent());Rect(OX+OW-T,OY,T,OH,Style::Accent());
        if(O->bHeatmap)
        {
            for(int32 I=0;I<64;++I)Rect(R.Right+16,R.Top+H*I/64,10,H/64+1,FLinearColor::FromSRGBColor(ShipHatch::Jet(1.-double(I)/63)));
            auto Text=[&](double Y,const FString& Value){FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(52,20),FSlateLayoutTransform(FVector2D(R.Right+12,Y))),Value,Style::Font(Style::LabelSize,M->GetFontScale()),ESlateDrawEffect::None,Style::Text());};
            Text(R.Top-20,FString::Printf(TEXT("%.1f m"),S->Depth));Text(R.Bottom,TEXT("0 m"));
        }
        auto XY=[&](FVector P){return FVector2f(Mapping.ToPixel(P,R));};
        if(const auto* Sim=M->GetSimulation())for(int32 I=0;I<3;++I)if(Sim->SensorEnabled(I))
        {
            // A real boom mount can sit beyond the aperture. Keep its exact SI
            // projection in the plot margin instead of moving it onto cargo.
            const auto Icon=XY(Sim->SensorOrigin(I));
            const FLinearColor C=I==0?FLinearColor(.1,.9,1):I==1?FLinearColor(1,.6,.06):FLinearColor(.9,.2,.8);
            Polygon(G,Out,Layer+3,{Icon+FVector2f(-5,-5),Icon+FVector2f(5,-5),Icon+FVector2f(5,5),Icon+FVector2f(-5,5)},C);
            TArray<FVector2f> Ring;for(int32 J=0;J<=16;++J)Ring.Add(Icon+FVector2f(FMath::Cos(J*2*PI/16),FMath::Sin(J*2*PI/16))*15);
            FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Ring,ESlateDrawEffect::None,C,true,1);
        }
        // Same aperture mapping as the measured grid. Off-map cameras retain an
        // exact raw local pose, with the icon clamped to the border and an arrow.
        const double Offset=M->SectionOffset();const auto* Surface=M->GetSurface();
        if(Surface)
        {
            const auto Point=XY(M->SectionAlongLength()?FVector(0,Offset,0):FVector(Offset,0,0));
            TArray<FVector2f> Line=M->SectionAlongLength()?TArray<FVector2f>{FVector2f(OX,FMath::Clamp(Point.Y,float(R.Top),float(R.Bottom))),FVector2f(OX+OW,FMath::Clamp(Point.Y,float(R.Top),float(R.Bottom)))}:TArray<FVector2f>{FVector2f(FMath::Clamp(Point.X,float(R.Left),float(R.Right)),OY),FVector2f(FMath::Clamp(Point.X,float(R.Left),float(R.Right)),OY+OH)};
            FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Line,ESlateDrawEffect::None,Style::Warning(),true,2);
        }
        FVector Camera,Direction;double Fov;
        if(M->CameraInHold(Camera,Direction,Fov))
        {
            auto Icon=XY(Camera);Icon.X=FMath::Clamp(Icon.X,float(OX+5),float(OX+OW-5));Icon.Y=FMath::Clamp(Icon.Y,float(OY+5),float(OY+OH-5));
            const auto Forward=FVector2f(Mapping.DirectionToPixel(Direction,R)).GetSafeNormal();const auto Side=FVector2f(-Forward.Y,Forward.X);
            Polygon(G,Out,Layer+2,{Icon,Icon+Forward*45+Side*22,Icon+Forward*45-Side*22},FLinearColor(.1,.9,.7,.22));
            if(Forward.IsNearlyZero())Polygon(G,Out,Layer+3,{Icon+FVector2f(-4,-4),Icon+FVector2f(4,-4),Icon+FVector2f(4,4),Icon+FVector2f(-4,4)},Style::Success());
            else Polygon(G,Out,Layer+3,{Icon+Forward*9,Icon-Forward*6+Side*5,Icon-Forward*6-Side*5},Style::Success());
            if(Direction.Z<-.01)
            {
                const auto Target=XY(Camera+Direction*((S->Depth*.7-Camera.Z)/Direction.Z));
                const FVector2f Clamped(FMath::Clamp(Target.X,float(OX),float(OX+OW)),FMath::Clamp(Target.Y,float(OY),float(OY+OH)));
                FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),TArray<FVector2f>{Icon,Clamped},ESlateDrawEffect::None,Style::Success(),true,1);
            }
        }
        return Layer+3;
    }
    bool MoveSection(const FGeometry& G,const FPointerEvent& E,bool Force)
    {
        const auto* O=Owner.Get();auto* M=O?O->Monitor.Get():nullptr;const auto* S=M?M->GetAnalysisSettings():nullptr;if(!M||!S||O->bLegend||O->bSection)return false;
        const auto R=Plot(G);const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());if(!R.ContainsPoint(P)&&!Force)return false;
        const auto Mapping=ShipHatch::FViewTransform::Top(*S);const double Offset=M->SectionOffset(),Coordinate=M->SectionAlongLength()?P.Y:P.X;
        const auto Line=Mapping.ToPixel(M->SectionAlongLength()?FVector(0,Offset,0):FVector(Offset,0,0),R);
        const double Drawn=M->SectionAlongLength()?FMath::Clamp(Line.Y,double(R.Top),double(R.Bottom)):FMath::Clamp(Line.X,double(R.Left),double(R.Right));
        if(!Force&&FMath::Abs(Coordinate-Drawn)>10)return false;
        const auto Local=Mapping.FromPixel(P,R);
        const double Full=M->SectionAlongLength()?S->Width:S->Length;
        M->SetSection(M->SectionAlongLength(),(M->SectionAlongLength()?Local.Y:Local.X)/Full+.5);return true;
    }
    FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override
    {
        if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
        if(MoveSection(G,E,false)){bDraggingSection=true;return FReply::Handled().CaptureMouse(SharedThis(this));}
        const auto* O=Owner.Get();auto* M=O?O->Monitor.Get():nullptr;const auto* S=M?M->GetAnalysisSettings():nullptr;
        if(M&&S&&!O->bSection&&!O->bLegend){const auto R=Plot(G);const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());if(R.ContainsPoint(P)){M->FocusAt(FVector2D(ShipHatch::FViewTransform::Top(*S).FromPixel(P,R)));return FReply::Handled();}}
        return FReply::Unhandled();
    }
    FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent& E) override
    {if(bDraggingSection&&E.GetEffectingButton()==EKeys::LeftMouseButton){bDraggingSection=false;return FReply::Handled().ReleaseMouseCapture();}return FReply::Unhandled();}
    void OnMouseCaptureLost(const FCaptureLostEvent&) override {bDraggingSection=false;}
    virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E) override
    {
        if(bDraggingSection){MoveSection(G,E,true);return FReply::Handled();}
        const auto* O=Owner.Get();const auto* M=O?O->Monitor.Get():nullptr;
        if(!O||!M||!O->bHeatmap)return FReply::Unhandled();
        const auto R=Plot(G);const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
        const auto* A=M->GetDisplayAnalysis();const auto* S=M->GetAnalysisSettings();
        int32 Cell=-1;
        if(A&&S&&P.X>=R.Left&&P.X<R.Right&&P.Y>=R.Top&&P.Y<R.Bottom)
        {const auto Local=ShipHatch::FViewTransform::Top(*S).FromPixel(P,R);const double B=Local.Y,L=Local.X;
         if(FMath::Abs(B)<=S->OpeningWidth*.5&&FMath::Abs(L)<=S->OpeningLength*.5){const int32 X=FMath::FloorToInt((L+S->Length*.5)/A->DX),Y=FMath::FloorToInt((B+S->Width*.5)/A->DY);if(X>=0&&X<A->NX&&Y>=0&&Y<A->NY)Cell=Y*A->NX+X;}}
        const double Now=FPlatformTime::Seconds();
        // Direct pointer changes stay immediate; stationary streaming refresh
        // is capped at5Hz. No tracing or string work happens in OnPaint/Tick.
        if(Cell!=HoveredCell||(Now-LastTooltipUpdate>=.2&&TooltipBuild!=M->AnalysisBuilds)){LastTooltipUpdate=Now;HoveredCell=Cell;TooltipBuild=M->AnalysisBuilds;SetToolTipText(M->CellTooltip(Cell));}
        return FReply::Handled();
    }
private:
    TWeakObjectPtr<UShipHatchAnalysisView> Owner;
    int32 HoveredCell=MIN_int32;
    uint64 TooltipBuild=0;
    double LastTooltipUpdate=0;
    bool bDraggingSection=false;
};
}
TSharedRef<SWidget> UShipHatchAnalysisView::RebuildWidget(){return SNew(SHatchGrid,this);}
