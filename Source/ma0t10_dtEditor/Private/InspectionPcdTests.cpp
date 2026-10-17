#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationControlSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "Core/DxDataSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Json.h"

namespace
{
FString InspectionScenario(double Seconds)
{
    TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower))),O);
    O->GetObjectField(TEXT("_meta"))->SetNumberField(TEXT("duration_sec"),Seconds);
    auto Rows=O->GetArrayField(TEXT("DATA_MAP"));Rows.SetNum(FMath::RoundToInt(Seconds*20));O->SetArrayField(TEXT("DATA_MAP"),Rows);
    FString Json;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));return Json;
}
class FInspectionPcdCommand final:public IAutomationLatentCommand
{
public:
    FInspectionPcdCommand(FAutomationTestBase* In,bool InBroker):Test(In),Broker(InBroker){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(LoggedStage!=Stage){LoggedStage=Stage;UE_LOG(LogTemp,Display,TEXT("[INSP-FIX-1] stage=%d"),Stage);}
        if(Now-Started>(FParse::Param(FCommandLine::Get(),TEXT("InspectionManual"))?300:90)){Test->AddError(TEXT("INSP-FIX-1 timeout"));return Finish();}
        if(Stage==6)return IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir()/TEXT("Reports/INSP-FIX-1/manual-stop")))?Finish():false;
        if(!Slab)
        {
            UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){Candidate=C.World();break;}
            if(World!=Candidate){World=Candidate;ReadyAt=Now;return false;}
            if(!World)return false;
            if(Now-ReadyAt<3||!FinalAcceptanceViewportReady(World))return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){Slab=*It;break;}
            for(TActorIterator<AVirtualSensorCoordinator> It(World);It;++It){Coordinator=*It;break;}
            if(!Slab||!Coordinator){Test->AddError(TEXT("inspection map fixture missing"));return Finish();}
            Catalog=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();Control=World->GetSubsystem<UAutomationControlSubsystem>();
            Panel=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);Panel->BindScenarioSlabActor(Slab);
            Test->TestTrue(TEXT("committed validation map defaults to PCD on"),Slab->GetSensorOutputs().bPointCloud&&Panel->GetLiveScenarioOutputs().bPointCloud);
            UE_LOG(LogTemp,Display,TEXT("[INSP-FIX-1] fixture ready"));return StartApi(false);
        }
        if(Stage<=1)
        {
            if(!Control->GetStatus()->GetBoolField(TEXT("terminal")))return false;
            Test->TestTrue(TEXT("scenario.start send_pcd=false completes"),Control->GetStatus()->GetBoolField(TEXT("success")));
            Test->TestFalse(TEXT("one-shot replay is observation only"),Catalog->GetReplayStatus().Outputs.bPointCloud);
            Test->TestEqual(TEXT("API leaves the user's next live PCD selection intact"),Slab->GetSensorOutputs().bPointCloud,Stage==0);
            auto* Prefs=UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();Test->TestTrue(TEXT("save preferences"),UVirtualSensorUiPreferencesSaveGame::Save(Prefs));
            auto* Restored=UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();Test->TestNotNull(TEXT("preferences restored"),Restored);
            auto* Reopened=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);Reopened->BindScenarioSlabActor(Slab);
            Test->TestEqual(TEXT("save/load and reopened UI preserve next live selection"),Reopened->GetLiveScenarioOutputs().bPointCloud,Stage==0);
            if(Stage==0)
            {
                // Explicit user off is preserved too. Exercise the archived UUID
                // branch after json_path exercised async import in the first run.
                Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());Stage=1;return StartApi(true);
            }
            Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection());
            for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It){It->SubmitSynthetic(false);break;}
            Test->TestTrue(TEXT("validation observation button preserves next live PCD default"),Slab->GetSensorOutputs().bPointCloud);
            Slab->StopSimulation();Stage=5;return false;
        }
        if(Stage==5)
        {
            const auto State=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus().State;
            if(State==EVirtualSlabSessionState::Draining||Catalog->IsReplayBusy())return false;
            if(!Broker)
            {
                if(FParse::Param(FCommandLine::Get(),TEXT("InspectionManual")))
                {
                    for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);
                    FFileHelper::SaveStringToFile(TEXT("ready"),*(FPaths::ProjectSavedDir()/TEXT("Reports/INSP-FIX-1/manual-ready")));Stage=6;return false;
                }
                return Finish();
            }
            auto* T=Coordinator->SharedTransportComponent.Get();auto P=T->GetTransportProfile();P.BrokerUrl=TEXT("tcp://127.0.0.1:61616");P.UserName=TEXT("guest");P.ExportTopic=TEXT("pcd.topic");T->ConfigureTransportProfile(P);T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;T->SetSessionCredentials(FString(),FString());
            Test->TestTrue(TEXT("TCP connection test admitted without a stream"),T->TestConnection().bSubmitted);Stage=2;return false;
        }
        if(Stage==2)
        {
            bool Connected=false,Expected=false;Coordinator->SharedTransportComponent->GetOutputServerConnection(Connected,Expected);if(!Connected)return false;
            Test->TestTrue(TEXT("idle connection test success observable"),Connected&&Expected);
            const FString Json=InspectionScenario(5);
            // Same DTCore receive path as the validation map, then DataSync/Actor.
            World->GetGameInstance()->GetSubsystem<UDxDataSubsystem>()->EnqueueWebSocketData(Json);
            Stage=3;ReadyAt=Now;return false;
        }
        const auto S=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus();
        if(Stage==3)
        {
            if(!Slab->IsSimulationActive())return false;
            Run=Slab->GetSimulationStatus().RunUUID;Test->TestTrue(TEXT("new incoming scenario applies default PCD"),S.Outputs.bPointCloud&&!S.bObservationOnly);Stage=4;return false;
        }
        if(S.State!=EVirtualSlabSessionState::Completed&&S.State!=EVirtualSlabSessionState::Incomplete)return false;
        Test->TestEqual(TEXT("scenario and output drain complete"),S.State,EVirtualSlabSessionState::Completed);
        Test->AddInfo(TEXT("INSP-FIX-1 E2E run_uuid=")+Run);
        FFileHelper::SaveStringToFile(Run,*(FPaths::ProjectSavedDir()/TEXT("Reports/INSP-FIX-1/e2e-run.txt")));
        return Finish();
    }
private:
    bool StartApi(bool ByUuid)
    {
        auto Body=MakeShared<FJsonObject>();Body->SetBoolField(TEXT("send_pcd"),false);
        if(ByUuid)Body->SetStringField(TEXT("uuid"),Catalog->GetReplayStatus().ScenarioUUID);
        else{const FString Path=FPaths::ProjectSavedDir()/TEXT("Reports/INSP-FIX-1/api-scenario.json");FFileHelper::SaveStringToFile(InspectionScenario(.5),*Path);Body->SetStringField(TEXT("json_path"),FPaths::ConvertRelativePathToFull(Path));}
        int32 Code=0;Control->Dispatch(TEXT("POST"),TEXT("/v1/scenario/start"),Body,Code);Test->TestEqual(TEXT("scenario API accepted"),Code,202);return Code==202?false:Finish();
    }
    bool Finish(){if(IsValid(Slab))Slab->StopSimulation();if(IsValid(World))World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->StopHighThroughputTransport();return true;}
    FAutomationTestBase* Test;bool Broker;int32 Stage=0,LoggedStage=-1;double Started=FPlatformTime::Seconds(),ReadyAt=0;UWorld* World=nullptr;ASlabActor* Slab=nullptr;AVirtualSensorCoordinator* Coordinator=nullptr;USlabScenarioReplaySubsystem* Catalog=nullptr;UAutomationControlSubsystem* Control=nullptr;UVirtualSensorCaptureExportPanelWidget* Panel=nullptr;FString Run;
};
bool RunInspection(FAutomationTestBase* Test,bool Broker)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FInspectionPcdCommand(Test,Broker));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionPcdDefaults,"MA0T10.Inspection.PcdDefaultsIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionPcdDefaults::RunTest(const FString&){return RunInspection(this,false);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionPcdBroker,"MA0T10.Inspection.ArtemisIncomingPcd",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionPcdBroker::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("InspectionArtemis"))){AddInfo(TEXT("[OPT-IN SKIP] requires session-owned local Artemis and -InspectionArtemis"));return true;}return RunInspection(this,true);}
#endif
