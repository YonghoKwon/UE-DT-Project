#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "UxrScenarioUiFixture.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Widgets/Layout/SScrollBox.h"

class FUxrScenarioRender : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Created=FPlatformTime::Seconds(),ReadyAt=0;int32 Phase=0;bool Submitted=false,Requested=false;
    TWeakObjectPtr<ASlabActor> Slab;FString Path;FDateTime OldTime;TUniquePtr<TGuardValue<bool>> DebugGuard;
public:
    explicit FUxrScenarioRender(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const bool SurfaceMode=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR08_RENDER"))==TEXT("1");
        if(FPlatformTime::Seconds()-Created>100){Test->AddError(TEXT("Scenario render timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        auto* Progress=Cast<USlabProgressPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::SlabProgress));
        if(!Progress)return false;
        if(!Submitted)
        {
            for(TActorIterator<ASlabActor> It(World);It;++It){Slab=*It;break;}
            if(!StartUxrScenarioUiFixture(Slab.Get())){Test->AddError(TEXT("scenario capture fixture admission rejected"));return true;}
            DebugGuard=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);Submitted=true;return false;
        }
        if(!Slab.IsValid()||Slab->GetSimulationStatus().RowCount==0)return false;
        if(!ReadyAt)
        {
            const int32 View=SurfaceMode?Phase%6:Phase%2;const bool Large=SurfaceMode?Phase>=6:Phase>=2;
            W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(Large?1.5f:1.f);W->EnsureOwnedReplayPanel();
            for(const auto Role:{ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Placement,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::SlabCharts})W->SetPanelOpen(Role,false);
            W->SetPanelOpen(ESensorToolPanelRole::SlabProgress,View<2);W->SetPanelOpen(ESensorToolPanelRole::Replay,View<2);
            if(Progress->OwnedDisplayArea.IsValid())Progress->OwnedDisplayArea->SetExpanded(View==1);
            if(SurfaceMode)
            {
                const ESlabSurfacePreset Presets[]={ESlabSurfacePreset::Default,ESlabSurfacePreset::HotRed,ESlabSurfacePreset::CooledSteel,ESlabSurfacePreset::AnalysisMatte};FString Reason;
                Test->TestTrue(TEXT("render uses only supported existing-material presets"),Slab->ApplySurfacePreset(View<2?ESlabSurfacePreset::Default:Presets[View-2],false,Reason));
                Slab->SetDiagnosticHelpersVisible(false);
                if(View==0){Slab->Click();Test->TestTrue(TEXT("DTCore Actor Click opens the existing information panel"),W->IsPanelOpen(ESensorToolPanelRole::SlabProgress)&&Progress->GetBoundSlabActor()==Slab.Get());}
            }
            if(View==1&&Progress->OwnedScenarioScroll&&Progress->OwnedDisplayArea)Progress->OwnedScenarioScroll->ScrollDescendantIntoView(Progress->OwnedDisplayArea.ToSharedRef(),false,EDescendantScrollDestination::TopOrLeft);
            Slab->SetSimulationPaused(true);ReadyAt=FPlatformTime::Seconds()+1.5;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();
        if(!Requested)
        {
            const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
            const TCHAR* SurfaceNames[]={TEXT("information"),TEXT("display"),TEXT("default"),TEXT("hot-red"),TEXT("cooled-steel"),TEXT("analysis-matte")};
            const int32 Font=SurfaceMode?(Phase<6?100:150):(Phase<2?100:150);
            Path=Root/FString::Printf(TEXT("rendered-%dx%d-%d-%s.png"),Size.X,Size.Y,Font,SurfaceMode?SurfaceNames[Phase%6]:Phase%2?TEXT("display"):TEXT("scenario"));
            OldTime=IFileManager::Get().GetTimeStamp(*Path);FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());
            Test->AddInfo(FString::Printf(TEXT("UXR04 automated capture actual client=%dx%d font=%d windowDpi=%.3f; mouse not tested"),Size.X,Size.Y,Phase<2?100:150,Window?Window->GetDPIScaleFactor():0));return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=OldTime)return false;
        if(++Phase==(SurfaceMode?12:4))return true;ReadyAt=0;Requested=false;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrScenarioRenderTest,"UXR04.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrScenarioRenderTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR04_RENDER"))!=TEXT("1")){AddInfo(TEXT("SKIP explicit UXR04 capture only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrScenarioRender(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrSurfaceRenderTest,"UXR08.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrSurfaceRenderTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR08_RENDER"))!=TEXT("1")){AddInfo(TEXT("SKIP explicit UXR08 capture only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrScenarioRender(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
