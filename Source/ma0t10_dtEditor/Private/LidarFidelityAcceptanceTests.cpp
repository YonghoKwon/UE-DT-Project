#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "UxrScenarioUiFixture.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

class FLidarFidelityPresentation : public IAutomationLatentCommand
{
public:
    FLidarFidelityPresentation(FAutomationTestBase* In,bool InManual):Test(In),Manual(InManual){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Created>(Manual?900:80)){Test->AddError(TEXT("LIDAR-FID presentation timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W?W->GetCoordinator():nullptr;
        auto* M=W?Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor)):nullptr;
        if(!C||!M)return false;
        if(!Ready)
        {
            Guard=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            for(auto* Actor:C->GetSensorActors())if(Actor)Actor->StopSensor();
            W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(1);
            for(int32 Role=0;Role<7;++Role)W->SetPanelOpen(static_cast<ESensorToolPanelRole>(Role),false);
            C->SelectLidarByIndex(0);C->SetViewMode(EVirtualSensorViewMode::Lidar);C->BindMonitorWidget(M);
            auto* Scan=C->GetSelectedLidar();auto* Sensor=Scan?Cast<AVirtualLidarSensorActor>(Scan->GetOwner()):nullptr;
            if(!Sensor)return false;
            C->SelectSensorActor(Sensor);W->SynchronizeOwnedSelection();
            FVirtualSensorEditableState State,Applied;Sensor->ReadEditableState(State);
            State.LidarProfile=EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;State.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;
            FString Error;if(!Sensor->ApplyProfileAndSimulationQuality(State,Applied,Error)){Test->AddError(Error);return true;}
            State=Applied;State.bLidarPointTime=State.bLidarMotionDistortion=State.bLidarMaterialResponse=!Manual;
            State.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;
            for(TActorIterator<ASlabActor> It(World);It;++It)
            {
                Test->TestTrue(TEXT("moving Slab starts in PIE memory"),StartUxrScenarioUiFixture(*It));
                const FVector Target=It->GetActorLocation();const FVector Origin=Target+FVector(-1000,-600,500);
                State.ActorTransform=FTransform((Target-Origin).Rotation(),Origin);break;
            }
            if(!Sensor->ApplyEditableState(State,Error)){Test->AddError(Error);return true;}
            Sensor->StartSensor();W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);W->SetPanelOpen(ESensorToolPanelRole::Settings,true);
            W->SynchronizeOwnedSelection();M->SetLidarColorMode(ELidarColorMode::ReflectionIntensity);
            Ready=true;Next=Now+4;
            if(Manual)Test->AddInfo(TEXT("LIDAR-FID-01 MANUAL READY: defaults off; finish.flag ends this session"));
            return false;
        }
        if(Manual)return IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir()/TEXT("Reports/LIDAR-FID-01/manual/finish.flag")));
        if(Now<Next)return false;
        const auto* Scan=C->GetSelectedLidar();const auto Frame=Scan?Scan->GetLastFrameSnapshot():nullptr;
        if(!Frame||!Frame->Points||Frame->Points->IsEmpty())return false;
        if(!Requested)
        {
            const ELidarColorMode Modes[]={ELidarColorMode::ReflectionIntensity,ELidarColorMode::MeasurementTime};
            M->SetLidarColorMode(Modes[Mode]);
            const TCHAR* Names[]={TEXT("intensity"),TEXT("point-time")};
            Path=FPaths::ProjectSavedDir()/FString::Printf(TEXT("Reports/LIDAR-FID-01/automated/1920x1080-%s.png"),Names[Mode]);
            OldTime=IFileManager::Get().GetTimeStamp(*Path);FSlateApplication::Get().CloseToolTip();
            // Wait for the existing <=5Hz preview cache after changing colour mode.
            Requested=true;Next=Now+1;return false;
        }
        if(!Captured){FScreenshotRequest::RequestScreenshot(Path,true,false);Captured=true;Next=Now+.4;return false;}
        if(IFileManager::Get().GetTimeStamp(*Path)<=OldTime)return false;
        const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());
        Test->AddInfo(FString::Printf(TEXT("LIDAR-FID automatic render client=%dx%d dpi=%.2f font=100 mode=%d path=%s; not direct mouse acceptance"),Size.X,Size.Y,Window?Window->GetDPIScaleFactor():0,Mode,*Path));
        Test->TestTrue(TEXT("nonzero acquisition point times"),Frame->Points->Last().PointTimeOffsetNanoseconds>0);
        if(++Mode<2){Requested=false;Captured=false;Next=Now+.3;return false;}
        return true;
    }
private:
    FAutomationTestBase* Test;bool Manual=false,Ready=false,Requested=false,Captured=false;int32 Mode=0;
    double Created=FPlatformTime::Seconds(),Next=0;FString Path;FDateTime OldTime;TUniquePtr<TGuardValue<bool>> Guard;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarFidelityPresentationTest,"MA0T10.LidarFidelity.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarFidelityPresentationTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddWarning(TEXT("SKIP: presentation requires RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FLidarFidelityPresentation(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarFidelityManualTest,"MA0T10.LidarFidelity.ManualPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarFidelityManualTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("LidarFidelityManual"))){AddInfo(TEXT("SKIP: explicit -LidarFidelityManual session only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FLidarFidelityPresentation(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
