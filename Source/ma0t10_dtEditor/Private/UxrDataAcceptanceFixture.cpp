#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

/** Explicit opt-in automated render. Never synthesizes a manual acceptance result. */
class FUxrDataRender : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Created=FPlatformTime::Seconds(),ReadyAt=0;
    int32 Phase=0;
    bool Requested=false;
    FString Path;
    FDateTime OldTime;
    TUniquePtr<TGuardValue<bool>> DebugGuard;
public:
    explicit FUxrDataRender(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Created>75){Test->AddError(TEXT("UXR03 capture timed out: viewport size or screenshot unavailable"));return true;}
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        auto* Data=Cast<UVirtualSensorCaptureExportPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Data));
        if(!Data)return false;
        const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();const float Scale=Phase<5?1.f:1.5f;
        if(!ReadyAt)
        {
            if(!DebugGuard)DebugGuard=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            Workspace->ResetOwnedWorkspaceLayout();Workspace->SetOwnedPanelFontScale(Scale);
            for(const auto Role:{ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Placement,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabCharts,ESensorToolPanelRole::SlabProgress})Workspace->SetPanelOpen(Role,false);
            Workspace->SetPanelOpen(ESensorToolPanelRole::Data,true);Data->OpenOwnedTab(static_cast<ESensorOwnedDataTab>(Phase%5));
            if(Phase%5==0)Test->TestNotNull(TEXT("first transfer entry prepares replay options without showing replay"),Workspace->GetOwnedPanel(ESensorToolPanelRole::Replay));
            ReadyAt=FPlatformTime::Seconds()+1.5;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(!Requested)
        {
            const TCHAR* Names[]={TEXT("transfer"),TEXT("results"),TEXT("files"),TEXT("server"),TEXT("diagnostics")};
            const FString ReportRoot=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
            Path=(ReportRoot.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-03"):ReportRoot)/FString::Printf(TEXT("rendered-%dx%d-%d-%s.png"),Size.X,Size.Y,FMath::RoundToInt(Scale*100),Names[Phase%5]);
            OldTime=IFileManager::Get().GetTimeStamp(*Path);
            FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());
            Test->AddInfo(FString::Printf(TEXT("UXR03 automated capture actual client=%dx%d font=%d windowDpi=%.3f tab=%s; mouse not tested"),Size.X,Size.Y,FMath::RoundToInt(Scale*100),Window?Window->GetDPIScaleFactor():0,Names[Phase%5]));
            return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=OldTime)return false;
        ++Phase;if(Phase==10)return true;ReadyAt=0;Requested=false;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrDataRenderTest,"UXR03.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrDataRenderTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR03_RENDER"))!=TEXT("1")){AddInfo(TEXT("SKIP: explicit UXR03 capture only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrDataRender(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
