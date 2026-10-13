#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"

namespace
{
class FUxrFloatingTitleInput : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),Next=0;
    int32 Phase=0,Case=0;
    const TArray<ESensorToolPanelRole> Roles{ESensorToolPanelRole::Placement,ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabProgress,ESensorToolPanelRole::SlabCharts};
    TWeakObjectPtr<UVirtualSensorPanelWidgetBase> Panel;
    FVector2D Press,Before;
    static FPointerEvent Pointer(FVector2D Now,FVector2D Last,bool Held,FKey Button=FKey())
    {
        // FPointerEvent retains a pointer to this set. Keep both sets alive
        // throughout routed dispatch instead of returning a dangling reference.
        static const TSet<FKey> Released;
        static const TSet<FKey> Pressed{EKeys::LeftMouseButton};
        return FPointerEvent(0,Now,Last,Held?Pressed:Released,Button,0,FModifierKeysState());
    }
public:
    explicit FUxrFloatingTitleInput(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>60){Test->AddError(TEXT("Floating title input timed out"));return true;}
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!World||!FinalAcceptanceViewportReady(World)||FPlatformTime::Seconds()<Next)return false;
        auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();if(!Workspace||!Workspace->GetCoordinator())return false;
        auto& App=FSlateApplication::Get();
        if(Phase==0)
        {
            if(FPlatformTime::Seconds()-Started<3)return false;
            Workspace->EnsureOwnedReplayPanel();Workspace->SetSidebarMode(false);Workspace->SetOwnedPanelFontScale(Case<Roles.Num()?1.f:1.5f);
            for(const auto Role:Roles)Workspace->SetPanelOpen(Role,false);
            Workspace->SetPanelOpen(Roles[Case%Roles.Num()],true);
            Panel=Workspace->GetOwnedPanel(Roles[Case%Roles.Num()]);
            if(!Panel.IsValid()){Test->AddError(TEXT("Owned floating panel missing"));return true;}
            if(Panel->IsPanelCollapsed())Panel->TogglePanelCollapsed();
            Panel->SetWorkspacePosition(FVector2D(100,100));
            Before=Panel->GetCurrentPanelPosition();Next=FPlatformTime::Seconds()+.4;Phase=1;return false;
        }
        if(!Panel.IsValid()){Test->AddError(TEXT("Floating panel disappeared"));return true;}
        if(Phase==1)
        {
            // Let mode/font/layout restoration finish before fixing the start pose.
            Panel->SetWorkspacePosition(FVector2D(100,150));
            Next=FPlatformTime::Seconds()+.3;Phase=2;return false;
        }
        if(Phase==2)
        {
            Before=Panel->GetCurrentPanelPosition();
            Test->TestFalse(TEXT("floating panel is no longer docked"),Panel->IsWorkspaceDocked());
            Test->TestTrue(TEXT("floating title drag capability restored"),Panel->bPanelDraggable);
            Press=Panel->GetCachedGeometry().LocalToAbsolute(FVector2D(30,14));
            const auto Path=App.LocateWindowUnderMouse(Press,App.GetInteractiveTopLevelWindows());
            FString Types;for(int32 I=0;I<Path.Widgets.Num();++I)Types+=Path.Widgets[I].Widget->GetTypeAsString()+TEXT("/");
            Test->AddInfo(FString::Printf(TEXT("case=%d role=%d font=%.2f floating title hit path=%s point=%s visibility=%d draggable=%d"),Case,int32(Roles[Case%Roles.Num()]),Workspace->GetOwnedPanelFontScale(),*Types,*Press.ToString(),int32(Panel->GetVisibility()),Panel->bPanelDraggable));
            auto Window=App.FindWidgetWindow(Panel->GetCachedWidget().ToSharedRef());
            App.ProcessMouseMoveEvent(Pointer(Press,Press,false),false);
            App.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),Pointer(Press,Press,true,EKeys::LeftMouseButton));
            Test->TestTrue(TEXT("actual Slate title press captures the owned panel"),Panel->GetCachedWidget()->HasMouseCapture());
            Phase=3;return false;
        }
        if(Phase==3)
        {
            App.ProcessMouseMoveEvent(Pointer(Press+FVector2D(180,60),Press,true),false);Phase=4;return false;
        }
        App.ProcessMouseButtonUpEvent(Pointer(Press+FVector2D(180,60),Press+FVector2D(180,60),false,EKeys::LeftMouseButton));
        const FVector2D After=Panel->GetCurrentPanelPosition();
        Test->AddInfo(FString::Printf(TEXT("floating position before=%s after=%s"),*Before.ToString(),*After.ToString()));
        Test->TestTrue(TEXT("routed title drag changes the stored panel position"),After.X>Before.X+50&&After.Y>Before.Y+10);
        Test->TestFalse(TEXT("release clears pointer capture"),App.HasAnyMouseCaptor());
        if(++Case<Roles.Num()*2){Phase=0;Next=FPlatformTime::Seconds()+.1;return false;}
        Workspace->SetOwnedPanelFontScale(1);Workspace->SetSidebarMode(true);
        Test->TestTrue(TEXT("sidebar restores docked state"),Panel->IsWorkspaceDocked());
        Test->TestFalse(TEXT("docked title remains nondraggable"),Panel->bPanelDraggable);
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrFloatingTitleInputTest,"MA0T10.UXR07.FloatingTitleDragRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrFloatingTitleInputTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_INPUT_ACCEPTANCE_RHI"))!=TEXT("1")){AddInfo(TEXT("[OPT-IN SKIP] MA0T10_INPUT_ACCEPTANCE_RHI unset; actual Slate pointer routing requires D3D12 PIE"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrFloatingTitleInput(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
