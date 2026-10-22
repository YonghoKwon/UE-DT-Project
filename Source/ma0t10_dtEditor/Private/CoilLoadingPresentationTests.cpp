#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CoilTestReportPaths.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingQuayLayout.h"
#include "Components/SceneComponent.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingPlanProjection.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"
#include "Materials/Material.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilProjectionTest,"MA0T10.CoilLoading.Presentation.ProjectionTypesAndDimensions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilProjectionTest::RunTest(const FString&)
{
    using namespace CoilLoading;FSettings S;FPlacement HR,CR,Next;HR.Center={0,-1,.65};HR.Spec.OD=1.2;CR=HR;CR.Spec=FSpec::Representative(ECoilType::ColdRolled);CR.Spec.OD=2;CR.Center={0,1,1.0515};CR.Spec.CoilId=TEXT("CR");Next=HR;Next.Center.Y=4;Next.Spec.CoilId=TEXT("NEXT");FResult R;
    R.Add(TEXT("fixtureCaution"),ESeverity::Caution,TEXT("caution"),0);R.Add(TEXT("fixtureDanger"),ESeverity::Warning,TEXT("danger"),1);
    auto P=CoilPlan::Build(S,{HR,CR},Next,R,{1},{1200,600},true,1);TestEqual(TEXT("two placed and ghost"),P.Glyphs.Num(),3);const auto& A=P.Glyphs[0];const auto& B=P.Glyphs[1];
    TestTrue(TEXT("HR/CR fill separated"),B.Fill.R>A.Fill.R+.4);TestTrue(TEXT("actual OD ratio, same physical scale"),FMath::IsNearlyEqual(B.Radius/A.Radius,2.003/1.2,1.e-9));TestTrue(TEXT("actual outer/bore ratio"),FMath::IsNearlyEqual(B.Bore/B.Radius,CR.Spec.ClearBore()/CR.Spec.OuterDiameter(),1.e-9));TestTrue(TEXT("packaging outline at actual radial thickness"),FMath::IsNearlyEqual(B.Radius-B.NetRadius,.0015*P.Scale,1.e-9));
    TestTrue(TEXT("danger red and caution orange independent from fill"),B.Edge.R==1&&B.Edge.G<.2&&A.Edge.G>.4);TestTrue(TEXT("selection halo preserves warning edge"),B.bHighlighted&&B.Edge.R==1);
    TestEqual(TEXT("annulus bore is not a false hit"),P.Hit(A.Center),INDEX_NONE);TestEqual(TEXT("ring click identifies coil"),P.Hit(B.Center+FVector2D((B.Radius+B.Bore)*.5,0)),1);TestTrue(TEXT("all requested information present"),B.Information.Contains(TEXT("CR"))&&B.Information.Contains(TEXT("OD 2.000"))&&B.Information.Contains(TEXT("ID 0.508"))&&B.Information.Contains(TEXT("폭 1.250"))&&B.Information.Contains(TEXT("1단 / 1열 / 1번")));
    auto Top=CoilPlan::Build(S,{HR,CR},Next,R,{1},{1200,600},false,1);TestTrue(TEXT("top actual OD/width ratio"),FMath::IsNearlyEqual(Top.Glyphs[1].Half.Y/Top.Glyphs[1].Half.X,CR.Spec.OuterDiameter()/CR.Spec.OverallWidth(),1.e-9));CR.bEyeForeAft=false;Top=CoilPlan::Build(S,{CR},Next,R,{}, {1200,600},false,1);TestTrue(TEXT("eye direction swaps actual top extents"),Top.Glyphs[0].Half.X>Top.Glyphs[0].Half.Y);
    FMatrix Matrix=FMatrix::Identity;Matrix.M[0][0]=1.732050807568877;const FVector4 Region(.2,130./1080,1908./1920,796./1080);CoilPlan::FitPlayerProjection(Matrix,FIntRect(0,0,1920,1080),Region);TestTrue(TEXT("off-axis center lands at owned scene center"),FMath::IsNearlyEqual((Matrix.M[2][0]+1)*960.,1146.,1.e-6)&&FMath::IsNearlyEqual((1-Matrix.M[2][1])*540.,463.,1.e-6));TestTrue(TEXT("60degree content spans actual scene width without shrinking render target"),FMath::IsNearlyEqual(2*Matrix.M[0][0]*FMath::Tan(FMath::Acos(-1.)/6)*960.,1524.,1.e-6));
    FString Text,E;TestTrue(TEXT("mixed example exists"),FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("docs/examples/coil-loading-mixed.json"))));TArray<FPlacement> Plan;TestTrue(TEXT("mixed is complete plan schema"),PlanFromJson(Text,S,Plan,E));TestEqual(TEXT("mixed has10 coils"),Plan.Num(),10);const auto Analysis=Analyze(S,Plan);TestFalse(TEXT("mixed unequal keys have real supports"),Analysis.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("UnsupportedKey");}));TestFalse(TEXT("mixed actual solids do not intersect"),Analysis.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("CoilInterference");}));TestTrue(TEXT("explicit overload example exercises red border"),Analysis.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("SWL")&&I.Severity==ESeverity::Warning;}));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilNClassTest,"MA0T10.CoilLoading.Presentation.NClassDimensionsAndClearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilNClassTest::RunTest(const FString&)
{
    CoilLoading::FSettings S;TestEqual(TEXT("published N-class overall length"),S.LOA,174.1);TestEqual(TEXT("4 box holds"),S.HoldCount,4);TestEqual(TEXT("aft working hold24.8m"),S.HoldLength,24.8);TestEqual(TEXT("middle holds27.4m"),S.HoldLengthAt(1),27.4);TestEqual(TEXT("actual beam22m"),S.Beam,22.);TestEqual(TEXT("open-hatch no-understow width18.8m"),S.OpeningWidth,18.8);TestEqual(TEXT("published uniform tanktop14.5"),S.UDLLimit,14.5);TestTrue(TEXT("hold height14 includes1.4m coaming above12.6m deck"),FMath::IsNearlyEqual(S.WeatherDeckZ(),12.6,1.e-8));TestTrue(TEXT("four holds fit within actual hull"),S.HoldCenterX(3)+S.HoldLengthAt(3)*.5+S.HoldLength*.5<S.LOA);
    const int Columns=FMath::FloorToInt((S.HoldWidth-2*FMath::Max(.8,S.GearWidth*.5)-2*S.ClearanceMargin)/1.6+1.e-8)+1;TestEqual(TEXT("18.8m hold representative full row =11"),Columns,11);
    for(int I=0;I<Columns;++I){CoilLoading::FPlacement P;P.Center={-S.OpeningLength*.5+1.45,-S.OpeningWidth*.5+1.3+I*1.6,.85};const auto R=CoilLoading::Analyze(S,{P});TestFalse(TEXT("all11 slots clear hatch and crane radius with unchanged margin"),R.Issues.ContainsByPredicate([](const CoilLoading::FIssue& I){return I.Code==TEXT("HatchClearance")||I.Code==TEXT("Outreach")||I.Code==TEXT("DeckCraneClearance");}));}
    TestTrue(TEXT("stowed twin jib actual inner edge includes .37m half width"),FMath::IsNearlyEqual(S.DeckCraneInnerY()-S.OpeningWidth*.5,.43,1.e-8));
    CoilLoading::FSettings Loaded;TArray<CoilLoading::FPlacement> Empty;FString Error;const FString Current=CoilLoading::PlanToJson(S,{});
    TestTrue(TEXT("N-class optional settings roundtrip"),CoilLoading::PlanFromJson(Current,Loaded,Empty,Error)&&Loaded.bDeckCranes&&Loaded.bFoldingCovers&&Loaded.IntermediateHoldLength==27.4&&Loaded.HoldGap==4);
    auto Legacy=S;Legacy.HoldCount=2;Legacy.HoldLength=60.59;Legacy.HoldWidth=17;Legacy.OpeningLength=60.59;Legacy.OpeningWidth=17;
    TSharedPtr<FJsonObject> Old;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(CoilLoading::PlanToJson(Legacy,{})),Old);auto OldSettings=Old->GetObjectField(TEXT("settings"));
    for(const TCHAR* Key:{TEXT("IntermediateHoldLength"),TEXT("HoldGap"),TEXT("bDeckCranes"),TEXT("bFoldingCovers")})OldSettings->RemoveField(Key);
    FString OldText;FJsonSerializer::Serialize(Old.ToSharedRef(),TJsonWriterFactory<>::Create(&OldText));TestTrue(TEXT("legacy two-hold geometry remains unchanged when fields absent"),CoilLoading::PlanFromJson(OldText,Loaded,Empty,Error)&&Loaded.HoldLength==60.59&&Loaded.HoldCount==2&&Loaded.HoldGap==3&&Loaded.IntermediateHoldLength==0&&!Loaded.bDeckCranes&&!Loaded.bFoldingCovers);
    S.OpeningWidth=S.Beam;const auto Bad=CoilLoading::Analyze(S,{});TestTrue(TEXT("stowed boom crossing opening is a real warning"),Bad.Issues.ContainsByPredicate([](const CoilLoading::FIssue& I){return I.Code==TEXT("DeckCraneClearance")&&I.Severity==CoilLoading::ESeverity::Warning;}));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilDraftResponseTest,"MA0T10.CoilLoading.Hydrostatics.LoadAndHoist",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilDraftResponseTest::RunTest(const FString&)
{
    using namespace CoilLoading;FSettings S;S.TPC=25;S.bTPCConfirmed=true;const auto H=DraftResponse(S,2500);
    TestTrue(TEXT("2500t /25t per cm =1m sinkage"),FMath::IsNearlyEqual(H.Sinkage,1.,1.e-9)&&FMath::IsNearlyEqual(H.Draft,7.2,1.e-9)&&H.bConfirmed);
    S.TPC=0;S.bTPCConfirmed=false;const auto Estimate=DraftResponse(S,1000);TestTrue(TEXT("synthetic Awp*seawater density /100 gives TPC"),FMath::IsNearlyEqual(Estimate.TonnesPerCm,31.81805,1.e-7)&&!Estimate.bConfirmed);
    FPlacement P;P.Center={0,0,.85};P.Spec.MassOverride=1000;auto R=Analyze(S,{P});TestTrue(TEXT("unapproved synthetic hydrostatics is data needed"),R.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("HydrostaticsUnknown")&&!I.bKnown;}));
    TestFalse(TEXT("default-estimate lower hoist limit clear"),R.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("HoistLimit");}));
    S.TPC=1;S.bTPCConfirmed=true;R=Analyze(S,{P});TestTrue(TEXT("loaded draft changes lower hoist and draft-limit screening"),R.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("HoistLimit");})&&R.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("DraftLimit");}));
    S=FSettings();S.HoldHeight=100;S.TPC=10;S.bTPCConfirmed=true;P.Center.Z=S.Draft-S.KeelToTanktop+3.5+28-S.GearHeadroom+.05;P.Spec.MassOverride=1;const auto Light=Analyze(S,{P});P.Spec.MassOverride=100;const auto Heavy=Analyze(S,{P});
    TestTrue(TEXT("upper hoist boundary uses loaded rather than fixed ballast draft"),Light.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("HoistLimit");})&&!Heavy.Issues.ContainsByPredicate([](const FIssue& I){return I.Code==TEXT("HoistLimit");}));
    FSettings Loaded;TArray<FPlacement> Plan;FString E;TestTrue(TEXT("TPC and confirmation survive plan roundtrip"),PlanFromJson(PlanToJson(S,{P}),Loaded,Plan,E)&&Loaded.TPC==10&&Loaded.bTPCConfirmed&&Loaded.WaterplaneCoefficient==.85&&Loaded.WaterDensity==1.025);
    S.TPC=0;TestFalse(TEXT("certified flag cannot approve zero TPC"),S.Validate(E));S.bTPCConfirmed=false;S.WaterplaneCoefficient=1.01;TestFalse(TEXT("invalid synthetic waterplane coefficient rejected"),S.Validate(E));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilProjectionResizeTest,"MA0T10.CoilLoading.Presentation.ProjectionResizeDpiAndPick",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilProjectionResizeTest::RunTest(const FString&)
{
    const FVector4 Regions[]={FVector4(.2,.12,.99,.74),FVector4(.4,.12,.99,.74),FVector4(.2,.25,.99,.60),FVector4(.2,.05,.99,.90)};
    for(double Dpi:{1.,1.25,1.5,2.})for(const auto R:Regions)
    {
        FMatrix P=FMatrix::Identity;P.M[0][0]=1.732050807568877;P.M[1][1]=P.M[0][0]*1920/1080;P.M[2][0]=.11;P.M[2][1]=-.07;P.M[2][2]=0;P.M[2][3]=1;P.M[3][2]=.1;P.M[3][3]=0;
        CoilPlan::FitPlayerProjection(P,FIntRect(0,0,1920,1080),R);
        const auto Screen=[&](FVector V){const FVector4 C=P.TransformFVector4(FVector4(V,1));return FVector2D((C.X/C.W+1)*960,(1-C.Y/C.W)*540);};
        const auto Center=Screen({0,0,20}),X=Screen({1,0,20}),Y=Screen({0,1,20});TestTrue(TEXT("drawer aspect changes preserve circular pixels"),FMath::IsNearlyEqual((X-Center).Size(),(Y-Center).Size(),1.e-6));
        TestTrue(TEXT("film offset remapped on both content axes"),FMath::IsNearlyEqual(Center.X,(R.X+R.Z+.11*(R.Z-R.X))*960,1.e-6)&&FMath::IsNearlyEqual(Center.Y,(R.Y+R.W+.07*(R.W-R.Y))*540,1.e-6));
        const FVector World(.3,-.2,20);const FVector4 C=P.TransformFVector4(FVector4(World,1));const auto Pixel=Screen(World);const FVector2D SlatePixel=Pixel/Dpi;const FVector4 U=P.Inverse().TransformFVector4(FVector4(SlatePixel.X*Dpi/960-1,1-SlatePixel.Y*Dpi/540,C.Z/C.W,1));
        TestTrue(TEXT("physical/slate DPI roundtrip yields same picking point"),FVector::Distance(FVector(U.X/U.W,U.Y/U.W,U.Z/U.W),World)<1.e-6);
        CoilLoading::FSettings S;CoilLoading::FPlacement A,Next;A.Center={0,0,.85};Next=A;Next.Center.Y=4;const FVector2D Logical(1000,600);auto Plan=CoilPlan::Build(S,{A},Next,{}, {},Logical,true,1);const auto Point=Plan.Glyphs[0].Center+FVector2D((Plan.Glyphs[0].Radius+Plan.Glyphs[0].Bore)*.5,0);const auto G=FGeometry::MakeRoot(Logical,FSlateLayoutTransform(float(Dpi),FVector2D(45,60)));TestEqual(TEXT("plan picking remains correct at DPI"),Plan.Hit(G.AbsoluteToLocal(G.LocalToAbsolute(Point))),0);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilRakedLayoutTest,"MA0T10.CoilLoading.Presentation.RakedPortalLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilRakedLayoutTest::RunTest(const FString&)
{
    CoilLoading::FSettings S;
    for(int Rail=0;Rail<2;++Rail)for(double Sign:{-1.,1.})
    {
        const auto A=CoilQuay::LegNode(S,Sign,Rail,1.4),K=CoilQuay::LegNode(S,Sign,Rail,16),B=CoilQuay::LegNode(S,Sign,Rail,28);
        TestTrue(TEXT("portal top contracts longitudinally"),FMath::IsNearlyEqual(FMath::Abs(B.X+8),3.4,1.e-9));
        if(Rail==0)
        {
            TestTrue(TEXT("sea leg lower portion .6m inset and upper 5m inset"),FMath::IsNearlyEqual((K.Y-A.Y)/S.QuaySide,.6,1.e-9)&&FMath::IsNearlyEqual((B.Y-A.Y)/S.QuaySide,5.,1.e-9));
            TestTrue(TEXT("main upper leg photo rake20.14degrees rather than old4degree column"),FMath::IsNearlyEqual(FMath::RadiansToDegrees(FMath::Atan(FMath::Abs(B.Y-K.Y)/12.)),20.1363034282,1.e-6));
            TestTrue(TEXT("continuous knee joins both primary leg segments"),FVector::Distance(CoilQuay::LegNode(S,Sign,Rail,16.-1.e-7),CoilQuay::LegNode(S,Sign,Rail,16.+1.e-7))<3.e-7);
        }
        else TestTrue(TEXT("land leg retains2m rake"),FMath::IsNearlyEqual((B.Y-A.Y)/S.QuaySide,-2.,1.e-9));
        TestTrue(TEXT("main leg connects directly beneath box girder without a detached brace"),B.Y>=CoilQuay::GirderLand(S)&&B.Y<=CoilQuay::GirderSea(S)&&B.Z==28);
    }
    for(double Z:CoilQuay::PortalTubeLevels)TestTrue(TEXT("each thick tube joins both actual leg faces at its level"),CoilQuay::LegNode(S,1,0,Z).Y>CoilQuay::LegNode(S,1,1,Z).Y);
    TestEqual(TEXT("four dominant tube levels"),int32(UE_ARRAY_COUNT(CoilQuay::PortalTubeLevels)),int32(4));
    TestEqual(TEXT("photo thick tube diameter .62m"),CoilQuay::PortalTubeDiameter,.62);
    TestTrue(TEXT("outreach and hoist screening inputs unchanged"),CoilQuay::GirderSea(S)>S.Beam*.5&&S.MaxRadius==35);
    S.RailGauge=1;TestTrue(TEXT("narrow gauge cannot invert portal order"),CoilQuay::LegNode(S,1,0,28).Y>CoilQuay::LegNode(S,1,1,28).Y);return true;
}
namespace
{
FString Root(){return CoilTestReportPaths::Root();}
class FCoilPresentationRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Step=0;
    uint64 IdleWrites=0,IdleCalls=0;TSharedPtr<int32> IdleEvents=MakeShared<int32>(0);
    TArray<TPair<TWeakObjectPtr<USceneComponent>,FDelegateHandle>> ProbeHandles;double Begin=FPlatformTime::Seconds(),Changed=0;TSharedRef<FJsonObject> Evidence=MakeShared<FJsonObject>();
    void Capture(UWorld& W,const FString& Name,bool CheckBackground=false)
    {
        TArray<FColor> Pixels;FIntVector Size;if(!Test->TestTrue(TEXT("actual D3D12 presentation screenshot"),FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;
        TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);IFileManager::Get().MakeDirectory(*(Root()/TEXT("progress")),true);FFileHelper::SaveArrayToFile(Bytes,*(Root()/TEXT("progress")/(Name+TEXT(".png"))));Evidence->SetNumberField(TEXT("client_width"),Size.X);Evidence->SetNumberField(TEXT("client_height"),Size.Y);
        if(Name==TEXT("08-ref4-coil-lifter"))
        {
            int Neutral=0,Count=0;for(int Y=Size.Y*.30;Y<Size.Y*.58;++Y)for(int X=Size.X*.50;X<Size.X*.70;++X){const auto C=Pixels[Y*Size.X+X];++Count;if(C.R+C.G+C.B<250&&FMath::Max3(C.R,C.G,C.B)-FMath::Min3(C.R,C.G,C.B)<40)++Neutral;}
            Evidence->SetNumberField(TEXT("lift_neutral_load_pixels"),Neutral);Test->TestTrue(TEXT("lift photo contains dark neutral coil instead of enclosing cabin wall"),Neutral>Count*.005);
        }
        if(Name==TEXT("03-mixed-3d-highlight"))
        {
            // COIL-02 frames the real full hold. Test its projected floor, not
            // the intentionally empty black surround of a wider camera view.
            TArray<FVector2D> Floor;for(TActorIterator<ACoilLoadingSimulation> I(&W);I;++I){for(auto P:{FVector(-I->Settings.HoldLength*.5,-I->Settings.HoldWidth*.5,.1),FVector(I->Settings.HoldLength*.5,-I->Settings.HoldWidth*.5,.1),FVector(I->Settings.HoldLength*.5,I->Settings.HoldWidth*.5,.1),FVector(-I->Settings.HoldLength*.5,I->Settings.HoldWidth*.5,.1)}){FVector2D Screen;if(W.GetFirstPlayerController()->ProjectWorldLocationToScreen(I->GetActorTransform().TransformPosition(P*100),Screen))Floor.Add(Screen);}break;}
            Test->TestEqual(TEXT("all four actual hold floor corners project"),Floor.Num(),4);
            const auto Inside=[&](FVector2D P){if(Floor.Num()!=4)return false;bool Positive=false,Negative=false;for(int I=0;I<4;++I){const auto A=Floor[(I+1)%4]-Floor[I],B=P-Floor[I];const double Cross=A.X*B.Y-A.Y*B.X;Positive|=Cross>0;Negative|=Cross<0;}return !(Positive&&Negative);};
            int Dark=0,Count=0;for(int Y=0;Y<Size.Y;Y+=4)for(int X=0;X<Size.X;X+=4)if(Inside(FVector2D(X,Y))){const auto C=Pixels[Y*Size.X+X];++Count;if(C.R+C.G+C.B<18)++Dark;}Test->TestTrue(TEXT("projected hold floor has meaningful visible pixel area"),Count>100);
            const double Fraction=double(Dark)/FMath::Max(1,Count);Evidence->SetNumberField(TEXT("hold_near_black_fraction"),Fraction);Test->TestTrue(TEXT("cutaway shows actual hold and coils rather than old target behind bulkhead"),Fraction<.5);
        }
        if(CheckBackground)
        {
            int Dark=0,Count=0;for(int Y=150;Y<790&&Y<Size.Y;Y+=4)for(int X=400;X<Size.X-20;X+=4){const auto C=Pixels[Y*Size.X+X];++Count;if(C.R+C.G+C.B<18)++Dark;}
            const double Fraction=double(Dark)/FMath::Max(1,Count);Evidence->SetNumberField(TEXT("dark_background_fraction"),Fraction);Test->TestTrue(TEXT("outdoor scene has no black background (under2percent near-black area)"),Fraction<.02);
        }
    }
    void Camera(ACoilLoadingMonitorHost& H,FVector Position,FVector Target)
    {H.ViewCamera->GetCameraComponent()->SetFieldOfView(60);H.ViewCamera->SetActorLocationAndRotation(H.Simulation->GetActorTransform().TransformPosition(Position*100),H.Simulation->GetActorQuat()*(Target-Position).Rotation().Quaternion());}
    void Construction(ACoilLoadingSimulation& S)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(&S);
        const auto Count=[&](const TCHAR* Tag){int32 N=0;for(auto* C:Parts)if(C->ComponentHasTag(Tag))N+=C->GetInstanceCount();return N;};
        for(auto* C:Parts)if(C->GetStaticMesh()&&C->GetStaticMesh()->GetName()==TEXT("Cylinder"))for(int I=0;I<C->GetInstanceCount();++I)
        {
            FTransform T;C->GetInstanceTransform(I,T,false);const FVector Axis=T.GetRotation().RotateVector(FVector::UpVector)*T.GetScale3D().Z*.5,Center=T.GetLocation()*.01,A=Center-Axis,B=Center+Axis;
            const auto Middle=[&](FVector P){return FMath::Abs(P.Y-S.Settings.RailCenterY())<.2&&FMath::Abs(P.Z-S.QuayZ()-19)<.2;};
            const auto SeaEnd=[&](FVector P){return FMath::Abs(P.Y-(S.Settings.RailCenterY()-S.Settings.QuaySide*(S.Settings.MaxRadius-1)))<.2&&FMath::Abs(P.Z-S.QuayZ()-27)<.2;};
            Test->TestFalse(TEXT("no unsupported lower cantilever knee strut"),(Middle(A)&&SeaEnd(B))||(Middle(B)&&SeaEnd(A)));
        }
        Test->TestEqual(TEXT("four legs: two sea lower/upper segments and two land columns"),Count(TEXT("CoilPortalLeg")),6);
        Test->TestEqual(TEXT("ref6 at least two rail bogies per leg"),Count(TEXT("CoilRailBogie")),8);
        Test->TestEqual(TEXT("each bogie has two axles / four wheel faces"),Count(TEXT("CoilBogieWheel")),32);
        Test->TestEqual(TEXT("trolley four wheels"),Count(TEXT("CoilTrolleyWheel")),4);
        Test->TestEqual(TEXT("four large horizontal tubes on each principal side frame"),Count(TEXT("CoilPortalHorizontal")),8);
        Test->TestEqual(TEXT("sparse photo diagonals replace repeated small X grid"),Count(TEXT("CoilPortalDiagonal")),4);
        Test->TestEqual(TEXT("three end ties per rail portal"),Count(TEXT("CoilPortalEndTie")),6);
        int CollisionLegs=0;TInlineComponentArray<UStaticMeshComponent*> Simple(&S);for(auto* C:Simple)if(C->ComponentHasTag(TEXT("CoilPortalLegCollision"))){++CollisionLegs;Test->TestTrue(TEXT("primary leg collision stays a simple box"),C->GetStaticMesh()->GetName()==TEXT("Cube")&&C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);}
        Test->TestEqual(TEXT("six primary simple leg boxes follow visible segments"),CollisionLegs,6);
        for(auto* C:Parts)if(C->ComponentHasTag(TEXT("CoilPortalHorizontal")))for(int I=0;I<C->GetInstanceCount();++I)
        {
            FTransform T;C->GetInstanceTransform(I,T,false);const FVector D=T.GetRotation().RotateVector(FVector::UpVector)*T.GetScale3D().Z*.5,A=T.GetLocation()*.01-D,B=T.GetLocation()*.01+D;
            Test->TestTrue(TEXT("horizontal tube is .62m thick and spans real leg centers"),FMath::IsNearlyEqual(T.GetScale3D().X,.62,1.e-6)&&FMath::IsNearlyEqual(A.Z,B.Z,1.e-6)&&T.GetScale3D().Z>4.);
            bool Joined=false;for(double Sign:{-1.,1.})for(double Z:CoilQuay::PortalTubeLevels){const auto Sea=CoilQuay::LegNode(S.Settings,Sign,0,Z)+FVector(0,0,S.BaseQuayZ()),Land=CoilQuay::LegNode(S.Settings,Sign,1,Z)+FVector(0,0,S.BaseQuayZ());Joined|=(A.Equals(Sea,1.e-5)&&B.Equals(Land,1.e-5))||(B.Equals(Sea,1.e-5)&&A.Equals(Land,1.e-5));}Test->TestTrue(TEXT("tube endpoints coincide with photographed main leg profile"),Joined);
        }
        Test->TestTrue(TEXT("zigzag stair tower with eight landings and144 treads"),Count(TEXT("CoilStairLanding"))==8&&Count(TEXT("CoilStairTread"))==144);
        Test->TestTrue(TEXT("box girder repeated vertical web stiffeners"),Count(TEXT("CoilGirderStiffener"))>=80);
        Test->TestTrue(TEXT("sagged festoon and two wire drums"),Count(TEXT("CoilFestoonCable"))==128&&Count(TEXT("CoilWinchDrum"))==2);
        Test->TestEqual(TEXT("one underhung operator cab"),Count(TEXT("CoilOperatorCab")),1);
        Test->TestEqual(TEXT("four standing folding panels"),Count(TEXT("CoilStandingCoverPanel")),4);
        Test->TestTrue(TEXT("folding hinges hydraulic cylinders and lifting lugs"),Count(TEXT("CoilCoverHinge"))==8&&Count(TEXT("CoilCoverCylinder"))==4&&Count(TEXT("CoilCoverLug"))==8);
        Test->TestTrue(TEXT("closed hatch seams and cleats"),Count(TEXT("CoilClosedCoverSeam"))==9&&Count(TEXT("CoilCoverCleat"))>=50);
        Test->TestEqual(TEXT("three stowed deck crane posts"),Count(TEXT("CoilDeckCranePost")),3);
        for(auto* C:Parts)
        {
            if(C->ComponentHasTag(TEXT("CoilBogieWheel")))for(int I=0;I<C->GetInstanceCount();++I)
            {
                FTransform T;C->GetInstanceTransform(I,T,true);const double Center=S.GetActorTransform().InverseTransformPosition(T.GetLocation()).Z*.01,Bottom=Center-CoilQuay::WheelTreadRadius;
                Test->TestTrue(TEXT("bogie steel tread at rail top; flange extends below rail head"),FMath::IsNearlyEqual(Bottom,S.QuayZ()+.16,.015));Test->TestTrue(TEXT("wheel has real flange profile"),FMath::IsNearlyEqual(C->GetStaticMesh()->GetBounds().BoxExtent.X*.01,CoilQuay::WheelFlangeRadius,.001));
                Test->TestTrue(TEXT("wheel axle perpendicular to rail"),FMath::Abs(T.GetRotation().RotateVector(FVector::UpVector).Y)>.99);
            }
            if(C->ComponentHasTag(TEXT("CoilDeckCranePost"))){FLinearColor Paint;C->GetMaterial(0)->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColdColor")),Paint);Test->TestTrue(TEXT("deck crane neutral white rather than pink"),FMath::Abs(Paint.R-Paint.B)<.04&&Paint.R>.75);}
        }
        int Numbers=0,Positive=0,Negative=0;TInlineComponentArray<UTextRenderComponent*> Labels(&S);for(auto* T:Labels)if(T->ComponentHasTag(TEXT("CoilEquipmentNumber"))){++Numbers;Test->TestEqual(TEXT("equipment number literal31"),T->Text.ToString(),FString(TEXT("31")));Test->TestTrue(TEXT("number has positive scale, no mirrored geometry"),T->GetComponentTransform().GetDeterminant()>0);const double Face=FVector::DotProduct(T->GetForwardVector(),S.GetActorForwardVector());Positive+=Face>.8;Negative+=Face<-.8;}
        Test->TestTrue(TEXT("four leg numbers on each outward face"),Numbers==8&&Positive==4&&Negative==4);
        for(auto* C:Parts)if(C->ComponentHasTag(TEXT("CoilPortalLeg")))for(int I=0;I<C->GetInstanceCount();++I){FTransform T;C->GetInstanceTransform(I,T,false);const FVector Axis=T.GetRotation().RotateVector(FVector::UpVector);Test->TestTrue(TEXT("lower sea, upper sea and land legs use actual profile angles"),FMath::IsNearlyEqual(FMath::Abs(Axis.Y),.6/FMath::Sqrt(FMath::Square(14.6)+FMath::Square(.6)+FMath::Square(.6*14.6/26.6)),1.e-6)||FMath::IsNearlyEqual(FMath::Abs(Axis.Y),(CoilQuay::PhotoSeaInset()-.6)/FMath::Sqrt(144+FMath::Square(CoilQuay::PhotoSeaInset()-.6)+FMath::Square(.6*12/26.6)),1.e-6)||FMath::IsNearlyEqual(FMath::Abs(Axis.Y),2/FMath::Sqrt(FMath::Square(26.6)+4+.36),1.e-6));}
        int StrongRake=0;for(auto* C:Parts)if(C->ComponentHasTag(TEXT("CoilPortalLeg")))for(int I=0;I<C->GetInstanceCount();++I){FTransform T;C->GetInstanceTransform(I,T,false);if(FMath::Abs(T.GetRotation().RotateVector(FVector::UpVector).Y)>.25)++StrongRake;}Test->TestEqual(TEXT("both sea upper main legs have strong photo rake"),StrongRake,2);
        Evidence->SetNumberField(TEXT("sea_top_inset_m"),CoilQuay::PhotoSeaInset());Evidence->SetNumberField(TEXT("sea_upper_rake_deg"),FMath::RadiansToDegrees(FMath::Atan((CoilQuay::PhotoSeaInset()-.6)/12.)));Evidence->SetNumberField(TEXT("horizontal_tube_diameter_m"),.62);
        Test->TestEqual(TEXT("ref1 upper stays visible; no lower knee struts"),Count(TEXT("CoilUpperStay")),2);
        Evidence->SetNumberField(TEXT("loaded_draft_m"),S.Hydrostatics().Draft);Evidence->SetNumberField(TEXT("estimated_tpc_t_per_cm"),S.Hydrostatics().TonnesPerCm);
        Evidence->SetBoolField(TEXT("ref6_construction_checks"),!Test->HasAnyErrors());
    }
public:
    explicit FCoilPresentationRun(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Begin>180){Test->AddError(TEXT("COIL-01d presentation timeout"));return true;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()){Changed=Now;return false;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;ACoilLoadingMonitorHost* H=nullptr;for(TActorIterator<ACoilLoadingMonitorHost> I(W);I;++I)H=*I;if(!H||!H->Simulation||!H->Monitor)return false;
        if(!Changed){Changed=Now;return false;}if(Now-Changed<1)return false;auto& S=*H->Simulation;auto& M=*H->Monitor;
        if(Step==0)
        {
            IdleWrites=S.RigTransformWriteCount();IdleCalls=S.RigRefreshCount();TInlineComponentArray<USceneComponent*> Cs(&S);for(auto* C:Cs){const auto Counter=IdleEvents;auto Handle=C->TransformUpdated.AddLambda([Counter](USceneComponent*,EUpdateTransformFlags,ETeleportType){++*Counter;});ProbeHandles.Add({C,Handle});}Step=100;Changed=Now;return false;
        }
        if(Step==100)
        {
            if(Now-Changed<1.5)return false;for(auto& P:ProbeHandles)if(P.Key.IsValid())P.Key->TransformUpdated.Remove(P.Value);ProbeHandles.Reset();
            Test->TestTrue(TEXT("idle probe covered at least5 real visual timer callbacks"),S.RigRefreshCount()-IdleCalls>=5);Test->TestEqual(TEXT("idle transform API writes0"),S.RigTransformWriteCount()-IdleWrites,uint64(0));Test->TestEqual(TEXT("idle observed component transform events0"),*IdleEvents,0);Evidence->SetNumberField(TEXT("idle_transform_writes"),S.RigTransformWriteCount()-IdleWrites);Evidence->SetNumberField(TEXT("idle_transform_events"),*IdleEvents);
            const auto Original=S.NextSpec;S.NextSpec.CoilId=TEXT("DRAFT-PROBE");S.NextSpec.MassOverride=100;Test->TestTrue(TEXT("draft probe placement"),S.PlaceNext(false));Test->TestTrue(TEXT("actual scene sinkage responds to placed cargo"),S.Hydrostatics().Sinkage>0&&FMath::IsNearlyEqual(S.QuayZ()-S.BaseQuayZ(),S.Hydrostatics().Sinkage,1.e-8));bool RootShift=false;TInlineComponentArray<USceneComponent*> Roots(&S);for(auto* C:Roots)if(C->ComponentHasTag(TEXT("CoilShoreMotionRoot")))RootShift=FMath::IsNearlyEqual(C->GetRelativeLocation().Z*.01,S.Hydrostatics().Sinkage,1.e-8);Test->TestTrue(TEXT("quay/crane/water common parent reflects physical relative height"),RootShift);Test->TestTrue(TEXT("undo restores draft"),S.Undo());Test->TestTrue(TEXT("empty ship returns to reference draft"),FMath::IsNearlyZero(S.Hydrostatics().Sinkage,1.e-9));S.NextSpec=Original;S.RefreshPreview();
            M.LoadMixedExample();Test->TestTrue(TEXT("UI button starts existing async load"),S.IsPlanLoadPending());Step=1;Changed=Now;return false;}
        if(Step==1)
        {
            if(S.IsPlanLoadPending())return false;Test->TestEqual(TEXT("mixed example loaded through UI"),S.Plan().Num(),10);M.SyncInputs();M.Refresh();M.SelectView(2);Step=11;Changed=Now;return false;
        }
        if(Step==11)
        {
            auto* Canvas=M.PlanCanvas();const auto Size=Canvas->GetCachedGeometry().GetLocalSize();const auto P=CoilPlan::Build(S.Settings,S.Plan(),S.PreviewPlacement(),S.Analysis(),{},Size,true,1);if(P.Glyphs.Num())Test->TestTrue(TEXT("real plan widget click selects ring"),Canvas->PickAt(P.Glyphs[0].Center+FVector2D((P.Glyphs[0].Radius+P.Glyphs[0].Bore)*.5,0),Size));Test->TestTrue(TEXT("selection shared with3D"),S.IsHighlighted(0));
            UCoilLoadingWarningButton* Warning=nullptr;M.WidgetTree->ForEachWidget([&](UWidget* V){if(auto* B=Cast<UCoilLoadingWarningButton>(V))if(auto* T=Cast<UTextBlock>(B->GetContent()))if(T->GetText().ToString().Contains(TEXT("SWL")))Warning=B;});Test->TestNotNull(TEXT("actual grouped SWL warning button"),Warning);if(Warning)Warning->OnClicked.Broadcast();Test->TestTrue(TEXT("warning click selects overload coil across views"),S.IsHighlighted(4));Step=2;Changed=Now;return false;
        }
        if(Step==2){Capture(*W,TEXT("01-mixed-section-legend"));M.SelectView(3);Test->TestTrue(TEXT("top view preserves shared warning selection"),S.IsHighlighted(4));Step=3;Changed=Now;return false;}
        if(Step==3){Capture(*W,TEXT("02-mixed-top-legend"));M.SelectView(1);Step=4;Changed=Now;return false;}
        if(Step==4){Capture(*W,TEXT("03-mixed-3d-highlight"));M.SelectView(0);Step=5;Changed=Now;return false;}
        if(Step==5)
        {
            Capture(*W,TEXT("04-quay-ref1-ref3"),true);Construction(S);int Instances=0,Groups=0;TInlineComponentArray<UInstancedStaticMeshComponent*> Cs(&S);for(auto* C:Cs)if(C->ComponentHasTag(TEXT("CoilQuayPresentation"))){++Groups;Instances+=C->GetInstanceCount();Test->TestEqual(TEXT("visual architecture excluded from sensor/physics collision"),C->GetCollisionEnabled(),ECollisionEnabled::NoCollision);Test->TestTrue(TEXT("persisted material has instancing shader, no gray fallback"),C->GetMaterial(0)->GetMaterial()->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));if(C->ComponentHasTag(TEXT("CoilPalette2"))){FLinearColor Paint;C->GetMaterial(0)->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColdColor")),Paint);Test->TestTrue(TEXT("crane yellow paint matches reference"),Paint.R>.8&&Paint.G>.45&&Paint.B<.08);}}
            Test->TestTrue(TEXT("quay details batched rather than individual actors"),Groups>5&&Instances>400);Evidence->SetNumberField(TEXT("decorative_instances"),Instances);Evidence->SetNumberField(TEXT("decorative_groups"),Groups);bool ScaleSign=false;TInlineComponentArray<UTextRenderComponent*> Signs(&S);for(auto* T:Signs)if(T->Text.ToString()==TEXT("1.8 m"))ScaleSign=true;Test->TestTrue(TEXT("1.8m signalman reference present"),ScaleSign);int Closed=0;TInlineComponentArray<UStaticMeshComponent*> Shapes(&S);for(auto* C:Shapes)if(C->ComponentHasTag(TEXT("CoilClosedHatchCover")))++Closed;Test->TestEqual(TEXT("only working hold open; other3 hatches closed"),Closed,3);Test->TestTrue(TEXT("12m traffic corridor plus5m rail/stair clearance before shed"),FMath::Abs(S.ShedCenterY()-S.Settings.QuayEdgeY())-5-(S.Settings.RailSetback+S.Settings.RailGauge)>=17-.00001);H->Focus(0);Step=6;Changed=Now;return false;
        }
        if(Step==6){Capture(*W,TEXT("05-portal-detail-ref1"));Test->TestTrue(TEXT("mixed demonstration can be undone before clean crane cycle"),S.Undo());S.HighlightCoils({});S.NextSpec.CoilId=TEXT("COIL-01d-LIFT");S.Settings.bCHook=true;S.RefreshPreview();M.SyncInputs();const FVector Ref4Position(-1,-5,S.QuayZ()+10.5),Ref4Target(-9,-17,S.Settings.HoldHeight+9);H->ViewCamera->GetCameraComponent()->SetFieldOfView(60);H->ViewCamera->SetActorLocationAndRotation(S.GetActorTransform().TransformPosition(Ref4Position*100),S.GetActorQuat()*(Ref4Target-Ref4Position).Rotation().Quaternion());Test->TestTrue(TEXT("detailed crane still operates"),S.PlaceNext());Step=7;Changed=Now;return false;}
        if(Step==7){if(Now-Changed<4.5)return false;if(!S.Suspended){Test->AddError(TEXT("lift subject missing"));return true;}S.SetActorTickEnabled(false);const auto Load=S.GetActorTransform().InverseTransformPosition(S.Suspended->GetActorLocation())*.01;Camera(*H,Load+FVector(9,13,3.5),Load);Step=700;Changed=Now;return false;}
        if(Step==700){if(Now-Changed<.5)return false;Capture(*W,TEXT("06-trolley-hook-sling-ref2"));Capture(*W,TEXT("08-ref4-coil-lifter"));S.SetActorTickEnabled(true);H->Focus(0);Step=71;Changed=Now;return false;}
        if(Step==71){Capture(*W,TEXT("15-ref6-lift"));Evidence->SetNumberField(TEXT("ref6_camera_height_m"),1.8);Evidence->SetNumberField(TEXT("ref6_camera_horizontal_fov_deg"),H->ViewCamera->GetCameraComponent()->FieldOfView);Test->TestTrue(TEXT("ref6 direction: boom and vessel project left"),FVector::DotProduct(H->ViewCamera->GetActorRightVector(),S.GetActorQuat().RotateVector(FVector(0,-S.Settings.QuaySide,0)))<0);S.CancelOperation();H->Focus(2);Step=8;Changed=Now;return false;}
        if(Step==8)
        {
            Capture(*W,TEXT("07-shed-cradles-ref3"));M.SelectView(0);const FVector Ref5Position(180,S.QuayOffset(50),S.QuayZ()+7.5),Ref5Target(S.Settings.VesselCenterX(),0,S.Settings.HoldHeight+1);H->ViewCamera->SetActorLocationAndRotation(S.GetActorTransform().TransformPosition(Ref5Position*100),S.GetActorQuat()*(Ref5Target-Ref5Position).Rotation().Quaternion());Step=9;Changed=Now;return false;
        }
        if(Step==9)
        {
            Capture(*W,TEXT("09-ref5-ship-quay-side"));H->Focus(0);Step=10;Changed=Now;return false;
        }
        if(Step==10){Capture(*W,TEXT("10-ref6-low-quay"));Camera(*H,{-19,S.QuayOffset(22),S.QuayZ()+2.4},{-12,S.QuayOffset(15),S.QuayZ()+.95});Step=12;Changed=Now;return false;}
        if(Step==12){Capture(*W,TEXT("11-bogie-close"));Camera(*H,{5,S.PickupPosition().Y-2,S.QuayZ()+26},{-6,S.PickupPosition().Y,S.QuayZ()+26.5});Step=13;Changed=Now;return false;}
        if(Step==13){Capture(*W,TEXT("12-trolley-cab-close"));Camera(*H,{-25,S.QuayOffset(24),S.QuayZ()+36},{-8,-8,S.QuayZ()+29});Step=14;Changed=Now;return false;}
        if(Step==14)
        {
            Capture(*W,TEXT("13-girder-top-close"));Camera(*H,{45,-5,S.QuayZ()+26},{-8,-8.5,S.QuayZ()+29});Step=15;Changed=Now;return false;
        }
        if(Step==15)
        {
            Capture(*W,TEXT("14-boom-side"));const auto Joint=CoilQuay::LegNode(S.Settings,1,0,26)+FVector(0,0,S.QuayZ());Camera(*H,Joint+FVector(18,9,-8),Joint);Step=16;Changed=Now;return false;
        }
        if(Step==16){Capture(*W,TEXT("16-leg-boom-connection"));Camera(*H,{18,S.QuayOffset(29),S.QuayZ()+4},{-8,S.Settings.RailCenterY(),S.QuayZ()+14});Step=17;Changed=Now;return false;}
        if(Step==17)
        {
            Capture(*W,TEXT("17-horizontal-tubes"));const auto Status=S.Lidar->GetSensorRuntimeStatus();Test->TestTrue(TEXT("presentation retains HWRT acquisition"),Status.ActiveAcquisitionBackend==TEXT("hardware_ray_tracing"));Evidence->SetStringField(TEXT("backend"),Status.ActiveAcquisitionBackend);Evidence->SetNumberField(TEXT("acquisition_hz"),Status.MeasuredAcquisitionRateHz);Evidence->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());Evidence->SetStringField(TEXT("scope"),TEXT("automated RHI and Slate delegation; distinct from direct mouse acceptance"));FString Text;FJsonSerializer::Serialize(Evidence,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Root()/TEXT("presentation.json")));return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilPresentationTest,"MA0T10.CoilLoading.Presentation.MixedAndQuayRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilPresentationTest::RunTest(const FString&)
{
    if(!GDynamicRHI){AddError(TEXT("presentation requires actual RHI"));return false;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilPresentationRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
