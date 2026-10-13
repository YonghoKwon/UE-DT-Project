#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorPlacementInteraction.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
// UXR-13: starting 이동·회전 must keep the view the user is looking at, send keys to
// the 3D view, and show the key guide; ending restores the hidden guide.
class FUxr13ManipulationView : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),Next=0;
    int32 Phase=0;FString Path;FDateTime Previous;
    EVirtualSensorViewMode ViewBefore=EVirtualSensorViewMode::Camera;
public:
    explicit FUxr13ManipulationView(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>90){Test->AddError(TEXT("UXR13 timed out"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World)||FPlatformTime::Seconds()<Next)return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W?W->GetCoordinator():nullptr;if(!C)return false;
        auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());auto& App=FSlateApplication::Get();
        if(Phase==0)
        {
            if(FPlatformTime::Seconds()-Started<3)return false;
            AVirtualSensorActorBase* Target=nullptr;for(auto* A:C->GetSensorActors())if(IsValid(A)){Target=A;break;}
            if(!Target){auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>(FVector(0,0,200),FRotator::ZeroRotator);C->RegisterSensorActor(Camera);Target=Camera;}
            if(!Target||!Bar){Test->AddError(TEXT("UXR13 needs a device and owned toolbar"));return true;}
            W->SetOwnedPanelFontScale(1);W->SetSidebarMode(true);C->SelectSensorActor(Target);W->SynchronizeOwnedSelection();
            Bar->OpenOwnedDestination(ESensorToolDestination::Placement);
            // Font/sidebar changes rebuild owned panels on a later frame; start moving after they settle.
            Phase=10;Next=FPlatformTime::Seconds()+1.0;return false;
        }
        if(Phase==10)
        {
            AVirtualSensorActorBase* Target=C->GetSelectedSensorActor();if(!Target){Test->AddError(TEXT("UXR13 lost selection"));return true;}
            ViewBefore=C->GetViewMode();
            auto* Placement=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));
            const bool bBegan=Placement&&Placement->SetSelectedManipulating(true);
            Test->TestTrue(FString::Printf(TEXT("placement starts 이동·회전 (%s)"),Placement?*Placement->GetPlacementStatusText():TEXT("no panel")),bBegan);
            Test->TestTrue(TEXT("interaction active"),W->GetPlacementInteraction()->IsManipulating());
            Test->TestEqual(TEXT("preview keeps the view the user was watching"),C->GetViewMode(),ViewBefore);
            const auto View=World->GetGameViewport()->GetGameViewportWidget();
            Test->TestTrue(TEXT("keyboard goes to the 3D view without an extra click"),View.IsValid()&&App.GetKeyboardFocusedWidget()==View);
            Test->TestTrue(TEXT("key guide visible while moving"),Bar->IsOwnedManipulationActive());
            const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
            Path=(Root.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-13"):Root)/TEXT("rendered-1920x1080-manipulation-guide.png");
            Phase=1;Next=FPlatformTime::Seconds()+.8;return false;
        }
        if(Phase==1){if(auto* Pl=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement)))Test->AddInfo(FString::Printf(TEXT("UXR13 phase1 manipulating=%d status=%s focus=%s"),W->GetPlacementInteraction()->IsManipulating(),*Pl->GetPlacementStatusText(),App.GetKeyboardFocusedWidget().IsValid()?*App.GetKeyboardFocusedWidget()->GetTypeAsString():TEXT("none")));Previous=IFileManager::Get().GetTimeStamp(*Path);App.CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Phase=2;return false;}
        if(Phase==2&&IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        Test->AddInfo(FString::Printf(TEXT("UXR13 render %s"),*Path));
        Test->TestTrue(TEXT("still moving when the frame was captured"),W->GetPlacementInteraction()->IsManipulating());
        auto* Placement=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));
        Test->TestTrue(TEXT("placement ends 이동·회전"),Placement&&Placement->SetSelectedManipulating(false));
        Test->TestFalse(TEXT("key guide hidden after end"),Bar&&Bar->IsOwnedManipulationActive());
        Test->TestEqual(TEXT("view still unchanged after end"),C->GetViewMode(),ViewBefore);
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr13ManipulationViewTest,"MA0T10.UXR13.KeepViewAndKeyGuideRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr13ManipulationViewTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("SKIP UXR13 guide render requires real RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr13ManipulationView(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
