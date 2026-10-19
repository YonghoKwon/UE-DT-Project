#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualSensorSlabContextSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPeriodicFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHitchDiagnostics.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/FactoryAgentScenarioTC.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "AgentTestSetPresentationChecks.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EngineGlobals.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericWindow.h"

namespace
{
// Baseline mode records the old defect without calling it a successful generation.
class FCompanyTestSetRun final : public IAutomationLatentCommand
{
public:
    explicit FCompanyTestSetRun(FAutomationTestBase* T):Test(T),Baseline(FParse::Param(FCommandLine::Get(),TEXT("TestSet04Baseline"))),CommitBaseline(FParse::Param(FCommandLine::Get(),TEXT("TestSet04CommitBaseline"))){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>150){Test->AddError(TEXT("company environment timeout"));return Finish();}
        if(!Service)
        {
            for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
            if(!World||!FinalAcceptanceViewportReady(World)||Now-Started<4)return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){It->StopSimulation();if(!Slab)Slab=*It;}
            for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It)if(It->GetSensorId()==TEXT("SLAB-LIDAR-001"))Lidar=*It;
            if(!Slab||!Lidar){Test->AddError(TEXT("Slab/LiDAR fixture missing"));return Finish();}
            Other=World->SpawnActor<AVirtualLidarSensorActor>();Other->StopSensor();Other->ScanComponent->SensorId=TEXT("ZZ-COMPANY-LIDAR");
            // A deployed second sensor observes the scene during warmup. An unplaced
            // sensor inside the floor first saw geometry only after the test moved Slab,
            // deferring its renderer's cold initialization into the measured interval.
            Other->SetActorTransform(Lidar->GetActorTransform());Other->AddActorWorldOffset(FVector(100,0,0));
            Other->ScanComponent->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Other->ScanComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Other->ScanComponent->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;
            Camera=World->SpawnActor<AVirtualCameraSensorActor>();Camera->StopSensor();
            Camera->CaptureComponent->SensorId=TEXT("ZZ-COMPANY-CAMERA");
            auto* Scan=Lidar->ScanComponent.Get();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->bEnableRangeNoise=false;
            Service=World->GetSubsystem<UAgentTestSetSubsystem>();Periodic=World->GetSubsystem<UVirtualSensorPeriodicFileSaveSubsystem>();Publisher=Lidar->OutputComponent->StreamPublisherComponent;
            if(FParse::Param(FCommandLine::Get(),TEXT("TestSet04Broker")))
            {
                auto* T=Lidar->OutputComponent->TransportComponent.Get();auto P=T->GetTransportProfile();P.BrokerUrl=TEXT("tcp://127.0.0.1:61616");P.UserName=TEXT("guest");T->ConfigureTransportProfile(P);T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
                T->SetSessionCredentials(FPlatformMisc::GetEnvironmentVariable(TEXT("TESTSET04_BROKER_PASSWORD")),FString());
                auto Stream=Publisher->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());Stream.TransportBackend=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;Stream.SensorId=Lidar->GetSensorId();Stream.StreamKind=EVirtualSensorStreamKind::PointCloud;Stream.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;Stream.PcdDataMode=EVirtualPcdDataMode::Binary;Publisher->ConfigureStream(Stream);
            }
            if(FParse::Param(FCommandLine::Get(),TEXT("TestSet04NoCommit")))World->GetSubsystem<UVirtualDatasetSubsystem>()->ProjectCommitForTests=FString();
            Report=FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-04")/(Baseline?TEXT("baseline-data"):TEXT("company-data"));FParse::Value(FCommandLine::Get(),TEXT("TestSet04Report="),Report);IFileManager::Get().MakeDirectory(*Report,true);
            // Exercise an ordinary scenario immediately before the trigger as requested.
            // This also establishes real scene geometry before the ten-second warmup.
            Slab->SetSensorOutputs(FParse::Param(FCommandLine::Get(),TEXT("TestSet04Broker"))?FVirtualSlabSensorOutputSelection():FVirtualSlabSensorOutputSelection::ObservationOnly());FString Ordinary;TSharedPtr<FJsonObject> O;
            FFileHelper::LoadFileToString(Ordinary,*(FPaths::ProjectDir()/TEXT("Config/AgentTestSetScenario.json")));FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Ordinary),O);
            for(auto& V:O->GetArrayField(TEXT("DATA_MAP")))V->AsObject()->SetStringField(TEXT("mtl_no"),TEXT("1234"));
            O->GetObjectField(TEXT("_meta"))->SetStringField(TEXT("UUID"),FGuid::NewGuid().ToString());FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Ordinary));
            Test->TestTrue(TEXT("ordinary scenario admitted before company trigger"),Slab->SubmitScenarioJson(Ordinary));At=Now;Phase=6;return false;
        }
        if(Phase==6)
        {if(!Slab->IsSimulationActive()||Now-At<1)return false;Slab->StopSimulation();Phase=7;return false;}
        if(Phase==7)
        {
            if(Slab->HasPendingScenarioInput()||World->GetSubsystem<UVirtualSensorSlabContextSubsystem>()->GetSlabSensorSessionStatus().State==EVirtualSlabSessionState::Draining)return false;
            Other->SetActorLocation(Slab->GetActorLocation()+FVector(0,-1000,800));Other->SetActorRotation((Slab->GetActorLocation()-Other->GetActorLocation()).Rotation());Begin();return false;
        }
        if(Phase==0)
        {
            if(Now-At<10)return false;
            BeginTrigger();Phase=1;return false;
        }
        if(Phase==1)
        {
            if(GFrameCounter!=LastFrame)
            {LastFrame=GFrameCounter;Engine.Add(World->GetDeltaSeconds()*1000.);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Wall.Add((Now-LastWall)*1000.);LastWall=Now;
             if(Engine.Last()>150||Game.Last()>150||Wall.Last()>150)Test->AddInfo(FString::Printf(TEXT("Company hitch: t=%.3f completed=%d engine=%.3f gt=%.3f wall=%.3f message=%s"),Now-At,Service->Completed(),Engine.Last(),Game.Last(),Wall.Last(),*Service->Message()));}
            if(CommitBaseline)
            {
                if(Service->Items().Num()!=100||Service->Items().ContainsByPredicate([](const auto& I){return I.State!=EAgentTestSetItemState::Failed;}))return false;
                Save(TEXT("commit-baseline-failed"));Test->TestEqual(TEXT("missing build SHA and runtime Git reproduces dataset finalization failure"),Service->Completed(),0);return Finish();
            }
            if(Baseline&&Now-At>2)
            {
                Save(TEXT("observed"));
                if(Case==0){Test->TestTrue(TEXT("baseline reproduces permanent busy wait"),Service->HasBusyWork()&&Service->Completed()==0);FString E;Service->StopBusyWorkAndStart(E);Phase=2;}
                else {Test->TestTrue(TEXT("baseline reproduces missing multi-LiDAR selection"),Service->Items().ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Failed;}));FString E;Service->Retry(0,E);Phase=3;At=Now;}
                return false;
            }
            if(!Baseline&&Service->Items().ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Failed;})){Save(TEXT("failure"));Test->AddError(Service->Message());return Finish();}
            if(!Service->IsComplete())return false;Phase=2;At=Now;
        }
        if(Phase==2)
        {
            if(!Service->IsComplete())return false;
            if(!CompletionSaved)
            {
                CompletionSaved=true;Save(TEXT("complete"));auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("seconds"),Service->TotalSeconds());
                if(FVirtualSensorHitchDiagnostics::IsEnabled())
                {const auto H=FVirtualSensorHitchDiagnostics::Snapshot();FVirtualSensorHitchDiagnostics::Stop();FFileHelper::SaveStringToFile(H.HitchCsv,*(Report/FString::Printf(TEXT("case-%d-hitches.csv"),Case)));FFileHelper::SaveStringToFile(H.SectionCsv,*(Report/FString::Printf(TEXT("case-%d-sections.csv"),Case)));O->SetNumberField(TEXT("max_mutex_hold_ms"),H.MaxOutstandingHoldMs);}
                for(auto P:{TPair<const TCHAR*,TArray<double>*>(TEXT("engine"),&Engine),{TEXT("game_thread"),&Game},{TEXT("callback_wall"),&Wall}})
                {double Max=0;int32 Over=0;for(double V:*P.Value){Max=FMath::Max(Max,V);Over+=V>150;}O->SetNumberField(FString(P.Key)+TEXT("_max_ms"),Max);O->SetNumberField(FString(P.Key)+TEXT("_over_150ms"),Over);if(!Baseline)Test->TestEqual(TEXT("company generation 150ms budget"),Over,0);}
                Write(O,TEXT("timing"));At=Now;return false;
            }
            if(Now-At<.5)return false;
            if(Case==0&&FParse::Param(FCommandLine::Get(),TEXT("TestSet04CompanyHold"))&&!FPaths::FileExists(Report/TEXT("finish.flag")))return false;
            Test->TestTrue(TEXT("measurement restored"),Lidar->IsSensorRunning());
            Test->TestTrue(TEXT("PCD intent and automatic resume preserved"),Publisher->IsStreamActiveOrAwaitingResume(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId())&&Publisher->bAutoResumePointCloudAfterOverload);
            Test->TestTrue(TEXT("same periodic session restored"),Periodic->GetPeriodicStatus(PeriodicId).bActive&&Periodic->GetPeriodicStatus(PeriodicId).Folder==PeriodicFolder);
            Test->TestTrue(TEXT("unselected LiDAR kept running"),Other->IsSensorRunning());
            Test->TestTrue(TEXT("unselected Camera kept running"),Camera->IsSensorRunning());
            Save(TEXT("restored"));Phase=4;
        }
        if(Phase==3){if(Now-At<1)return false;Save(TEXT("retry-observed"));Phase=4;}
        if(Phase==4){StopWork();FString E;Service->ResetConfirmed(E);Phase=5;}
        if(Phase==5&&!Service->HasItems()){if(++Case<2){Begin();return false;}return Finish();}
        return false;
    }
private:
    void Begin()
    {
        FAgentTestSetSettings C;C.Target=100;if(Case==0)C.LidarId=Lidar->GetSensorId();Service->ConfigureForTests(C);
        Lidar->StartSensor();Other->StartSensor();Camera->StartSensor();Publisher->bAutoResumePointCloudAfterOverload=true;Publisher->StartStream(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());
        FVirtualSensorCaptureSelection Selection;Selection.bPointCloud=true;Selection.bLidarPayload=false;Selection.PointCloudFormat=EVirtualSensorExportKind::PointCloudPcd;Selection.IntervalSeconds=.2f;PeriodicId=Periodic->StartPeriodicSave(Lidar,Selection);PeriodicFolder=Periodic->GetPeriodicStatus(PeriodicId).Folder;
        CompletionSaved=false;Phase=0;At=FPlatformTime::Seconds();
    }
    void BeginTrigger()
    {
        Test->TestTrue(TEXT("background FullSpec sensor observes geometry during ten-second preparation"),Other->ScanComponent->GetLastHitPointCount()>0);
        if(FParse::Param(FCommandLine::Get(),TEXT("TestSet04Hitch")))FVirtualSensorHitchDiagnostics::Start();
        auto* TC=NewObject<UFactoryAgentScenarioTC>(World);TC->ReceiverId=Slab->ReceiverId;
        FString Json=TEXT("{\"MESSAGE_ID\":\"IFactory-agent\",\"DATA_MAP\":[{\"mtl_no\":\"SQ23456 077\"}]}");
        if(Case==1)Test->TestTrue(TEXT("complete scenario accompanies the second company trigger"),FFileHelper::LoadFileToString(Json,*(FPaths::ProjectDir()/TEXT("Config/AgentTestSetScenario.json"))));
        TC->ProcessStructData(TC->ParseToStruct(Json));
        At=LastWall=FPlatformTime::Seconds();Engine.Reset();Game.Reset();Wall.Reset();
    }
    void Write(const TSharedRef<FJsonObject>& O,const TCHAR* Name){FString J;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&J));FFileHelper::SaveStringToFile(J,*(Report/FString::Printf(TEXT("case-%d-%s.json"),Case,Name)),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);}
    void Save(const TCHAR* Name)
    {
        auto O=Service->StatusJson();TArray<TSharedPtr<FJsonValue>> Streams;
        for(const auto& S:World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->GetStreamTelemetry())
        {auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("sensor_id"),S.SensorId);V->SetNumberField(TEXT("submitted"),S.SubmittedCount);V->SetNumberField(TEXT("receipt"),S.ReceiptCount);Streams.Add(MakeShared<FJsonValueObject>(V));}
        O->SetArrayField(TEXT("transport_streams"),Streams);Write(O,Name);
    }
    void StopWork(){Lidar->StopSensor();Other->StopSensor();Camera->StopSensor();Publisher->StopStream(EVirtualSensorStreamKind::PointCloud,Lidar->GetSensorId());Periodic->StopPeriodicSave(PeriodicId);}
    bool Finish(){if(FVirtualSensorHitchDiagnostics::IsEnabled())FVirtualSensorHitchDiagnostics::Stop();if(Service){StopWork();FString E;Service->ResetConfirmed(E);}return true;}
    FAutomationTestBase* Test;UWorld* World=nullptr;UAgentTestSetSubsystem* Service=nullptr;ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;AVirtualLidarSensorActor* Other=nullptr;AVirtualCameraSensorActor* Camera=nullptr;UVirtualSensorPeriodicFileSaveSubsystem* Periodic=nullptr;UVirtualSensorStreamPublisherComponent* Publisher=nullptr;
    FString Report,PeriodicId,PeriodicFolder;bool Baseline=false,CommitBaseline=false,CompletionSaved=false;int32 Phase=0,Case=0;double Started=FPlatformTime::Seconds(),At=0,LastWall=0;uint64 LastFrame=0;TArray<double> Engine,Game,Wall;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentCompanyEnvironment,"MA0T10.TestSet.CompanyEnvironment",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentCompanyEnvironment::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCompanyTestSetRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

namespace
{
class FCompanyRecoveryRun final : public IAutomationLatentCommand
{
public:
    explicit FCompanyRecoveryRun(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>180){Test->AddError(TEXT("company recovery timeout"));return Finish();}
        if(!Service)
        {
            for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
            if(!World||!FinalAcceptanceViewportReady(World)||Now-Started<4)return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){It->StopSimulation();if(!Slab)Slab=*It;}
            for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It)if(It->GetSensorId()==TEXT("SLAB-LIDAR-001"))Lidar=*It;
            Service=World->GetSubsystem<UAgentTestSetSubsystem>();Dataset=World->GetSubsystem<UVirtualDatasetSubsystem>();Dataset->ProjectCommitForTests=FString();
            Report=FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-04/recovery-data");FParse::Value(FCommandLine::Get(),TEXT("TestSet04RecoveryReport="),Report);IFileManager::Get().MakeDirectory(*Report,true);
            Manual=FParse::Param(FCommandLine::Get(),TEXT("TestSet04Manual"));if(Manual)World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->SetOwnedPanelFontScale(FParse::Param(FCommandLine::Get(),TEXT("TestSet04Font150"))?1.5f:1.f);Begin();return false;
        }
        if(Phase==0)
        {
            if(Service->Items().IsEmpty())return false;
            if(Case==4)
            {if(!Service->HasManifestFailure()||Service->Completed()!=1)return false;}
            else if(Service->Items()[0].State!=EAgentTestSetItemState::Failed)return false;
            if(FailedAt==0){FailedAt=Now;return false;}if(Now-FailedAt<.4)return false;
            Save(TEXT("failed"));const auto Message=Case==4?Service->Message():Service->Items()[0].Message;
            const FString Expected=Case==0?TEXT("설정 센서 ID"):Case==1?TEXT("템플릿"):Case==2?TEXT("pending 쓰기 실패"):Case==3?TEXT("최종 이동 실패"):TEXT("testset.json 저장 실패");
            Test->TestTrue(TEXT("failure identifies the concrete cause"),Message.Contains(Expected));
            auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();Workspace->OpenTestSet();
            const auto Text=AgentTestSetVisibleText(Workspace->GetTestSetWidget());
            Test->TestTrue(TEXT("failure reason persists in visible Slate after refresh"),Text.ContainsByPredicate([&](const FString& V){return V.Contains(Expected);}));
            FString Error;
            if(Case==0)
            {
                Test->TestFalse(TEXT("unchanged missing sensor retry immediately rejects"),Service->Retry(0,Error));Test->TestTrue(TEXT("rejection includes configured ID"),Error.Contains(TEXT("COMPANY-RECOVERY")));
                const FString Id=Service->Items()[0].RunId;Service->EnqueueDemoTrigger(Service->Settings().Trigger,Slab,Error);Test->TestEqual(TEXT("failed set duplicate preserves run"),Service->Items()[0].RunId,Id);
                Lidar->ScanComponent->SensorId=TEXT("COMPANY-RECOVERY");
            }
            if(Case==1){FString Json;FFileHelper::LoadFileToString(Json,*(FPaths::ProjectDir()/TEXT("Config/AgentTestSetScenario.json")));FFileHelper::SaveStringToFile(Json,*Template);}
            if(Case==2||Case==3||Case==4)Test->TestTrue(TEXT("remove only the fixture-owned empty blocker directory"),IFileManager::Get().DeleteDirectory(*Blocker,false,false));
            OldRun=Service->Items()[0].RunId;
            if(Manual&&Case==0){FFileHelper::SaveStringToFile(TEXT("Cause removed; click retry"),*(Report/TEXT("ready.flag")));Phase=1;return false;}
            Test->TestTrue(TEXT("retry accepted after concrete cause removed"),Service->Retry(Case==4?INDEX_NONE:0,Error));Phase=1;return false;
        }
        if(Phase==1)
        {
            if(!Service->IsComplete())return false;
            if(Manual&&Case==0&&!FPaths::FileExists(Report/TEXT("finish.flag")))
            {if(!ManualCompletionSaved){Save(TEXT("manual-complete"));ManualCompletionSaved=true;}return false;}
            Save(TEXT("recovered"));
            if(Case==4)Test->TestEqual(TEXT("manifest retry preserves successful capture run"),Service->Items()[0].RunId,OldRun);
            else Test->TestNotEqual(TEXT("item retry uses a fresh run"),Service->Items()[0].RunId,OldRun);
            Dataset->OnSceneSnapshotForTests=nullptr;FString Error;Service->ResetConfirmed(Error);Phase=2;return false;
        }
        if(Phase==2&&!Service->HasItems())
        {
            Lidar->ScanComponent->SensorId=TEXT("SLAB-LIDAR-001");if(++Case<5){Begin();return false;}
            AVirtualCameraSensorActor* Camera=nullptr;for(TActorIterator<AVirtualCameraSensorActor> It(World);It;++It){Camera=*It;break;}
            FVirtualDatasetSettings C;C.Enabled=true;C.Scenes=1;C.Name=TEXT("TESTSET04_unknown_commit");C.CameraTruth=true;C.LidarFidelity=true;FString E;
            if(!Test->TestTrue(TEXT("ordinary DATASET-01 admits missing build metadata"),Dataset->Start(C,Slab,Lidar,Camera,nullptr,E))){Test->AddError(E);return Finish();}Phase=7;
        }
        if(Phase==7&&!Dataset->IsActive())
        {
            Test->TestTrue(TEXT("ordinary DATASET-01 succeeds without Git"),Dataset->HasSucceeded());
            FString Json;TSharedPtr<FJsonObject> Manifest;FFileHelper::LoadFileToString(Json,*(Dataset->Directory()/TEXT("dataset.json")));FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Manifest);
            if(Test->TestTrue(TEXT("ordinary manifest exists"),bool(Manifest)))
            {Test->TestEqual(TEXT("missing commit explicitly unknown"),Manifest->GetStringField(TEXT("project_commit")),FString(TEXT("unknown")));Test->TestEqual(TEXT("missing provenance explicitly unavailable"),Manifest->GetStringField(TEXT("project_commit_source")),FString(TEXT("unavailable")));}
            FFileHelper::SaveStringToFile(Dataset->Directory(),*(Report/TEXT("ordinary-dataset-path.txt")));return Finish();
        }
        return false;
    }
private:
    void Begin()
    {
        FAgentTestSetSettings C;C.Target=1;C.LidarId=Case==0?TEXT("COMPANY-RECOVERY"):Lidar->GetSensorId();
        Template=Report/TEXT("retry-template.json");if(Case==1){IFileManager::Get().Delete(*Template);C.TemplatePath=Template;}
        Test->TestTrue(TEXT("fresh recovery configuration"),Service->ConfigureForTests(C));
        Injected=false;Blocker.Reset();Dataset->OnSceneSnapshotForTests=[this](const FJsonObject&)
        {if(!Injected&&(Case==2||Case==3)){Injected=true;Blocker=Dataset->Directory()/(Case==2?TEXT("dataset.json.pending"):TEXT("dataset.json"));IFileManager::Get().MakeDirectory(*Blocker,true);}};
        FString E;Test->TestTrue(TEXT("recovery trigger admitted"),Service->EnqueueDemoTrigger(C.Trigger,Slab,E));
        World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->OpenTestSet();
        if(Case==4){Blocker=Service->Directory()/TEXT("testset.json");IFileManager::Get().MakeDirectory(*Blocker,true);}
        FailedAt=0;Phase=0;
    }
    void Save(const TCHAR* Name)
    {
        auto O=Service->StatusJson();const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();O->SetNumberField(TEXT("client_width"),Size.X);O->SetNumberField(TEXT("client_height"),Size.Y);O->SetNumberField(TEXT("font_scale"),World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->GetOwnedPanelFontScale());
        if(auto W=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef()))if(W->GetNativeWindow())O->SetNumberField(TEXT("window_dpi_scale"),W->GetNativeWindow()->GetDPIScaleFactor());
        FString Json;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Report/FString::Printf(TEXT("case-%d-%s.json"),Case,Name)),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
    bool Finish(){if(Dataset)Dataset->OnSceneSnapshotForTests=nullptr;if(Service){FString E;Service->ResetConfirmed(E);}return true;}
    FAutomationTestBase* Test;UWorld* World=nullptr;UAgentTestSetSubsystem* Service=nullptr;UVirtualDatasetSubsystem* Dataset=nullptr;ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;
    bool Injected=false,Manual=false,ManualCompletionSaved=false;int32 Case=0,Phase=0;double Started=FPlatformTime::Seconds(),FailedAt=0;FString Report,Blocker,Template,OldRun;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentCompanyRecovery,"MA0T10.TestSet.CompanyFailureRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentCompanyRecovery::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCompanyRecoveryRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
