#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabScenarioReplaySubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationControlSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMemory.h"
#include "Json.h"
#include "RHIGlobals.h"
#include "EngineGlobals.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

namespace
{
FSlabScenarioDataPtr Fixture(int32 Rows,const FString& Mtl)
{
    const FString Uuid=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FSlabScenarioCodec::MakeSyntheticJson(Uuid)),Root);
    auto Values=Root->GetArrayField(TEXT("DATA_MAP"));Values.SetNum(Rows);Root->GetObjectField(TEXT("_meta"))->SetNumberField(TEXT("duration_sec"),Rows*.05);
    for(int32 I=0;I<Rows;++I){auto V=Values[I]->AsObject();V->SetStringField(TEXT("mtl_no"),Mtl);V->SetNumberField(TEXT("frame_no"),100+I*2);V->SetNumberField(TEXT("center_x"),I*3);V->SetNumberField(TEXT("left_skew_angle"),I*2-3);V->SetNumberField(TEXT("slab_len"),800);V->SetNumberField(TEXT("slab_wth"),200);V->SetNumberField(TEXT("slab_thk"),50);}
    Root->SetArrayField(TEXT("DATA_MAP"),Values);FString Json,Error;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));FSlabScenarioDataPtr Data;FSlabScenarioCodec::Parse(Json,Data,Error,false);return Data;
}
class FTestSetRun final:public IAutomationLatentCommand
{
public:
    FTestSetRun(FAutomationTestBase* In,bool Benchmark,bool InPipeline):Test(In),Large(Benchmark),Pipeline(InPipeline),Manual(FParse::Param(FCommandLine::Get(),TEXT("TestSetManual"))){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>600){Test->AddError(TEXT("TESTSET-01 timeout"));return Finish();}
        if(FPaths::FileExists(FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-01/stop.flag"))){Test->AddError(TEXT("TESTSET-01 fixture cancelled by owner"));return Finish();}
        if(!Service)
        {
            UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){Candidate=C.World();break;}
            if(World!=Candidate){World=Candidate;Observed=Now;return false;}
            if(!World)return false;
        }
        if(!Service)
        {
            if(Now-Observed<3||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()))return false;
            for(TActorIterator<ASlabActor> It(World);It;++It)It->StopSimulation();for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            Slab=World->SpawnActorDeferred<ASlabActor>(ASlabActor::StaticClass(),FTransform(FVector(1500,0,200)));Slab->ReceiverId=TEXT("TESTSET-Fixture");Slab->Tags.Add(TEXT("Slab"));Slab->InitialDimensions=FVector(800,200,50);Slab->FinishSpawning(FTransform(FVector(1500,0,200)));Slab->SetDiagnosticHelpersVisible(false);Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
            Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Lidar->StopSensor();Lidar->SetActorLocation(FVector(1500,-1000,800));Lidar->SetActorRotation((FVector(1500,0,225)-Lidar->GetActorLocation()).Rotation());
            auto* Scan=Lidar->ScanComponent.Get();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Scan->HorizontalSamples=512;Scan->VerticalChannels=32;Scan->OutputMode=EVirtualLidarOutputMode::None;Scan->bEnableRangeNoise=false;Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->SensorId=Large?TEXT("TESTSET-LIDAR"):TEXT("UNAVAILABLE");
            Service=World->GetSubsystem<UAgentTestSetSubsystem>();Dataset=World->GetSubsystem<UVirtualDatasetSubsystem>();Catalog=World->GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();Catalog->RegisterPlaybackAdapter(Slab);
            Dataset->OnSceneSnapshotForTests=[this](const FJsonObject&){Test->TestFalse(TEXT("generation captures direct poses without real-time playback"),Slab->IsSimulationActive());};
            FAgentTestSetSettings Config;Config.Target=Large?100:3;Config.LidarId=TEXT("TESTSET-LIDAR");Config.Pipeline=Pipeline;Test->TestTrue(TEXT("fixture config before admission"),Service->ConfigureForTests(Config));
            Test->TestFalse(TEXT("default state has no set or output"),Service->HasItems());Test->TestFalse(TEXT("preview requires target completion"),Service->SelectPreview(0,Error));
            Source=Fixture(Large?3:20,TEXT(" 9999 "));Test->TestTrue(TEXT("trigger whitespace normalized"),Service->IsTrigger(*Source));
            Test->TestTrue(TEXT("submit through production async parser"),Slab->SubmitScenarioJson(Source->OriginalJson));Injected=1;Phase=Large?2:0;MeasureStart=Now;MemoryStart=FPlatformMemory::GetStats().UsedPhysical;return false;
        }
        if(GFrameCounter!=LastFrame){LastFrame=GFrameCounter;const double Ms=World->GetDeltaSeconds()*1000.;Frames.Add(Ms);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));PeakMemory=FMath::Max(PeakMemory,FPlatformMemory::GetStats().UsedPhysical);}
        if(Phase==0)
        {
            if(Service->Items().IsEmpty()||Service->Items()[0].State!=EAgentTestSetItemState::Failed)return false;
            Test->TestFalse(TEXT("trigger never starts live simulation"),Slab->IsSimulationActive());Test->TestEqual(TEXT("trigger does not enter normal archive"),Catalog->GetScenarios().Num(),0);
            Service->SetPaused(true);Lidar->ScanComponent->SensorId=TEXT("TESTSET-LIDAR");const auto Run=Service->Items()[0].RunId;
            Test->TestTrue(TEXT("retry accepted after missing sensor fixed"),Service->Retry(0,Error));Test->TestNotEqual(TEXT("retry has a new generation UUID"),Run,Service->Items()[0].RunId);
            Test->TestFalse(TEXT("duplicate trigger is rejected"),Slab->ReceiveScenario(Source));
            PauseFrame=Lidar->ScanComponent->GetRuntimeStatus().FrameId;PauseAt=Now;Phase=1;return false;
        }
        if(Phase==1)
        {
            if(Now-PauseAt<.3)return false;Test->TestEqual(TEXT("paused generator does not acquire"),Lidar->ScanComponent->GetRuntimeStatus().FrameId,PauseFrame);Service->SetPaused(false);Phase=2;
        }
        if(Phase==2)
        {
            if(Service->Items().ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Failed;}))
            {Test->AddError(TEXT("generation failed: ")+Service->Message());return Finish();}
            if(Injected<Service->Settings().Target)
            {auto Input=Fixture(2+Injected%4,TEXT("9999"));if(Slab->SubmitScenarioJson(Input->OriginalJson))++Injected;}
            const int32 Total=Service->Completed();
            const bool Insert=Large?(NormalCount==0&&Total>=20)||(NormalCount==1&&Total>=60):(NormalCount<2&&Dataset->CompletedScenes()>=1&&Dataset->IsActive()&&!Slab->IsSimulationActive()&&Now-NormalAt>.6);
            if(Insert&& !Catalog->IsReplayBusy())
            {
                const auto Input=Fixture(6,TEXT("NORMAL-MATERIAL"));Test->TestTrue(TEXT("normal scenario admitted while dataset holds World"),Slab->ReceiveScenario(Input));
                Test->TestTrue(TEXT("normal scenario plays immediately"),Slab->IsSimulationActive());Test->TestTrue(TEXT("test set yields before ordinary pose application"),Service->IsPaused());++NormalCount;NormalAt=Now;
            }
            if(!Service->IsComplete())return false;
            Test->TestEqual(TEXT("exact target count reached"),Service->Completed(),Service->Settings().Target);Test->TestEqual(TEXT("two ordinary scenarios kept separate"),Catalog->GetScenarios().Num(),2);
            Test->TestFalse(TEXT("full set rejects excess trigger"),Slab->ReceiveScenario(Fixture(2,TEXT("9999"))));
            Test->TestEqual(TEXT("ordinary scenarios were both run"),NormalCount,2);CheckFiles();Record(Now);PreviewFrame=Lidar->ScanComponent->GetRuntimeStatus().FrameId;Phase=3;return false;
        }
        if(Phase==3)
        {
            if(Manual)
            {if(!FPaths::FileExists(FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-01/manual/finish.flag")))return false;Service->StopPreview();Test->AddInfo(TEXT("Manual fixture completed by finish.flag; direct acceptance is recorded separately"));return Finish();}
            if(!Service->IsComplete())return false;
            if(PreviewCount<3)
            {
                if(!PreviewRequested&&!Catalog->IsReplayBusy())
                {Test->TestTrue(TEXT("completed selection requests existing replay"),Service->SelectPreview(PreviewCount,Error));PreviewRequested=true;return false;}
                if(PreviewRequested&&Catalog->GetReplayStatus().State==ESlabScenarioReplayState::Playing)
                {
                    const auto Status=Catalog->GetReplayStatus();Test->TestFalse(TEXT("selected playback is observation only"),Status.Outputs.HasAnyOutput());PreviewRuns.Add(Status.RunUUID);
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-01")/FString::Printf(TEXT("automated-preview-%d.png"),PreviewCount+1),true,false);
                    ++PreviewCount;PreviewRequested=false;Service->StopPreview();return false;
                }
                return false;
            }
            if(Catalog->IsReplayBusy())return false;
            Test->TestEqual(TEXT("selected scenarios each create independent replay runs"),PreviewRuns.Num(),3);
            Test->TestEqual(TEXT("preview never acquires or sends dataset PCD"),Lidar->ScanComponent->GetRuntimeStatus().FrameId,PreviewFrame);Test->TestEqual(TEXT("preview preserves completed test set"),Service->Completed(),Service->Settings().Target);
            Test->TestEqual(TEXT("preview does not evict normal archives"),Catalog->GetScenarios().Num(),2);
            int32 Code=0;const auto Api=World->GetSubsystem<UAutomationControlSubsystem>()->Dispatch(TEXT("GET"),TEXT("/v1/testset"),nullptr,Code);Test->TestEqual(TEXT("read-only status API"),Code,200);Test->TestEqual(TEXT("API completed count"),int32(Api->GetNumberField(TEXT("completed_scenarios"))),Service->Settings().Target);
            Test->TestTrue(TEXT("confirmed reset accepted"),Service->ResetConfirmed(Error));Phase=4;return false;
        }
        if(Phase==4)
        {if(Service->HasItems())return false;Test->TestFalse(TEXT("reset releases active work"),Dataset->IsActive());Test->TestTrue(TEXT("reset preserves artifacts on disk"),FPaths::FileExists(Root/TEXT("testset.json")));return Finish();}
        return false;
    }
private:
    void CheckFiles()
    {
        Root=Service->Directory();
        for(const auto& I:Service->Items())
        {
            Test->TestTrue(TEXT("scenario summary committed before complete"),FPaths::FileExists(I.Directory/TEXT("summary.json")));
            for(int32 N=0;N<I.Frames;++N)
            {
                const FString Dir=I.Directory/(TEXT("run_")+I.RunId)/FString::Printf(TEXT("scene_%05d"),N);FString Text;TSharedPtr<FJsonObject> Label;
                if(!FFileHelper::LoadFileToString(Text,*(Dir/TEXT("label.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Label)||!Label){Test->AddError(TEXT("label missing"));return;}
                Test->TestEqual(TEXT("source frame retained independently of sensor frame"),Label->GetStringField(TEXT("scenario_frame_no")),LexToString(100+N*2));Test->TestEqual(TEXT("source material retained"),Label->GetStringField(TEXT("mtl_no")).TrimStartAndEnd(),TEXT("9999"));
                const auto Pose=Label->GetObjectField(TEXT("slab_pose"));const auto XYZ=Pose->GetArrayField(TEXT("xyz_m"));const auto RPY=Pose->GetArrayField(TEXT("pitch_yaw_roll_deg"));
                Test->TestTrue(TEXT("saved pose uses actual source position and left angle"),FMath::IsNearlyEqual(XYZ[0]->AsNumber(),15.+N*.03,1.e-4)&&FMath::IsNearlyEqual(XYZ[2]->AsNumber(),2.25,1.e-4)&&FMath::IsNearlyEqual(RPY[1]->AsNumber(),double(N*2-3),1.e-3));
                TArray<uint8> Labels;FFileHelper::LoadFileToArray(Labels,*(Dir/TEXT("lidar_labels.bin")));bool HasSlab=false;for(int32 P=0;P<Labels.Num();P+=5)HasSlab|=Labels[P]==2;Test->TestTrue(TEXT("training frame contains actual Slab returns"),HasSlab);
                Test->TestFalse(TEXT("default LiDAR-only excludes camera capture output"),FPaths::FileExists(Dir/TEXT("rgb.png")));Test->TestTrue(TEXT("actual PCD present"),FPaths::FileExists(Dir/TEXT("lidar.pcd")));
            }
        }
    }
    void Record(double Now)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-01/automated-completed.png"),true,false);
        auto O=Service->StatusJson();O->SetNumberField(TEXT("wall_seconds"),Now-MeasureStart);O->SetNumberField(TEXT("seconds_per_scenario"),(Now-MeasureStart)/Service->Completed());O->SetNumberField(TEXT("resident_start_bytes"),double(MemoryStart));O->SetNumberField(TEXT("resident_peak_bytes"),double(PeakMemory));
        O->SetStringField(TEXT("measured_start_utc"),(FDateTime::UtcNow()-FTimespan::FromSeconds(Now-MeasureStart)).ToIso8601());O->SetStringField(TEXT("measured_end_utc"),FDateTime::UtcNow().ToIso8601());
        Frames.Sort();Game.Sort();int32 Hitches=0;double Sum=0,GT=0;for(double V:Frames){Sum+=V;Hitches+=V>150;}for(double V:Game)GT+=V;
        if(!Frames.IsEmpty()){O->SetNumberField(TEXT("engine_mean_ms"),Sum/Frames.Num());O->SetNumberField(TEXT("engine_p95_ms"),Frames[FMath::FloorToInt((Frames.Num()-1)*.95)]);O->SetNumberField(TEXT("engine_max_ms"),Frames.Last());O->SetNumberField(TEXT("game_thread_mean_ms"),GT/Game.Num());O->SetNumberField(TEXT("hitches_over_150ms"),Hitches);}
        O->SetStringField(TEXT("profile"),TEXT("IYOBOT_MLX80_NATIVE FullSpec override 512x32 GPU depth; noise off; one dedicated LiDAR; UI open; D3D12 offscreen"));
        FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-01")/(Large?(Pipeline?TEXT("throughput-pipeline.json"):TEXT("throughput-sequential.json")):TEXT("lifecycle.json"))));
        if(Large)Test->TestEqual(TEXT("generation stays below 150ms engine hitch budget"),Hitches,0);
        Test->AddInfo(FString::Printf(TEXT("TESTSET-01 %d scenarios %.3f seconds, engine max %.3f ms"),Service->Completed(),Now-MeasureStart,Frames.IsEmpty()?0:Frames.Last()));
    }
    bool Finish(){if(Service){Service->StopPreview();FString E;Service->ResetConfirmed(E);}if(Dataset){Dataset->OnSceneSnapshotForTests=nullptr;Dataset->Stop();}if(Lidar)Lidar->Destroy();if(Slab)Slab->Destroy();return true;}
    FAutomationTestBase* Test;bool Large,Pipeline,Manual,PreviewRequested=false;UWorld* World=nullptr;UAgentTestSetSubsystem* Service=nullptr;UVirtualDatasetSubsystem* Dataset=nullptr;USlabScenarioReplaySubsystem* Catalog=nullptr;ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;
    FSlabScenarioDataPtr Source;FString Error,Root;int32 Phase=0,Injected=0,NormalCount=0,PreviewCount=0;double Started=FPlatformTime::Seconds(),Observed=0,MeasureStart=0,PauseAt=0,NormalAt=0;int64 PauseFrame=0,PreviewFrame=0;uint64 LastFrame=0,MemoryStart=0,PeakMemory=0;TArray<double> Frames,Game;TSet<FString> PreviewRuns;
};
bool Run(FAutomationTestBase* Test,bool Large,bool Pipeline)
{
    if(GUsingNullRHI){Test->AddInfo(TEXT("[OPT-IN SKIP] TESTSET-01 requires D3D12"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FTestSetRun(Test,Large,Pipeline));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSetLifecycle,"MA0T10.TestSet.LifecycleAndOrdinaryPriority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTestSetLifecycle::RunTest(const FString&){return Run(this,false,true);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSetPipeline,"MA0T10.TestSet.Throughput100Pipeline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTestSetPipeline::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("TestSet100"))){AddInfo(TEXT("[OPT-IN SKIP] pass -TestSet100 for 100 scenario generation"));return true;}return Run(this,true,true);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSetSequential,"MA0T10.TestSet.Throughput100Sequential",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTestSetSequential::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("TestSet100"))){AddInfo(TEXT("[OPT-IN SKIP] pass -TestSet100 for 100 scenario generation"));return true;}return Run(this,true,false);}
#endif
