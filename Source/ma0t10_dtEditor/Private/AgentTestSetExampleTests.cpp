#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetExamples.h"
#include "ma0t10_dt/MA0T10/Scenario/AgentTestSetSubsystem.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/AgentTestSetWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EngineGlobals.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RHIGlobals.h"
#include "HAL/FileManager.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/IToolTip.h"

namespace
{
FSlabScenarioDataPtr ExampleTestSource()
{
    FString Json,Error;FSlabScenarioDataPtr Data;
    FFileHelper::LoadFileToString(Json,*(FPaths::ProjectDir()/TEXT("Source/ma0t10_dtEditor/Private/Fixtures/inspection-testset-20.json")));
    FSlabScenarioCodec::Parse(Json,Data,Error);return Data;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentExampleDeterminism,"MA0T10.TestSet.ExampleDeterminismAndUnits",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentExampleDeterminism::RunTest(const FString&)
{
    const auto Source=ExampleTestSource();if(!TestTrue(TEXT("portable received fixture"),bool(Source)))return false;
    const int32 Seed=AgentTestSetExamples::SeedFor(Source->OriginalContentSha1,1337);TSet<FString> Variations;
    for(int32 Unit=0;Unit<3;++Unit)for(int32 I=1;I<100;++I)
    {
        const auto U=ESlabInputUnit(Unit);const FString V=AgentTestSetExamples::Variation(Seed,I,U,U);
        TestEqual(TEXT("same content and fixed seed have identical variation values"),V,AgentTestSetExamples::Variation(AgentTestSetExamples::SeedFor(Source->OriginalContentSha1,1337),I,U,U));
        Variations.Add(V);FSlabScenarioDataPtr A,B;FString Error;
        TestTrue(TEXT("example passes the unmodified production codec"),AgentTestSetExamples::Generate(*Source,FGuid::NewGuid().ToString(),V,A,Error));
        TestTrue(TEXT("repeat example passes codec"),AgentTestSetExamples::Generate(*Source,FGuid::NewGuid().ToString(),V,B,Error));
        if(!A||!B)return false;
        TestNotEqual(TEXT("each example has a new scenario UUID"),A->ScenarioUUID,B->ScenarioUUID);
        TestEqual(TEXT("row structure retained"),A->Rows.Num(),Source->Rows.Num());
        for(int32 N=0;N<A->Rows.Num();++N)
        {
            const auto& R=A->Rows[N];TestEqual(TEXT("source frame identity"),R.FrameNo,Source->Rows[N].FrameNo);TestEqual(TEXT("source time"),R.ElapsedSec,Source->Rows[N].ElapsedSec);
            TestEqual(TEXT("dimension stability"),R.Length,A->Rows[0].Length);TestEqual(TEXT("deterministic position"),R.CenterX,B->Rows[N].CenterX);
            TestEqual(TEXT("optional presence preserved"),R.bHasCenterY,Source->Rows[N].bHasCenterY);
            const double Width=FSlabScenarioCodec::ToCm(R.Width,U);TestTrue(TEXT("realistic width cap follows input unit"),Width>=30&&Width<=400);
            const FTransform Track(FRotator(0,35,0),FVector(10,20,30));const FVector Size(100,50,20);
            const auto Pose=USlabMotionComponent::BuildPose(R,Track,Size,U);
            TestTrue(TEXT("existing TrackReference and BuildPose rule retained"),Pose.GetLocation().Equals(Track.TransformPositionNoScale(FVector(FSlabScenarioCodec::ToCm(R.CenterX,U),0,10))));
        }
    }
    TestEqual(TEXT("all 99 examples in each unit have distinct variations"),Variations.Num(),297);
    const auto Rows=AgentTestSetExamples::SampleRows(20,3,3);TestTrue(TEXT("cap keeps stride endpoints and spacing"),Rows==TArray<int32>({0,9,18}));
    TestEqual(TEXT("single representative frame"),AgentTestSetExamples::SampleRows(20,1,1).Num(),1);
    TestEqual(TEXT("received stride unchanged"),AgentTestSetExamples::SampleRows(20,3,0).Num(),7);
    return true;
}

namespace
{
class FAgentExampleRun final:public IAutomationLatentCommand
{
public:
    FAgentExampleRun(FAutomationTestBase* T,bool Full):Test(T),Hundred(Full),Hold(FParse::Param(FCommandLine::Get(),TEXT("TestSet02Hold"))),ManualBusy(FParse::Param(FCommandLine::Get(),TEXT("TestSet02ManualBusy"))){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Started>(Hold?1800:240)){Test->AddError(TEXT("TESTSET-02 timeout"));return Finish();}
        if(Hold&&!Report.IsEmpty()&&FPaths::FileExists(Report/TEXT("finish.flag"))){Test->AddInfo(TEXT("Owned capture/manual fixture finished; acceptance is recorded separately"));return Finish();}
        if(!Service)
        {
            UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){Candidate=C.World();break;}
            if(World!=Candidate){World=Candidate;Observed=Now;return false;}
            if(!World||Now-Observed<3||!FinalAcceptanceViewportReady(World))return false;
            for(TActorIterator<ASlabActor> It(World);It;++It){It->StopSimulation();if(!Slab)Slab=*It;}
            for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            for(TActorIterator<AVirtualLidarSensorActor> It(World);It;++It)if(It->GetSensorId()==TEXT("SLAB-LIDAR-001")){Lidar=*It;break;}
            if(!Slab||!Lidar){Test->AddError(TEXT("validation map Slab and LiDAR missing"));return Finish();}
            auto* Scan=Lidar->ScanComponent.Get();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->bEnableRangeNoise=false;
            Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
            Service=World->GetSubsystem<UAgentTestSetSubsystem>();FAgentTestSetSettings C;C.Target=Hundred?100:6;C.LidarId=Lidar->GetSensorId();Test->TestTrue(TEXT("autofill defaults on, examples use one representative frame"),C.AutoFillExamples&&C.ExampleMaxFrames==1);Service->ConfigureForTests(C);
            Source=ExampleTestSource();if(!Source){Test->AddError(TEXT("fixture parse failed"));return Finish();}
            Report=FPaths::ProjectSavedDir()/TEXT("Reports/TESTSET-02")/(Hundred?TEXT("hundred"):TEXT("lifecycle"));FParse::Value(FCommandLine::Get(),TEXT("TestSet02Report="),Report);IFileManager::Get().MakeDirectory(*Report,true);
            if(ManualBusy)Lidar->StartSensor();
            MeasureStart=LastWall=Now;
            if(Hundred)Test->TestTrue(TEXT("single production asynchronous receive"),Slab->SubmitScenarioJson(Source->OriginalJson));
            else {Test->TestTrue(TEXT("small lifecycle production admission"),Slab->ReceiveScenario(Source));Service->SetPaused(true);}
            return false;
        }
        // Observe every callback, including the owned panel's 5Hz opening interval.
        // Returning before this sampling would fabricate a wall gap while engine frames continue.
        if(Phase<=1&&GFrameCounter!=LastFrame)
        {LastFrame=GFrameCounter;Engine.Add(World->GetDeltaSeconds()*1000.);Wall.Add((Now-LastWall)*1000.);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));LastWall=Now;}
        if(Phase==0&&Service->HasItems())
        {
            if(!World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->GetTestSetWidget())
            {if(Now-MeasureStart>5){Test->AddError(TEXT("auto-open owned panel did not appear within 5 seconds"));return Finish();}return false;}
            Test->TestEqual(TEXT("one receive fills the target immediately"),Service->Items().Num(),Service->Settings().Target);
            Test->TestNotNull(TEXT("receive opens existing owned panel"),World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->GetTestSetWidget());
            Test->TestFalse(TEXT("first is received"),Service->Items()[0].Example);for(int32 I=1;I<Service->Items().Num();++I)Test->TestTrue(TEXT("remaining slots are explicitly examples"),Service->Items()[I].Example);
            Root=Service->Directory();Phase=1;
            if(!Hundred)Service->SetPaused(false);
            if(ManualBusy)
            {
                for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);
                World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->SetOwnedPanelFontScale(1.5f);
                FFileHelper::SaveStringToFile(TEXT("Manual fixture ready: no automatic stop of sensor work"),*(Report/TEXT("ready.flag")));
            }
        }
        if(Service->Items().ContainsByPredicate([](const auto& I){return I.State==EAgentTestSetItemState::Failed;})){Test->AddError(TEXT("autofill failed: ")+Service->Message());return Finish();}
        if(Phase==1&&!Hundred&&Early==0&&Service->Items().Num()>1&&Service->Items()[1].State==EAgentTestSetItemState::Generating)
        {
            ReceiveNew();ReceiveNew();Early=2;
            Test->TestEqual(TEXT("active and waiting example slots reserve real inputs"),int32(Service->StatusJson()->GetNumberField(TEXT("pending_received_replacements"))),2);
        }
        if(Phase==1&&Service->IsComplete())
        {
            Save(TEXT("initial-complete"));auto Stats=MakeShared<FJsonObject>();Stats->SetNumberField(TEXT("seconds"),Now-MeasureStart);
            if(!Hundred)Test->TestEqual(TEXT("active and waiting replacement paths were exercised"),Early,2);
            Stats->SetNumberField(TEXT("first_item_seconds"),Service->Items()[0].Seconds);
            TArray<double> ItemTimes;for(const auto& I:Service->Items())ItemTimes.Add(I.Seconds);ItemTimes.Sort();Stats->SetNumberField(TEXT("item_median_seconds"),ItemTimes[ItemTimes.Num()/2]);Stats->SetNumberField(TEXT("item_max_seconds"),ItemTimes.Last());
            for(auto Pair:{TPair<const TCHAR*,TArray<double>*>(TEXT("engine"),&Engine),{TEXT("callback_wall"),&Wall},{TEXT("game_thread"),&Game}})
            {
                int32 Hitches=0;double Max=0;for(double Ms:*Pair.Value){Hitches+=Ms>150;Max=FMath::Max(Max,Ms);}Stats->SetNumberField(FString(Pair.Key)+TEXT("_max_ms"),Max);Stats->SetNumberField(FString(Pair.Key)+TEXT("_over_150ms"),Hitches);Test->TestEqual(TEXT("150ms budget unchanged"),Hitches,0);
            }
            FString Json;FJsonSerializer::Serialize(Stats,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Report/TEXT("timing.json")));
            // Do not convert a requested speed target into a passing assertion or hide measured limits.
            Test->AddInfo(FString::Printf(TEXT("TESTSET-02 %d scenarios in %.3fs"),Service->Completed(),Now-MeasureStart));
            int32 Frames=0;for(const auto& I:Service->Items())Frames+=I.Frames;Test->TestEqual(TEXT("uniform frame count includes received and replacement slots"),Frames,Service->Settings().Target);
            if(!CheckFiles())return Finish();
            if(Hold){Phase=5;return false;}
            for(int32 N=Early;N<5;++N)ReceiveNew();
            Test->TestFalse(TEXT("completion disabled while replacements drain"),Service->IsComplete());Phase=2;return false;
        }
        if(Phase==2&&Service->IsComplete())
        {
            const auto Status=Service->StatusJson();Test->TestEqual(TEXT("received count after five replacements"),int32(Status->GetNumberField(TEXT("received_scenarios"))),6);Test->TestEqual(TEXT("examples replaced without growing set"),int32(Status->GetNumberField(TEXT("example_scenarios"))),Service->Settings().Target-6);
            Test->TestEqual(TEXT("all replaced examples retained"),Status->GetArrayField(TEXT("replaced_examples")).Num(),5);Save(TEXT("after-replacements"));if(!CheckFiles())return Finish();
            for(const auto& V:Status->GetArrayField(TEXT("replaced_examples")))Test->TestTrue(TEXT("every archived example retains a reason, including ungenerated slots"),FPaths::FileExists(V->AsObject()->GetStringField(TEXT("directory"))/TEXT("replacement.json")));
            FString E;Test->TestTrue(TEXT("F14 saved point cloud preview"),Service->SelectPreview(0,E));Phase=3;return false;
        }
        if(Phase==3&&Service->PreviewStatus().Frame>=0&&Service->PreviewStatus().Points>0)
        {
            Service->SetPreviewPaused(true);Test->TestFalse(TEXT("saved point cloud pause"),Service->PreviewStatus().Playing);Service->StopPreview();FString E;Service->ResetConfirmed(E);Phase=4;
        }
        if(Phase==4&&!Service->HasItems())return Finish();return false;
    }
private:
    void ReceiveNew()
    {
        TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source->OriginalJson),O);O->GetObjectField(TEXT("_meta"))->SetStringField(TEXT("UUID"),FGuid::NewGuid().ToString());FString Input;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Input));
        FSlabScenarioDataPtr Data;FString E;FSlabScenarioCodec::Parse(Input,Data,E);Test->TestTrue(TEXT("real input admitted despite full example set"),Slab->ReceiveScenario(Data));
    }
    void Save(const TCHAR* Name)
    {
        FString Json;FJsonSerializer::Serialize(Service->StatusJson(),TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Report/(FString(Name)+TEXT(".json"))));
        // Inspect this owned widget's actual Slate labels/tooltips, never global/Designer widgets.
        if(auto* Widget=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>()->GetTestSetWidget())
        {
            TArray<TSharedPtr<FJsonValue>> Texts;TSet<const SWidget*> Seen;
            TFunction<void(TSharedRef<SWidget>)> Visit=[&](TSharedRef<SWidget> W)
            {
                if(Seen.Contains(&W.Get()))return;Seen.Add(&W.Get());
                if(W->GetType()==FName(TEXT("STextBlock")))
                {
                    const FString Text=StaticCastSharedRef<STextBlock>(W)->GetText().ToString();Texts.Add(MakeShared<FJsonValueString>(Text));
                    for(const auto* Word:{TEXT("시연용"),TEXT("대표 프레임"),TEXT("demonstration"),TEXT("representative")})Test->TestFalse(TEXT("test set widget excludes unwanted purpose hints"),Text.Contains(Word));
                }
                if(auto Tip=W->GetToolTip())Visit(Tip->GetContentWidget());
                if(auto* Children=W->GetChildren())for(int32 N=0;N<Children->Num();++N)Visit(Children->GetChildAt(N));
            };
            Visit(Widget->TakeWidget());auto O=MakeShared<FJsonObject>();O->SetArrayField(TEXT("texts"),Texts);FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Report/(FString(TEXT("widget-text-"))+Name+TEXT(".json"))));
        }
    }
    bool CheckFiles()
    {
        for(const auto& I:Service->Items())for(int32 F=0;F<I.Frames;++F)
        {
            const FString Dir=I.Directory/(TEXT("run_")+I.RunId)/FString::Printf(TEXT("scene_%05d"),F);FString Json;TSharedPtr<FJsonObject> Label;
            if(!FFileHelper::LoadFileToString(Json,*(Dir/TEXT("label.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Label)||!Label)return false;
            Test->TestEqual(TEXT("label source is explicit"),Label->GetStringField(TEXT("source")),I.Example?FString(TEXT("example")):FString(TEXT("received")));
            Test->TestEqual(TEXT("label identity"),Label->GetStringField(TEXT("scenario_id")),I.ScenarioId);
            Test->TestEqual(TEXT("autofill uses the same frame policy for every source"),Label->GetStringField(TEXT("frame_policy")),FString(TEXT("uniform_capped_stride_samples")));
            Test->TestEqual(TEXT("received and examples have identical frame caps"),I.Frames,1);
            if(I.Example)Test->TestEqual(TEXT("label derived source UUID"),Label->GetStringField(TEXT("source_uuid")),Source->ScenarioUUID);
        }return true;
    }
    bool Finish(){if(Service){Service->StopPreview();FString E;Service->ResetConfirmed(E);}if(ManualBusy&&World)for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(true);return true;}
    FAutomationTestBase* Test;bool Hundred,Hold,ManualBusy;UWorld* World=nullptr;ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;UAgentTestSetSubsystem* Service=nullptr;FSlabScenarioDataPtr Source;
    FString Report,Root;int32 Phase=0,Early=0;uint64 LastFrame=0;double Started=FPlatformTime::Seconds(),Observed=0,MeasureStart=0,LastWall=0;TArray<double> Engine,Wall,Game;
};
bool RunAgentExamples(FAutomationTestBase* Test,bool Hundred)
{
    if(GUsingNullRHI){Test->AddInfo(TEXT("[OPT-IN SKIP] TESTSET-02 requires actual RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FAgentExampleRun(Test,Hundred));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentExamplesLifecycle,"MA0T10.TestSet.ExamplesLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentExamplesLifecycle::RunTest(const FString&){return RunAgentExamples(this,false);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentExamples100,"MA0T10.TestSet.Examples100",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAgentExamples100::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("TestSetExamples100"))){AddInfo(TEXT("[OPT-IN SKIP] pass -TestSetExamples100 for actual 100-example timing"));return true;}return RunAgentExamples(this,true);}
#endif
