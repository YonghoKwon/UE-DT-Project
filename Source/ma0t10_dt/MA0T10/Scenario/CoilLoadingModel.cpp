#include "CoilLoadingModel.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/DefaultValueHelper.h"

namespace CoilLoading
{
namespace
{
bool Positive(double V){return FMath::IsFinite(V)&&V>0;}
bool Nonnegative(double V){return FMath::IsFinite(V)&&V>=0;}
void SpecJson(const FSpec& S,TSharedRef<FJsonObject> O)
{
    O->SetStringField(TEXT("coil_id"),S.CoilId);
    O->SetStringField(TEXT("type"),S.Type==ECoilType::ColdRolled?TEXT("CR"):TEXT("HR"));O->SetNumberField(TEXT("steel_mu"),S.SteelMu);O->SetNumberField(TEXT("package_radial_m"),S.PackageRadial);O->SetNumberField(TEXT("package_end_m"),S.PackageEnd);O->SetBoolField(TEXT("packaged_friction_confirmed"),S.bPackagedFrictionConfirmed);
    O->SetNumberField(TEXT("od"),S.OD);O->SetNumberField(TEXT("inner_diameter"),S.ID);O->SetNumberField(TEXT("width"),S.Width);
    O->SetNumberField(TEXT("density"),S.Density);O->SetNumberField(TEXT("fill"),S.Fill);O->SetNumberField(TEXT("weight"),S.MassOverride);O->SetNumberField(TEXT("mu"),S.Mu);
}
bool ReadSpec(const TSharedPtr<FJsonObject>& O,FSpec& S,FString& E,bool Full)
{
    if(!O){E=TEXT("코일 JSON 객체가 필요합니다.");return false;}
    FString Type;if(O->TryGetStringField(TEXT("type"),Type))
    {if(!Type.Equals(TEXT("HR"),ESearchCase::IgnoreCase)&&!Type.Equals(TEXT("CR"),ESearchCase::IgnoreCase)){E=TEXT("type은 HR 또는 CR입니다.");return false;}S=FSpec::Representative(Type.Equals(TEXT("CR"),ESearchCase::IgnoreCase)?ECoilType::ColdRolled:ECoilType::HotRolled);}
    const auto* CoilId=O->Values.Find(TEXT("coil_id"));const auto* LegacyId=O->Values.Find(TEXT("id"));
    if(CoilId&&(*CoilId)->Type==EJson::String)S.CoilId=(*CoilId)->AsString();
    else if(!CoilId&&LegacyId&&(*LegacyId)->Type==EJson::String)S.CoilId=(*LegacyId)->AsString();
    else {E=TEXT("coil_id: 문자열 식별자 필요 (숫자 id는 내경 전용)");return false;}
    if(!O->TryGetNumberField(TEXT("od"),S.OD)){E=TEXT("od: 숫자 필요 [m]");return false;}
    if(!O->TryGetNumberField(TEXT("width"),S.Width)){E=TEXT("width: 숫자 필요 [m]");return false;}
    if(!O->TryGetNumberField(TEXT("inner_diameter"),S.ID)&&(!LegacyId||(*LegacyId)->Type!=EJson::Number||!O->TryGetNumberField(TEXT("id"),S.ID))){E=TEXT("inner_diameter: 숫자 필요 [m] (또는 숫자 id)");return false;}
#define READ_SPEC(Field,Target) if((Full||O->HasField(TEXT(Field)))&&!O->TryGetNumberField(TEXT(Field),Target)){E=TEXT(Field) TEXT(": 숫자 필요");return false;}
    READ_SPEC("density",S.Density) READ_SPEC("fill",S.Fill) READ_SPEC("weight",S.MassOverride) READ_SPEC("mu",S.Mu)
#undef READ_SPEC
    O->TryGetNumberField(TEXT("steel_mu"),S.SteelMu);O->TryGetNumberField(TEXT("package_radial_m"),S.PackageRadial);O->TryGetNumberField(TEXT("package_end_m"),S.PackageEnd);O->TryGetBoolField(TEXT("packaged_friction_confirmed"),S.bPackagedFrictionConfirmed);
    return S.Validate(E);
}
ESeverity Utilization(double U){return U>1?ESeverity::Warning:U>=.8?ESeverity::Caution:ESeverity::Normal;}
}
double FSpec::CalculatedMass() const{return Density*Fill*PI*.25*(OD*OD-ID*ID)*Width;}
FSpec FSpec::Representative(ECoilType Kind){FSpec S;S.Type=Kind;if(Kind==ECoilType::ColdRolled){S.OD=1.5;S.ID=.508;S.Fill=1.;S.Mu=S.SteelMu=0;}return S;}
double FSpec::GrossMass() const{return MassOverride>0?MassOverride:CalculatedMass();}
bool FSpec::Validate(FString& E) const
{
    if(CoilId.TrimStartAndEnd().IsEmpty()||CoilId.Len()>128||CoilId.Contains(TEXT("\n"))||CoilId.Contains(TEXT("\r"))){E=TEXT("코일 ID는 줄바꿈 없는 1~128자입니다.");return false;}
    if(!Positive(OD)||!Positive(ID)||OD<=ID||!Positive(Width)||!Positive(Density)||!Positive(Fill)||Fill>1||!Nonnegative(MassOverride)||!Nonnegative(Mu))
    {E=TEXT("OD > ID > 0, 폭·밀도 양수, 0 < 충전율 ≤ 1, 질량·마찰 비음수 필요");return false;}
    if(!Nonnegative(SteelMu)||!Nonnegative(PackageRadial)||!Nonnegative(PackageEnd)||ClearBore()<=0||!Positive(GrossMass())){E=TEXT("마찰/계산 질량/포장 두께/유효 내경 오류");return false;}return true;
}
bool FSettings::Validate(FString& E) const
{
    const double Values[]={LOA,Beam,Depth,Draft,HoldLength,HoldWidth,HoldHeight,OpeningLength,OpeningWidth,UDLLimit,DunnageWidth,DunnageThickness,ContactLength,Displacement,LBP,HookSWL,MaxRadius,GearHeadroom,GearWidth,GearLength};
    for(double V:Values)if(!Positive(V)){E=TEXT("선박·장구·받침 치수와 하중 입력은 유한한 양수여야 합니다.");return false;}
    const double Optional[]={Coaming,ApprovedPatchArea,LocalPressureLimit,FreeSurfaceCorrection,MCT1cm,KeelToTanktop,SpeedKnots,GearMass,MinRadius,ClearanceMargin,WindMps,BerthGap,RailSetback,LCFFromAP,IntermediateHoldLength,HoldGap};
    for(double V:Optional)if(!Nonnegative(V)){E=TEXT("선박/장구 옵션은 유한한 비음수여야 합니다.");return false;}
    if(!FMath::IsFinite(KG)||!FMath::IsFinite(KM)||!FMath::IsFinite(Ax)||!FMath::IsFinite(Ay)||!FMath::IsFinite(Az)||!FMath::IsFinite(Outreach)||!Nonnegative(LegWLLTonnes)||!Nonnegative(LegAngleDegrees)||LegAngleDegrees>=89||HoldCount<1||DunnageLines<1||MinRadius>=MaxRadius||PositionFromAP<0||PositionFromAP>1||!FMath::IsFinite(PositionFromAP))
    {E=TEXT("선박/크레인 상태 입력 범위 오류");return false;}
    if(Coaming>=HoldHeight){E=TEXT("Coaming: 홀드 높이보다 작아야 합니다.");return false;}
    if(!Positive(RailGauge)||(QuaySide!=-1&&QuaySide!=1)||(bLCFConfirmed&&LCFFromAP>LBP)){E=TEXT("RailGauge/QuaySide/LCFFromAP 범위 오류");return false;}return true;
}
void FResult::Add(FName C,ESeverity L,const FString& R,int32 I,double V,double Limit,bool Known)
{
    Issues.Add({C,L,R,I,V,Limit,Known});if(uint8(L)>uint8(Severity))Severity=L;
    if(!Known){bComplete=false;++UnknownCount;}
}
FKeyGeometry KeyGeometry(double R,double r,double Gap,double P)
{
    FKeyGeometry K;const double D=2*R+Gap;
    if(!Positive(R)||!Positive(r)||!Nonnegative(Gap)||D>=2*(R+r))return K;
    K.bValid=true;K.Height=FMath::Sqrt(FMath::Square(R+r)-FMath::Square(D*.5));
    const double Theta=FMath::Asin(D/(2*(R+r)));K.AngleDegrees=FMath::RadiansToDegrees(Theta);
    K.Center=FVector2D(0,K.Height);K.PenetrationRatio=(R+r-K.Height)/(2*r);
    K.NormalKN=P/(2*FMath::Cos(Theta));K.SpreadKN=P*FMath::Tan(Theta)*.5;return K;
}
bool UnequalKey(FVector2D A,double RA,FVector2D B,double RB,double R,FVector2D& C)
{
    const double D=(B-A).Size(),S1=RA+R,S2=RB+R;
    if(!Positive(RA)||!Positive(RB)||!Positive(R)||D<=FMath::Abs(S1-S2)+1.e-9||D>=S1+S2-1.e-9)return false;
    const FVector2D E=(B-A)/D,N(-E.Y,E.X);const double A1=(S1*S1-S2*S2+D*D)/(2*D);
    const double H=FMath::Sqrt(FMath::Max(0.,S1*S1-A1*A1));const FVector2D C1=A+E*A1+N*H,C2=A+E*A1-N*H;
    C=C1.Y>C2.Y?C1:C2;return C.Y>FMath::Max(A.Y,B.Y)&&C.X>FMath::Min(A.X,B.X)&&C.X<FMath::Max(A.X,B.X);
}
double Heel(double D,double GM,double M){return D>0&&GM>0?FMath::RadiansToDegrees(FMath::Atan(M/(D*GM))):0;}
double PolygonRadialError(double R,int32 N){return N>=3?R*(1-FMath::Cos(PI/N)):R;}
bool CylindersInterfere(const FPlacement& A,const FPlacement& B,double Tolerance)
{
    const double Shrink=Tolerance*.5,RA=A.Spec.OuterDiameter()*.5-Shrink,RB=B.Spec.OuterDiameter()*.5-Shrink;
    const double IA=A.Spec.ClearBore()*.5+Shrink,IB=B.Spec.ClearBore()*.5+Shrink;
    const double HA=A.Spec.OverallWidth()*.5-Shrink,HB=B.Spec.OverallWidth()*.5-Shrink;
    if(RA<=IA||RB<=IB||HA<=0||HB<=0)return false;
    const FVector D=B.Center-A.Center;
    if(A.bEyeForeAft==B.bEyeForeAft)
    {
        const double Axial=A.bEyeForeAft?FMath::Abs(D.X):FMath::Abs(D.Y);
        const double Radial=A.bEyeForeAft?FVector2D(D.Y,D.Z).Size():FVector2D(D.X,D.Z).Size();
        return Axial<HA+HB&&Radial<RA+RB&&Radial+RA>IB&&Radial+RB>IA;
    }
    // At any z, each annulus constrains only the other cylinder's axial
    // coordinate. Project its feasible radial interval onto z, then intersect
    // the two pairs of intervals. This is exact for eye-X versus eye-Y,
    // including finite end caps and clear bores, rather than an AABB test.
    const FPlacement& X=A.bEyeForeAft?A:B;const FPlacement& Y=A.bEyeForeAft?B:A;
    const auto ZIntervals=[&](const FPlacement& C,double OtherAxis,double OtherHalf,bool EyeX,TArray<FVector2D>& Out)
    {
        const double R=C.Spec.OuterDiameter()*.5-Shrink,I=C.Spec.ClearBore()*.5+Shrink;
        const double Delta=FMath::Abs(OtherAxis-(EyeX?C.Center.Y:C.Center.X));
        const double Min=FMath::Max(0.,Delta-OtherHalf),Max=Delta+OtherHalf;
        if(Min>=R)return;
        const double Outer=FMath::Sqrt(R*R-Min*Min),Inner=FMath::Sqrt(FMath::Max(0.,I*I-Max*Max));
        if(Inner>=Outer)return;
        Out.Add({C.Center.Z-Outer,C.Center.Z-Inner});Out.Add({C.Center.Z+Inner,C.Center.Z+Outer});
    };
    TArray<FVector2D> ZX,ZY;
    ZIntervals(X,Y.Center.Y,Y.Spec.OverallWidth()*.5-Shrink,true,ZX);
    ZIntervals(Y,X.Center.X,X.Spec.OverallWidth()*.5-Shrink,false,ZY);
    for(const auto& U:ZX)for(const auto& V:ZY)if(FMath::Min(U.Y,V.Y)>FMath::Max(U.X,V.X)+1.e-9)return true;
    return false;
}
bool CSSAcceleration(const FSettings& S,FVector& A,FString& E)
{
    const double GM=S.KM-S.KG-S.FreeSurfaceCorrection,Ratio=S.Beam/GM;
    if(S.LBP<50||S.LBP>300||GM<=0||Ratio<3||S.PositionFromAP<.1||S.PositionFromAP>.9){E=TEXT("CSS lower hold 표 적용 범위 밖: LBP50~300m, B/GM≥3, AP위치0.1~0.9");return false;}
    const double X=S.PositionFromAP;
    const double T=X<=.5?(X-.1)/.4:(X-.5)/.4;
    const double Y=FMath::Lerp(X<=.5?5.5:5.,X<=.5?5.:5.9,T),Z=FMath::Lerp(X<=.5?7.6:4.3,X<=.5?4.3:9.2,T);
    const double CLv=.345*S.SpeedKnots/FMath::Sqrt(S.LBP)+(58.62*S.LBP-1034.5)/(S.LBP*S.LBP);
    const double CGM=Ratio>=13?1.:Ratio>=7?FMath::Lerp(1.15,1.,(Ratio-7)/6):FMath::Lerp(1.24,1.15,(Ratio-3)/4);
    A=FVector(1.5*CLv,Y*CLv*CGM,Z*CLv);return true;
}
FResult Analyze(const FSettings& S,const TArray<FPlacement>& Plan)
{
    FResult R;FString Error;if(!S.Validate(Error)){R.Add(TEXT("InvalidSettings"),ESeverity::Warning,Error);return R;}
    const int32 N=Plan.Num();R.VerticalTonnes.Init(0,N);R.SpreadKN.Init(0,N);R.ContactPressure.Init(0,N);R.PatchPressure.Init(0,N);R.Ovality.Init(-1,N);
    TArray<int32> Order;TArray<double> HorizontalKN,NormalSumKN;HorizontalKN.Init(0,N);NormalSumKN.Init(0,N);TSet<FString> Ids;double MX=0,MY=0,MZ=0;FVector Acc(S.Ax,S.Ay,S.Az);
    if(S.bUseCSS&&!CSSAcceleration(S,Acc,Error))R.Add(TEXT("CSSUnknown"),ESeverity::Caution,Error,INDEX_NONE,0,0,false);
    if(S.bDeckCranes){const double Margin=S.DeckCraneInnerY()-S.OpeningWidth*.5;if(Margin<S.ClearanceMargin)R.Add(TEXT("DeckCraneClearance"),Margin<0?ESeverity::Warning:ESeverity::Caution,TEXT("수납 갑판 붐 / 작업 해치 외곽 여유 [m]"),INDEX_NONE,Margin,S.ClearanceMargin);}
    const double Geff=Gravity-FMath::Abs(Acc.Z);
    if(Geff<=0)R.Add(TEXT("ContactLiftOff"),ESeverity::Warning,TEXT("수직가속도가 중력 이상: 접촉 유지 불가"));
    for(int32 I=0;I<N;++I)
    {
        const auto& P=Plan[I];const auto& C=P.Spec;
        if(!C.Validate(Error)||P.Center.ContainsNaN()||P.Tier<1){R.Add(TEXT("InvalidCoil"),ESeverity::Warning,Error,I);continue;}
        Order.Add(I);const double M=C.GrossMass();R.VerticalTonnes[I]=M;R.TotalMass+=M;MX+=M*P.Center.X;MY+=M*P.Center.Y;MZ+=M*(P.Center.Z+S.KeelToTanktop);
        if(Ids.Contains(C.CoilId.ToLower()))R.Add(TEXT("DuplicateId"),ESeverity::Warning,TEXT("중복 코일 ID"),I);Ids.Add(C.CoilId.ToLower());
        for(int32 J=0;J<I;++J)
        {
            if(CylindersInterfere(P,Plan[J]))R.Add(TEXT("CoilInterference"),ESeverity::Warning,TEXT("기존 코일과 유한 중공 원통 간섭 (eye 축·끝면·내경, 허용0.003m)"),I);
        }
        if(C.MassOverride>0&&FMath::Abs(M-C.CalculatedMass())/C.CalculatedMass()>.05)R.Add(TEXT("MassMismatch"),ESeverity::Caution,TEXT("라벨 질량과 밀도 계산값 차이 >5% (예제 경고)"),I,M,C.CalculatedMass());
        if(!P.bEyeForeAft)R.Add(TEXT("Orientation"),ESeverity::Caution,TEXT("eye 횡방향: 별도 CSM/받침 검증 필요"),I,0,0,false);
        const double XHalf=P.bEyeForeAft?C.OverallWidth()*.5:C.OuterDiameter()*.5,YHalf=P.bEyeForeAft?C.OuterDiameter()*.5:C.OverallWidth()*.5;
        const double Clearance=FMath::Min(S.OpeningLength*.5-FMath::Max(XHalf,S.GearLength*.5)-FMath::Abs(P.Center.X),S.OpeningWidth*.5-FMath::Max(YHalf,S.GearWidth*.5)-FMath::Abs(P.Center.Y));
        if(Clearance<S.ClearanceMargin-1.e-6)R.Add(TEXT("HatchClearance"),Clearance<0?ESeverity::Warning:ESeverity::Caution,TEXT("해치/장구 swept envelope 여유 [m], 제안 마진0.20m"),I,Clearance,S.ClearanceMargin);
        if(FMath::Abs(P.Center.X)+XHalf>S.HoldLength*.5||FMath::Abs(P.Center.Y)+YHalf>S.HoldWidth*.5||P.Center.Z+C.OuterDiameter()*.5>S.HoldHeight||P.Center.Z-C.OuterDiameter()*.5<0)
            R.Add(TEXT("HoldInterference"),ESeverity::Warning,TEXT("홀드 벽/탱크톱/높이 범위 침범"),I);
        const double U=(M+S.GearMass)/S.HookSWL;
        if(U>=.8)R.Add(TEXT("SWL"),Utilization(U),TEXT("훅 총하중/SWL (장구 자중 포함, 합성 load chart)"),I,M+S.GearMass,S.HookSWL);
        const double Radius=S.Outreach>0?S.Outreach:S.CraneRadius(P.Center);
        if(Radius<S.MinRadius||Radius>S.MaxRadius)R.Add(TEXT("Outreach"),ESeverity::Warning,TEXT("레일 X 정렬 후 크레인 작업 반경 범위 초과 [m]"),I,Radius,S.MaxRadius);
        const double RailZ=S.Draft-S.KeelToTanktop+3.5,HookZ=P.Center.Z+S.GearHeadroom;
        if(HookZ>RailZ+28||HookZ<RailZ-15)R.Add(TEXT("HoistLimit"),ESeverity::Warning,TEXT("훅 양정 범위 초과: 레일 위28/아래15m [m]"),I,HookZ-RailZ,28);
        const double Leg=(M+S.GearMass)/(2*FMath::Cos(FMath::DegreesToRadians(S.LegAngleDegrees)));
        if(S.LegWLLTonnes<=0)R.Add(TEXT("GearWLLUnknown"),ESeverity::Caution,TEXT("슬링/C-hook 인증 WLL 미확인. 형상 시연"),I,Leg,0,false);
        else if(Leg>S.LegWLLTonnes)R.Add(TEXT("GearWLL"),ESeverity::Warning,TEXT("슬링 다리 장력/WLL [t 상당]"),I,Leg,S.LegWLLTonnes);
        if(S.bCHook&&C.ClearBore()<.61)R.Add(TEXT("ProngClearance"),ESeverity::Warning,TEXT("예제 C-hook 삽입 단면+여유0.61m > ID"),I,C.ClearBore(),.61);
        if(P.Tier>1&&!S.bLoadingManual)R.Add(TEXT("TierManual"),ESeverity::Caution,TEXT("다단 적재: Loading Manual/제조사 허용 확인 필요. 배치는 유지"),I,P.Tier,0,false);
        if(P.Tier>1)R.Add(TEXT("SteelContact"),ESeverity::Caution,TEXT("코일 간 직접 접촉: 모서리/표면 손상 및 보호재 확인"),I);
        if(C.Type==ECoilType::ColdRolled)
        {
            if(!C.bPackagedFrictionConfirmed)R.Add(TEXT("PackagedFriction"),ESeverity::Caution,TEXT("냉연 포장재-목재/강재 마찰 인증값 필요. 나강재 표를 적용하지 않음"),I,0,0,false);
            if(C.MassOverride<=0)R.Add(TEXT("PackagedGrossMass"),ESeverity::Caution,TEXT("냉연 자동 질량은 순강재: 포장 포함 계근 gross mass 필요"),I,0,0,false);
            if(P.Tier>1||!P.bDunnage)R.Add(TEXT("ColdSurfaceDamage"),ESeverity::Caution,TEXT("포장 냉연: 커버·모서리 직접 접촉 손상, 보호재/운송 지시 확인"),I);
        }
        if(P.bLashing&&P.LashingCapacityKN<=0)R.Add(TEXT("LashingUnknown"),ESeverity::Caution,TEXT("래싱 있음: 선체 연결/인증 내력 미제공, 외력0으로 계산"),I,0,0,false);
        if(P.bChocks&&P.ChockCapacityKN<=0)R.Add(TEXT("ChockUnknown"),ESeverity::Caution,TEXT("쐐기 있음: 고정/압축/전단 내력 미제공"),I,0,0,false);
    }
    Order.Sort([&](int32 A,int32 B){return Plan[A].Center.Z>Plan[B].Center.Z;});
    for(int32 I:Order)
    {
        const auto& P=Plan[I];const double M=R.VerticalTonnes[I],r=P.Spec.OuterDiameter()*.5;
        const bool A=Plan.IsValidIndex(P.SupportA)&&P.SupportA!=I,B=Plan.IsValidIndex(P.SupportB)&&P.SupportB!=I&&P.SupportB!=P.SupportA;
        if(P.Tier>1)
        {
            bool Valid=A&&B;
            if(Valid)
            {
                const auto& P1=Plan[P.SupportA];const auto& P2=Plan[P.SupportB];FVector2D Key;
                const double Overlap1=(P.Spec.OverallWidth()+P1.Spec.OverallWidth())*.5-FMath::Abs(P.Center.X-P1.Center.X),Overlap2=(P.Spec.OverallWidth()+P2.Spec.OverallWidth())*.5-FMath::Abs(P.Center.X-P2.Center.X);
                Valid=P.bEyeForeAft&&P1.bEyeForeAft&&P2.bEyeForeAft&&P1.Center.Z<P.Center.Z&&P2.Center.Z<P.Center.Z&&Overlap1>=FMath::Min(P.Spec.OverallWidth(),P1.Spec.OverallWidth())*.8&&Overlap2>=FMath::Min(P.Spec.OverallWidth(),P2.Spec.OverallWidth())*.8&&
                    UnequalKey({P1.Center.Y,P1.Center.Z},P1.Spec.OuterDiameter()*.5,{P2.Center.Y,P2.Center.Z},P2.Spec.OuterDiameter()*.5,r,Key)&&FVector2D::Distance(Key,{P.Center.Y,P.Center.Z})<.025;
                if(Valid)
                {
                    const FVector2D N1=(Key-FVector2D(P1.Center.Y,P1.Center.Z)).GetSafeNormal(),N2=(Key-FVector2D(P2.Center.Y,P2.Center.Z)).GetSafeNormal();
                    const double Det=N1.X*N2.Y-N2.X*N1.Y,Force=M*Gravity,Q=M*FMath::Abs(Acc.Y),Fx=HorizontalKN[I];
                    const double Reaction1=(-Fx*N2.Y-Force*N2.X)/Det,Reaction2=(N1.X*Force+N1.Y*Fx)/Det;
                    NormalSumKN[I]=Reaction1+Reaction2;
                    R.VerticalTonnes[P.SupportA]+=Reaction1*N1.Y/Gravity;R.VerticalTonnes[P.SupportB]+=Reaction2*N2.Y/Gravity;
                    // Preserve direction through the entire contact graph. Two
                    // adjacent key coils push a shared lower coil in opposite
                    // directions; summing absolute H manufactured false risk.
                    HorizontalKN[P.SupportA]-=Reaction1*N1.X;HorizontalKN[P.SupportB]-=Reaction2*N2.X;
                    if((-(Fx+Q)*N2.Y-Force*N2.X)/Det<0||(N1.X*Force+N1.Y*(Fx+Q))/Det<0||(-(Fx-Q)*N2.Y-Force*N2.X)/Det<0||(N1.X*Force+N1.Y*(Fx-Q))/Det<0)
                        R.Add(TEXT("KeyLiftOff"),ESeverity::Warning,TEXT("수평 관성력에서 key 접촉 이탈 (양방향 힘 평형)"),I);
                    const double Pen=(FMath::Min(P1.Center.Z+P1.Spec.OuterDiameter()*.5,P2.Center.Z+P2.Spec.OuterDiameter()*.5)-(P.Center.Z-r))/P.Spec.OuterDiameter();
                    if(Pen>1./3)R.Add(TEXT("DeepKey"),ESeverity::Caution,TEXT("key 침하/OD >1/3 (예제 검토 기준)"),I,Pen,1./3);
                }
            }
            if(!Valid){R.Add(TEXT("UnsupportedKey"),ESeverity::Warning,TEXT("두 하단 접촉/축방향 겹침/실제 지지 위치 부적합"),I);R.bComplete=false;}
        }
        else
        {
            R.FloorReaction+=M;
            const double Area=P.bDunnage?S.DunnageLines*S.DunnageWidth*S.ContactLength:0;
            if(Area>0)R.ContactPressure[I]=M/Area;
            else R.Add(TEXT("NoDunnage"),ESeverity::Warning,TEXT("받침목 없음: 직접 강재 접촉/손상, 국부 지지면적 미확인"),I);
            if(Area>0&&S.LocalPressureLimit>0&&M/Area>=S.LocalPressureLimit*.8)R.Add(TEXT("LocalPressure"),Utilization(M/Area/S.LocalPressureLimit),TEXT("받침목 실제 접촉 평균압 [t/m²] / 국부 허용"),I,M/Area,S.LocalPressureLimit);
            else if(S.LocalPressureLimit<=0)R.Add(TEXT("LocalUnknown"),ESeverity::Caution,TEXT("내저판·보강재·목재 국부 강도 미확인 [t/m²]"),I,R.ContactPressure[I],0,false);
            if(S.ApprovedPatchArea>0)
            {
                R.PatchPressure[I]=M/S.ApprovedPatchArea;const double U=R.PatchPressure[I]/S.UDLLimit;R.MaxUtilization=FMath::Max(R.MaxUtilization,U);
                if(U>=.8)R.Add(TEXT("UDL"),Utilization(U),TEXT("승인 유효 분산면적의 하중 [t/m²] / UDL"),I,R.PatchPressure[I],S.UDLLimit);
            }
            else R.Add(TEXT("PatchUnknown"),ESeverity::Caution,TEXT("승인 유효 분산면적 없음: 국부 접촉압을 UDL과 직접 비교하지 않음"),I,0,0,false);
        }
        R.SpreadKN[I]=FMath::Abs(HorizontalKN[I]);
        const bool SteelContact=P.Tier>1||!P.bDunnage;
        const double Mu=SteelContact?(S.bWetSteel?0:P.Spec.SteelMu):P.Spec.Mu;
        const double Restraint=(P.bChocks?P.ChockCapacityKN:0)+(P.bLashing?P.LashingCapacityKN:0);
        const double Friction=Mu*(P.Tier>1?NormalSumKN[I]*FMath::Max(0.,Geff)/Gravity:M*FMath::Max(0.,Geff));
        // A key's two normals already balance transverse force. Its remaining
        // friction budget resists axial slip; do not charge balanced H again.
        const double Demand=P.Tier>1?M*FMath::Abs(Acc.X):FVector2D(M*Acc.X,R.SpreadKN[I]+M*FMath::Abs(Acc.Y)).Size();
        const bool WetWood=S.bWetSteel&&P.bDunnage&&P.Tier==1;
        if(WetWood)R.Add(TEXT("WetWoodFriction"),ESeverity::Caution,TEXT("젖은 목재/포장 계면의 마찰 인증값 필요. CSS 나강재 값을 확정하지 않음"),I,0,0,false);
        const bool FrictionKnown=!WetWood&&(P.Spec.Type!=ECoilType::ColdRolled||P.Spec.bPackagedFrictionConfirmed);
        if(FrictionKnown&&Demand>Friction+Restraint+1.e-6)
        {
            const bool MissingRestraint=(P.bChocks&&P.ChockCapacityKN<=0)||(P.bLashing&&P.LashingCapacityKN<=0);
            R.Add(MissingRestraint?FName(TEXT("RestraintCapacity")):FName(TEXT("SlidingSpread")),MissingRestraint?ESeverity::Caution:ESeverity::Warning,MissingRestraint?TEXT("마찰 단독 여유 부족: 쐐기/래싱 내력 자료 필요 [kN]"):TEXT("마찰 원뿔/지지 외력 부족 [kN], 축·횡 여유 중복사용 없음"),I,Demand,Friction+Restraint,!MissingRestraint);
        }
        if(P.Tier==1)
        {
            // Moment balance about the contact: M*a*r versus N*b + F_cert*r.
            // b is a supplied effective rolling-resistance arm; no friction
            // coefficient is silently reused as rolling resistance. Certified
            // restraint is an equivalent horizontal coil force (geometry and
            // fixing included), not an inferred timber capacity from a flag.
            const double RollingAcceleration=FMath::Abs(P.bEyeForeAft?Acc.Y:Acc.X);
            const double Resistance=FMath::Max(0.,Geff)*P.RollingResistanceM/r+Restraint/M;
            if(RollingAcceleration>Resistance+1.e-6)
            {
                const bool Missing=(P.bChocks&&P.ChockCapacityKN<=0)||(P.bLashing&&P.LashingCapacityKN<=0);
                R.Add(Missing?TEXT("RollingCapacityUnknown"):TEXT("Rolling"),Missing?ESeverity::Caution:Utilization(RollingAcceleration/FMath::Max(Resistance,1.e-9)),Missing?TEXT("구름 모멘트 평형: 쐐기/래싱 등가 내력 자료 필요 [m/s²]"):TEXT("구름 모멘트 평형 초과: 가속도 / 저항 한계 [m/s²]"),I,RollingAcceleration,Resistance,!Missing);
            }
            else if(Resistance>0&&RollingAcceleration>=Resistance*.8)R.Add(TEXT("Rolling"),ESeverity::Caution,TEXT("구름 저항 여유20% 이하 [m/s²]"),I,RollingAcceleration,Resistance);
        }
        const double Above=FMath::Max(0.,M-P.Spec.GrossMass()),Load=Above*Gravity;
        if(P.DiametralStiffnessKNPerM>0&&P.OvalityLimit>0)
        {R.Ovality[I]=2*Load/P.DiametralStiffnessKNPerM/P.Spec.OuterDiameter();if(R.Ovality[I]>=P.OvalityLimit*.8)R.Add(TEXT("Ovality"),Utilization(R.Ovality[I]/P.OvalityLimit),TEXT("보정 강성 기반 ovality 근사 / 제조사 입력 임계"),I,R.Ovality[I],P.OvalityLimit);}
        else R.Add(TEXT("OvalityUnknown"),ESeverity::Caution,TEXT("변형 강성/허용 미제공: ovality 미확인, 상부 선하중[kN/m]"),I,Load/P.Spec.Width,0,false);
    }
    const double D=S.Displacement+R.TotalMass;R.KG=(S.Displacement*S.KG+MZ)/D;R.GM=S.KM-R.KG-S.FreeSurfaceCorrection;
    if(R.GM<=0)R.Add(TEXT("GM"),ESeverity::Warning,TEXT("GM≤0: 간이 횡경사 계산 무효"),INDEX_NONE,R.GM,0);
    else {R.HeelDegrees=Heel(D,R.GM,MY);if(FMath::Abs(R.HeelDegrees)>=2)R.Add(TEXT("List"),FMath::Abs(R.HeelDegrees)>=5?ESeverity::Warning:ESeverity::Caution,TEXT("간이 횡경사 [deg],2°/5°는 예제 임계"),INDEX_NONE,R.HeelDegrees,2);}
    if(S.bLCFConfirmed)
    {
        R.TrimMomentTonnesM=MX+R.TotalMass*(S.PositionFromAP*S.LBP-S.LCFFromAP);
        if(S.MCT1cm>0)R.TrimCm=R.TrimMomentTonnesM/S.MCT1cm;
        else R.Add(TEXT("TrimUnknown"),ESeverity::Caution,TEXT("MCT1cm 없음: 트림 미확인, LCF 기준 종 모멘트[t·m]"),INDEX_NONE,R.TrimMomentTonnesM,0,false);
    }
    else R.Add(TEXT("LCFUnknown"),ESeverity::Caution,TEXT("LCF(AP 기준 m)/정수력표 없음: 홀드 원점으로 트림 계산 불가"),INDEX_NONE,0,0,false);
    R.Add(TEXT("StabilityScope"),ESeverity::Caution,TEXT("합성 Δ/KG/KM: 실제 복원성·GZ·SF/BM·총홀드 하중 미확인"),INDEX_NONE,0,0,false);
    return R;
}

// Explicit JSON fields avoid changes to existing SaveGame contracts.
#define COIL_SETTINGS_FIELDS(X) X(LOA) X(Beam) X(Depth) X(Draft) X(HoldLength) X(HoldWidth) X(HoldHeight) X(OpeningLength) X(OpeningWidth) X(Coaming) X(UDLLimit) X(DunnageWidth) X(DunnageThickness) X(ContactLength) X(ApprovedPatchArea) X(LocalPressureLimit) X(Displacement) X(KG) X(KM) X(FreeSurfaceCorrection) X(MCT1cm) X(KeelToTanktop) X(LBP) X(SpeedKnots) X(PositionFromAP) X(HookSWL) X(GearMass) X(MinRadius) X(MaxRadius) X(GearHeadroom) X(GearWidth) X(GearLength) X(LegAngleDegrees) X(LegWLLTonnes) X(Outreach) X(ClearanceMargin) X(WindMps) X(Ax) X(Ay) X(Az)
#define COIL_BOOL_FIELDS(X) X(bCHook) X(bWetSteel) X(bLoadingManual) X(bUseCSS)
FString PlanToJson(const FSettings& S,const TArray<FPlacement>& Plan)
{
    auto Root=MakeShared<FJsonObject>(),Settings=MakeShared<FJsonObject>();Root->SetStringField(TEXT("schema"),TEXT("coil-loading.v1"));
#define WRITE_SETTING(Name) Settings->SetNumberField(TEXT(#Name),S.Name);
    COIL_SETTINGS_FIELDS(WRITE_SETTING)
#undef WRITE_SETTING
#define WRITE_BOOL(Name) Settings->SetBoolField(TEXT(#Name),S.Name);
    COIL_BOOL_FIELDS(WRITE_BOOL)
#undef WRITE_BOOL
    Settings->SetNumberField(TEXT("HoldCount"),S.HoldCount);Settings->SetNumberField(TEXT("DunnageLines"),S.DunnageLines);Root->SetObjectField(TEXT("settings"),Settings);
    Settings->SetNumberField(TEXT("LCFFromAP"),S.LCFFromAP);Settings->SetBoolField(TEXT("bLCFConfirmed"),S.bLCFConfirmed);
    Settings->SetNumberField(TEXT("BerthGap"),S.BerthGap);Settings->SetNumberField(TEXT("RailSetback"),S.RailSetback);Settings->SetNumberField(TEXT("RailGauge"),S.RailGauge);Settings->SetNumberField(TEXT("QuaySide"),S.QuaySide);
    Settings->SetNumberField(TEXT("IntermediateHoldLength"),S.IntermediateHoldLength);Settings->SetNumberField(TEXT("HoldGap"),S.HoldGap);Settings->SetBoolField(TEXT("bDeckCranes"),S.bDeckCranes);Settings->SetBoolField(TEXT("bFoldingCovers"),S.bFoldingCovers);
    TArray<TSharedPtr<FJsonValue>> Items;
    for(const auto& P:Plan)
    {
        auto O=MakeShared<FJsonObject>();SpecJson(P.Spec,O);
        O->SetNumberField(TEXT("x"),P.Center.X);O->SetNumberField(TEXT("y"),P.Center.Y);O->SetNumberField(TEXT("z"),P.Center.Z);
        O->SetNumberField(TEXT("tier"),P.Tier);O->SetNumberField(TEXT("row"),P.Row);O->SetNumberField(TEXT("column"),P.Column);O->SetNumberField(TEXT("support_a"),P.SupportA);O->SetNumberField(TEXT("support_b"),P.SupportB);
        O->SetBoolField(TEXT("dunnage"),P.bDunnage);O->SetBoolField(TEXT("chocks"),P.bChocks);O->SetBoolField(TEXT("lashing"),P.bLashing);O->SetBoolField(TEXT("eye_fore_aft"),P.bEyeForeAft);
        O->SetNumberField(TEXT("chock_kn"),P.ChockCapacityKN);O->SetNumberField(TEXT("lashing_kn"),P.LashingCapacityKN);O->SetNumberField(TEXT("k_diam"),P.DiametralStiffnessKNPerM);O->SetNumberField(TEXT("ovality_limit"),P.OvalityLimit);
        O->SetNumberField(TEXT("rolling_resistance_m"),P.RollingResistanceM);
        Items.Add(MakeShared<FJsonValueObject>(O));
    }
    Root->SetArrayField(TEXT("coils"),Items);FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));return Text;
}
bool PlanFromJson(const FString& Text,FSettings& OutS,TArray<FPlacement>& OutP,FString& E)
{
    TSharedPtr<FJsonObject> Root;auto Reader=TJsonReaderFactory<>::Create(Text);FString Version;
    E.Reset();if(Text.Len()>16*1024*1024){E=TEXT("root: 계획 JSON 16MiB 초과");return false;}
    if(!FJsonSerializer::Deserialize(Reader,Root)||!Root){E=TEXT("root: JSON 객체 구문 오류");return false;}
    if(!Root->TryGetStringField(TEXT("schema"),Version)||Version!=TEXT("coil-loading.v1")){E=TEXT("schema: coil-loading.v1 필요");return false;}
    const TSharedPtr<FJsonObject>* Obj=nullptr;const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;
    if(!Root->TryGetObjectField(TEXT("settings"),Obj)){E=TEXT("settings: 객체 필요");return false;}
    if(!Root->TryGetArrayField(TEXT("coils"),Items)||Items->Num()>10000){E=TEXT("coils: 배열 필요 (최대10000개)");return false;}
    FSettings S;const auto& O=*Obj;
#define READ_SETTING(Name) if(!O->TryGetNumberField(TEXT(#Name),S.Name)){E=TEXT("settings.") TEXT(#Name) TEXT(": 숫자 필요");return false;}
    COIL_SETTINGS_FIELDS(READ_SETTING)
#undef READ_SETTING
#define READ_BOOL(Name) if(!O->TryGetBoolField(TEXT(#Name),S.Name)){E=TEXT("settings.") TEXT(#Name) TEXT(": bool 필요");return false;}
    COIL_BOOL_FIELDS(READ_BOOL)
#undef READ_BOOL
#define READ_INT(Name) if(!O->TryGetNumberField(TEXT(#Name),S.Name)){E=TEXT("settings.") TEXT(#Name) TEXT(": 정수 필요");return false;}
    READ_INT(HoldCount) READ_INT(DunnageLines)
#undef READ_INT
    // New screening inputs are optional for existing v1 plans; unavailable LCF
    // and zero rolling resistance keep their explicit unknown/ideal meanings.
#define READ_OPTIONAL(Name) if(O->HasField(TEXT(#Name))&&!O->TryGetNumberField(TEXT(#Name),S.Name)){E=TEXT("settings.") TEXT(#Name) TEXT(": 숫자 필요");return false;}
    READ_OPTIONAL(LCFFromAP) READ_OPTIONAL(BerthGap) READ_OPTIONAL(RailSetback) READ_OPTIONAL(RailGauge) READ_OPTIONAL(QuaySide) READ_OPTIONAL(IntermediateHoldLength) READ_OPTIONAL(HoldGap)
#undef READ_OPTIONAL
    if(!O->HasField(TEXT("IntermediateHoldLength")))S.IntermediateHoldLength=0;if(!O->HasField(TEXT("HoldGap")))S.HoldGap=3;
    S.bDeckCranes=false;S.bFoldingCovers=false;
    if(O->HasField(TEXT("bDeckCranes"))&&!O->TryGetBoolField(TEXT("bDeckCranes"),S.bDeckCranes)){E=TEXT("settings.bDeckCranes: bool 필요");return false;}
    if(O->HasField(TEXT("bFoldingCovers"))&&!O->TryGetBoolField(TEXT("bFoldingCovers"),S.bFoldingCovers)){E=TEXT("settings.bFoldingCovers: bool 필요");return false;}
    if(O->HasField(TEXT("bLCFConfirmed"))&&!O->TryGetBoolField(TEXT("bLCFConfirmed"),S.bLCFConfirmed)){E=TEXT("settings.bLCFConfirmed: bool 필요");return false;}
    if(!S.Validate(E)){E=TEXT("settings.validation: ")+E;return false;}
    TArray<FPlacement> Plan;TSet<FString> Ids;
    for(int32 Index=0;Index<Items->Num();++Index)
    {
        const auto& Item=(*Items)[Index];const FString Prefix=FString::Printf(TEXT("coils[%d]."),Index);
        if(!Item||Item->Type!=EJson::Object){E=Prefix+TEXT("object: 객체 필요");return false;}
        const auto C=Item->AsObject();FPlacement P;if(!ReadSpec(C,P.Spec,E,true)){E=Prefix+E;return false;}
#define READ_COIL(Field,Target) if(!C->TryGetNumberField(TEXT(Field),Target)){E=Prefix+TEXT(Field) TEXT(": 숫자 필요");return false;}
        READ_COIL("x",P.Center.X) READ_COIL("y",P.Center.Y) READ_COIL("z",P.Center.Z)
        READ_COIL("tier",P.Tier) READ_COIL("row",P.Row) READ_COIL("column",P.Column) READ_COIL("support_a",P.SupportA) READ_COIL("support_b",P.SupportB)
        READ_COIL("chock_kn",P.ChockCapacityKN) READ_COIL("lashing_kn",P.LashingCapacityKN) READ_COIL("k_diam",P.DiametralStiffnessKNPerM) READ_COIL("ovality_limit",P.OvalityLimit)
        if(C->HasField(TEXT("rolling_resistance_m"))){READ_COIL("rolling_resistance_m",P.RollingResistanceM)}
#undef READ_COIL
#define READ_COIL_BOOL(Field,Target) if(!C->TryGetBoolField(TEXT(Field),Target)){E=Prefix+TEXT(Field) TEXT(": bool 필요");return false;}
        READ_COIL_BOOL("dunnage",P.bDunnage) READ_COIL_BOOL("chocks",P.bChocks) READ_COIL_BOOL("lashing",P.bLashing) READ_COIL_BOOL("eye_fore_aft",P.bEyeForeAft)
#undef READ_COIL_BOOL
#define CHECK_COIL(Condition,Field) if(!(Condition)){E=Prefix+TEXT(Field) TEXT(": 범위/중복 오류");return false;}
        CHECK_COIL(!P.Center.ContainsNaN(),"x/y/z") CHECK_COIL(P.Tier>=1,"tier") CHECK_COIL(P.Row>=1,"row") CHECK_COIL(P.Column>=1,"column")
        CHECK_COIL(P.SupportA>=-1&&P.SupportA<Items->Num(),"support_a") CHECK_COIL(P.SupportB>=-1&&P.SupportB<Items->Num(),"support_b")
        CHECK_COIL(Nonnegative(P.ChockCapacityKN),"chock_kn") CHECK_COIL(Nonnegative(P.LashingCapacityKN),"lashing_kn") CHECK_COIL(Nonnegative(P.DiametralStiffnessKNPerM),"k_diam") CHECK_COIL(Nonnegative(P.OvalityLimit),"ovality_limit") CHECK_COIL(Nonnegative(P.RollingResistanceM),"rolling_resistance_m")
        CHECK_COIL(!Ids.Contains(P.Spec.CoilId.ToLower()),"coil_id")
#undef CHECK_COIL
        Ids.Add(P.Spec.CoilId.ToLower());Plan.Add(P);
    }
    OutS=S;OutP=MoveTemp(Plan);return true;
}
bool ImportManifest(const FString& Text,bool Json,TArray<FSpec>& Out,FString& E)
{
    if(Text.Len()>16*1024*1024){E=TEXT("목록16MiB 초과");return false;}TArray<FSpec> Specs;TSet<FString> Ids;
    if(Json)
    {
        TArray<TSharedPtr<FJsonValue>> Items;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Items)){E=TEXT("코일 목록은 JSON 배열입니다.");return false;}
        if(Items.Num()>10000){E=TEXT("목록10000개 초과");return false;}
        for(const auto& I:Items){FSpec S;if(!ReadSpec(I->AsObject(),S,E,false))return false;Specs.Add(S);}
    }
    else
    {
        TArray<FString> Lines;Text.ParseIntoArrayLines(Lines,true);
        if(Lines.Num()<2||Lines.Num()>10001){E=TEXT("CSV 헤더+데이터 필요, 최대10000개");return false;}
        TArray<FString> Header;Lines[0].ParseIntoArray(Header,TEXT(","),false);
        if((Header.Num()!=5&&Header.Num()!=6)||Header[1].TrimStartAndEnd()!=TEXT("od")||Header[3].TrimStartAndEnd()!=TEXT("width")||Header[4].TrimStartAndEnd()!=TEXT("weight")||(Header.Num()==6&&Header[5].TrimStartAndEnd()!=TEXT("type"))){E=TEXT("CSV: coil_id,od,inner_diameter,width,weight[,type] (기존 id,od,id,width,weight 허용)");return false;}
        for(int32 I=1;I<Lines.Num();++I)
        {
            TArray<FString> Cells;Lines[I].ParseIntoArray(Cells,TEXT(","),false);FSpec S;
            if(Cells.Num()!=Header.Num()){E=FString::Printf(TEXT("CSV %d행: 헤더와 열 수 일치 필요, 인용쉼표 미지원"),I+1);return false;}
            if(Cells.Num()==6){const auto Kind=Cells[5].TrimStartAndEnd();if(!Kind.Equals(TEXT("HR"),ESearchCase::IgnoreCase)&&!Kind.Equals(TEXT("CR"),ESearchCase::IgnoreCase)){E=TEXT("type은 HR 또는 CR");return false;}S=FSpec::Representative(Kind.Equals(TEXT("CR"),ESearchCase::IgnoreCase)?ECoilType::ColdRolled:ECoilType::HotRolled);}
            S.CoilId=Cells[0].TrimStartAndEnd();
            if(!FDefaultValueHelper::ParseDouble(Cells[1],S.OD)||!FDefaultValueHelper::ParseDouble(Cells[2],S.ID)||!FDefaultValueHelper::ParseDouble(Cells[3],S.Width)||!FDefaultValueHelper::ParseDouble(Cells[4],S.MassOverride)||!S.Validate(E))return false;
            Specs.Add(S);
        }
    }
    for(const auto& S:Specs){if(Ids.Contains(S.CoilId.ToLower())){E=TEXT("목록 ID 중복");return false;}Ids.Add(S.CoilId.ToLower());}
    if(Specs.IsEmpty()){E=TEXT("빈 코일 목록");return false;}Out=MoveTemp(Specs);return true;
}
#undef COIL_SETTINGS_FIELDS
#undef COIL_BOOL_FIELDS
}
