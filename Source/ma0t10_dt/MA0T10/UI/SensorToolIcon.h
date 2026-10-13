#pragma once
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
/** Tiny repo-native line icons; no font glyph dependency or asset package. */
class SSensorToolIcon : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSensorToolIcon):_Kind(0),_Color(FLinearColor::White){}
        SLATE_ARGUMENT(int32,Kind)
        SLATE_ATTRIBUTE(FLinearColor,Color)
    SLATE_END_ARGS()
    void Construct(const FArguments& A){Kind=A._Kind;Color=A._Color;}
    FVector2D ComputeDesiredSize(float) const override {return FVector2D(18,18);}
    int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto Size=G.GetLocalSize();auto Line=[&](std::initializer_list<FVector2D> Values){TArray<FVector2D> P;for(const auto& V:Values)P.Add(V*Size/24);FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color.Get(),true,1.8f);};
        if(Kind==0){Line({{12,3},{12,21}});Line({{3,12},{21,12}});}
        else if(Kind==1){Line({{3,5},{21,5},{21,19},{3,19},{3,5}});Line({{9,9},{15,9},{15,15},{9,15},{9,9}});}
        else if(Kind==2)Line({{7,4},{20,12},{7,20},{7,4}});
        else if(Kind==3){Line({{3,12},{21,12}});Line({{14,5},{21,12},{14,19}});}
        else if(Kind==4){Line({{5,20},{5,10}});Line({{12,20},{12,4}});Line({{19,20},{19,13}});}
        else {for(float X:{6.f,12.f,18.f})Line({{X,11},{X,13}});}
        return Layer;
    }
private:int32 Kind=0;TAttribute<FLinearColor> Color;
};
