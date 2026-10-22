#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
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
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"

namespace
{
FString CoilPresentationRoot()
{
    FString P;FParse::Value(FCommandLine::Get(),TEXT("CoilPresentationOutput="),P);
    return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/PRESENTATION-20261022"):P;
}
TArray<TSharedPtr<FJsonValue>> CoilPresentationVector(FVector P)
{return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)};}
class FCoilThreeTierPresentation final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int Step=0;double Began=FPlatformTime::Seconds(),Changed=0;
    FVector Target;FString FrozenPlan;
    void Layout(ACoilLoadingMonitorHost& H,bool Section)
    {
        auto& M=*H.Monitor;const auto Pixels=FinalAcceptanceViewportSize();
        if(!Section){M.SetRenderOpacity(0);H.SetSceneRect(FSlateRect(0,0,Pixels.X,Pixels.Y),FVector2D(Pixels.X,Pixels.Y),true);return;}
        M.SetRenderOpacity(1);
        const auto Size=UWidgetLayoutLibrary::GetViewportSize(&M)/UWidgetLayoutLibrary::GetViewportScale(&M);
        auto* Root=Cast<UCanvasPanel>(M.WidgetTree->RootWidget);
        auto* Plot=M.WidgetTree->FindWidget(TEXT("CoilPlanCard"));auto* Footer=M.WidgetTree->FindWidget(TEXT("CoilAnalysis"));
        for(int I=0;I<Root->GetChildrenCount();++I)
        {auto* C=Root->GetChildAt(I);C->SetVisibility(C==Plot||C==Footer?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);}
        // Reuse one existing opaque scene mask as a full section backdrop.
        auto* Backdrop=Root->GetChildAt(0);Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* BackdropSlot=Cast<UCanvasPanelSlot>(Backdrop->Slot);BackdropSlot->SetPosition({0,0});BackdropSlot->SetSize(Size);
        const float FooterH=Size.Y*.29;
        auto* PlotSlot=Cast<UCanvasPanelSlot>(Plot->Slot);PlotSlot->SetPosition({12,12});PlotSlot->SetSize({Size.X-24,Size.Y-FooterH-32});
        auto* FooterSlot=Cast<UCanvasPanelSlot>(Footer->Slot);FooterSlot->SetPosition({12,Size.Y-FooterH-12});FooterSlot->SetSize({Size.X-24,FooterH});
        H.SetSceneRect(FSlateRect(0,0,Pixels.X,Pixels.Y),FVector2D(Pixels.X,Pixels.Y),false);
    }
    bool Capture(UWorld& W,ACoilLoadingMonitorHost& H,bool Section)
    {
        auto& S=*H.Simulation;auto& M=*H.Monitor;Layout(H,Section);
        TArray<FColor> Pixels;FIntVector Size;bool Ok;
        if(Section)Ok=FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size);
        else{auto* V=W.GetGameViewport()->Viewport;const auto N=V->GetSizeXY();Size={N.X,N.Y,0};Ok=V->ReadPixels(Pixels);}
        const auto Expected=FinalAcceptanceViewportSize();
        if(!Test->TestTrue(TEXT("native requested-resolution coil presentation pixels"),Ok&&Size.X==Expected.X&&Size.Y==Expected.Y))return false;
        int Bright=0,Dark=0,Green=0,Count=0;
        for(int I=0;I<Pixels.Num();I+=16){const auto C=Pixels[I];++Count;Bright+=C.R+C.G+C.B>100;Dark+=C.R+C.G+C.B<18;Green+=C.G>80&&C.G>C.R*1.25&&C.G>C.B*1.15;}
        Test->TestTrue(TEXT("presentation contains populated scene or section"),Bright>Count*.12);
        if(!Section)
        {
            auto* PC=W.GetFirstPlayerController();
            Test->TestFalse(TEXT("actual suspended body visible in player view"),PC->HiddenPrimitiveComponents.Contains(S.Suspended->Body));
            for(const auto& P:S.Plan())for(double Y:{-1.,1.})for(double Z:{-1.,1.})
            {FVector2D Pixel;const auto Local=P.Center+FVector(0,Y*P.Spec.OuterDiameter()*.5,Z*P.Spec.OuterDiameter()*.5);Test->TestTrue(TEXT("all stacked coils fit inside presentation frame"),PC->ProjectWorldLocationToScreen(S.GetActorTransform().TransformPosition(Local*100),Pixel)&&Pixel.X>12&&Pixel.X<Size.X-12&&Pixel.Y>12&&Pixel.Y<Size.Y-12);}
            FVector2D HookPixel;Test->TestTrue(TEXT("hook block fits inside frame with room for visible wire"),PC->ProjectWorldLocationToScreen(S.GetActorTransform().TransformPosition((S.HookPosition()+FVector(0,0,.7))*100),HookPixel)&&HookPixel.Y>Size.Y*.08&&HookPixel.Y<Size.Y*.8);
            Test->TestTrue(TEXT("hold shot is not predominantly near-black"),Dark<Count*.3);Test->TestTrue(TEXT("actual green target ghost visible"),Green>40);}
        TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);
        const FString Name=FString::Printf(TEXT("pres-%s-%dx%d"),Section?TEXT("05-coil-3tier-section"):TEXT("04-coil-3tier-adding"),Size.X,Size.Y);
        if(!Test->TestTrue(TEXT("coil presentation PNG saved"),FFileHelper::SaveArrayToFile(Bytes,*(CoilPresentationRoot()/(Name+TEXT(".png"))))))return false;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);
        R->SetStringField(TEXT("kind"),Section?TEXT("actual Slate section + actual analysis footer; sidebar/header excluded"):TEXT("actual native D3D12 player viewport; no UI panels"));
        R->SetStringField(TEXT("rhi"),GDynamicRHI->GetName());R->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
        R->SetNumberField(TEXT("placed_coils"),S.Plan().Num());R->SetNumberField(TEXT("pending_tier"),S.NextTier);R->SetNumberField(TEXT("pending_column"),S.NextColumn);
        R->SetNumberField(TEXT("danger_count"),M.DangerCount);R->SetNumberField(TEXT("caution_count"),M.CautionCount);R->SetNumberField(TEXT("data_needed_count"),M.DataNeededCount);
        R->SetNumberField(TEXT("near_black_fraction"),double(Dark)/Count);R->SetNumberField(TEXT("green_sampled_pixels"),Green);
        R->SetArrayField(TEXT("target_hold_m"),CoilPresentationVector(Target));R->SetArrayField(TEXT("suspended_hold_m"),CoilPresentationVector(S.GetActorTransform().InverseTransformPosition(S.Suspended->GetActorLocation())*.01));
        R->SetArrayField(TEXT("hook_hold_m"),CoilPresentationVector(S.HookPosition()));R->SetStringField(TEXT("phase"),TEXT("Lowering; actual production animation frozen for paired captures"));
        R->SetArrayField(TEXT("camera_hold_m"),CoilPresentationVector(S.GetActorTransform().InverseTransformPosition(H.ViewCamera->GetActorLocation())*.01));
        R->SetNumberField(TEXT("exposure_bias"),H.ViewCamera->GetCameraComponent()->PostProcessSettings.AutoExposureBias);
        R->SetBoolField(TEXT("same_plan_and_pending"),FrozenPlan==CoilLoading::PlanToJson(S.Settings,S.Plan()));R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(CoilPresentationRoot()/(Name+TEXT(".json"))));return true;
    }
public:
    explicit FCoilThreeTierPresentation(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Began>180){Test->AddError(TEXT("coil three-tier presentation timeout"));return true;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        ACoilLoadingMonitorHost* H=nullptr;for(TActorIterator<ACoilLoadingMonitorHost> I(W);I;++I)H=*I;
        if(!H||!H->Monitor||!H->Simulation)return false;auto& S=*H->Simulation;auto& M=*H->Monitor;
        if(Step==0)
        {
            if(!Test->TestTrue(TEXT("presentation requires real offscreen D3D12"),GDynamicRHI&&FString(GDynamicRHI->GetName())==TEXT("D3D12")&&FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen"))))return true;
            IFileManager::Get().MakeDirectory(*CoilPresentationRoot(),true);
            // Matched 1.600m gross OD: HR steel1.600 / CR steel1.597 +3mm package.
            // 762mm bore permits the same C-hook; no unequal-OD key interference.
            // Unknown strength/manual/hydrostatic approvals stay unknown, never fabricated.
            S.Settings.bCHook=true;
            for(int Tier=1;Tier<=3;++Tier)for(int Col=1;Col<=8-Tier;++Col)
            {
                if(Tier==3&&Col==3)continue;
                S.NextTier=Tier;S.NextRow=5;S.NextColumn=Col;
                S.NextSpec=CoilLoading::FSpec::Representative((Tier==1&&(Col==2||Col==5))||(Tier==2&&Col==4)?CoilLoading::ECoilType::ColdRolled:CoilLoading::ECoilType::HotRolled);
                if(S.NextSpec.Type==CoilLoading::ECoilType::ColdRolled)S.NextSpec.OD=1.597;
                S.NextSpec.ID=.762;S.NextSpec.CoilId=FString::Printf(TEXT("PRES-T%d-C%d"),Tier,Col);
                if(!Test->TestTrue(TEXT("three-tier fixture uses production placement"),S.PlaceNext(false)))return true;
            }
            Test->TestEqual(TEXT("7 base +6 middle +4 top coils"),S.Plan().Num(),17);
            S.NextTier=3;S.NextRow=5;S.NextColumn=3;S.NextSpec=CoilLoading::FSpec::Representative(CoilLoading::ECoilType::HotRolled);S.NextSpec.CoilId=TEXT("PRES-ADDING-18");
            S.RefreshPreview();M.SyncInputs();M.Refresh();Target=S.PreviewPlacement().Center;
            FString PlanJson=CoilLoading::PlanToJson(S.Settings,S.Plan());FFileHelper::SaveStringToFile(PlanJson,*(CoilPresentationRoot()/TEXT("coil-3tier-plan.json")));
            for(const auto& Issue:S.Analysis().Issues)if(Issue.bKnown&&Issue.Severity==CoilLoading::ESeverity::Warning)UE_LOG(LogTemp,Warning,TEXT("[CoilPresentationDanger] %s coil=%d %s"),*Issue.Code.ToString(),Issue.Coil,*Issue.Reason);
            if(!Test->TestEqual(TEXT("no actual danger issues including next placement"),M.DangerCount,0))return true;
            Test->TestTrue(TEXT("next coil has both real lower supports"),S.PreviewPlacement().SupportA>=0&&S.PreviewPlacement().SupportB>=0);
            // Existing player-only interior cutaway, then retain actual wire/hook geometry.
            H->Focus(1);const FVector Position=FVector(Target.X-17,Target.Y-8,12),Aim=FVector(Target.X,Target.Y,4.6);
            H->ViewCamera->SetActorLocationAndRotation(S.GetActorTransform().TransformPosition(Position*100),S.GetActorQuat()*(Aim-Position).Rotation().Quaternion());H->ViewCamera->GetCameraComponent()->SetFieldOfView(60);
            if(auto* PC=W->GetFirstPlayerController()){TInlineComponentArray<UPrimitiveComponent*> Parts(&S);for(auto* C:Parts)if(C->ComponentHasTag(TEXT("CoilHoistFalls"))||C->ComponentHasTag(TEXT("CoilHookBlock"))||C->ComponentHasTag(TEXT("CoilCHook")))PC->HiddenPrimitiveComponents.Remove(C);}
            Layout(*H,false);if(!Test->TestTrue(TEXT("start actual hook lift/traverse/lower"),S.PlaceNext(true)))return true;
            Step=1;Changed=Now;return false;
        }
        if(Step==1)
        {
            Layout(*H,false);
            if(S.Phase()!=ECoilLoadingPhase::Lowering||!S.Suspended)return false;
            const auto P=S.GetActorTransform().InverseTransformPosition(S.Suspended->GetActorLocation())*.01;
            if(P.Z-Target.Z>2.4)return false;
            // The pickup body was outside the hold when the interior preset hid it.
            // Reveal this same body only in the player view after the actual traverse.
            if(auto* PC=W->GetFirstPlayerController()){TInlineComponentArray<UPrimitiveComponent*> Parts(S.Suspended.Get());for(auto* C:Parts)PC->HiddenPrimitiveComponents.Remove(C);}
            S.SetActorTickEnabled(false);FrozenPlan=CoilLoading::PlanToJson(S.Settings,S.Plan());Changed=Now;Step=2;return false;
        }
        Layout(*H,Step>=3);if(Now-Changed<1)return false;
        if(Step==2)
        {
            if(!Capture(*W,*H,false))return true;
            // Existing badge moved into the existing analysis footer only in transient PIE.
            auto* Footer=Cast<UBorder>(M.WidgetTree->FindWidget(TEXT("CoilAnalysis")));auto* Rows=Cast<UVerticalBox>(Footer->GetContent());
            TArray<UWidget*> Widgets;M.WidgetTree->GetAllWidgets(Widgets);for(auto* Widget:Widgets)if(auto* Text=Cast<UTextBlock>(Widget))if(Text->GetText().ToString()==FString::Printf(TEXT("위험 %d · 주의 %d"),M.DangerCount,M.CautionCount)){Text->RemoveFromParent();Rows->AddChildToVerticalBox(Text);break;}
            M.SelectView(2);Layout(*H,true);Step=3;Changed=Now;return false;
        }
        Test->TestTrue(TEXT("section retains same frozen production plan"),FrozenPlan==CoilLoading::PlanToJson(S.Settings,S.Plan()));
        Capture(*W,*H,true);S.SetActorTickEnabled(true);S.CancelOperation();return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilThreeTierPresentationTest,"MA0T10.CoilLoading.Presentation.ThreeTierAddingCapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilThreeTierPresentationTest::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilThreeTierPresentation(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
