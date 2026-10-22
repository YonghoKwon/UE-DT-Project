#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingPlanProjection.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "DynamicRHI.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ShaderCompiler.h"
#include "TimerManager.h"

// COIL-CAP-01: capture-only fixture consumer. No product code, asset writes,
// load-limit overrides, approval data or calculated outputs are changed here.
namespace
{
FString EnvironmentRoot()
{
    FString P;FParse::Value(FCommandLine::Get(),TEXT("CoilEnvironmentOutput="),P);
    return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/COIL-CAP-01"):P;
}
void EnvironmentJson(const FString& Name,TSharedRef<FJsonObject> O)
{FString J;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&J));FFileHelper::SaveStringToFile(J,*(EnvironmentRoot()/Name),FFileHelper::EEncodingOptions::ForceUTF8);}
TArray<TSharedPtr<FJsonValue>> EnvironmentVector(FVector P)
{return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)};}
TSharedRef<FJsonObject> AnalysisJson(const CoilLoading::FResult& A)
{
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("total_mass_t"),A.TotalMass);O->SetNumberField(TEXT("kg_m"),A.KG);O->SetNumberField(TEXT("gm_m"),A.GM);O->SetNumberField(TEXT("list_deg"),A.HeelDegrees);O->SetNumberField(TEXT("draft_m"),A.Hydro.Draft);O->SetBoolField(TEXT("draft_confirmed"),A.Hydro.bConfirmed);
    TArray<TSharedPtr<FJsonValue>> Issues;
    for(const auto& I:A.Issues)
    {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("code"),I.Code.ToString());J->SetStringField(TEXT("reason"),I.Reason);J->SetNumberField(TEXT("coil_index"),I.Coil);J->SetNumberField(TEXT("severity"),int32(I.Severity));J->SetBoolField(TEXT("known"),I.bKnown);J->SetNumberField(TEXT("value"),I.Value);J->SetNumberField(TEXT("limit"),I.Limit);Issues.Add(MakeShared<FJsonValueObject>(J));}
    O->SetArrayField(TEXT("issues"),Issues);return O;
}
struct FEnvironmentShot { FString Name;int View=1;bool UI=false,Plot=false;FVector Position,Aim; };
class FCoilEnvironmentCapture final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;FString Scene;int Step=0,Shot=0;double Began=FPlatformTime::Seconds(),Changed=0;
    TArray<FEnvironmentShot> Shots;TArray<ESlateVisibility> Visibility;FVector Target;
    CoilLoading::FResult PlacedAnalysis;TArray<CoilLoading::FPlacement> Expected;
    void Layout(ACoilLoadingMonitorHost& H,const FEnvironmentShot& Q)
    {
        auto& M=*H.Monitor;auto* Root=Cast<UCanvasPanel>(M.WidgetTree->RootWidget);
        if(Visibility.IsEmpty())for(int I=0;I<Root->GetChildrenCount();++I)Visibility.Add(Root->GetChildAt(I)->GetVisibility());
        for(int I=0;I<Visibility.Num();++I)Root->GetChildAt(I)->SetVisibility(Visibility[I]);
        M.Refresh();M.SetRenderOpacity(Q.UI?1:0);
        const auto Pixels=FinalAcceptanceViewportSize();
        if(!Q.UI){H.SetSceneRect(FSlateRect(0,0,Pixels.X,Pixels.Y),FVector2D(Pixels.X,Pixels.Y),true);return;}
        if(!Q.Plot)return;
        const auto Size=UWidgetLayoutLibrary::GetViewportSize(&M)/UWidgetLayoutLibrary::GetViewportScale(&M);
        auto* Plot=M.WidgetTree->FindWidget(TEXT("CoilPlanCard"));auto* Footer=M.WidgetTree->FindWidget(TEXT("CoilAnalysis"));
        for(int I=0;I<Root->GetChildrenCount();++I){auto* C=Root->GetChildAt(I);C->SetVisibility(C==Plot||C==Footer?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);}
        auto* Back=Root->GetChildAt(0);Back->SetVisibility(ESlateVisibility::HitTestInvisible);auto* BS=Cast<UCanvasPanelSlot>(Back->Slot);BS->SetPosition({0,0});BS->SetSize(Size);
        const float FH=Size.Y*.30;auto* PS=Cast<UCanvasPanelSlot>(Plot->Slot);PS->SetPosition({12,12});PS->SetSize({Size.X-24,Size.Y-FH-32});auto* FS=Cast<UCanvasPanelSlot>(Footer->Slot);FS->SetPosition({12,Size.Y-FH-12});FS->SetSize({Size.X-24,FH});
        H.SetSceneRect(FSlateRect(0,0,Pixels.X,Pixels.Y),FVector2D(Pixels.X,Pixels.Y),false);
    }
    void SetShot(ACoilLoadingMonitorHost& H)
    {
        const auto& Q=Shots[Shot];auto& S=*H.Simulation;H.Monitor->SelectView(Q.View);H.Monitor->Refresh();
        if(Q.View==1)H.Focus(1); // existing player-only aft-wall cutaway
        const auto Pose=S.GetActorTransform();H.ViewCamera->SetActorLocationAndRotation(Pose.TransformPosition(Q.Position*100),Pose.GetRotation()*(Q.Aim-Q.Position).Rotation().Quaternion());H.ViewCamera->GetCameraComponent()->SetFieldOfView(60);
        if(Scene==TEXT("lowering"))if(auto* PC=H.GetWorld()->GetFirstPlayerController())
        {TInlineComponentArray<UPrimitiveComponent*> Cs(&S);for(auto* C:Cs)if(C->ComponentHasTag(TEXT("CoilHoistFalls"))||C->ComponentHasTag(TEXT("CoilHookBlock"))||C->ComponentHasTag(TEXT("CoilCHook")))PC->HiddenPrimitiveComponents.Remove(C);if(S.Suspended){TInlineComponentArray<UPrimitiveComponent*> Body(S.Suspended.Get());for(auto* C:Body)PC->HiddenPrimitiveComponents.Remove(C);}}
        Layout(H,Q);
    }
    bool Capture(UWorld& W,ACoilLoadingMonitorHost& H)
    {
        auto& S=*H.Simulation;auto& M=*H.Monitor;const auto& Q=Shots[Shot];
        TArray<FColor> Pixels;FIntVector Size;bool Ok;
        if(Q.UI)Ok=FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size);
        else{auto* V=W.GetGameViewport()->Viewport;const auto N=V->GetSizeXY();Size={N.X,N.Y,0};Ok=V->ReadPixels(Pixels);}
        if(!Test->TestTrue(TEXT("native 1920x1080 RHI capture"),Ok&&Size.X==1920&&Size.Y==1080))return false;
        FIntRect Sample(0,0,Size.X,Size.Y);
        if(Q.Plot)
        {
            // Reuse the existing presentation subject-envelope QA, retaining
            // the full SI hold in the PNG (no crop or enlarged coil drawing).
            const auto G=M.PlanCanvas()->GetCachedGeometry(),VG=W.GetGameViewport()->GetGameViewportWidget()->GetCachedGeometry();const auto P=CoilPlan::Build(S.Settings,S.Plan(),S.PreviewPlacement(),S.Analysis(),{},G.GetLocalSize(),Q.View==2,S.NextRow);FVector2D Min(DBL_MAX,DBL_MAX),Max(-DBL_MAX,-DBL_MAX);
            int N=1;for(const auto& Coil:S.Plan())if(Q.View==3||Coil.Row==S.NextRow)++N;Test->TestEqual(TEXT("all subject glyphs including actual next coil rendered"),P.Glyphs.Num(),N);
            for(const auto& Glyph:P.Glyphs){Min.X=FMath::Min(Min.X,Glyph.Center.X-Glyph.Half.X);Min.Y=FMath::Min(Min.Y,Glyph.Center.Y-Glyph.Half.Y);Max.X=FMath::Max(Max.X,Glyph.Center.X+Glyph.Half.X);Max.Y=FMath::Max(Max.Y,Glyph.Center.Y+Glyph.Half.Y);}
            const auto A=VG.AbsoluteToLocal(G.LocalToAbsolute(Min))*FVector2D(Size.X,Size.Y)/VG.GetLocalSize(),Z=VG.AbsoluteToLocal(G.LocalToAbsolute(Max))*FVector2D(Size.X,Size.Y)/VG.GetLocalSize();Sample=FIntRect(FMath::Clamp(FMath::FloorToInt(A.X)-3,0,Size.X-1),FMath::Clamp(FMath::FloorToInt(A.Y)-3,0,Size.Y-1),FMath::Clamp(FMath::CeilToInt(Z.X)+3,1,Size.X),FMath::Clamp(FMath::CeilToInt(Z.Y)+3,1,Size.Y));
        }
        int Bright=0,Dark=0,Count=0,Green=0;for(int Y=Sample.Min.Y;Y<Sample.Max.Y;Y+=4)for(int X=Sample.Min.X;X<Sample.Max.X;X+=4){const auto C=Pixels[Y*Size.X+X];++Count;Bright+=C.R+C.G+C.B>100;Dark+=C.R+C.G+C.B<18;Green+=C.G>80&&C.G>C.R*1.25&&C.G>C.B*1.15;}
        auto B=MakeShared<FJsonObject>();B->SetNumberField(TEXT("populated_fraction"),double(Bright)/Count);B->SetNumberField(TEXT("near_black_fraction"),double(Dark)/Count);B->SetNumberField(TEXT("green_sampled_pixels"),Green);
        B->SetStringField(TEXT("sample"),Q.Plot?TEXT("existing glyph subject envelope; PNG retains full hold"):TEXT("full native frame"));
        // Sparse measured points additionally require actual renderer points;
        // the prior populated-scene criterion remains on scene/UI captures.
        if(Q.View!=4)Test->TestTrue(TEXT("existing12 percent populated-pixel criterion"),Bright>Count*.12);
        else Test->TestTrue(TEXT("measured point view has real visible points"),S.Lidar->VisualizationComponent->GetVisiblePointCount()>100&&Bright>100);
        TSet<UStaticMesh*> BodyMeshes;for(TActorIterator<ACoilLoadingBody> I(&W);I;++I)if(I->GetOwner()==&S&&S.Plan().ContainsByPredicate([&](const CoilLoading::FPlacement& P){return I->ActorHasTag(FName(*(TEXT("CoilId=")+P.Spec.CoilId)));}))BodyMeshes.Add(I->Body->GetStaticMesh());
        int Instances=0;TInlineComponentArray<UInstancedStaticMeshComponent*> Bs(&S);for(auto* C:Bs)if(C->ComponentHasTag(TEXT("CoilSettledBatch"))&&BodyMeshes.Contains(C->GetStaticMesh()))Instances+=C->GetInstanceCount();
        Test->TestEqual(TEXT("rendered coil instances match actual loaded plan"),Instances,S.Plan().Num());
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("scene"),Scene);R->SetStringField(TEXT("name"),Q.Name);R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetStringField(TEXT("rhi"),GDynamicRHI->GetName());R->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));R->SetNumberField(TEXT("dpi_scale"),UWidgetLayoutLibrary::GetViewportScale(&M));R->SetNumberField(TEXT("font_scale"),M.FontScale());
        R->SetStringField(TEXT("plan_file"),TEXT("fixtures/")+Scene+TEXT(".json"));R->SetNumberField(TEXT("coil_count"),S.Plan().Num());R->SetNumberField(TEXT("rendered_coil_instances"),Instances);
        auto Tiers=MakeShared<FJsonObject>();int HR=0,CR=0;for(int T=1;T<=5;++T){int N=0;for(const auto& P:S.Plan())if(P.Tier==T)++N;Tiers->SetNumberField(FString::FromInt(T),N);}for(const auto& P:S.Plan())(P.Spec.Type==CoilLoading::ECoilType::ColdRolled?CR:HR)++;
        R->SetObjectField(TEXT("tier_distribution"),Tiers);R->SetNumberField(TEXT("hr_count"),HR);R->SetNumberField(TEXT("cr_count"),CR);R->SetObjectField(TEXT("placed_analysis"),AnalysisJson(PlacedAnalysis));R->SetObjectField(TEXT("ui_analysis_including_next"),AnalysisJson(S.Analysis()));R->SetNumberField(TEXT("danger_count"),M.DangerCount);R->SetNumberField(TEXT("caution_count"),M.CautionCount);R->SetNumberField(TEXT("data_needed_count"),M.DataNeededCount);R->SetStringField(TEXT("warning_text"),M.WarningText());R->SetObjectField(TEXT("brightness"),B);
        R->SetArrayField(TEXT("camera_hold_m"),EnvironmentVector(Q.Position));R->SetArrayField(TEXT("aim_hold_m"),EnvironmentVector(Q.Aim));R->SetNumberField(TEXT("exposure_bias"),H.ViewCamera->GetCameraComponent()->PostProcessSettings.AutoExposureBias);R->SetStringField(TEXT("state"),Scene==TEXT("lowering")?TEXT("actual Lowering animation frozen; one suspended + one ghost, neither is a placed coil"):TEXT("loaded production plan; Chaos settling not run; next coil excluded from placed count"));
        R->SetStringField(TEXT("maximum_state_note"),TEXT("MaximumTier=5. Warnings do not reject planned placement; unknown manual/strength/TPC remain unknown. No limit was relaxed."));
        R->SetStringField(TEXT("fixture_rejection_note"),TEXT("Initial17th complete row failed production Analyze HatchClearance0.20m swept-gear criterion. Retained16 complete rows. Five tiers were not rejected by product. This is a plan, not a safe-load approval."));
        R->SetStringField(TEXT("lidar_profile"),TEXT("OusterOS1_64_Rev7 / FullSpec"));R->SetNumberField(TEXT("lidar_visible_points"),S.Lidar->VisualizationComponent->GetVisiblePointCount());R->SetStringField(TEXT("lidar_backend"),S.Lidar->GetSensorRuntimeStatus().ActiveAcquisitionBackend);
        R->SetNumberField(TEXT("lidar_frame_id"),S.Lidar->GetSensorRuntimeStatus().FrameId);R->SetStringField(TEXT("point_renderer"),S.Lidar->VisualizationComponent->GetActiveRendererName());R->SetStringField(TEXT("point_renderer_fallback"),S.Lidar->VisualizationComponent->GetRendererFallbackReason());
        if(S.Suspended){R->SetArrayField(TEXT("target_hold_m"),EnvironmentVector(Target));R->SetArrayField(TEXT("suspended_hold_m"),EnvironmentVector(S.GetActorTransform().InverseTransformPosition(S.Suspended->GetActorLocation())*.01));R->SetArrayField(TEXT("hook_hold_m"),EnvironmentVector(S.HookPosition()));}
        if(Q.Plot){const auto P=CoilPlan::Build(S.Settings,S.Plan(),S.PreviewPlacement(),S.Analysis(),{},M.PlanCanvas()->GetCachedGeometry().GetLocalSize(),Q.View==2,S.NextRow);R->SetNumberField(TEXT("plan_glyph_count_including_next"),P.Glyphs.Num());R->SetNumberField(TEXT("section_row"),S.NextRow);}
        R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);Test->TestTrue(TEXT("PNG saved"),FFileHelper::SaveArrayToFile(Bytes,*(EnvironmentRoot()/(Q.Name+TEXT(".png")))));EnvironmentJson(Q.Name+TEXT(".json"),R);return !Test->HasAnyErrors();
    }
public:
    FCoilEnvironmentCapture(FAutomationTestBase* T,FString InScene):Test(T),Scene(MoveTemp(InScene)){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Began>360){Test->AddError(TEXT("environment capture timeout"));return true;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()){Changed=Now;return false;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        ACoilLoadingMonitorHost* H=nullptr;for(TActorIterator<ACoilLoadingMonitorHost> I(W);I;++I)H=*I;if(!H||!H->Monitor||!H->Simulation)return false;auto& S=*H->Simulation;auto& M=*H->Monitor;
        if(Step==0)
        {
            if(!Test->TestTrue(TEXT("real D3D12 capture"),GDynamicRHI&&FString(GDynamicRHI->GetName())==TEXT("D3D12")))return true;
            FString Text,Error;CoilLoading::FSettings Settings;FFileHelper::LoadFileToString(Text,*(EnvironmentRoot()/TEXT("fixtures")/(Scene+TEXT(".json"))));
            if(!Test->TestTrue(TEXT("fixture parsed by actual product parser"),CoilLoading::PlanFromJson(Text,Settings,Expected,Error)))return true;
            PlacedAnalysis=CoilLoading::Analyze(Settings,Expected);
            for(const auto& I:PlacedAnalysis.Issues)if(I.Code==TEXT("CoilInterference")||I.Code==TEXT("HoldInterference")||I.Code==TEXT("UnsupportedKey")||I.Code==TEXT("HatchClearance"))Test->AddError(I.Code.ToString()+TEXT(": ")+I.Reason);
            if(Test->HasAnyErrors())return true;
            if(!Test->TestTrue(TEXT("production asynchronous LoadPlan accepted"),S.LoadPlan(EnvironmentRoot()/TEXT("fixtures")/(Scene+TEXT(".json")))))return true;
            Step=1;return false;
        }
        if(Step==1)
        {
            if(S.IsPlanLoadPending())return false;if(!Test->TestEqual(TEXT("loaded exact fixture coil count"),S.Plan().Num(),Expected.Num()))return true;
            // Capture owns this transient PIE host. Avoid its200ms layout timer
            // alternating the cropped native UI projection with the full-frame
            // presentation projection between automation callbacks.
            W->GetTimerManager().ClearAllTimersForObject(H);
            S.bRowsForeToAft=true;const auto& Last=Expected.Last();S.NextSpec=Last.Spec;S.NextSpec.CoilId=TEXT("CAP-NEXT");S.NextTier=Scene==TEXT("half")?3:Scene==TEXT("full")?5:4;S.NextRow=9;S.NextColumn=Scene==TEXT("full")?8:1;
            if(Scene==TEXT("lowering")){S.NextTier=5;S.NextRow=Last.Row;S.NextColumn=Last.Column+1;}S.RefreshPreview();M.SyncInputs();M.Refresh();Target=S.PreviewPlacement().Center;
            FVirtualSensorEditableState State,Applied;FString Error;S.Lidar->ReadEditableState(State);State.LidarProfile=EVirtualLidarDeviceProfile::OusterOS1_64_Rev7;State.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;Test->TestTrue(TEXT("public Ouster default profile applied"),S.Lidar->ApplyProfileAndSimulationQuality(State,Applied,Error));
            if(S.Ghost)S.Ghost->SetActorHiddenInGame(Scene!=TEXT("lowering"));
            if(Scene==TEXT("full"))Shots={{TEXT("cap-02-full-overview"),0,false,false,{-35,40,54},{6,-7,6}},{TEXT("cap-03-full-interior"),1,false,false,{-18,-22,23},{0,0,4}},{TEXT("cap-04-full-top"),3,true,true,{},{}},{TEXT("cap-05-full-section"),2,true,true,{},{}},{TEXT("cap-12-full-lidar-normal"),1,false,false,{-18,-22,23},{0,0,4}},{TEXT("cap-13-full-lidar-points"),4,false,false,{-18,-22,23},{0,0,4}},{TEXT("cap-14-full-hatch-above"),0,false,false,{-10,-6,48},{0,0,4}}};
            else if(Scene==TEXT("lowering"))Shots={{TEXT("cap-11-full-lowering"),1,false,false,Target+FVector(-17,-12,14),Target+FVector(0,0,2)},{TEXT("cap-17-lowering-section"),2,true,true,{},{}}};
            else
            {
                const FString Prefix=Scene==TEXT("half")?TEXT("01-half"):Scene==TEXT("mixed")?TEXT("06-mixed"):Scene==TEXT("varied")?TEXT("07-varied"):TEXT("08-list");
                Shots.Add({TEXT("cap-")+Prefix+TEXT("-interior"),1,false,false,{-18,-22,23},{0,0,3}});
                if(Scene==TEXT("list"))Shots.Add({TEXT("cap-09-list-ui"),1,true,false,{-18,-22,23},{0,0,3}});
                else Shots.Add({Scene==TEXT("half")?TEXT("cap-10-half-top"):Scene==TEXT("mixed")?TEXT("cap-15-mixed-section"):TEXT("cap-16-varied-section"),Scene==TEXT("half")?3:2,true,true,{},{}});
            }
            if(Scene==TEXT("list"))Test->TestTrue(TEXT("actual eccentric load produces list warning"),FMath::Abs(PlacedAnalysis.HeelDegrees)>=2);
            // Keep the product's own badge in the native analysis footer for
            // the enlarged plot captures, as in the prior presentation path.
            auto* Footer=Cast<UBorder>(M.WidgetTree->FindWidget(TEXT("CoilAnalysis")));auto* Rows=Cast<UVerticalBox>(Footer->GetContent());TArray<UWidget*> Widgets;M.WidgetTree->GetAllWidgets(Widgets);for(auto* Widget:Widgets)if(auto* Text=Cast<UTextBlock>(Widget))if(Text->GetText().ToString()==FString::Printf(TEXT("위험 %d · 주의 %d"),M.DangerCount,M.CautionCount)){Text->RemoveFromParent();Rows->AddChildToVerticalBox(Text);break;}
            SetShot(*H);Changed=Now;Step=2;if(Scene==TEXT("lowering")){Test->TestTrue(TEXT("actual production lowering started"),S.PlaceNext(true));Step=3;}return false;
        }
        if(Step==3)
        {
            Layout(*H,Shots[Shot]);if(S.Phase()!=ECoilLoadingPhase::Lowering||!S.Suspended)return false;
            if(S.GetActorTransform().InverseTransformPosition(S.Suspended->GetActorLocation()).Z*.01-Target.Z>2.4)return false;
            S.SetActorTickEnabled(false);SetShot(*H);Changed=Now;Step=2;return false;
        }
        Layout(*H,Shots[Shot]);if(Now-Changed<2)return false;
        if(!Capture(*W,*H))return true;if(++Shot>=Shots.Num()){S.CancelOperation();return true;}
        SetShot(*H);Changed=Now;return false;
    }
};
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCoilEnvironmentCapturesTest,"MA0T10.CoilLoading.Presentation.EnvironmentsRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FCoilEnvironmentCapturesTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{for(const FString S:{TEXT("half"),TEXT("full"),TEXT("mixed"),TEXT("varied"),TEXT("list"),TEXT("lowering")}){Names.Add(S);Commands.Add(S);}}
bool FCoilEnvironmentCapturesTest::RunTest(const FString& Parameters)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilEnvironmentCapture(this,Parameters));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
