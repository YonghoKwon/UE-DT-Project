#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMetricsComponent.h"
#include "UObject/UnrealType.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Json.h"

// This same instrument runs against the before/after Runtime DLLs in this checkout.
// New toggle access is reflected so the before DLL needs no added native export.
class FUi02LabelRenderCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ASlabActor> Slab;
    TWeakObjectPtr<ACameraActor> Camera;
    double Started=FPlatformTime::Seconds(),ReadyAt=0;
    int32 Phase=0;
    bool Requested=false;
    FString Path;
    FDateTime Previous;
    TUniquePtr<TGuardValue<bool>> Debug;
public:
    explicit FUi02LabelRenderCommand(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("UI-02 render timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        auto* PC=World->GetFirstPlayerController();if(!W||!PC)return false;
        if(!Slab.IsValid())
        {
            for(TActorIterator<ASlabActor> It(World);It;++It){Slab=*It;break;}
            if(!Slab.IsValid()){Test->AddError(TEXT("UI02 fixture Slab missing"));return true;}
            Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);
            W->ResetOwnedWorkspaceLayout();W->SetSidebarMode(true);W->SetOwnedPanelFontScale(1);
            for(const auto Role:{ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabCharts,ESensorToolPanelRole::SlabProgress})W->SetPanelOpen(Role,false);
            W->SetPanelOpen(ESensorToolPanelRole::Placement,true);W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);
            Slab->SetSimulationPaused(true);Slab->SetActorRotation(FRotator(0,-7,0));
            // Test-owned reflected fixture data, never a production input or save.
            auto& Status=*FindFProperty<FStructProperty>(ASlabActor::StaticClass(),TEXT("Status"))->ContainerPtrToValuePtr<FSlabSimulationStatus>(Slab.Get());
            Status.MtlNo=TEXT("SQ83521 047");Status.FrameNo=120;Status.RowCount=601;Status.State=ESlabSimulationState::Paused;
            Status.ElapsedSec=18;Status.DurationSec=90;Status.Progress=.2;Status.ScenarioUUID=TEXT("UI02-memory");
            FSlabMetrics M;M.LeftAngle=-7;M.CenterOffsetCm=-2;M.MarginLeftCm=-3;M.MarginRightCm=20;M.bMarginsValid=true;
            Slab->FindComponentByClass<USlabMetricsComponent>()->Update(M);
            Slab->FindComponentByClass<USlabVisualizationComponent>()->UpdateAnalysis(FSlabScenarioRow(),M,Status,Slab->SlabMesh->Bounds.BoxExtent*2,FTransform(FQuat::Identity,Slab->GetActorLocation()),-70,70,true);
            Camera=World->SpawnActor<ACameraActor>();PC->SetViewTarget(Camera.Get());
        }
        if(!ReadyAt)
        {
            const bool Far=Phase==1||Phase==3;const bool Ortho=Phase==4;
            const FVector Center=Slab->GetActorLocation();const FVector Offset=FVector(-1000,-1100,950)*(Far?2.8:1.);
            Camera->SetActorLocation(Center+Offset);auto Rotation=(-Offset).Rotation();if(Ortho)Rotation.Roll=20;
            Camera->SetActorRotation(Rotation);auto* C=Camera->GetCameraComponent();C->ProjectionMode=Ortho?ECameraProjectionMode::Orthographic:ECameraProjectionMode::Perspective;
            C->FieldOfView=75;C->OrthoWidth=2400;C->bConstrainAspectRatio=false;
            Slab->SetHotAppearance(Phase==5);Slab->SetDiagnosticHelpersVisible(true);
            if(auto* Fn=Slab->FindFunction(TEXT("SetDetailedDiagnosticLabels"))){bool Detailed=Phase==2||Phase==3;Slab->ProcessEvent(Fn,&Detailed);}
            ReadyAt=FPlatformTime::Seconds()+1.2;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(!Requested)
        {
            const auto V=World->GetGameViewport()->Viewport->GetSizeXY();FString Root;FParse::Value(FCommandLine::Get(),TEXT("Ui02Output="),Root);
            if(Root.IsEmpty())Root=FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UI-02");
            IFileManager::Get().MakeDirectory(*Root,true);
            const TCHAR* Names[]={TEXT("near-basic"),TEXT("far-basic"),TEXT("near-detail"),TEXT("far-detail"),TEXT("orthographic-roll"),TEXT("hot-slab")};
            Path=Root/FString::Printf(TEXT("rendered-%dx%d-%s.png"),V.X,V.Y,Names[Phase]);Previous=IFileManager::Get().GetTimeStamp(*Path);
            WriteLayout(World,PC,V,Path+TEXT(".json"));FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        Requested=false;ReadyAt=0;return ++Phase==6;
    }
    void WriteLayout(UWorld* World,APlayerController* PC,FIntPoint V,const FString& Target)
    {
        auto Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
        TArray<UTextRenderComponent*> Texts;Slab->GetComponents(Texts);
        const auto Exclusions=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->GetOwnedDiagnosticExclusionRects();
        TArray<FBox2D> Accepted;
        for(auto* T:Texts)if(T->IsVisible())
        {
            auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),T->GetName());R->SetStringField(TEXT("text"),T->Text.ToString());
            if(T->GetName()==TEXT("SlabDiagnosticText1"))Test->TestTrue(TEXT("intrusion suffix preserves Korean meaning"),T->Text.ToString().EndsWith(TEXT("[침범]")));
            FVector2D P;PC->ProjectWorldLocationToScreen(T->GetComponentLocation(),P);
            R->SetNumberField(TEXT("x"),P.X);R->SetNumberField(TEXT("y"),P.Y);R->SetNumberField(TEXT("world_size"),T->WorldSize);
            TArray<UStaticMeshComponent*> Backgrounds;Slab->GetComponents(Backgrounds);
            for(auto* B:Backgrounds)if(B->GetName()==T->GetName().Replace(TEXT("SlabDiagnosticText"),TEXT("SlabLabelBackground")))
            {
                FBox2D Rect(ForceInit);
                for(int32 X=-1;X<=1;X+=2)for(int32 Y=-1;Y<=1;Y+=2)for(int32 Z=-1;Z<=1;Z+=2)
                {
                    FVector2D Screen;PC->ProjectWorldLocationToScreen(B->GetComponentTransform().TransformPosition(FVector(X*50,Y*50,Z*50)),Screen);Rect+=Screen;
                }
                R->SetNumberField(TEXT("min_x"),Rect.Min.X);R->SetNumberField(TEXT("min_y"),Rect.Min.Y);R->SetNumberField(TEXT("max_x"),Rect.Max.X);R->SetNumberField(TEXT("max_y"),Rect.Max.Y);
                Test->TestTrue(TEXT("actual background within viewport"),Rect.Min.X>=0&&Rect.Min.Y>=0&&Rect.Max.X<=V.X&&Rect.Max.Y<=V.Y);
                for(const auto& Block:Exclusions)Test->TestFalse(TEXT("actual projected label avoids arranged owned UI"),Rect.Intersect(Block));
                for(const auto& Prior:Accepted)Test->TestFalse(TEXT("actual projected labels do not overlap"),Rect.Intersect(Prior));Accepted.Add(Rect);
            }
            Rows.Add(MakeShared<FJsonValueObject>(R));
        }
        Report->SetArrayField(TEXT("visible_labels"),Rows);Report->SetNumberField(TEXT("viewport_width"),V.X);Report->SetNumberField(TEXT("viewport_height"),V.Y);
        Test->TestTrue(TEXT("basic view exposes one or two core labels"),Phase==2||Phase==3||(Rows.Num()>0&&Rows.Num()<=2));
        if(Phase==2||Phase==3)Test->TestTrue(TEXT("explicit detail selection exposes extra labels"),Rows.Num()>2);
        TArray<TSharedPtr<FJsonValue>> Blocks;
        for(const auto& B:Exclusions){auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("min_x"),B.Min.X);Row->SetNumberField(TEXT("min_y"),B.Min.Y);Row->SetNumberField(TEXT("max_x"),B.Max.X);Row->SetNumberField(TEXT("max_y"),B.Max.Y);Blocks.Add(MakeShared<FJsonValueObject>(Row));}
        Report->SetArrayField(TEXT("exclusions"),Blocks);
        Report->SetStringField(TEXT("scope"),TEXT("automated actual PIE client, font100, no mouse acceptance; fixed synthetic diagnostics; no map saved"));
        FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*Target,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUi02LabelRenderTest,"MA0T10.UI02.LabelPresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUi02LabelRenderTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("Ui02Render"))){AddInfo(TEXT("SKIP explicit UI-02 capture only (-Ui02Render)"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUi02LabelRenderCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
