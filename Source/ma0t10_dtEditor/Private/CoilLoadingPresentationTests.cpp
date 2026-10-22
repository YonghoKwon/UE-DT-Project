#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
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
namespace
{
FString Root(){return FPaths::ProjectSavedDir()/TEXT("Reports/COIL-01b");}
class FCoilPresentationRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Step=0;double Begin=FPlatformTime::Seconds(),Changed=0;TSharedRef<FJsonObject> Evidence=MakeShared<FJsonObject>();
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
            int Dark=0,Count=0;for(int Y=200;Y<740&&Y<Size.Y;Y+=4)for(int X=500;X<Size.X-100;X+=4){const auto C=Pixels[Y*Size.X+X];++Count;if(C.R+C.G+C.B<18)++Dark;}
            const double Fraction=double(Dark)/FMath::Max(1,Count);Evidence->SetNumberField(TEXT("hold_near_black_fraction"),Fraction);Test->TestTrue(TEXT("cutaway shows actual hold and coils rather than old target behind bulkhead"),Fraction<.5);
        }
        if(CheckBackground)
        {
            int Dark=0,Count=0;for(int Y=150;Y<790&&Y<Size.Y;Y+=4)for(int X=400;X<Size.X-20;X+=4){const auto C=Pixels[Y*Size.X+X];++Count;if(C.R+C.G+C.B<18)++Dark;}
            const double Fraction=double(Dark)/FMath::Max(1,Count);Evidence->SetNumberField(TEXT("dark_background_fraction"),Fraction);Test->TestTrue(TEXT("outdoor scene has no black background (under2percent near-black area)"),Fraction<.02);
        }
    }
    void Camera(ACoilLoadingMonitorHost& H,FVector Position,FVector Target)
    {H.ViewCamera->SetActorLocationAndRotation(H.Simulation->GetActorTransform().TransformPosition(Position*100),H.Simulation->GetActorQuat()*(Target-Position).Rotation().Quaternion());}
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
        Test->TestEqual(TEXT("ref6 four tapered box legs"),Count(TEXT("CoilPortalLeg")),4);
        Test->TestEqual(TEXT("ref6 at least two rail bogies per leg"),Count(TEXT("CoilRailBogie")),8);
        Test->TestEqual(TEXT("each bogie has two axles / four wheel faces"),Count(TEXT("CoilBogieWheel")),32);
        Test->TestEqual(TEXT("trolley four wheels"),Count(TEXT("CoilTrolleyWheel")),4);
        Test->TestTrue(TEXT("multi-level circular X/K portal bracing"),Count(TEXT("CoilPortalTube"))>=30);
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
                FTransform T;C->GetInstanceTransform(I,T,false);const double Bottom=T.GetLocation().Z*.01-C->GetStaticMesh()->GetBounds().BoxExtent.X*T.GetScale3D().X*.01;
                Test->TestTrue(TEXT("bogie wheel tread at rail top, not floating"),FMath::IsNearlyEqual(Bottom,S.QuayZ()+.16,.015));
                Test->TestTrue(TEXT("wheel axle perpendicular to rail"),FMath::Abs(T.GetRotation().RotateVector(FVector::UpVector).Y)>.99);
            }
            if(C->ComponentHasTag(TEXT("CoilDeckCranePost"))){FLinearColor Paint;C->GetMaterial(0)->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColdColor")),Paint);Test->TestTrue(TEXT("deck crane neutral white rather than pink"),FMath::Abs(Paint.R-Paint.B)<.04&&Paint.R>.75);}
        }
        Evidence->SetBoolField(TEXT("ref6_construction_checks"),!Test->HasAnyErrors());
    }
public:
    explicit FCoilPresentationRun(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Begin>180){Test->AddError(TEXT("COIL-01b presentation timeout"));return true;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()){Changed=Now;return false;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;ACoilLoadingMonitorHost* H=nullptr;for(TActorIterator<ACoilLoadingMonitorHost> I(W);I;++I)H=*I;if(!H||!H->Simulation||!H->Monitor)return false;
        if(!Changed){Changed=Now;return false;}if(Now-Changed<1)return false;auto& S=*H->Simulation;auto& M=*H->Monitor;
        if(Step==0){M.LoadMixedExample();Test->TestTrue(TEXT("UI button starts existing async load"),S.IsPlanLoadPending());Step=1;Changed=Now;return false;}
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
        if(Step==6){Capture(*W,TEXT("05-portal-detail-ref1"));Test->TestTrue(TEXT("mixed demonstration can be undone before clean crane cycle"),S.Undo());S.HighlightCoils({});S.NextSpec.CoilId=TEXT("COIL-01b-LIFT");S.Settings.bCHook=true;S.RefreshPreview();M.SyncInputs();const FVector Ref4Position(-1,-5,S.QuayZ()+10.5),Ref4Target(-9,-17,S.Settings.HoldHeight+9);H->ViewCamera->SetActorLocationAndRotation(S.GetActorTransform().TransformPosition(Ref4Position*100),S.GetActorQuat()*(Ref4Target-Ref4Position).Rotation().Quaternion());Test->TestTrue(TEXT("detailed crane still operates"),S.PlaceNext());Step=7;Changed=Now;return false;}
        if(Step==7){if(Now-Changed<4.5)return false;Capture(*W,TEXT("06-trolley-hook-sling-ref2"));Capture(*W,TEXT("08-ref4-coil-lifter"));S.CancelOperation();H->Focus(2);Step=8;Changed=Now;return false;}
        if(Step==8)
        {
            Capture(*W,TEXT("07-shed-cradles-ref3"));M.SelectView(0);const FVector Ref5Position(180,S.QuayOffset(50),S.QuayZ()+7.5),Ref5Target(S.Settings.VesselCenterX(),0,S.Settings.HoldHeight+1);H->ViewCamera->SetActorLocationAndRotation(S.GetActorTransform().TransformPosition(Ref5Position*100),S.GetActorQuat()*(Ref5Target-Ref5Position).Rotation().Quaternion());Step=9;Changed=Now;return false;
        }
        if(Step==9)
        {
            Capture(*W,TEXT("09-ref5-ship-quay-side"));Camera(*H,{-52,S.QuayOffset(57),S.QuayZ()+2.2},{-8,S.Settings.RailCenterY(),S.QuayZ()+15});Step=10;Changed=Now;return false;
        }
        if(Step==10){Capture(*W,TEXT("10-ref6-low-quay"));Camera(*H,{-19,S.QuayOffset(22),S.QuayZ()+2.4},{-12,S.QuayOffset(15),S.QuayZ()+.95});Step=12;Changed=Now;return false;}
        if(Step==12){Capture(*W,TEXT("11-bogie-close"));Camera(*H,{-8,0,S.QuayZ()+27},{-5.3,S.PickupPosition().Y+2.3,S.QuayZ()+26.5});Step=13;Changed=Now;return false;}
        if(Step==13){Capture(*W,TEXT("12-trolley-cab-close"));Camera(*H,{-25,S.QuayOffset(24),S.QuayZ()+36},{-8,-8,S.QuayZ()+29});Step=14;Changed=Now;return false;}
        if(Step==14)
        {
            Capture(*W,TEXT("13-girder-top-close"));const auto Status=S.Lidar->GetSensorRuntimeStatus();Test->TestTrue(TEXT("presentation retains HWRT acquisition"),Status.ActiveAcquisitionBackend==TEXT("hardware_ray_tracing"));Evidence->SetStringField(TEXT("backend"),Status.ActiveAcquisitionBackend);Evidence->SetNumberField(TEXT("acquisition_hz"),Status.MeasuredAcquisitionRateHz);Evidence->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());Evidence->SetStringField(TEXT("scope"),TEXT("automated RHI and Slate delegation; distinct from direct mouse acceptance"));FString Text;FJsonSerializer::Serialize(Evidence,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Root()/TEXT("presentation.json")));return true;
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
