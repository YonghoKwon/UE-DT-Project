#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"

namespace
{
class FAcceptanceResizeProbe final:public IAutomationLatentCommand
{
public:
    explicit FAcceptanceResizeProbe(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>20){Test->AddError(TEXT("client resize did not converge after deliberate six-pixel drift"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!World||!World->GetGameViewport()||!World->GetGameViewport()->Viewport)return false;
        auto& View=*World->GetGameViewport();const auto Wanted=FinalAcceptanceViewportSize();
        if(!Perturbed)
        {
            if(!FinalAcceptanceViewportReady(World))return false;
            auto Window=FSlateApplication::Get().FindWidgetWindow(View.GetGameViewportWidget().ToSharedRef());
            Window->Resize(FVector2D(Window->GetClientSizeInScreen())+FVector2D(6,6));Perturbed=true;Frame=GFrameCounter;return false;
        }
        if(GFrameCounter<=Frame+1)return false;
        if(!ObservedDrift)
        {
            Test->TestNotEqual(TEXT("fixture really perturbed the rendered viewport"),View.Viewport->GetSizeXY(),Wanted);
            ObservedDrift=true;
        }
        if(!FinalAcceptanceViewportReady(World))return false;
        Test->TestEqual(TEXT("client-space correction restores exact requested viewport"),View.Viewport->GetSizeXY(),Wanted);return true;
    }
private:
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();uint64 Frame=0;bool Perturbed=false,ObservedDrift=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcceptanceViewportSizing,"MA0T10.Validation.ViewportClientSizingRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAcceptanceViewportSizing::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("[OPT-IN SKIP] rendered viewport requires RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FAcceptanceResizeProbe(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
