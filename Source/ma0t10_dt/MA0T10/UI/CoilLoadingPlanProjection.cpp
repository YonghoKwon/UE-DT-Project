#include "CoilLoadingPlanProjection.h"

namespace CoilPlan
{
void FitPlayerProjection(FMatrix& P,const FIntRect& Full,const FVector4& R)
{
    const double W=Full.Width(),H=Full.Height(),SX=R.Z-R.X,SY=R.W-R.Y;if(W<=0||H<=0||SX<=0||SY<=0)return;
    // Build the fixed-horizontal-FOV camera for the actual content rectangle,
    // then remap both axes into the full render target. Pixel scale is isotropic
    // even when a drawer/DPI change alters SX and SY independently.
    const double ContentAspect=(W*SX)/(H*SY),Horizontal=P.M[0][0],Vertical=Horizontal*ContentAspect;
    P.M[0][0]=Horizontal*SX;P.M[1][1]=Vertical*SY;
    P.M[2][0]=P.M[2][0]*SX+(R.X+R.Z)-1;P.M[2][1]=P.M[2][1]*SY+1-(R.Y+R.W);
}
bool FGlyph::Contains(FVector2D P) const
{
    const auto D=P-Center;
    if(bAnnulus){const double R=D.Size();return R<=Radius+3&&R>=FMath::Max(0.,Bore-3);}
    return FMath::Abs(D.X)<=Half.X+3&&FMath::Abs(D.Y)<=Half.Y+3;
}
FVector2D FProjection::Project(FVector2D P) const{return Origin+FVector2D((P.X-Min.X)*Scale,(bSection?Max.Y-P.Y:P.Y-Min.Y)*Scale);}
int32 FProjection::Hit(FVector2D P) const
{
    // Upper tiers are painted last. A ghost never steals a placed coil's pick.
    for(int32 I=Glyphs.Num()-1;I>=0;--I)if(!Glyphs[I].bGhost&&Glyphs[I].Contains(P))return Glyphs[I].Index;
    for(const auto& G:Glyphs)if(G.bGhost&&G.Contains(P))return G.Index;
    return INDEX_NONE;
}
FString Describe(const CoilLoading::FPlacement& P)
{
    const auto& C=P.Spec;
    return FString::Printf(TEXT("%s · %s · %d단 / %d열 / %d번\nOD %.3fm · ID %.3fm · 폭 %.3fm · %.3ft\n외피 OD %.3fm / 유효 ID %.3fm / 전체 폭 %.3fm%s"),*C.CoilId,C.Type==CoilLoading::ECoilType::ColdRolled?TEXT("냉연 CR · 포장"):TEXT("열연 HR · 무포장"),P.Tier,P.Row,P.Column,C.OD,C.ID,C.Width,C.GrossMass(),C.OuterDiameter(),C.ClearBore(),C.OverallWidth(),C.Type==CoilLoading::ECoilType::ColdRolled?*FString::Printf(TEXT(" · 포장 방사 %.1fmm / 측면당 %.1fmm"),C.PackageRadial*1000,C.PackageEnd*1000):TEXT(""));
}
FProjection Build(const CoilLoading::FSettings& S,const TArray<CoilLoading::FPlacement>& Plan,const CoilLoading::FPlacement& Next,const CoilLoading::FResult& Analysis,const TArray<int32>& Selected,FVector2D Size,bool Section,int32 Row)
{
    FProjection R;R.bSection=Section;R.Min={DBL_MAX,DBL_MAX};R.Max={-DBL_MAX,-DBL_MAX};
    const auto Extent=[&](const CoilLoading::FPlacement& P)
    {return Section?FVector2D(P.bEyeForeAft?P.Spec.OuterDiameter()*.5:P.Spec.OverallWidth()*.5,P.Spec.OuterDiameter()*.5):FVector2D(P.bEyeForeAft?P.Spec.OverallWidth()*.5:P.Spec.OuterDiameter()*.5,P.bEyeForeAft?P.Spec.OuterDiameter()*.5:P.Spec.OverallWidth()*.5);};
    const auto Center=[&](const CoilLoading::FPlacement& P){return Section?FVector2D(P.Center.Y,P.Center.Z):FVector2D(P.Center.X,P.Center.Y);};
    TArray<int32> Order;for(int32 I=0;I<=Plan.Num();++I){const auto& P=I==Plan.Num()?Next:Plan[I];if(Section&&P.Row!=Row)continue;Order.Add(I);const auto C=Center(P),H=Extent(P);R.Min.X=FMath::Min(R.Min.X,C.X-H.X);R.Min.Y=FMath::Min(R.Min.Y,C.Y-H.Y);R.Max.X=FMath::Max(R.Max.X,C.X+H.X);R.Max.Y=FMath::Max(R.Max.Y,C.Y+H.Y);}
    // Every view uses the same SI hold envelope, including an empty hold.
    // A single coil must never be magnified to look like it fills the hold.
    R.Min=Section?FVector2D(-S.HoldWidth*.5-.5,-.2):FVector2D(-S.HoldLength*.5-.5,-S.HoldWidth*.5-.5);
    R.Max=Section?FVector2D(S.HoldWidth*.5+.5,S.HoldHeight+S.Coaming+.5):FVector2D(S.HoldLength*.5+.5,S.HoldWidth*.5+.5);
    const auto Span=R.Max-R.Min;const FVector2D Available(FMath::Max(1.,Size.X-60),FMath::Max(1.,Size.Y-164));R.Scale=FMath::Min(Available.X/Span.X,Available.Y/Span.Y);R.Origin={30+(Available.X-Span.X*R.Scale)*.5,70+(Available.Y-Span.Y*R.Scale)*.5};
    Order.StableSort([&](int32 A,int32 B){return (A==Plan.Num()?Next.Tier:Plan[A].Tier)<(B==Plan.Num()?Next.Tier:Plan[B].Tier);});
    for(int32 I:Order)
    {
        const auto& P=I==Plan.Num()?Next:Plan[I];const auto& C=P.Spec;FGlyph G;G.Index=I;G.bGhost=I==Plan.Num();G.bSection=Section;G.bAnnulus=Section&&P.bEyeForeAft;G.bForeAft=P.bEyeForeAft;G.bCold=C.Type==CoilLoading::ECoilType::ColdRolled;G.bHighlighted=Selected.Contains(I);G.Center=R.Project(Center(P));G.Half=Extent(P)*R.Scale;G.Radius=C.OuterDiameter()*.5*R.Scale;G.Bore=C.ClearBore()*.5*R.Scale;G.NetRadius=C.OD*.5*R.Scale;
        G.NetHalf=(Section?FVector2D(P.bEyeForeAft?C.OD*.5:C.Width*.5,C.OD*.5):FVector2D(P.bEyeForeAft?C.Width*.5:C.OD*.5,P.bEyeForeAft?C.OD*.5:C.Width*.5))*R.Scale;
        G.Fill=G.bCold?FLinearColor(.68,.73,.78):FLinearColor(.13,.17,.21);G.Edge=G.bCold?FLinearColor(.95,.96,1):FLinearColor(.025,.03,.035);
        if(G.bGhost){G.Fill=FLinearColor(.10,.36,.17);G.Edge=FLinearColor(.3,1,.45);}
        CoilLoading::ESeverity Level=CoilLoading::ESeverity::Normal;for(const auto& Issue:Analysis.Issues)if(Issue.Coil==I&&Issue.bKnown&&uint8(Issue.Severity)>uint8(Level))Level=Issue.Severity;
        if(Level==CoilLoading::ESeverity::Warning)G.Edge=FLinearColor(1,.12,.08);else if(Level==CoilLoading::ESeverity::Caution)G.Edge=FLinearColor(1,.5,.08);
        G.Information=Describe(P);G.ShortLabel=FString::Printf(TEXT("%s · %.2f"),G.bCold?TEXT("CR"):TEXT("HR"),C.OD);R.Glyphs.Add(MoveTemp(G));
    }
    return R;
}
}
