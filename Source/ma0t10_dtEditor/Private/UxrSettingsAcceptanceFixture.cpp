#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

class FUxrSettingsRender : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Created=FPlatformTime::Seconds(),ReadyAt=0;int32 Phase=0;bool Requested=false;FString Path;FDateTime OldTime;TUniquePtr<TGuardValue<bool>> DebugGuard;
public:
    explicit FUxrSettingsRender(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Created>100){Test->AddError(TEXT("Settings render timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));if(!C||!Settings)return false;
        const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();const int32 View=Phase%3;const bool Lidar=(Phase/3)%2==1,Large=Phase>=6;
        if(!ReadyAt)
        {
            if(!DebugGuard)DebugGuard=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(Large?1.5f:1.f);
            for(const auto Role:{ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Placement,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabCharts,ESensorToolPanelRole::SlabProgress})W->SetPanelOpen(Role,false);
            W->SetPanelOpen(ESensorToolPanelRole::Settings,true);Settings->SelectTargetKind(Lidar?EVirtualSensorTargetKind::Lidar:EVirtualSensorTargetKind::Camera);Settings->WorkspaceSettingsTab=View;Settings->bOwnedSettingHelpOpen=false;
            if(Settings->OwnedSettingsScroll)Settings->OwnedSettingsScroll->ScrollToStart();ReadyAt=FPlatformTime::Seconds()+1.5;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(!Requested)
        {
            const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));const TCHAR* Names[]={TEXT("basic"),TEXT("advanced"),TEXT("compatibility")};
            Path=Root/FString::Printf(TEXT("rendered-%dx%d-%d-%s-%s.png"),Size.X,Size.Y,Large?150:100,Lidar?TEXT("lidar"):TEXT("camera"),Names[View]);OldTime=IFileManager::Get().GetTimeStamp(*Path);FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Test->AddInfo(FString::Printf(TEXT("UXR05 automated capture actual client=%dx%d font=%d windowDpi=%.3f; mouse not tested"),Size.X,Size.Y,Large?150:100,Window?Window->GetDPIScaleFactor():0));return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=OldTime)return false;
        if(++Phase==12)return true;ReadyAt=0;Requested=false;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrSettingsRenderTest,"UXR05.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrSettingsRenderTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR05_RENDER"))!=TEXT("1")){AddInfo(TEXT("[OPT-IN SKIP] MA0T10_UXR05_RENDER unset or !=1; SKIP explicit UXR05 capture only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrSettingsRender(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
