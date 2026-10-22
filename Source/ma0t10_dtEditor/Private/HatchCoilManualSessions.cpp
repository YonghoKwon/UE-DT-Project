#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Misc/FileHelper.h"
#include "Json.h"

namespace
{
class FOwnedManualSession final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;bool Hatch,Ready=false;double Start=FPlatformTime::Seconds();
public:
    FOwnedManualSession(FAutomationTestBase* T,bool H):Test(T),Hatch(H){}
    bool Update() override
    {
        FString Finish;FParse::Value(FCommandLine::Get(),TEXT("ManualFinish="),Finish);
        if(Finish.IsEmpty()){Test->AddError(TEXT("Manual session requires an explicit completion flag path"));return true;}
        if(FPaths::FileExists(Finish))return true;
        if(FPlatformTime::Seconds()-Start>1800){Test->AddError(TEXT("manual session expired without completion flag"));return true;}
        if(Ready)return false;
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&FinalAcceptanceViewportReady(C.World()))
        {
            int32 Font=100;FParse::Value(FCommandLine::Get(),TEXT("ManualFont="),Font);
            float ActualFont=1;
            if(Hatch){for(TActorIterator<AShipHatchMonitorHost> H(C.World());H;++H)if(H->Monitor&&H->Monitor->GetAnalysis()){H->Monitor->SetFontPercent(Font);H->Monitor->SetSplitView(false);ActualFont=H->Monitor->GetFontScale();Ready=true;}}
            else {for(TActorIterator<ACoilLoadingMonitorHost> H(C.World());H;++H)if(H->Monitor){H->Monitor->SetFontPercent(Font);H->Monitor->SelectView(1);ActualFont=H->Monitor->FontScale();Ready=true;}}
            if(Ready)
            {
                auto* View=C.World()->GetGameViewport();const auto Size=View->Viewport->GetSizeXY();const auto Window=View->GetWindow();auto O=MakeShared<FJsonObject>();
                O->SetNumberField(TEXT("client_width"),Size.X);O->SetNumberField(TEXT("client_height"),Size.Y);O->SetNumberField(TEXT("window_dpi_scale"),Window?Window->GetDPIScaleFactor():-1);
                O->SetNumberField(TEXT("umg_viewport_scale"),UWidgetLayoutLibrary::GetViewportScale(C.World()));O->SetNumberField(TEXT("owned_font_percent"),ActualFont*100);O->SetStringField(TEXT("scope"),TEXT("manual session setup; Computer Use operations and screenshots are separate evidence"));
                FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*FPaths::ChangeExtension(Finish,TEXT("metrics.json")));
                Test->AddInfo(TEXT("Manual PIE ready; actual Computer Use actions are recorded separately, this harness is not an automated UI pass."));
            }
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatch07Manual,"HATCH07.ManualSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatch07Manual::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FOwnedManualSession(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoil02Manual,"COIL02.ManualSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoil02Manual::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FOwnedManualSession(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
