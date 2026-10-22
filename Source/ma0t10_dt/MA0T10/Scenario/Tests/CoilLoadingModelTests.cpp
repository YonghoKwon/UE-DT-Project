#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingModel.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Json.h"
using namespace CoilLoading;
namespace
{
const FIssue* Issue(const FResult& R,FName Code){return R.Issues.FindByPredicate([&](const FIssue& I){return I.Code==Code;});}
TArray<FPlacement> Triangle(double Gap=0)
{
    FPlacement A,B,K;A.Spec.CoilId=TEXT("A");B.Spec.CoilId=TEXT("B");K.Spec.CoilId=TEXT("K");
    A.Center={0,-.8-Gap*.5,.85};B.Center={0,.8+Gap*.5,.85};K.Center={0,0,.85+KeyGeometry(.8,.8,Gap).Height};K.Tier=2;K.SupportA=0;K.SupportB=1;return {A,B,K};
}
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCoilBasisCases,"MA0T10.CoilLoading.Analysis",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FCoilBasisCases::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    for(const TCHAR* C:{TEXT("N1Mass"),TEXT("N2KeyReaction"),TEXT("N3Patch"),TEXT("N4List"),TEXT("W1UDL"),TEXT("W2Tier"),TEXT("W3DeepKey"),TEXT("W4Clearance"),TEXT("D1Overload"),TEXT("D2InvalidKey"),TEXT("D3Spread"),TEXT("D4WetFriction"),TEXT("D5SWL"),TEXT("D6GM"),TEXT("U1UnknownOvality"),TEXT("B1CSS"),TEXT("B2Overlap"),TEXT("B3PolygonSensitivity"),TEXT("B4UnboundedTiers")}){Names.Add(C);Commands.Add(C);}
}
bool FCoilBasisCases::RunTest(const FString& Case)
{
    FSettings S;S.UDLLimit=15;FSpec C;FPlacement P;P.Center={0,0,.85};
    if(Case==TEXT("N1Mass")){TestTrue(TEXT("HR annulus density7850 and fill0.97 ~=14.797t"),FMath::IsNearlyEqual(C.CalculatedMass(),14.797,0.001));const double M=7.85*.97*PI*.25*(1600.*1600.-762.*762.)*1250.*1.e-9;TestTrue(TEXT("mm/m units agree"),FMath::IsNearlyEqual(M,C.CalculatedMass(),1.e-9));}
    if(Case==TEXT("N2KeyReaction")){const auto K=KeyGeometry(.8,.8,0,100);TestTrue(TEXT("two point support valid"),K.bValid);TestTrue(TEXT("30 degree normal"),FMath::IsNearlyEqual(K.AngleDegrees,30.,1.e-6));TestTrue(TEXT("h sqrt(1.6²-.8²)"),FMath::IsNearlyEqual(K.Height,1.385640646,1.e-8));TestTrue(TEXT("N57.735"),FMath::IsNearlyEqual(K.NormalKN,57.7350269,1.e-6));TestTrue(TEXT("H28.868"),FMath::IsNearlyEqual(K.SpreadKN,28.86751346,1.e-6));}
    if(Case==TEXT("N3Patch")){P.Spec.MassOverride=40;S.ApprovedPatchArea=4;const auto R=Analyze(S,{P});TestEqual(TEXT("approved effective patch q=10"),R.PatchPressure[0],10.);TestTrue(TEXT("contact pressure independent"),R.ContactPressure[0]>R.PatchPressure[0]);TestFalse(TEXT("no UDL warning under80percent"),Issue(R,TEXT("UDL"))!=nullptr);}
    if(Case==TEXT("N4List")){TestTrue(TEXT("heel 0.24446deg"),FMath::IsNearlyEqual(Heel(25000,1.5,160),.24446,.00001));TestEqual(TEXT("sign reversal"),Heel(25000,1.5,-160),-Heel(25000,1.5,160));}
    if(Case==TEXT("W1UDL")){P.Spec.MassOverride=54;S.ApprovedPatchArea=4;const auto R=Analyze(S,{P});const auto* I=Issue(R,TEXT("UDL"));TestTrue(TEXT("q13.5 caution, placement retained"),I&&I->Severity==ESeverity::Caution&&FMath::IsNearlyEqual(I->Value,13.5));}
    if(Case==TEXT("W2Tier")){auto Plan=Triangle();Plan[2].Spec.MassOverride=16;const auto R=Analyze(S,Plan);TestTrue(TEXT("manual unknown has separate warning"),Issue(R,TEXT("TierManual"))&&!R.bComplete);}
    if(Case==TEXT("W3DeepKey")){const auto K=KeyGeometry(.8,.8,.96);TestTrue(TEXT("gap60percent gives depth40percent"),K.bValid&&FMath::IsNearlyEqual(K.PenetrationRatio,.4,1.e-6));TestTrue(TEXT("one third criterion separately exceeded"),K.PenetrationRatio>1./3);}
    if(Case==TEXT("W4Clearance")){S.OpeningWidth=2.4;S.GearWidth=2.2;const auto R=Analyze(S,{P});const auto* I=Issue(R,TEXT("HatchClearance"));TestTrue(TEXT("10cm margin caution, no penetration"),I&&I->Severity==ESeverity::Caution&&FMath::IsNearlyEqual(I->Value,.1,1.e-6));}
    if(Case==TEXT("D1Overload")){S.ApprovedPatchArea=2;P.Spec.MassOverride=40;const auto R=Analyze(S,{P});const auto* I=Issue(R,TEXT("UDL"));TestTrue(TEXT("q20 warning versus15"),I&&I->Severity==ESeverity::Warning&&I->Value==20);}
    if(Case==TEXT("D2InvalidKey")){const auto K=KeyGeometry(.8,.8,1.61);TestFalse(TEXT("no sqrtNaN for unsupported geometry"),K.bValid);TestTrue(TEXT("finite invalid result"),FMath::IsFinite(K.Height));}
    if(Case==TEXT("D3Spread")){auto Plan=Triangle();for(auto& B:Plan){B.Spec.Mu=0;B.ChockCapacityKN=50;}Plan[2].Spec.MassOverride=35.3119;const auto R=Analyze(S,Plan);TestTrue(TEXT("H~100 > certified50 warning"),R.SpreadKN[0]>99.9&&Issue(R,TEXT("SlidingSpread")));}
    if(Case==TEXT("D4WetFriction")){S.Ay=5;S.bWetSteel=true;P.Spec.Mu=0;P.bChocks=false;P.bDunnage=false;const auto R=Analyze(S,{P});const auto* I=Issue(R,TEXT("SlidingSpread"));TestTrue(TEXT("wet steel/steel zero is preserved"),I&&I->Limit==0&&I->Value>0);}
    if(Case==TEXT("D5SWL")){P.Spec.MassOverride=39;S.GearMass=3;const auto R=Analyze(S,{P});const auto* I=Issue(R,TEXT("SWL"));TestTrue(TEXT("42/40 exceedsSWL"),I&&I->Value==42&&I->Limit==40&&I->Severity==ESeverity::Warning);}
    if(Case==TEXT("D6GM")){S.KM=6;const auto R=Analyze(S,{P});TestTrue(TEXT("GMnonpositive invalid, never green"),Issue(R,TEXT("GM"))&&R.Severity==ESeverity::Warning);}
    if(Case==TEXT("U1UnknownOvality")){const auto R=Analyze(S,Triangle());TestTrue(TEXT("unknown stiffness does not manufacture zero ovality pass"),Issue(R,TEXT("OvalityUnknown"))&&R.Ovality[0]<0&&!R.bComplete);}
    if(Case==TEXT("B1CSS")){S.PositionFromAP=.5;S.LBP=140;S.SpeedKnots=15;S.Beam=23.4;S.KG=7;S.KM=8.7;S.FreeSurfaceCorrection=.2;FVector A;FString E;TestTrue(TEXT("CSS lower hold applicable"),CSSAcceleration(S,A,E));TestTrue(TEXT("base x1.20495 y4.01651 z3.45419 gravity excluded in z"),FMath::IsNearlyEqual(A.X,1.20495,.00002)&&FMath::IsNearlyEqual(A.Y,4.01651,.00002)&&FMath::IsNearlyEqual(A.Z,3.45419,.00002));S.KM=10.2;FVector B;CSSAcceleration(S,B,E);TestTrue(TEXT("higherGM increases transverse acceleration"),B.Y>A.Y);}
    if(Case==TEXT("B2Overlap")){auto Plan=Triangle();Plan[2].Center.X=2;const auto R=Analyze(S,Plan);TestTrue(TEXT("2Dkey with insufficient3Doverlap rejected"),Issue(R,TEXT("UnsupportedKey"))!=nullptr);FVector2D Center;TestTrue(TEXT("unequal OD upper intersection"),UnequalKey({-.8,.8},.8,{.9,1.},1.,.8,Center));}
    if(Case==TEXT("B3PolygonSensitivity")){TestTrue(TEXT("24sides6.84mm"),FMath::IsNearlyEqual(PolygonRadialError(.8,24),.0068441,.000001));TestTrue(TEXT("48sides1.71mm"),FMath::IsNearlyEqual(PolygonRadialError(.8,48),.0017129,.000001));TestTrue(TEXT("actual32 below24 error"),PolygonRadialError(.8,32)<PolygonRadialError(.8,24));}
    if(Case==TEXT("B4UnboundedTiers"))
    {
        TArray<FPlacement> Plan;TArray<int32> Last;
        for(int32 Tier=1;Tier<=7;++Tier)
        {
            TArray<int32> Current;for(int32 Col=0;Col<8-Tier;++Col)
            {FPlacement C1;C1.Tier=Tier;C1.Spec.CoilId=FString::Printf(TEXT("T%dC%d"),Tier,Col);C1.Center={0,-4.8+(Col+(Tier-1)*.5)*1.6,.85+(Tier-1)*1.385640646};if(Tier>1){C1.SupportA=Last[Col];C1.SupportB=Last[Col+1];}Current.Add(Plan.Add(C1));}Last=Current;
        }
        const auto R=Analyze(S,Plan);TestTrue(TEXT("seven tiers load follows contact network and conserves mass"),FMath::IsNearlyEqual(R.FloorReaction,R.TotalMass,.0001));TestFalse(TEXT("valid contact network"),Issue(R,TEXT("UnsupportedKey"))!=nullptr);
        FSettings Restored;TArray<FPlacement> P2;FString E;TestTrue(TEXT("unbounded tiers persist"),PlanFromJson(PlanToJson(S,Plan),Restored,P2,E));TestEqual(TEXT("roundtrip all placements"),P2.Num(),Plan.Num());TestEqual(TEXT("roundtrip top tier"),P2.Last().Tier,7);P2.Pop();TestTrue(TEXT("remove recalculates mass"),Analyze(S,P2).TotalMass<R.TotalMass);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilSerialization,"MA0T10.CoilLoading.ManifestAndAtomicLoad",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilSerialization::RunTest(const FString&)
{
    FString E;TArray<FSpec> Coils;TestTrue(TEXT("legacy CSV duplicate id columns are positional"),ImportManifest(TEXT("id,od,id,width,weight\nC1,1.6,0.762,1.25,0\nC2,1.5,0.610,1.2,14"),false,Coils,E));TestEqual(TEXT("two coils"),Coils.Num(),2);TestEqual(TEXT("bore independent fromcoilid"),Coils[0].ID,.762);
    TestTrue(TEXT("JSON unambiguous fields"),ImportManifest(TEXT("[{\"id\":\"J1\",\"od\":1.6,\"inner_diameter\":0.762,\"width\":1.25,\"weight\":14.8}]"),true,Coils,E));
    TestFalse(TEXT("duplicateID fails without changing previous manifest"),ImportManifest(TEXT("id,od,id,width,weight\nD,1.6,0.762,1.25,0\nD,1.6,0.762,1.25,0"),false,Coils,E));TestEqual(TEXT("prior manifest preserved"),Coils[0].CoilId,FString(TEXT("J1")));
    FSettings S;TArray<FPlacement> P=Triangle();const FString Original=PlanToJson(S,P);FString Invalid=Original.Replace(TEXT("coil-loading.v1"),TEXT("coil-loading.v2"));TestFalse(TEXT("unsupported schema rejected"),PlanFromJson(Invalid,S,P,E));TestEqual(TEXT("failed load atomic"),PlanToJson(S,P),Original);
    FSpec Bad;Bad.OD=Bad.ID;TestFalse(TEXT("OD> ID enforced"),Bad.Validate(E));Bad=FSpec();Bad.Mu=-.1;TestFalse(TEXT("invalid friction rejected"),Bad.Validate(E));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilSignedContact,"MA0T10.CoilLoading.SignedContactsAndDefaultRisk",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilSignedContact::RunTest(const FString&)
{
    FSettings S;TArray<FPlacement> Plan;TArray<int32> Below;
    for(int32 Tier=1;Tier<=3;++Tier)
    {
        TArray<int32> Current;for(int32 Col=0;Col<11-Tier;++Col)
        {FPlacement P;P.Tier=Tier;P.Spec.CoilId=FString::Printf(TEXT("T%dC%d"),Tier,Col);P.Center={0,-7.2+(Col+(Tier-1)*.5)*1.6,.85+(Tier-1)*1.385640646};if(Tier>1){P.SupportA=Below[Col];P.SupportB=Below[Col+1];}Current.Add(Plan.Add(P));}Below=Current;
    }
    const auto R=Analyze(S,Plan);TestTrue(TEXT("opposite key forces cancel on central bottom coil"),R.SpreadKN[4]<1.e-6);TestTrue(TEXT("three tier edge spreading remains physical~83.8kN"),R.SpreadKN[0]>83&&R.SpreadKN[0]<85);TestFalse(TEXT("representative three tier static fixture has no false friction risk"),Issue(R,TEXT("SlidingSpread"))!=nullptr);TestFalse(TEXT("three tier fixture does not conceal force error as missing restraint"),Issue(R,TEXT("RestraintCapacity"))!=nullptr);
    TestEqual(TEXT("default network known dangers zero; data requirements separate"),R.Issues.FilterByPredicate([](const FIssue& I){return I.bKnown&&I.Severity==ESeverity::Warning;}).Num(),0);TestTrue(TEXT("mass and reaction remain conserved"),FMath::IsNearlyEqual(R.TotalMass,R.FloorReaction,1.e-6));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilCylinderFit,"MA0T10.CoilLoading.CylinderFitSIAndIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilCylinderFit::RunTest(const FString&)
{
    for(bool ForeAft:{true,false})
    {
        FPlacement P;P.Center={12,-5,1.2};P.bEyeForeAft=ForeAft;TArray<FVector> Points;const FVector Offset(.012,-.018,.009);const FQuat R=FRotator(0,ForeAft?0:90,0).Quaternion();
        for(int32 I=0;I<100;++I){const double A=.2+I*.02;const double Noise=.001*FMath::Sin(I*1.23);Points.Add(P.Center+Offset+R.RotateVector(FVector(-.5+I*.01,(.8+Noise)*FMath::Cos(A),(.8+Noise)*FMath::Sin(A))));if(I%2==0)Points.Add(P.Center+Offset+R.RotateVector(FVector(.625+Noise,.6*FMath::Cos(A),.6*FMath::Sin(A))));}
        const auto Fit=FitCylinder(P,Points);TestTrue(TEXT("annulus fit is observable for eye-X and eye-Y"),Fit.bValid);TestTrue(TEXT("center error <3mm; cm/m never mixed"),FVector::Distance(Fit.Center,P.Center+Offset)<.003);TestTrue(TEXT("surface residual <3mm"),Fit.ResidualP95<.003);
    }
    FPlacement P;TestFalse(TEXT("unobserved is data needed, not zero deviation pass"),FitCylinder(P,{}).bValid);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilTypes,"MA0T10.CoilLoading.HRCRPackagingAndManifest",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilTypes::RunTest(const FString&)
{
    const auto HR=FSpec::Representative(ECoilType::HotRolled),CR=FSpec::Representative(ECoilType::ColdRolled);TestEqual(TEXT("HR bare geometry equals net OD"),HR.OuterDiameter(),HR.OD);TestTrue(TEXT("CR net mass ~15.351t with fill1"),FMath::IsNearlyEqual(CR.CalculatedMass(),15.351,.001));
    TestTrue(TEXT("CR explicit synthetic packaging adds3mm OD and4mm width"),FMath::IsNearlyEqual(CR.OuterDiameter()-CR.OD,.003,1.e-9)&&FMath::IsNearlyEqual(CR.OverallWidth()-CR.Width,.004,1.e-9));
    FPlacement P;P.Spec=CR;P.Center={0,0,CR.OuterDiameter()*.5+.05};FSettings S;S.Ay=2;const auto Result=Analyze(S,{P});TestTrue(TEXT("packaged friction is data needed"),Issue(Result,TEXT("PackagedFriction"))&&!Issue(Result,TEXT("PackagedFriction"))->bKnown);TestFalse(TEXT("unknown packaged friction cannot assert actual slip exceedance"),Issue(Result,TEXT("SlidingSpread"))!=nullptr);
    TArray<FSpec> Coils;FString E;TestTrue(TEXT("typed CSV input"),ImportManifest(TEXT("coil_id,od,inner_diameter,width,weight,type\nH1,1.6,0.762,1.25,0,HR\nC1,1.5,0.508,1.25,0,CR"),false,Coils,E));TestTrue(TEXT("CR type and fill preserved"),Coils.Num()==2&&Coils[1].Type==ECoilType::ColdRolled&&Coils[1].Fill==1);
    TestTrue(TEXT("typed JSON input"),ImportManifest(TEXT("[{\"id\":\"C2\",\"type\":\"CR\",\"od\":1.5,\"inner_diameter\":0.508,\"width\":1.25}]"),true,Coils,E));
    FSettings Restored;TArray<FPlacement> Plan;TestTrue(TEXT("type/package fields persist"),PlanFromJson(PlanToJson(S,{P}),Restored,Plan,E));TestTrue(TEXT("packaging exact roundtrip"),Plan[0].Spec.Type==ECoilType::ColdRolled&&Plan[0].Spec.PackageRadial==CR.PackageRadial);return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCoilIndependentReview,"MA0T10.CoilLoading.IndependentReview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FCoilIndependentReview::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    for(const TCHAR* C:{TEXT("R1CrossAxisCylinders"),TEXT("R2OrientedHatch"),TEXT("R3NumericLegacyId"),TEXT("R4FieldDiagnostics"),TEXT("R5RollingMoment"),TEXT("R6LCFTrim"),TEXT("R8SceneCraneRadius")}){Names.Add(C);Commands.Add(C);}
}
bool FCoilIndependentReview::RunTest(const FString& Case)
{
    FSettings S;FPlacement A,B;A.Center={0,0,.85};B=A;B.Spec.CoilId=TEXT("B");FString E;
    if(Case==TEXT("R1CrossAxisCylinders"))
    {
        B.bEyeForeAft=false;TestTrue(TEXT("cross-axis annuli intersect"),CylindersInterfere(A,B));TestTrue(TEXT("analysis checks differing eye directions"),Issue(Analyze(S,{A,B}),TEXT("CoilInterference"))!=nullptr);
        B.Center={1.4,1.4,1.35};TestFalse(TEXT("overlapping AABBs can have separated circular sections"),CylindersInterfere(A,B));
        B.Center=A.Center;B.Spec.OD=.2;B.Spec.ID=.1;B.Spec.Width=.2;TestFalse(TEXT("small cross-axis cylinder entirely in clear bore"),CylindersInterfere(A,B));
        B=A;B.Center.Y=1.6;TestFalse(TEXT("tangent cylinders do not penetrate"),CylindersInterfere(A,B));B.Center.Y=1.59;TestTrue(TEXT("10mm penetration exceeds3mm tolerance"),CylindersInterfere(A,B));
    }
    if(Case==TEXT("R2OrientedHatch"))
    {
        S.OpeningLength=1.5;S.OpeningWidth=2;S.GearLength=S.GearWidth=.2;
        const auto Fore=Analyze(S,{A});const auto* I=Issue(Fore,TEXT("HatchClearance"));TestTrue(TEXT("eye-X width fits along hatch X"),I&&I->Value>0);
        A.bEyeForeAft=false;const auto Across=Analyze(S,{A});I=Issue(Across,TEXT("HatchClearance"));TestTrue(TEXT("eye-Y OD along X penetrates5cm"),I&&I->Severity==ESeverity::Warning&&FMath::IsNearlyEqual(I->Value,-.05,1.e-6));
    }
    if(Case==TEXT("R3NumericLegacyId"))
    {
        TArray<FSpec> Specs;TestTrue(TEXT("legacy numeric id is bore with explicit coil_id"),ImportManifest(TEXT("[{\"coil_id\":\"LEGACY-1\",\"id\":0.762,\"od\":1.6,\"width\":1.25}]"),true,Specs,E));
        TestTrue(TEXT("identifier independent from bore"),Specs.Num()==1&&Specs[0].CoilId==TEXT("LEGACY-1")&&Specs[0].ID==.762);
        TestFalse(TEXT("numeric bore cannot become identifier"),ImportManifest(TEXT("[{\"id\":0.762,\"od\":1.6,\"width\":1.25}]"),true,Specs,E));TestTrue(TEXT("missing identifier diagnostic"),E.Contains(TEXT("coil_id")));
        TestFalse(TEXT("numeric coil_id cannot be coerced either"),ImportManifest(TEXT("[{\"coil_id\":42,\"id\":0.762,\"od\":1.6,\"width\":1.25}]"),true,Specs,E));
    }
    if(Case==TEXT("R4FieldDiagnostics"))
    {
        const FString Original=PlanToJson(S,{A,B});
        const auto Reject=[&](TSharedPtr<FJsonObject> Root,const FString& Expected)
        {FString Text;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));FSettings OutS;TArray<FPlacement> OutP={A};E=TEXT("stale");TestFalse(*Expected,PlanFromJson(Text,OutS,OutP,E));TestTrue(*(Expected+TEXT(" named in diagnostic")),E.Contains(Expected)&&E!=TEXT("stale"));TestEqual(TEXT("failure preserves output"),OutP.Num(),1);};
        for(const TCHAR* Field:{TEXT("x"),TEXT("y"),TEXT("z"),TEXT("tier"),TEXT("row"),TEXT("column"),TEXT("support_a"),TEXT("support_b"),TEXT("dunnage"),TEXT("chocks"),TEXT("lashing"),TEXT("eye_fore_aft"),TEXT("chock_kn"),TEXT("lashing_kn"),TEXT("k_diam"),TEXT("ovality_limit"),TEXT("coil_id"),TEXT("od"),TEXT("inner_diameter"),TEXT("width"),TEXT("density"),TEXT("fill"),TEXT("weight"),TEXT("mu")})
        {
            TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Original),Root);Root->GetArrayField(TEXT("coils"))[1]->AsObject()->RemoveField(Field);Reject(Root,FString(TEXT("coils[1]."))+Field);
        }
        for(const TCHAR* Field:{TEXT("HoldCount"),TEXT("DunnageLines"),TEXT("Beam"),TEXT("bUseCSS")})
        {TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Original),Root);Root->GetObjectField(TEXT("settings"))->RemoveField(Field);Reject(Root,FString(TEXT("settings."))+Field);}
        TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Original),Root);Root->GetArrayField(TEXT("coils"))[1]->AsObject()->SetNumberField(TEXT("support_a"),99);Reject(Root,TEXT("coils[1].support_a"));
    }
    if(Case==TEXT("R5RollingMoment"))
    {
        S.bUseCSS=true;TestTrue(TEXT("default first-tier chocks enabled"),A.bChocks);const auto Default=Analyze(S,{A});TestEqual(TEXT("default CSS placement known risks zero, unprovided capacity separate"),Default.Issues.FilterByPredicate([](const FIssue& I){return I.bKnown&&I.Severity==ESeverity::Warning;}).Num(),0);TestTrue(TEXT("missing certified rolling restraint is data needed"),Issue(Default,TEXT("RollingCapacityUnknown"))&&!Issue(Default,TEXT("RollingCapacityUnknown"))->bKnown);
        S.bUseCSS=false;S.Ay=.01;A.bChocks=false;A.RollingResistanceM=.002;TestFalse(TEXT("finite rolling resistance tolerates small acceleration"),Issue(Analyze(S,{A}),TEXT("Rolling"))!=nullptr);
        S.Ay=.03;const auto Above=Analyze(S,{A});const auto* I=Issue(Above,TEXT("Rolling"));TestTrue(TEXT("a_critical=g*b/r=.024525m/s2"),I&&I->bKnown&&I->Severity==ESeverity::Warning&&FMath::IsNearlyEqual(I->Limit,.024525,1.e-7));
        A.bEyeForeAft=false;S.Ay=.5;S.Ax=0;TestFalse(TEXT("transverse eye rolls along X, not along its Y axis"),Issue(Analyze(S,{A}),TEXT("Rolling"))!=nullptr);
    }
    if(Case==TEXT("R6LCFTrim"))
    {
        S.MCT1cm=100;S.LBP=160;S.PositionFromAP=.25;S.LCFFromAP=80;S.bLCFConfirmed=true;A.Spec.MassOverride=20;A.Center.X=10;
        const auto R=Analyze(S,{A});TestEqual(TEXT("20t at AP50 versus LCF80 gives -600tm"),R.TrimMomentTonnesM,-600.);TestEqual(TEXT("trim uses LCF and hold AP station"),R.TrimCm,-6.);
        S.PositionFromAP=.75;TestEqual(TEXT("moving hold changes longitudinal moment"),Analyze(S,{A}).TrimCm,10.);
        S.bLCFConfirmed=false;const auto Missing=Analyze(S,{A});TestTrue(TEXT("MCT alone cannot establish reference LCF"),Issue(Missing,TEXT("LCFUnknown"))&&!Issue(Missing,TEXT("LCFUnknown"))->bKnown);TestEqual(TEXT("no fabricated trim when LCF missing"),Missing.TrimCm,0.);
    }
    if(Case==TEXT("R8SceneCraneRadius"))
    {
        TestTrue(TEXT("default rail midpoint derived from beam, gap, setback and gauge"),FMath::IsNearlyEqual(S.RailCenterY(),-22.0));TestTrue(TEXT("radius to centerline22m"),FMath::IsNearlyEqual(S.CraneRadius(A.Center),22.0));
        S.Beam=26;S.BerthGap=4;S.RailSetback=5;S.RailGauge=14;S.QuaySide=1;A.Center.Y=-2;TestEqual(TEXT("port quay radius31m"),S.CraneRadius(A.Center),31.);S.MaxRadius=30;const auto R=Analyze(S,{A});const auto* I=Issue(R,TEXT("Outreach"));TestTrue(TEXT("analysis uses same actual scene radius"),I&&I->Value==31);
        FSettings Restored;TArray<FPlacement> Plan;TestTrue(TEXT("rail and LCF metadata roundtrip"),PlanFromJson(PlanToJson(S,{A}),Restored,Plan,E));TestEqual(TEXT("rail dimensions persist"),Restored.RailCenterY(),S.RailCenterY());
    }
    return true;
}
#endif
