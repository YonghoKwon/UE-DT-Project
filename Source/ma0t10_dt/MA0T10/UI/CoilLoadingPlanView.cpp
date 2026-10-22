#include "CoilLoadingMonitor.h"
#include "CoilLoadingPlanProjection.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"

namespace
{
class SCoilPlan final:public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCoilPlan){}SLATE_END_ARGS()
    void Construct(const FArguments&,UCoilLoadingPlanView* V){Owner=V;SetCursor(EMouseCursor::Hand);}
    FVector2D ComputeDesiredSize(float) const override{return FVector2D(500,320);}
    FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E) override
    {
        Update(G);const int32 Index=Projection.Hit(G.AbsoluteToLocal(E.GetScreenSpacePosition()));
        if(Index!=Hovered){Hovered=Index;const auto* Item=Projection.Glyphs.FindByPredicate([&](const CoilPlan::FGlyph& X){return X.Index==Index;});SetToolTipText(Item?FText::FromString(Item->Information):FText::GetEmpty());Invalidate(EInvalidateWidgetReason::Paint);}
        return FReply::Handled();
    }
    void OnMouseLeave(const FPointerEvent& E) override{SLeafWidget::OnMouseLeave(E);Hovered=INDEX_NONE;SetToolTipText(FText::GetEmpty());Invalidate(EInvalidateWidgetReason::Paint);}
    FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override
    {if(E.GetEffectingButton()==EKeys::LeftMouseButton)if(auto* O=Owner.Get())if(O->PickAt(G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize()))return FReply::Handled();return FReply::Unhandled();}
    int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const override
    {
        auto* O=Owner.Get();auto* M=O?O->Monitor.Get():nullptr;auto* S=M?M->Simulation():nullptr;if(!S)return L;Update(G);const FVector2D Size=G.GetLocalSize();
        const auto Line=[&](const TArray<FVector2f>& P,FLinearColor C,float W=1){FSlateDrawElement::MakeLines(Out,L+2,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,W);};
        const auto Text=[&](FVector2D P,const FString& T,FLinearColor C=FSensorOwnedPresentationStyle::Text(),int32 Font=12){FSlateDrawElement::MakeText(Out,L+4,G.ToPaintGeometry(FVector2D(Size.X-16,80),FSlateLayoutTransform(P)),T,FSensorOwnedPresentationStyle::Font(Font,M->FontScale()),ESlateDrawEffect::None,C);};
        Text({8,0},Projection.bSection?FString::Printf(TEXT("횡단면 · %d열 · 실제 OD/ID 비율 · 전체 홀드 실치수"),S->NextRow):TEXT("2D 상면 · X 선수 → / Y 좌현 ↓ · 실제 외피 비율 · 전체 홀드 실치수"));
        const TCHAR* Names[]={TEXT("열연 HR"),TEXT("냉연 CR / 포장선"),TEXT("고스트"),TEXT("위험"),TEXT("주의"),TEXT("선택")};
        const FLinearColor Colors[]={FLinearColor(.30,.34,.38),FLinearColor(.8,.84,.88),FLinearColor(.3,1,.45),FLinearColor(1,.12,.08),FLinearColor(1,.5,.08),FLinearColor(0,1,1)};
        for(int I=0;I<6;++I){const double X=8+I*FMath::Min(175.,(Size.X-16)/6);FSlateDrawElement::MakeBox(Out,L+3,G.ToPaintGeometry(FVector2D(16,12),FSlateLayoutTransform(FVector2D(X,35))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Colors[I]);Text({X+22,30},Names[I],Colors[I],11);}
        for(const auto& P:Grid)Line(P,FLinearColor(.15,.19,.22));
        Line(HoldBoundary,FLinearColor(.45,.75,.85),2);Line(HatchBoundary,FLinearColor(1,.8,.25),2);
        if(Vertices.Num())FSlateDrawElement::MakeCustomVerts(Out,L+1,FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush")),Vertices,Indices,nullptr,0,0);
        for(int32 I=0;I<Projection.Glyphs.Num();++I)
        {
            const auto& X=Projection.Glyphs[I];if(X.bHighlighted)Line(Edges[I],FLinearColor(0,1,1),8);if(X.bCold)Line(NetEdges[I],FLinearColor(.38,.45,.52),1);Line(Edges[I],X.Edge,X.bCold?3:3.5);
            Line(Bores[I],X.Edge,1.5);
            if(X.Half.X>=24&&X.Half.Y>=14)Text(X.Center+FVector2D(-X.Half.X+4,-7),X.ShortLabel,X.bCold?FLinearColor(.04,.06,.08):FLinearColor(.9,.93,.96),10);
        }
        FString Info=TEXT("호버 또는 클릭: 코일 ID·종류·OD·ID·폭·질량·단/열/번 · 시안색은 모든 뷰의 공통 선택");
        int32 Selected=Hovered;if(Selected==INDEX_NONE&&S->HighlightedCoils().Num())Selected=S->HighlightedCoils()[0];
        if(const auto* X=Projection.Glyphs.FindByPredicate([&](const CoilPlan::FGlyph& P){return P.Index==Selected;}))Info=X->Information;
        Text({8,Size.Y-82},Info,FSensorOwnedPresentationStyle::Text(),11);Text({8,Size.Y-22},FString::Printf(TEXT("계획 위치 · 1m 격자 · 포장선 좌표는 실치수 / 선 굵기만 가독성 표시 · 홀드 %.2f×%.2fm"),S->Settings.HoldLength,S->Settings.HoldWidth),FSensorOwnedPresentationStyle::Muted(),10);return L+4;
    }
private:
    TWeakObjectPtr<UCoilLoadingPlanView> Owner;int32 Hovered=INDEX_NONE;
    mutable uint64 Revision=MAX_uint64;mutable FVector2D LastSize,LastPosition;mutable float LastScale=0;mutable int32 LastView=-1;
    mutable CoilPlan::FProjection Projection;mutable TArray<FSlateVertex> Vertices;mutable TArray<SlateIndex> Indices;
    mutable TArray<TArray<FVector2f>> Edges,Bores,NetEdges,Grid;
    mutable TArray<FVector2f> HoldBoundary,HatchBoundary;
    void Update(const FGeometry& G) const
    {
        auto* O=Owner.Get();auto* M=O?O->Monitor.Get():nullptr;auto* S=M?M->Simulation():nullptr;if(!S)return;
        if(Revision==S->Revision()&&LastSize==G.GetLocalSize()&&LastPosition==G.GetAbsolutePosition()&&LastScale==G.GetAccumulatedLayoutTransform().GetScale()&&LastView==S->View())return;
        Revision=S->Revision();LastSize=G.GetLocalSize();LastPosition=G.GetAbsolutePosition();LastScale=G.GetAccumulatedLayoutTransform().GetScale();LastView=S->View();Projection=CoilPlan::Build(S->Settings,S->Plan(),S->PreviewPlacement(),S->Analysis(),S->HighlightedCoils(),LastSize,LastView==2,S->NextRow);
        Vertices.Reset();Indices.Reset();Edges.Reset();Bores.Reset();NetEdges.Reset();Grid.Reset();
        const auto Vertex=[&](FVector2D P,FLinearColor C){return FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(P),FVector2f(.5,.5),C.ToFColor(true));};
        const auto Rect=[&](FVector2D C,FVector2D H,FLinearColor Color){const int32 Base=Vertices.Num();for(const FVector2D P:{C+FVector2D(-H.X,-H.Y),C+FVector2D(H.X,-H.Y),C+H,C+FVector2D(-H.X,H.Y)})Vertices.Add(Vertex(P,Color));for(int V:{0,1,2,0,2,3})Indices.Add(Base+V);};
        const auto Ring=[&](FVector2D C,double R){TArray<FVector2f> P;for(int I=0;I<=64;++I){const double A=I*2*PI/64;P.Add(FVector2f(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*R));}return P;};
        const auto Outline=[&](FVector2D C,FVector2D H){return TArray<FVector2f>{FVector2f(C-H),FVector2f(C+FVector2D(H.X,-H.Y)),FVector2f(C+H),FVector2f(C+FVector2D(-H.X,H.Y)),FVector2f(C-H)};};
        const auto MeterOutline=[&](FVector2D Min,FVector2D Max){const auto A=Projection.Project(Min),B=Projection.Project(Max);return Outline((A+B)*.5,FVector2D(FMath::Abs(B.X-A.X),FMath::Abs(B.Y-A.Y))*.5);};
        const auto& Settings=S->Settings;
        HoldBoundary=Projection.bSection?MeterOutline({-Settings.HoldWidth*.5,0},{Settings.HoldWidth*.5,Settings.HoldHeight}):MeterOutline({-Settings.HoldLength*.5,-Settings.HoldWidth*.5},{Settings.HoldLength*.5,Settings.HoldWidth*.5});
        HatchBoundary=Projection.bSection?MeterOutline({-Settings.OpeningWidth*.5,Settings.WeatherDeckZ()},{Settings.OpeningWidth*.5,Settings.WeatherDeckZ()+Settings.Coaming}):MeterOutline({-Settings.OpeningLength*.5,-Settings.OpeningWidth*.5},{Settings.OpeningLength*.5,Settings.OpeningWidth*.5});
        for(const auto& P:S->Plan())if(P.Tier==1&&P.bDunnage&&(!Projection.bSection||P.Row==S->NextRow))
            for(int32 J=0;J<Settings.DunnageLines;++J)
            {
                const FVector2D C=Projection.bSection?FVector2D(P.Center.Y,Settings.DunnageThickness*.5):FVector2D(P.Center.X+(J-(Settings.DunnageLines-1)*.5)*.45,P.Center.Y);
                const FVector2D Half=Projection.bSection?FVector2D(Settings.ContactLength,Settings.DunnageThickness)*.5:FVector2D(Settings.DunnageWidth,Settings.ContactLength)*.5;
                Rect(Projection.Project(C),Half*Projection.Scale,FLinearColor(.45,.24,.08));
            }
        for(double X=FMath::CeilToDouble(Projection.Min.X);X<=Projection.Max.X;X+=1)Grid.Add({FVector2f(Projection.Project({X,Projection.Min.Y})),FVector2f(Projection.Project({X,Projection.Max.Y}))});
        for(double Y=FMath::CeilToDouble(Projection.Min.Y);Y<=Projection.Max.Y;Y+=1)Grid.Add({FVector2f(Projection.Project({Projection.Min.X,Y})),FVector2f(Projection.Project({Projection.Max.X,Y}))});
        for(const auto& X:Projection.Glyphs)
        {
            if(X.bAnnulus)
            {
                const int Base=Vertices.Num();for(int I=0;I<64;++I){const double A=I*2*PI/64;const FVector2D D(FMath::Cos(A),FMath::Sin(A));Vertices.Add(Vertex(X.Center+D*X.Radius,X.Fill));Vertices.Add(Vertex(X.Center+D*X.Bore,X.Fill));}
                for(int I=0;I<64;++I){const int A=Base+2*I,B=Base+2*((I+1)%64);for(int V:{A,B,B+1,A,B+1,A+1})Indices.Add(V);}Edges.Add(Ring(X.Center,X.Radius));Bores.Add(Ring(X.Center,X.Bore));NetEdges.Add(Ring(X.Center,X.NetRadius));
            }
            else
            {
                if(X.bSection){const double H=(X.Half.Y-X.Bore)*.5;Rect(X.Center+FVector2D(0,X.Bore+H),{X.Half.X,H},X.Fill);Rect(X.Center-FVector2D(0,X.Bore+H),{X.Half.X,H},X.Fill);}
                else Rect(X.Center,X.Half,X.Fill);
                Edges.Add(Outline(X.Center,X.Half));NetEdges.Add(Outline(X.Center,X.NetHalf));
                Bores.Add(X.bSection?Outline(X.Center,{X.Half.X,X.Bore}):X.bForeAft?TArray<FVector2f>{FVector2f(X.Center+FVector2D(-X.Half.X,-X.Bore)),FVector2f(X.Center+FVector2D(X.Half.X,-X.Bore)),FVector2f(X.Center+FVector2D(X.Half.X,X.Bore)),FVector2f(X.Center+FVector2D(-X.Half.X,X.Bore))}:TArray<FVector2f>{FVector2f(X.Center+FVector2D(-X.Bore,-X.Half.Y)),FVector2f(X.Center+FVector2D(-X.Bore,X.Half.Y)),FVector2f(X.Center+FVector2D(X.Bore,X.Half.Y)),FVector2f(X.Center+FVector2D(X.Bore,-X.Half.Y))});
            }
        }
    }
};
}
bool UCoilLoadingPlanView::PickAt(FVector2D Point,FVector2D Size)
{
    auto* M=Monitor.Get();auto* S=M?M->Simulation():nullptr;if(!S)return false;const auto P=CoilPlan::Build(S->Settings,S->Plan(),S->PreviewPlacement(),S->Analysis(),S->HighlightedCoils(),Size,S->View()==2,S->NextRow);const int32 I=P.Hit(Point);if(I==INDEX_NONE)return false;S->HighlightCoils({I});return true;
}
TSharedRef<SWidget> UCoilLoadingPlanView::RebuildWidget(){return SNew(SCoilPlan,this);}
