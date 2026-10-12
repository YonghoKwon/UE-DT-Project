#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

class FUxrPreviewRender : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Created=FPlatformTime::Seconds(),ReadyAt=0;int32 Phase=0;bool Requested=false;FString Path;FDateTime OldTime;TUniquePtr<TGuardValue<bool>> DebugGuard;
    TArray<TWeakObjectPtr<AVirtualSensorActorBase>> DetachedSensors;
public:
    explicit FUxrPreviewRender(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Created>100){Test->AddError(TEXT("Preview render timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();auto* Monitor=Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));if(!C||!Monitor)return false;
        const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();const int32 View=Phase%5;const bool Large=Phase>=5,Lidar=View==2||View==3;
        if(!ReadyAt)
        {
            if(!DebugGuard)DebugGuard=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(Large?1.5f:1.f);
            for(const auto Role:{ESensorToolPanelRole::Placement,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabCharts,ESensorToolPanelRole::SlabProgress})W->SetPanelOpen(Role,false);
            W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);
            if(View==4)
            {
                for(auto* A:C->GetSensorActors()){A->StopSensor();DetachedSensors.AddUnique(A);C->UnregisterSensorActor(A);}
                Monitor->BindVirtualCamera(nullptr);Monitor->BindVirtualLidar(nullptr);
            }
            else
            {
                if(View==0&&Large){for(const auto& A:DetachedSensors)if(A.IsValid())C->RegisterSensorActor(A.Get());DetachedSensors.Reset();}
                C->SetViewMode(Lidar?EVirtualSensorViewMode::Lidar:EVirtualSensorViewMode::Camera);C->BindMonitorWidget(Monitor);
                if(auto* A=C->GetSelectedSensorActor()){if(View==0||View==2)A->StartSensor();else A->StopSensor();}
            }
            Monitor->RefreshOwnedMonitorText();ReadyAt=FPlatformTime::Seconds()+1.5;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(View==4){Monitor->BindVirtualCamera(nullptr);Monitor->BindVirtualLidar(nullptr);Monitor->RefreshOwnedMonitorText();Test->TestEqual(TEXT("no-equipment capture has zero registered devices"),C->GetSensorActors().Num(),0);}
        if(!Requested)
        {
            const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));const TCHAR* Names[]={TEXT("camera-measuring"),TEXT("camera-stopped"),TEXT("lidar-measuring"),TEXT("lidar-stopped"),TEXT("no-equipment")};
            Path=Root/FString::Printf(TEXT("rendered-%dx%d-%d-%s.png"),Size.X,Size.Y,Large?150:100,Names[View]);OldTime=IFileManager::Get().GetTimeStamp(*Path);FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Test->AddInfo(FString::Printf(TEXT("UXR06 automated capture actual client=%dx%d font=%d windowDpi=%.3f; mouse not tested"),Size.X,Size.Y,Large?150:100,Window?Window->GetDPIScaleFactor():0));return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=OldTime)return false;
        if(++Phase==10)return true;ReadyAt=0;Requested=false;return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrPreviewRenderTest,"UXR06.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrPreviewRenderTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR06_RENDER"))!=TEXT("1")){AddInfo(TEXT("SKIP explicit UXR06 capture only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrPreviewRender(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
