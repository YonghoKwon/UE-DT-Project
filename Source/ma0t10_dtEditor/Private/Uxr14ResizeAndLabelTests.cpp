#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
// UXR-14: the docked preview card resizes from every edge/corner through the real
// cursor path (OS cursor + Slate hit test), keeps the opposite edge fixed, respects
// limits, and resets from ⋯. Larger slab text settings reach the 3D labels.
class FUxr14Check : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),Next=0;
    int32 Phase=0,Case=0;
    FVector2D StartPos,StartSize,DefaultSize;
    float Small=0;FString Path;FDateTime Previous;
    const FVector2D OriginalCursor=FSlateApplication::Get().GetCursorPos();
public:
    explicit FUxr14Check(FAutomationTestBase* T):Test(T){}
    ~FUxr14Check(){if(FSlateApplication::IsInitialized()){auto& App=FSlateApplication::Get();App.ReleaseAllPointerCapture();App.GetPlatformApplication()->Cursor->SetPosition(OriginalCursor.X,OriginalCursor.Y);}}
    static float MaxLabel(UWorld* World)
    {float M=0;for(TActorIterator<ASlabActor> It(World);It;++It){TArray<UTextRenderComponent*> T;It->GetComponents(T);for(auto* C:T)if(C->IsVisible()&&!C->Text.IsEmpty())M=FMath::Max(M,C->WorldSize);}return M;}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>150){Test->AddError(TEXT("UXR14 timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World)||FPlatformTime::Seconds()<Next)return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();if(!W||!W->GetCoordinator())return false;
        auto& App=FSlateApplication::Get();auto* Panel=W->GetOwnedPanel(ESensorToolPanelRole::Monitor);
        if(Phase==0)
        {
            if(FPlatformTime::Seconds()-Started<3)return false;
            W->SetOwnedPanelFontScale(1);W->SetSidebarMode(true);W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);W->ResetOwnedPanelLayout(ESensorToolPanelRole::Monitor);
            Phase=1;Next=FPlatformTime::Seconds()+1;return false;
        }
        if(!Panel||!Panel->GetCachedWidget().IsValid()){Test->AddError(TEXT("UXR14 preview card missing"));return true;}
        const auto& G=Panel->GetCachedGeometry();
        // Cases: left edge, top edge, top-left corner, bottom-right corner, right edge pushed past the limit.
        const FVector2D Local[]={{3,.5f},{.5f,3},{5,5},{-5,-5},{-3,.5f},{3,.5f}};
        const FVector2D Moves[]={{-120,0},{0,-90},{-60,-50},{40,30},{4000,0},{4000,0}};
        if(Phase==1)
        {
            if(Case==0)DefaultSize=Panel->GetPanelExpandedSize();
            const FVector2D S=G.GetLocalSize(),L=Local[Case];
            const FVector2D P(L.X>=1?L.X:(L.X<0?S.X+L.X:S.X*L.X),L.Y>=1?L.Y:(L.Y<0?S.Y+L.Y:S.Y*L.Y));
            const FVector2D Press=G.LocalToAbsolute(P);
            StartPos=Panel->GetCurrentPanelPosition();StartSize=Panel->GetPanelExpandedSize();
            App.OnApplicationActivationChanged(true);App.GetPlatformApplication()->Cursor->SetPosition(Press.X,Press.Y);App.OnMouseMove();
            App.OnMouseDown(App.FindWidgetWindow(Panel->GetCachedWidget().ToSharedRef())->GetNativeWindow(),EMouseButtons::Left);
            Test->TestTrue(FString::Printf(TEXT("case %d press on the border captures the card"),Case),Panel->GetCachedWidget()->HasMouseCapture());
            App.GetPlatformApplication()->Cursor->SetPosition(Press.X+Moves[Case].X,Press.Y+Moves[Case].Y);App.OnMouseMove();
            App.OnMouseUp(EMouseButtons::Left);Phase=2;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Phase==2)
        {
            const FVector2D Pos=Panel->GetCurrentPanelPosition(),Size=Panel->GetPanelExpandedSize();
            Test->AddInfo(FString::Printf(TEXT("UXR14 case=%d pos %s->%s size %s->%s"),Case,*StartPos.ToString(),*Pos.ToString(),*StartSize.ToString(),*Size.ToString()));
            const FVector2D StartFar=StartPos+StartSize,Far=Pos+Size;
            if(Case==0){Test->TestTrue(TEXT("left edge widens"),Size.X>StartSize.X+80);Test->TestTrue(TEXT("left edge keeps right edge"),FMath::Abs(Far.X-StartFar.X)<2&&FMath::Abs(Size.Y-StartSize.Y)<1);}
            if(Case==1){Test->TestTrue(TEXT("top edge grows taller"),Size.Y>StartSize.Y+60);Test->TestTrue(TEXT("top edge keeps bottom edge"),FMath::Abs(Far.Y-StartFar.Y)<2&&FMath::Abs(Size.X-StartSize.X)<1);}
            if(Case==2){Test->TestTrue(TEXT("top-left corner grows both"),Size.X>StartSize.X+30&&Size.Y>StartSize.Y+30);Test->TestTrue(TEXT("corner keeps bottom-right"),Far.Equals(StartFar,2));}
            const FBox2D B=W->GetDockedCardBounds();
            if(Case==3||Case==4){Test->TestTrue(TEXT("dragging out at the screen edge never slides the card"),Pos.Equals(StartPos,1));Test->TestTrue(TEXT("dragged edge stops inside the 3D view"),Far.X<=B.Max.X+1&&Far.Y<=B.Max.Y+1);}
            if(Case==5){Test->TestTrue(TEXT("pushing the left edge far right stops at minimum size"),FMath::Abs(Size.X-W->GetDockedCardMinimum().X)<1&&FMath::Abs(Far.X-StartFar.X)<2);}
            Test->TestFalse(TEXT("release clears capture"),App.HasAnyMouseCaptor());
            Test->TestTrue(TEXT("card never covers the sidebar"),Pos.X>=W->GetSidebarWidth()-1);
            if(++Case<UE_ARRAY_COUNT(Moves)){Phase=1;return false;}
            W->ResetOwnedPanelLayout(ESensorToolPanelRole::Monitor);Phase=3;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Phase==3)
        {
            Test->TestTrue(TEXT("⋯ reset returns the default card size"),Panel->GetPanelExpandedSize().Equals(DefaultSize,1));
            for(TActorIterator<ASlabActor> It(World);It;++It){auto S=It->GetAnalysisDisplaySettings();S.TextHeightPixels=200;It->SetAnalysisDisplaySettings(S);Test->TestEqual(TEXT("text size limit is now 120"),It->GetAnalysisDisplaySettings().TextHeightPixels,120.f);S.TextHeightPixels=56;It->SetAnalysisDisplaySettings(S);}
            Phase=4;Next=FPlatformTime::Seconds()+.6;return false;
        }
        if(Phase==4)
        {
            Small=MaxLabel(World);
            for(TActorIterator<ASlabActor> It(World);It;++It){auto S=It->GetAnalysisDisplaySettings();S.TextHeightPixels=112;It->SetAnalysisDisplaySettings(S);}
            Phase=5;Next=FPlatformTime::Seconds()+.6;return false;
        }
        if(Phase==5)
        {
            const float Large=MaxLabel(World);
            Test->AddInfo(FString::Printf(TEXT("UXR14 label world size 56->%.2f 112->%.2f"),Small,Large));
            // Visible-label size is checked in MA0T10.UI02.LabelPresentationRhi (near-text-112).
            const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
            Path=(Root.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-14"):Root)/TEXT("rendered-1920x1080-large-slab-text.png");
            Previous=IFileManager::Get().GetTimeStamp(*Path);FScreenshotRequest::RequestScreenshot(Path,true,false);Phase=6;return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        for(TActorIterator<ASlabActor> It(World);It;++It){auto S=It->GetAnalysisDisplaySettings();S.TextHeightPixels=28;It->SetAnalysisDisplaySettings(S);}
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr14Test,"MA0T10.UXR14.CardEdgeResizeAndSlabTextRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr14Test::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP UXR14 native cursor requires real RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr14Check(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
