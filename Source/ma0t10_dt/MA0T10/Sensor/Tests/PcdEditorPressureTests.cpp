#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "FinalAcceptanceTestSession.h"
#include "Json.h"

namespace
{
class FPcdEditorProbeWait final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;FString Directory;double Started=FPlatformTime::Seconds(),Duration=90,NextPoll=0;
public:
    FPcdEditorProbeWait(FAutomationTestBase* In,FString Dir):Test(In),Directory(MoveTemp(Dir)){FParse::Value(FCommandLine::Get(),TEXT("PcdOptSeconds="),Duration);}
    bool Update() override
    {
        if(FPlatformTime::Seconds()<NextPoll)return false;NextPoll=FPlatformTime::Seconds()+.2;
        FString Text;TSharedPtr<FJsonObject> Result;
        if(FFileHelper::LoadFileToString(Text,*(Directory/TEXT("result.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Result))
        {
            Test->TestTrue(TEXT("PIE pressure probe actually completed"),Result->GetBoolField(TEXT("valid")));
            Test->TestTrue(TEXT("real game frames measured"),Result->GetNumberField(TEXT("frames"))>100);
            if(FParse::Param(FCommandLine::Get(),TEXT("PcdOptNoHitch")))Test->TestEqual(TEXT("no frame exceeds 150ms"),Result->GetNumberField(TEXT("hitchesOver150ms")),0.0);
            return true;
        }
        if(FPlatformTime::Seconds()-Started>Duration+180){Test->AddError(TEXT("PIE pressure probe timed out"));return true;}return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdEditorPressureTest,"MA0T10.SensorStream.PcdOpt.EditorPressure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdEditorPressureTest::RunTest(const FString&)
{
    FString Directory,Snapshot;
    if(!FParse::Value(FCommandLine::Get(),TEXT("PcdOptReport="),Directory))
    {AddInfo(TEXT("[OPT-IN SKIP] -PcdOptReport and local pressure harness required"));return true;}
    if(auto* Config=GConfig->FindConfigFile(GGameIni))Config->NoSave=true;
    for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("WebSocketUrl"),TEXT("MA0T10_ARTEMIS_URL")},{TEXT("WebSocketLogin"),TEXT("MA0T10_ARTEMIS_USER")},{TEXT("WebSocketPasscode"),TEXT("MA0T10_ARTEMIS_PASSWORD")}})
        GConfig->SetString(TEXT("DTCoreRuntimeOverride"),*Pair.Key,*FPlatformMisc::GetEnvironmentVariable(*Pair.Value),GGameIni);
    FString Map=TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap");
    if(FParse::Value(FCommandLine::Get(),TEXT("SensorSmokeMapDirectory="),Snapshot))
    {FPackageName::RegisterMountPoint(TEXT("/PcdProbeMap/"),Snapshot+TEXT("/"));Map=TEXT("/PcdProbeMap/SensorTestMap");}
    if(!AutomationOpenMap(Map,true)){AddError(TEXT("PIE probe map load failed"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());
    ADD_LATENT_AUTOMATION_COMMAND(FPcdEditorProbeWait(this,Directory));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
