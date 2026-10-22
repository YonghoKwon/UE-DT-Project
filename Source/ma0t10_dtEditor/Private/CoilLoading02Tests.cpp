#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabSimulationUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioReplayUiHostActor.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"

namespace
{
class FCoil02PolicyCommand final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();
public:
    explicit FCoil02PolicyCommand(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>45){Test->AddError(TEXT("COIL02 fixture timeout"));return true;}
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&FinalAcceptanceViewportReady(C.World()))
        for(TActorIterator<ACoilLoadingMonitorHost> H(C.World());H;++H)if(H->Monitor&&H->Simulation)
        {
            auto& S=*H->Simulation;const int32 Count=S.Plan().Num();
            for(int32 Tier:{6,99})for(bool Animate:{false,true})
            {S.NextTier=Tier;Test->TestFalse(TEXT("sixth and higher tier rejected for both placement paths"),S.PlaceNext(Animate));Test->TestEqual(TEXT("rejection is atomic"),S.Plan().Num(),Count);Test->TestTrue(TEXT("tier rejection has a reason"),S.Status.Contains(TEXT("5단")));}
            S.NextTier=5;Test->TestTrue(TEXT("fifth tier remains allowed; analysis warnings stay advisory"),S.PlaceNext(false));Test->TestTrue(TEXT("undo fifth tier"),S.Undo());
            CoilLoading::FPlacement P;P.Tier=6;CoilLoading::FSettings Settings;TArray<CoilLoading::FPlacement> Plan;FString Error;
            Test->TestFalse(TEXT("plan import cannot bypass five-tier cap"),CoilLoading::PlanFromJson(CoilLoading::PlanToJson(S.Settings,{P}),Settings,Plan,Error));
            S.NextTier=1;S.RefreshPreview();
            auto* Workspace=C.World()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
            Test->TestFalse(TEXT("coil map disables Slab capabilities before tools are opened"),Workspace->AreSlabToolsEnabled());H->ShowTools();
            for(auto Role:{ESensorToolPanelRole::SlabProgress,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabCharts})
            {Workspace->SetPanelOpen(Role,true);Test->TestNull(TEXT("direct/back/preflight navigation cannot create Slab UI"),Workspace->GetOwnedPanel(Role));}
            Test->TestFalse(TEXT("replay lazy factory is also blocked"),Workspace->EnsureOwnedReplayPanel());
            if(auto* Toolbar=Cast<USensorToolToolbarWidget>(Workspace->GetOwnedNavigation()))
            {Test->TestFalse(TEXT("scenario button cannot open Slab"),Toolbar->OpenOwnedDestination(ESensorToolDestination::Scenario));Test->TestFalse(TEXT("charts button cannot open Slab"),Toolbar->OpenOwnedDestination(ESensorToolDestination::ScenarioCharts));}
            int32 SlabHosts=0;for(TActorIterator<ASlabSimulationUiHostActor> I(C.World());I;++I)++SlabHosts;for(TActorIterator<ASlabScenarioReplayUiHostActor> I(C.World());I;++I)++SlabHosts;
            Test->TestEqual(TEXT("blocked buttons do not spawn Slab hosts"),SlabHosts,0);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoil02PolicyRhi,"MA0T10.CoilLoading.Coil02PolicyRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoil02PolicyRhi::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoil02PolicyCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
