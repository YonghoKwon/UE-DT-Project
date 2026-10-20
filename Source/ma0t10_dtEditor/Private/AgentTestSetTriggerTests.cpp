#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetTrigger.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHitchDiagnostics.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/FactoryAgentScenarioTC.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "WebSocket/FTransactionCodeDataBase.h"
#include "Async/Async.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineGlobals.h"
#include "Engine/GameInstance.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
#include "AgentTestSetPresentationChecks.h"

namespace
{
class FTriggerLegacyProbe final : public IAutomationLatentCommand
{
public:
    explicit FTriggerLegacyProbe(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>30){Test->AddError(TEXT("legacy probe timeout"));return true;}
        if(!Slab)
        {
            for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
            if(!World||!FinalAcceptanceViewportReady(World))return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){Slab=*It;break;}
            if(!Slab)return false;
            {FAgentTestSetSettings C;C.AutoFillExamples=false;Test->TestTrue(TEXT("received-mode configuration"),World->GetSubsystem<UAgentTestSetSubsystem>()->ConfigureForTests(C));}
            const FString Json=TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":\"SQ23456 077\"}]}");
            auto* TC=NewObject<UFactoryAgentScenarioTC>(World);TC->ReceiverId=Slab->ReceiverId;
            TC->ProcessStructData(TC->ParseToStruct(Json));
            Test->TestFalse(TEXT("legacy TC codec rejects fieldless trigger"),World->GetSubsystem<UAgentTestSetSubsystem>()->HasItems());
            return false;
        }
        if(Slab->HasPendingScenarioInput())return false;
        Test->TestFalse(TEXT("legacy Submit codec rejects fieldless trigger"),World->GetSubsystem<UAgentTestSetSubsystem>()->HasItems());
        Test->TestEqual(TEXT("legacy missing CREATE_TIMESTAMP failure"),Slab->GetSimulationStatus().Message,FString(TEXT("CREATE_TIMESTAMP 문자열이 필요합니다.")));
        if(Phase==0){++Phase;Test->TestTrue(TEXT("strict Submit schedules parser"),Slab->SubmitScenarioJson(TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":\"SQ23456 077\"}]}")));return false;}
        if(Phase>=1&&Phase<=5)
        {
            if(Phase==1){FAgentTestSetSettings C;C.AutoFillExamples=true;World->GetSubsystem<UAgentTestSetSubsystem>()->ConfigureForTests(C);}
            const TArray<FString> Values={TEXT("1234"),TEXT("9999"),TEXT("SQ23456077"),TEXT("sq23456 077")};
            const FString Ordinary=Phase<=4?TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"mtl_no\":\"")+Values[Phase-1]+TEXT("\",\"DATA_MAP\":[]}"):TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":9999}]}");++Phase;
            auto* TC=NewObject<UFactoryAgentScenarioTC>(World);TC->ReceiverId=Slab->ReceiverId;TC->ProcessStructData(TC->ParseToStruct(Ordinary));
            Test->TestTrue(TEXT("incomplete ordinary input still parsed normally"),Slab->SubmitScenarioJson(Ordinary));return false;
        }
        return true;
    }
private:
    FAutomationTestBase* Test;UWorld* World=nullptr;ASlabActor* Slab=nullptr;int32 Phase=0;double Started=FPlatformTime::Seconds();
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentTriggerLegacy,"MA0T10.TestSet.StrictReceivedAndOrdinaryValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentTriggerLegacy::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FTriggerLegacyProbe(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentTriggerTemplate,"MA0T10.TestSet.TriggerEnvelopeAndTemplate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentTriggerTemplate::RunTest(const FString&)
{
    const FAgentTestSetSettings Defaults;
    TestEqual(TEXT("native default includes the internal space"),Defaults.Trigger,FString(TEXT("SQ23456 077")));
    auto* Service=FAutomationEditorCommonUtils::CreateNewMap()->GetSubsystem<UAgentTestSetSubsystem>();
    TestEqual(TEXT("runtime ini default includes the internal space"),Service->Settings().Trigger,Defaults.Trigger);
    for(const FString Value:{FString(TEXT("SQ23456 077")),FString(TEXT("\"SQ23456 077\""))})
    {
        FConfigFile Ini;Ini.ProcessInputFileContents(FString(TEXT("[AgentTestSet]\nTriggerMtlNo="))+Value+TEXT("\n"),TEXT("TESTSET-03 in-memory fixture"));
        FString Parsed;TestTrue(TEXT("ini key read"),Ini.GetString(TEXT("AgentTestSet"),TEXT("TriggerMtlNo"),Parsed));
        TestEqual(TEXT("quoted and unquoted ini preserve internal space"),Parsed,Defaults.Trigger);
    }
    TestTrue(TEXT("fresh string trigger configuration"),Service->ConfigureForTests(Defaults));
    for(const FString Value:{Defaults.Trigger,TEXT("  ")+Defaults.Trigger+TEXT("  ")})
    {
        const FString Json=TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"_meta\":{\"mtl_no\":\"")+Value+TEXT("\"}}");
        TestEqual(TEXT("envelope trims only outside whitespace"),AgentTestSetTrigger::Material(Json),Defaults.Trigger);
        TestTrue(TEXT("exact or outer-padded material matches"),Service->IsDemoTrigger(AgentTestSetTrigger::Material(Json)));
        FSlabScenarioData Data;Data.Rows.AddDefaulted();Data.Rows[0].MtlNo=Value;
        TestTrue(TEXT("strict received-mode matching follows same rule"),UAgentTestSetSubsystem::Matches(Data,Defaults.Trigger));
    }
    for(const FString Value:{FString(TEXT("9999")),FString(TEXT("SQ23456077")),FString(TEXT("sq23456 077"))})
    {
        TestFalse(TEXT("former trigger, missing internal space and wrong case do not match"),Service->IsDemoTrigger(Value));
        FSlabScenarioData Data;Data.Rows.AddDefaulted();Data.Rows[0].MtlNo=Value;
        TestFalse(TEXT("strict matching also rejects those materials"),UAgentTestSetSubsystem::Matches(Data,Defaults.Trigger));
    }
    TestTrue(TEXT("numeric material is not a string trigger"),AgentTestSetTrigger::Material(TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":9999}]}")).IsEmpty());
    TestTrue(TEXT("routing requires existing MESSAGE_ID"),AgentTestSetTrigger::Material(TEXT("{\"mtl_no\":\"SQ23456 077\"}")).IsEmpty());
    TestTrue(TEXT("invalid JSON is never a trigger"),AgentTestSetTrigger::Material(TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"mtl_no\":\"SQ23456 077\"")).IsEmpty());
    TestEqual(TEXT("first row takes precedence over unrelated top key"),AgentTestSetTrigger::Material(TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"mtl_no\":\"SQ23456 077\",\"DATA_MAP\":[{\"mtl_no\":\"1234\"}]}")),FString(TEXT("1234")));
    for(auto Unit:{ESlabInputUnit::Centimeters,ESlabInputUnit::Millimeters,ESlabInputUnit::Meters})
    {
        FSlabScenarioDataPtr Data;FString Error;
        TestTrue(TEXT("committed template passes unchanged strict codec"),AgentTestSetTrigger::LoadTemplate(FPaths::ProjectDir()/TEXT("Config/AgentTestSetScenario.json"),TEXT("7777"),Unit,Unit,Data,Error));
        if(!Data){AddError(Error);continue;}
        TestEqual(TEXT("template uses configured trigger"),Data->Rows[0].MtlNo,FString(TEXT("7777")));
        TestEqual(TEXT("explicit template mm converted to receiving units"),FSlabScenarioCodec::ToCm(Data->Rows[0].Length,Unit),1083.);
        TestTrue(TEXT("template retains realistic continuous motion"),Data->Rows.Num()==20&&Data->Rows.Last().CenterX>Data->Rows[0].CenterX);
    }
    return true;
}

namespace
{
class FFormerTriggerOrdinaryRun final:public IAutomationLatentCommand
{
public:
    explicit FFormerTriggerOrdinaryRun(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>60){Test->AddError(TEXT("former trigger ordinary routing timeout"));return Finish();}
        if(!Service)
        {
            for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
            if(!World||!FinalAcceptanceViewportReady(World)||Now-Started<3)return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){It->StopSimulation();if(!Slab)Slab=*It;}
            for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            for(TActorIterator<AVirtualSensorCoordinator> It(World);It;++It)Coordinator=*It;
            if(!Slab||!Coordinator){Test->AddError(TEXT("ordinary routing fixture missing"));return Finish();}
            // Session-only endpoint. No external Broker or deployment settings are touched.
            auto* Transport=Coordinator->SharedTransportComponent.Get();auto Profile=Transport->GetTransportProfile();
            Profile.BrokerUrl=TEXT("tcp://127.0.0.1:1");Transport->ConfigureTransportProfile(Profile);Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;Transport->SetSessionCredentials(FString(),FString());
            Test->TestTrue(TEXT("ordinary fixture has default PCD-only selection"),Slab->GetSensorOutputs().bPointCloud&&!Slab->GetSensorOutputs().bCameraImage&&!Slab->GetSensorOutputs().bLidarTelemetry);
            Service=World->GetSubsystem<UAgentTestSetSubsystem>();
            TC=NewObject<UFactoryAgentScenarioTC>(World);TC->AddToRoot();TC->ReceiverId=Slab->ReceiverId;
            InitialArchive=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>()->GetScenarios().Num();
        }
        if(Parse.IsValid())
        {if(!Parse.IsReady())return false;TC->ProcessStructData(Parse.Get());Parse=TFuture<TSharedPtr<FTransactionCodeDataBase>>();}
        auto* Session=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
        if(Phase==0)
        {
            const auto State=Session->GetSlabSensorSessionStatus().State;
            if(Slab->HasPendingScenarioInput()||State==EVirtualSlabSessionState::Draining)return false;
            TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower))),Root);
            auto Rows=Root->GetArrayField(TEXT("DATA_MAP"));Rows.SetNum(20);for(const auto& V:Rows)V->AsObject()->SetStringField(TEXT("mtl_no"),TEXT("9999"));Root->SetArrayField(TEXT("DATA_MAP"),Rows);Root->GetObjectField(TEXT("_meta"))->SetNumberField(TEXT("duration_sec"),1.);
            FString Json;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
            if(Route){auto* Target=TC;Parse=Async(EAsyncExecution::ThreadPool,[Target,Json](){return Target->ParseToStruct(Json);});}
            else Test->TestTrue(TEXT("former trigger Submit accepted"),Slab->SubmitScenarioJson(Json));
            Phase=1;return false;
        }
        if(Phase==1)
        {
            if(!Slab->IsSimulationActive())return false;
            Test->TestEqual(TEXT("former trigger plays as ordinary material"),Slab->GetSimulationStatus().MtlNo,FString(TEXT("9999")));
            Test->TestFalse(TEXT("former trigger creates no test set"),Service->HasItems());
            const auto Admission=Slab->GetLastScenarioAdmissionStatus();const auto S=Session->GetSlabSensorSessionStatus();
            Test->TestTrue(TEXT("ordinary receive keeps requested and applied default PCD"),Admission.RequestedOutputs.bPointCloud&&Admission.AppliedOutputs.bPointCloud&&S.Outputs.bPointCloud&&!S.bObservationOnly);
            Test->TestEqual(TEXT("ordinary receive archives exactly once"),World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>()->GetScenarios().Num(),InitialArchive+Route+1);
            Test->TestTrue(TEXT("ordinary session enables PCD output demand"),Session->AllowsOutputDemand(TEXT("SLAB-LIDAR-001"),EVirtualSensorAcquisitionOutput::PointCloud));
            FirstElapsed=Slab->GetSimulationStatus().ElapsedSec;PlayingAt=Now;Phase=2;return false;
        }
        if(Now-PlayingAt<.15)return false;
        Test->TestTrue(TEXT("ordinary motion advances through world-time playback"),Slab->GetSimulationStatus().ElapsedSec>FirstElapsed);
        Slab->StopSimulation();if(++Route==2)return Finish();Phase=0;return false;
    }
private:
    bool Finish(){if(Slab)Slab->StopSimulation();if(World)World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->StopHighThroughputTransport();if(TC)TC->RemoveFromRoot();return true;}
    FAutomationTestBase* Test;UWorld* World=nullptr;UAgentTestSetSubsystem* Service=nullptr;ASlabActor* Slab=nullptr;AVirtualSensorCoordinator* Coordinator=nullptr;UFactoryAgentScenarioTC* TC=nullptr;
    TFuture<TSharedPtr<FTransactionCodeDataBase>> Parse;int32 Route=0,Phase=0,InitialArchive=0;double Started=FPlatformTime::Seconds(),PlayingAt=0,FirstElapsed=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFormerTriggerOrdinary,"MA0T10.TestSet.FormerTriggerOrdinaryRouting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentFormerTriggerOrdinary::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FFormerTriggerOrdinaryRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

namespace
{
class FTriggerRoutingRun final:public IAutomationLatentCommand
{
public:
    explicit FTriggerRoutingRun(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>600){Test->AddError(TEXT("TESTSET-03 routing timeout"));return Finish();}
        if(!Service)
        {
            UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)Candidate=C.World();
            if(World!=Candidate){World=Candidate;Observed=Now;return false;}
            if(!World||Now-Observed<3||!FinalAcceptanceViewportReady(World))return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){It->StopSimulation();if(!Slab)Slab=*It;}
            for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It)if(It->GetSensorId()==TEXT("SLAB-LIDAR-001")){Lidar=*It;break;}
            if(!Slab||!Lidar){Test->AddError(TEXT("validation fixture missing"));return Finish();}
            auto* Scan=Lidar->ScanComponent.Get();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->bEnableRangeNoise=false;
            Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
            Service=World->GetSubsystem<UAgentTestSetSubsystem>();FAgentTestSetSettings C;C.Target=FParse::Param(FCommandLine::Get(),TEXT("TestSet03Hundred"))?100:6;C.LidarId=Lidar->GetSensorId();Service->ConfigureForTests(C);
            TC=NewObject<UFactoryAgentScenarioTC>(World);TC->AddToRoot();TC->ReceiverId=Slab->ReceiverId;
            Report=FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-03/routing");FParse::Value(FCommandLine::Get(),TEXT("TestSet03Report="),Report);IFileManager::Get().MakeDirectory(*Report,true);
            Cases={TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":\"SQ23456 077\"}]}"),TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"mtl_no\":\"SQ23456 077\",\"DATA_MAP\":[]}"),TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"_meta\":{\"mtl_no\":\"SQ23456 077\"}}"),TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":\"  SQ23456 077  \"}]}"),TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":\"SQ23456 077\",\"slab_len\":-10,\"frame_no\":\"invalid\"}]}"),FSlabScenarioCodec::MakeSyntheticJson()};
            // The complete input deliberately differs from the default template in size, motion and UUID.
            TSharedPtr<FJsonObject> Full;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Cases.Last()),Full);
            for(const auto& V:Full->GetArrayField(TEXT("DATA_MAP")))V->AsObject()->SetStringField(TEXT("mtl_no"),TEXT("SQ23456 077"));FJsonSerializer::Serialize(Full.ToSharedRef(),TJsonWriterFactory<>::Create(&Cases.Last()));
            FString Error;FSlabScenarioCodec::Parse(Cases.Last(),Complete,Error,true);
            InitialArchive=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>()->GetScenarios().Num();
        }
        if(Parse.IsValid())
        {
            if(!Parse.IsReady())return false;TC->ProcessStructData(Parse.Get());Parse=TFuture<TSharedPtr<FTransactionCodeDataBase>>();
        }
        if(Phase==0)
        {
            if(Service->HasItems()||Slab->HasPendingScenarioInput())return false;
            Times.Reset();Engine.Reset();Wall.Reset();Game.Reset();Duplicate=false;ProgressChecked=false;CompleteObserved=0;Measure=LastWall=Now;LastFrame=GFrameCounter;
            if(FParse::Param(FCommandLine::Get(),TEXT("TestSet03Hitch")))FVirtualSensorHitchDiagnostics::Start();
            Send(Cases[Case/2],Case%2);Phase=1;return false;
        }
        if(Phase==1)
        {
            if(CompleteObserved==0&&GFrameCounter!=LastFrame){LastFrame=GFrameCounter;Engine.Add(World->GetDeltaSeconds()*1000.);Wall.Add((Now-LastWall)*1000.);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));LastWall=Now;}
            if(Service->Items().ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Failed;})){Test->AddError(Service->Message());return Finish();}
            if(!Service->HasItems())return false;
            if(Case==0&&FParse::Param(FCommandLine::Get(),TEXT("TestSet03Capture")))
            {
                const int32 Stage=Service->IsComplete()?2:Service->Completed()>5?1:0;
                if(Stage>CaptureStage&&Stage<2&&World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->GetTestSetWidget())
                {
                    CaptureStage=Stage;const FString Path=FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-03/captures")/FString::Printf(TEXT("%d-%s.png"),Stage,Stage==0?TEXT("received"):Stage==1?TEXT("generating"):TEXT("complete"));
                    FScreenshotRequest::RequestScreenshot(Path,true,false);auto Meta=Service->StatusJson();Save(Meta,FString::Printf(TEXT("capture-%d-state.json"),Stage));
                }
            }
            if(!Duplicate)
            {
                Root=Service->Directory();FirstId=Service->Items()[0].ScenarioId;
                // Duplicate an entire different valid scenario while the template worker/capture is running.
                Test->TestTrue(TEXT("complete received trigger is consumed without replacement"),Slab->ReceiveScenario(Complete));Duplicate=true;
            }
            if(!ProgressChecked&&!Service->IsComplete())
            {
                auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Toolbar=Cast<USensorToolToolbarWidget>(Workspace->GetOwnedNavigation());
                if(AgentTestSetHasPauseControl(AgentTestSetVisibleText(Workspace->GetTestSetWidget()))&&Toolbar&&!Toolbar->GetGlobalStatus().TestSet.IsEmpty()&&!Toolbar->GetGlobalStatus().TestSet.ToString().Contains(TEXT("완료")))
                {CheckAgentTestSetPresentation(Test,World,false,Report/FString::Printf(TEXT("case-%02d-ui-progress.json"),Case));ProgressChecked=true;}
            }
            if(!Service->IsComplete())return false;
            if(CompleteObserved==0)
            {
                CompleteObserved=Now;
                if(FVirtualSensorHitchDiagnostics::IsEnabled())
                {
                    const auto H=FVirtualSensorHitchDiagnostics::Snapshot();FVirtualSensorHitchDiagnostics::Stop();
                    FFileHelper::SaveStringToFile(H.HitchCsv,*(Report/FString::Printf(TEXT("case-%02d-hitches.csv"),Case)));
                    FFileHelper::SaveStringToFile(H.SectionCsv,*(Report/FString::Printf(TEXT("case-%02d-sections.csv"),Case)));
                }
            }
            if(Now-CompleteObserved<.45)return false; // Allow the existing 5Hz presentation to reflect completion.
            Test->TestTrue(TEXT("running UI and reset-to-running controls observed"),ProgressChecked);
            CheckAgentTestSetPresentation(Test,World,true,Report/FString::Printf(TEXT("case-%02d-ui-complete.json"),Case));
            FrozenTotal=Service->TotalSeconds();
            if(Case==0&&FParse::Param(FCommandLine::Get(),TEXT("TestSet03Capture")))
            {FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-03/captures/2-complete.png"),true,false);Save(Service->StatusJson(),TEXT("capture-2-state.json"));}
            Test->TestEqual(TEXT("template target reached"),Service->Completed(),Service->Settings().Target);
            Test->TestEqual(TEXT("duplicate preserves set directory"),Service->Directory(),Root);Test->TestEqual(TEXT("duplicate preserves slot identity"),Service->Items()[0].ScenarioId,FirstId);
            Test->TestTrue(TEXT("trigger automatically opens owned UI"),World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->IsPanelOpen(ESensorToolPanelRole::TestSet));
            Test->TestEqual(TEXT("trigger bypasses ordinary archive"),World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>()->GetScenarios().Num(),InitialArchive);
            Test->TestFalse(TEXT("demo does not start live playback"),Slab->IsSimulationActive());
            TArray<FString> Variations;TSet<FString> Distinct;
            for(const auto& I:Service->Items())
            {
                Test->TestTrue(TEXT("every slot derived, including slot zero"),I.Example);Test->TestEqual(TEXT("same single-frame policy"),I.Frames,1);Variations.Add(I.VariationJson);Distinct.Add(I.VariationJson);Times.Add(I.Seconds);
                const FString Dir=I.Directory/(TEXT("run_")+I.RunId)/TEXT("scene_00000");TArray<uint8> Pcd,Labels;
                Test->TestTrue(TEXT("PCD exists"),FFileHelper::LoadFileToArray(Pcd,*(Dir/TEXT("lidar.pcd"))));Test->TestTrue(TEXT("point labels exist"),FFileHelper::LoadFileToArray(Labels,*(Dir/TEXT("lidar_labels.bin"))));
                const ANSICHAR Marker[]="DATA binary\n";int32 Offset=INDEX_NONE;
                for(int32 N=0;N+sizeof(Marker)-1<=Pcd.Num();++N)if(FMemory::Memcmp(Pcd.GetData()+N,Marker,sizeof(Marker)-1)==0){Offset=N+sizeof(Marker)-1;break;}
                Test->TestTrue(TEXT("binary header located"),Offset!=INDEX_NONE);
                if(Offset!=INDEX_NONE){Test->TestEqual(TEXT("explicit 33B point record"),(Pcd.Num()-Offset)%33,0);Test->TestEqual(TEXT("5B point label records"),Labels.Num(),(Pcd.Num()-Offset)/33*5);}
            }
            Test->TestEqual(TEXT("all scenario variations distinct"),Distinct.Num(),Service->Settings().Target);
            if(Reference.IsEmpty())Reference=Variations;else Test->TestTrue(TEXT("all message shapes/routes/reset cycles use identical template variations"),Reference==Variations);
            const auto Status=Service->StatusJson();Test->TestEqual(TEXT("no received source slots"),int32(Status->GetNumberField(TEXT("received_scenarios"))),0);Test->TestEqual(TEXT("no replacement reservations"),int32(Status->GetNumberField(TEXT("pending_received_replacements"))),0);
            auto Timing=MakeShared<FJsonObject>();Timing->SetNumberField(TEXT("case"),Case);Timing->SetStringField(TEXT("route"),Case%2?TEXT("DTCore TC worker -> Process -> DataSync -> Slab"):TEXT("SubmitScenarioJson -> DataSync -> Slab"));Timing->SetNumberField(TEXT("seconds"),CompleteObserved-Measure);Timing->SetNumberField(TEXT("count"),Service->Completed());Times.Sort();Timing->SetNumberField(TEXT("item_max_seconds"),Times.Last());
            for(auto Pair:{TPair<const TCHAR*,TArray<double>*>(TEXT("engine"),&Engine),{TEXT("callback_wall"),&Wall},{TEXT("game_thread"),&Game}})
            {int32 Hitches=0;double Max=0;for(double Ms:*Pair.Value){Hitches+=Ms>150;Max=FMath::Max(Max,Ms);}Timing->SetNumberField(FString(Pair.Key)+TEXT("_max_ms"),Max);Timing->SetNumberField(FString(Pair.Key)+TEXT("_over_150ms"),Hitches);Test->TestEqual(TEXT("150ms budget unchanged"),Hitches,0);}
            Save(Timing,FString::Printf(TEXT("case-%02d-timing.json"),Case));Save(Status,FString::Printf(TEXT("case-%02d-complete.json"),Case));
            if(FParse::Param(FCommandLine::Get(),TEXT("TestSet03Hold"))){Phase=4;return false;}
            Send(Cases[0],1-Case%2);DuplicateSent=Now;Phase=2;return false;
        }
        if(Phase==2)
        {
            if(Slab->HasPendingScenarioInput()||Now-DuplicateSent<.3)return false;
            Test->TestEqual(TEXT("repeat after completion retains set"),Service->Directory(),Root);Test->TestEqual(TEXT("repeat after completion retains slots"),Service->Items()[0].ScenarioId,FirstId);
            Test->TestTrue(TEXT("repeat explains retained set"),Service->Message().Contains(TEXT("기존 테스트셋 유지")));
            Test->TestEqual(TEXT("duplicate and post-completion wait do not advance total"),Service->TotalSeconds(),FrozenTotal);
            Test->TestTrue(TEXT("duplicate history retained in status/manifest"),Service->StatusJson()->GetNumberField(TEXT("duplicate_triggers"))>=2);
            CheckAgentTestSetPresentation(Test,World,true,Report/FString::Printf(TEXT("case-%02d-ui-duplicate.json"),Case));
            FString E;Service->ResetConfirmed(E);Phase=3;return false;
        }
        if(Phase==3&&!Service->HasItems()){if(++Case<Cases.Num()*2){Phase=0;return false;}return Finish();}
        if(Phase==4)
        {
            if(FPaths::FileExists(Report/TEXT("finish.flag")))return Finish();
            if(Now-LastStatus>.2){Save(Service->StatusJson(),TEXT("manual-state.json"));LastStatus=Now;}
        }
        return false;
    }
private:
    void Send(const FString& Json,int32 Route)
    {if(Route){auto* Target=TC;Parse=Async(EAsyncExecution::ThreadPool,[Target,Json](){return Target->ParseToStruct(Json);});}else Test->TestTrue(TEXT("Submit accepts asynchronous input"),Slab->SubmitScenarioJson(Json));}
    void Save(const TSharedRef<FJsonObject>& O,const FString& Name)
    {
        if(auto* View=World->GetGameInstance()->GetGameViewportClient();View&&View->Viewport)
        {
            const auto Size=View->Viewport->GetSizeXY();O->SetNumberField(TEXT("client_width"),Size.X);O->SetNumberField(TEXT("client_height"),Size.Y);
            if(auto Window=FSlateApplication::Get().FindWidgetWindow(View->GetGameViewportWidget().ToSharedRef()))O->SetNumberField(TEXT("window_dpi_scale"),Window->GetDPIScaleFactor());
            O->SetNumberField(TEXT("owned_font_scale"),World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->GetOwnedPanelFontScale());
        }
        FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));FFileHelper::SaveStringToFile(S,*(Report/Name));
    }
    bool Finish(){if(FParse::Param(FCommandLine::Get(),TEXT("TestSet03Hitch")))FVirtualSensorHitchDiagnostics::Stop();if(Parse.IsValid())Parse.Wait();if(TC)TC->RemoveFromRoot();if(Service){FString E;Service->ResetConfirmed(E);}return true;}
    FAutomationTestBase* Test;UWorld* World=nullptr;ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;UAgentTestSetSubsystem* Service=nullptr;UFactoryAgentScenarioTC* TC=nullptr;
    TFuture<TSharedPtr<FTransactionCodeDataBase>> Parse;TArray<FString> Cases,Reference;TArray<double> Times,Engine,Wall,Game;
    FSlabScenarioDataPtr Complete;
    int32 Phase=0,Case=0,InitialArchive=0,CaptureStage=-1;bool Duplicate=false,ProgressChecked=false;uint64 LastFrame=0;double Started=FPlatformTime::Seconds(),Observed=0,Measure=0,LastWall=0,LastStatus=0,CompleteObserved=0,FrozenTotal=0,DuplicateSent=0;FString Report,Root,FirstId;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentTriggerRouting,"MA0T10.TestSet.TriggerRoutingLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentTriggerRouting::RunTest(const FString&)
{
    if(GUsingNullRHI){AddInfo(TEXT("[OPT-IN SKIP] actual RHI required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FTriggerRoutingRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
