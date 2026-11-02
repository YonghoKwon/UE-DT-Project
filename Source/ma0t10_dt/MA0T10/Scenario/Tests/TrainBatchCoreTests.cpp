#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchStorage.h"
#include "ma0t10_dt/MA0T10/Contracts/LocalCapturePath.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Json.h"

namespace
{
FString Json(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
struct FFixture
{
    FString Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),Raw,Hash;
    TSharedRef<FJsonObject> Manifest=MakeShared<FJsonObject>();
    FFixture()
    {
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("MESSAGE_ID"),TEXT("IFactory-agent"));O->SetStringField(TEXT("CREATE_TIMESTAMP"),TEXT("20261102000000"));
        auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("UUID"),FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));M->SetNumberField(TEXT("duration_sec"),.15);O->SetObjectField(TEXT("_meta"),M);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(int32 K=0;K<2;++K)
        {
            auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("frame_no"),K+7);R->SetStringField(TEXT("mtl_no"),TEXT("TRAIN-UNIT"));
            R->SetNumberField(TEXT("slab_thk"),250);R->SetNumberField(TEXT("slab_wth"),1100);R->SetNumberField(TEXT("slab_len"),10830);R->SetNumberField(TEXT("slab_wt"),1);
            R->SetNumberField(TEXT("left_skew_angle"),0);R->SetNumberField(TEXT("right_skew_angle"),0);R->SetNumberField(TEXT("center_x"),K*10);R->SetNumberField(TEXT("elapsed_sec"),K*.1);
            Rows.Add(MakeShared<FJsonValueObject>(R));
        }
        O->SetArrayField(TEXT("DATA_MAP"),Rows);Raw=Json(O);FTCHARToUTF8 U(*Raw);TrainBatchCodec::Sha256(U.Get(),U.Length(),Hash);
        Manifest->SetStringField(TEXT("schema"),TrainBatchContract::Schema);Manifest->SetStringField(TEXT("MESSAGE_ID"),TrainBatchContract::MessageId);Manifest->SetStringField(TEXT("kind"),TEXT("job"));Manifest->SetStringField(TEXT("job_id"),Id);
        Manifest->SetStringField(TEXT("preset_id"),TEXT("slab-train-v1"));Manifest->SetStringField(TEXT("dimension_unit"),TEXT("mm"));Manifest->SetStringField(TEXT("position_unit"),TEXT("cm"));Manifest->SetNumberField(TEXT("scenario_count"),1);Manifest->SetNumberField(TEXT("worker_limit"),4);
        auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("mode"),TEXT("sampled_timeline"));E->SetNumberField(TEXT("sample_hz"),20);Manifest->SetObjectField(TEXT("execution"),E);
        auto S=MakeShared<FJsonObject>();S->SetStringField(TEXT("scenario_id"),TEXT("case-001"));S->SetNumberField(TEXT("duration_sec"),.15);S->SetStringField(TEXT("input_sha256"),Hash);S->SetNumberField(TEXT("input_bytes"),U.Length());S->SetStringField(TEXT("input_file"),TEXT("input.json"));
        Manifest->SetArrayField(TEXT("scenarios"),{MakeShared<FJsonValueObject>(S)});
    }
    FTrainBatchMessage Job() const {FTrainBatchMessage M;FString E;TrainBatchCodec::Parse(Json(Manifest),M,E);return M;}
    FTrainBatchMessage Item() const
    {
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("schema"),TrainBatchContract::Schema);O->SetStringField(TEXT("MESSAGE_ID"),TrainBatchContract::MessageId);O->SetStringField(TEXT("kind"),TEXT("scenario"));O->SetStringField(TEXT("job_id"),Id);O->SetStringField(TEXT("scenario_id"),TEXT("case-001"));O->SetStringField(TEXT("input_sha256"),Hash);O->SetStringField(TEXT("scenario_json"),Raw);
        FTrainBatchMessage M;FString E;TrainBatchCodec::Parse(Json(O),M,E);return M;
    }
    ~FFixture()
    {
        const FString Owned=TrainBatchCodec::JobDirectory(Id);
        if(TrainBatchCodec::CanonicalGuid(Id)&&LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),Owned))IFileManager::Get().DeleteDirectory(*Owned,false,true);
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchContractTest,"MA0T10.TrainBatch.Contract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchContractTest::RunTest(const FString&)
{
    FString H,E;TestTrue(TEXT("shared SHA256 available"),TrainBatchCodec::Sha256("abc",3,H));TestEqual(TEXT("independent SHA256 known vector"),H,FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    TestTrue(TEXT("empty hash"),TrainBatchCodec::Sha256(nullptr,0,H));TestEqual(TEXT("empty known vector"),H,FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
    TestFalse(TEXT("nonempty null hash input rejected"),TrainBatchCodec::Sha256(nullptr,1,H));
    TestTrue(TEXT("failed hash clears output"),H.IsEmpty());
    FFixture F;FTrainBatchMessage M;TestTrue(TEXT("job accepted"),TrainBatchCodec::Parse(Json(F.Manifest),M,E));TestEqual(TEXT("worker request retained; 1A runs one"),M.Job.WorkerLimit,4);
    TestFalse(TEXT("ordinary scenario not swallowed"),TrainBatchCodec::Parse(F.Raw,M,E));
    F.Manifest->SetStringField(TEXT("archive_format"),TEXT("tar.zst"));TestFalse(TEXT("deferred codec rejected"),TrainBatchCodec::Parse(Json(F.Manifest),M,E));
    F.Manifest->SetStringField(TEXT("archive_format"),TEXT("tar.gz"));TestTrue(TEXT("tar.gz selection"),TrainBatchCodec::Parse(Json(F.Manifest),M,E));
    F.Manifest->SetNumberField(TEXT("scenario_count"),1001);TestFalse(TEXT("K upper bound"),TrainBatchCodec::Parse(Json(F.Manifest),M,E));F.Manifest->SetNumberField(TEXT("scenario_count"),1);
    F.Manifest->GetObjectField(TEXT("execution"))->SetNumberField(TEXT("sample_hz"),19);TestFalse(TEXT("no rate degradation"),TrainBatchCodec::Parse(Json(F.Manifest),M,E));
    F.Manifest->GetObjectField(TEXT("execution"))->SetNumberField(TEXT("sample_hz"),20);
    auto Capture=MakeShared<FJsonObject>();Capture->SetBoolField(TEXT("include_imu"),true);F.Manifest->SetObjectField(TEXT("capture"),Capture);TestFalse(TEXT("unknown option is not silently ignored"),TrainBatchCodec::Parse(Json(F.Manifest),M,E));
    for(const auto* Bad:{TEXT("../x"),TEXT("a/b"),TEXT("A B"),TEXT("CON"),TEXT("LPT1"),TEXT("a\nput evil")})TestFalse(TEXT("safe local ID"),TrainBatchCodec::SafeId(Bad));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchTimelineTest,"MA0T10.TrainBatch.Timeline20Hz",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchTimelineTest::RunTest(const FString&)
{
    FFixture F;FString E;FSlabScenarioDataPtr D;const auto Job=F.Job();if(!TestEqual(TEXT("fixture descriptor"),Job.Job.Scenarios.Num(),1))return false;const auto Spec=Job.Job.Scenarios[0];
    TestTrue(TEXT("raw validated against descriptor"),TrainBatchCodec::ValidateScenario(Spec,F.Raw,D,E));if(!D)return false;
    FVirtualDatasetFramePlan P;const FTransform Track(FRotator(0,20,0),FVector(100,200,300));
    TestTrue(TEXT("plan created"),TrainBatchCodec::Plan(*D,.15,Track,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,P,E));TestEqual(TEXT("all [0,D) samples"),P.Frames.Num(),3);if(P.Frames.Num()!=3)return false;
    TestEqual(TEXT("middle continuous time"),P.Frames[1].ElapsedSec,.05);TestEqual(TEXT("separate source row time"),P.Frames[1].SourceElapsedSec,0.);TestEqual(TEXT("source frame retained"),P.Frames[1].FrameNo,int64(7));
    FSlabScenarioRow Sample;int32 Lower;FSlabScenarioCodec::Sample(*D,.05,Sample,Lower);
    TestTrue(TEXT("same live codec interpolation and pose oracle"),P.Frames[1].SlabPose.Equals(USlabMotionComponent::BuildPose(Sample,Track,P.Frames[1].SizeCm,ESlabInputUnit::Centimeters)));
    TestEqual(TEXT("dimension mm converted to cm"),P.Frames[0].SizeCm.Z,25.);
    TestFalse(TEXT("duration mismatch cannot truncate"),TrainBatchCodec::Plan(*D,.1,Track,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,P,E));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchPathTest,"MA0T10.TrainBatch.PlanPathBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchPathTest::RunTest(const FString&)
{
    FFixture F;FVirtualDatasetFramePlan P;P.BatchJobId=F.Id;P.ScenarioId=TEXT("case-001");P.RunId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);P.LocalStorageRoot=TrainBatchCodec::JobsRoot();P.Root=TrainBatchCodec::RunDirectory(P.BatchJobId,P.ScenarioId,P.RunId);
    TestTrue(TEXT("exact local Job scope"),IsValidDatasetPlanRoot(P));P.Root+=TEXT("/other");TestFalse(TEXT("no arbitrary subroot"),IsValidDatasetPlanRoot(P));P.Root=LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("Inspection"));TestFalse(TEXT("protected area not a plan"),IsValidDatasetPlanRoot(P));
    TestFalse(TEXT("prefix sibling is not descendant"),LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),TrainBatchCodec::JobsRoot()+TEXT("-other/a")));
    TestFalse(TEXT("parent traversal rejected even when it returns inside"),LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),TrainBatchCodec::JobsRoot()/TEXT("a/../b")));
    P=FVirtualDatasetFramePlan();P.Root=LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("TestSets/unit/run"));TestTrue(TEXT("legacy TestSet still admitted"),IsValidDatasetPlanRoot(P));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchStorageTest,"MA0T10.TrainBatch.StorageAdmission",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchStorageTest::RunTest(const FString&)
{
    FFixture F;FTrainBatchStorage Store;FString E;const auto Job=F.Job(),Item=F.Item();
    TestTrue(TEXT("manifest persisted"),Store.Receive(Job,E));if(Store.Jobs.IsEmpty())return false;
    TestFalse(TEXT("missing input blocks readiness"),Store.Jobs[0].IsReady());TestTrue(TEXT("original admitted"),Store.Receive(Item,E));TestTrue(TEXT("all original inputs durable"),Store.Jobs[0].IsReady());
    FString Before,After;const FString Path=TrainBatchCodec::JobDirectory(F.Id)/TEXT("input/case-001/original.json");TestTrue(TEXT("raw input readable"),FTrainBatchStorage::ReadUtf8(Path,TrainBatchContract::ScenarioBytes,Before));TestEqual(TEXT("original unchanged"),Before,F.Raw);
    TestTrue(TEXT("duplicate item idempotent"),Store.Receive(Item,E));FTrainBatchStorage::ReadUtf8(Path,TrainBatchContract::ScenarioBytes,After);TestEqual(TEXT("duplicate does not rewrite original"),Before,After);
    auto Conflict=Job;Conflict.Job.ManifestSha256=TEXT("different");TestFalse(TEXT("same ID different Job rejected"),Store.Receive(Conflict,E));
    FTrainBatchStorage Restart;TestFalse(TEXT("1A cannot overwrite a previous process Job"),Restart.Receive(Job,E));
    if(!TestTrue(TEXT("explicit disk resume validates original and journal"),Restart.Restore(F.Id,E)))AddError(E);
    if(!Restart.Jobs.IsEmpty())TestTrue(TEXT("restored input ready without replaying completed work"),Restart.Jobs[0].IsReady());
    FString Journal;FFileHelper::LoadFileToString(Journal,*(TrainBatchCodec::JobDirectory(F.Id)/TEXT("journal.ndjson")));TestTrue(TEXT("state transitions are journaled"),Journal.Contains(TEXT("input_stored")));TestTrue(TEXT("journal carries chain checksum"),Journal.Contains(TEXT("previous_sha256")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchIncompleteResultTest,"MA0T10.TrainBatch.NoFalseCompletion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchIncompleteResultTest::RunTest(const FString&)
{
    FFixture F;FTrainBatchStorage Store;FString E;std::atomic<bool> Cancel(false);
    if(!TestTrue(TEXT("job"),Store.Receive(F.Job(),E))||!TestTrue(TEXT("input"),Store.Receive(F.Item(),E)))return false;
    auto P=Store.Prepare(F.Id,0,FTransform::Identity,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,1,1024,E);TestTrue(TEXT("planned"),P.IsValid());
    TestFalse(TEXT("missing dataset is not completion"),Store.Finish(F.Id,0,true,TEXT(""),Cancel,E));TestTrue(TEXT("failure state retained"),Store.Jobs[0].Items[0].State==ETrainBatchItemState::Failed);
    if(P)TestFalse(TEXT("no misleading complete marker"),IFileManager::Get().FileExists(*(P->Root/TEXT("complete.json"))));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchImportTest,"MA0T10.TrainBatch.LocalImportBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchImportTest::RunTest(const FString&)
{
    FFixture F;FTrainBatchStorage Store;FString E;std::atomic<bool> Cancel(false);
    const FString Base=LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("TrainBatchImportTests")/F.Id);IFileManager::Get().MakeDirectory(*Base,true);
    auto Cleanup=[&](){if(LocalCapturePath::SafeDescendant(LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("TrainBatchImportTests")),Base))IFileManager::Get().DeleteDirectory(*Base,false,true);};
    FFileHelper::SaveStringToFile(F.Raw,*(Base/TEXT("input.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    const auto Descriptor=F.Manifest->GetArrayField(TEXT("scenarios"))[0]->AsObject();Descriptor->SetStringField(TEXT("input_file"),TEXT("../outside.json"));
    FFileHelper::SaveStringToFile(Json(F.Manifest),*(Base/TEXT("job.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    TestFalse(TEXT("traversal rejected before Job admission"),Store.Import(Base/TEXT("job.json"),Cancel,E));TestTrue(TEXT("no partial catalog admission for unsafe path"),Store.Jobs.IsEmpty());
    Descriptor->SetStringField(TEXT("input_file"),TEXT("input.json"));FFileHelper::SaveStringToFile(Json(F.Manifest),*(Base/TEXT("job.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    TestTrue(TEXT("local manifest and raw original imported"),Store.Import(Base/TEXT("job.json"),Cancel,E));if(!Store.Jobs.IsEmpty())TestTrue(TEXT("ready after durable import"),Store.Jobs[0].IsReady());
    Cleanup();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchVerifiedResultTest,"MA0T10.TrainBatch.VerifiedCompletion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchVerifiedResultTest::RunTest(const FString&)
{
    FFixture F;FTrainBatchStorage Store;FString E;std::atomic<bool> Cancel(false);
    if(!Store.Receive(F.Job(),E)||!Store.Receive(F.Item(),E)){AddError(E);return false;}
    auto P=Store.Prepare(F.Id,0,FTransform::Identity,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,1,1024,E);if(!P){AddError(E);return false;}
    auto WriteJson=[&](const FString& Path,const TSharedRef<FJsonObject>& O){return FFileHelper::SaveStringToFile(Json(O),*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);};
    IFileManager::Get().MakeDirectory(*P->Root,true);auto Dataset=MakeShared<FJsonObject>();Dataset->SetStringField(TEXT("schema_version"),TEXT("ma0t10.dataset.v1"));Dataset->SetNumberField(TEXT("completed_scenes"),3);
    if(!WriteJson(P->Root/TEXT("dataset.json"),Dataset))return false;
    FVirtualLidarPoint Point;Point.bHit=true;Point.SensorLocalPositionMeters=FVector(1,0,0);TArray<const FVirtualLidarPoint*> Points={&Point};TArray<uint8> Pcd,Labels;Labels.SetNumZeroed(5);
    if(!TestTrue(TEXT("independent canonical writer fixture"),FVirtualPcdCodec::Encode(Points,TEXT("{}"),FVirtualPcdSendOptions(),Pcd)))return false;
    for(int32 K=0;K<3;++K)
    {
        const FString Dir=P->Root/FString::Printf(TEXT("scene_%05d"),K);IFileManager::Get().MakeDirectory(*Dir,true);
        if(!FFileHelper::SaveArrayToFile(Pcd,*(Dir/TEXT("lidar.pcd")))||!FFileHelper::SaveArrayToFile(Labels,*(Dir/TEXT("lidar_labels.bin"))))return false;
        auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("schema_version"),TEXT("ma0t10.dataset.scene.v1"));L->SetStringField(TEXT("scenario_id"),TEXT("case-001"));L->SetStringField(TEXT("run_uuid"),P->RunId);L->SetNumberField(TEXT("simulation_elapsed_s"),double(K)/20);
        if(!WriteJson(Dir/TEXT("label.json"),L))return false;
    }
    TestTrue(TEXT("files validated then completed"),Store.Finish(F.Id,0,true,TEXT(""),Cancel,E));TestTrue(TEXT("terminal success"),Store.Jobs[0].Items[0].State==ETrainBatchItemState::Completed);
    FString Checksums;TestTrue(TEXT("checksum list saved"),FFileHelper::LoadFileToString(Checksums,*(P->Root/TEXT("checksums.sha256"))));TArray<FString> Lines;Checksums.ParseIntoArrayLines(Lines);TestEqual(TEXT("dataset plus 3 files per sample"),Lines.Num(),10);
    TestFalse(TEXT("no self-referential completion hash"),Checksums.Contains(TEXT("complete.json")));TestTrue(TEXT("complete marker last"),IFileManager::Get().FileExists(*(P->Root/TEXT("complete.json"))));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchSampleBoundaryTest,"MA0T10.TrainBatch.SampleCountBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchSampleBoundaryTest::RunTest(const FString&)
{
    FFixture F;FSlabScenarioDataPtr Original;FString Error;FSlabScenarioCodec::Parse(F.Raw,Original,Error,false);if(!Original)return false;
    for(const double Duration:{.05,.1,.15,.35,1.,300.})
    {
        const int32 Expected=int32(Duration*20+.5);int32 Count=0;
        TestTrue(TEXT("boundary count accepted"),TrainBatchCodec::SampleCount(Duration,Count,Error));TestEqual(TEXT("no extra endpoint sample"),Count,Expected);
        FSlabScenarioData Data=*Original;Data.DurationSec=Duration;FVirtualDatasetFramePlan Plan;
        TestTrue(TEXT("plan accepts boundary"),TrainBatchCodec::Plan(Data,Duration,FTransform::Identity,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,Plan,Error));TestEqual(TEXT("planner shares count"),Plan.Frames.Num(),Expected);
        for(const auto& Frame:Plan.Frames)if(!TestTrue(TEXT("every sample strictly before D"),Frame.ElapsedSec<Duration))break;
    }
    int32 Count=0;TestTrue(TEXT("representation noise accepted"),TrainBatchCodec::SampleCount(.15+1.e-15,Count,Error));TestEqual(TEXT("noise is not a fourth frame"),Count,3);
    TestTrue(TEXT("genuine fractional sample retained"),TrainBatchCodec::SampleCount(.05+1.e-7,Count,Error));TestEqual(TEXT("fractional duration has second sample"),Count,2);
    TestTrue(TEXT("positive sub-interval retains t0"),TrainBatchCodec::SampleCount(1.e-12,Count,Error));TestEqual(TEXT("one t0"),Count,1);
    Error.Reset();TestFalse(TEXT("frame cap rejected"),TrainBatchCodec::SampleCount(600,Count,Error));TestFalse(TEXT("cap has reason"),Error.IsEmpty());
    FSlabScenarioData Data=*Original;Data.DurationSec=600;FVirtualDatasetFramePlan Plan;Error.Reset();TestFalse(TEXT("Plan cap rejected"),TrainBatchCodec::Plan(Data,600,FTransform::Identity,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,Plan,Error));TestTrue(TEXT("Plan cap reason names frame limit"),Error.Contains(TEXT("frame limit")));
    return true;
}
namespace
{
bool WriteRecoveryDataset(const FVirtualDatasetFramePlan& P)
{
    auto Write=[&](const FString& Path,const TSharedRef<FJsonObject>& O){IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);return FFileHelper::SaveStringToFile(Json(O),*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);};
    auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("schema_version"),TEXT("ma0t10.dataset.v1"));D->SetNumberField(TEXT("completed_scenes"),P.Frames.Num());if(!Write(P.Root/TEXT("dataset.json"),D))return false;
    FVirtualLidarPoint Point;Point.bHit=true;Point.SensorLocalPositionMeters=FVector(1,0,0);TArray<const FVirtualLidarPoint*> Points={&Point};TArray<uint8> Pcd,Labels;Labels.SetNumZeroed(5);
    if(!FVirtualPcdCodec::Encode(Points,TEXT("{}"),FVirtualPcdSendOptions(),Pcd))return false;
    for(int32 K=0;K<P.Frames.Num();++K)
    {
        const FString Dir=P.Root/FString::Printf(TEXT("scene_%05d"),K);IFileManager::Get().MakeDirectory(*Dir,true);
        if(!FFileHelper::SaveArrayToFile(Pcd,*(Dir/TEXT("lidar.pcd")))||!FFileHelper::SaveArrayToFile(Labels,*(Dir/TEXT("lidar_labels.bin"))))return false;
        auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("schema_version"),TEXT("ma0t10.dataset.scene.v1"));L->SetStringField(TEXT("scenario_id"),P.ScenarioId);L->SetStringField(TEXT("run_uuid"),P.RunId);L->SetNumberField(TEXT("simulation_elapsed_s"),double(K)/20);if(!Write(Dir/TEXT("label.json"),L))return false;
    }
    return true;
}
bool SaveOwnedText(const FString& Path,const FString& Text)
{IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);return FFileHelper::SaveStringToFile(Text,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainAtomicRecoveryTest,"MA0T10.TrainBatch.AtomicCrashRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainAtomicRecoveryTest::RunTest(const FString&)
{
    FFixture F;FTrainBatchStorage S;FString E;std::atomic<bool> Cancel(false);
    if(!TestTrue(TEXT("manifest"),S.Receive(F.Job(),E)))return false;
    const FString Original=TrainBatchCodec::JobDirectory(F.Id)/TEXT("input/case-001/original.json");
    TestTrue(TEXT("stale input pending fixture"),SaveOwnedText(Original+TEXT(".pending"),TEXT("interrupted")));
    // A crash may publish immutable input before journaling the waiting state.
    TestTrue(TEXT("published original before state"),SaveOwnedText(Original,F.Raw));const auto OriginalTime=IFileManager::Get().GetTimeStamp(*Original);
    TestTrue(TEXT("original re-reception recovers exact published bytes"),S.Receive(F.Item(),E));TestTrue(TEXT("original not rewritten"),OriginalTime==IFileManager::Get().GetTimeStamp(*Original));
    FTrainBatchStorage Restart;TestTrue(TEXT("crash-state resume"),Restart.Restore(F.Id,E));TestTrue(TEXT("original resend after restore"),Restart.Receive(F.Item(),E));
    auto P=Restart.Prepare(F.Id,0,FTransform::Identity,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,1,1024,E);if(!P||!WriteRecoveryDataset(*P))return false;
    TestTrue(TEXT("stale checksum pending"),SaveOwnedText(P->Root/TEXT("checksums.sha256.pending"),TEXT("partial")));TestTrue(TEXT("stale completion pending"),SaveOwnedText(P->Root/TEXT("complete.json.pending"),TEXT("partial")));
    if(!TestTrue(*(TEXT("completion unaffected by stale pending: ")+E),Restart.Finish(F.Id,0,true,TEXT(""),Cancel,E)))return false;
    FString Checksum,Marker;FFileHelper::LoadFileToString(Checksum,*(P->Root/TEXT("checksums.sha256")));FFileHelper::LoadFileToString(Marker,*(P->Root/TEXT("complete.json")));
    TestTrue(TEXT("retry identical confirmed marker is idempotent"),Restart.Finish(F.Id,0,true,TEXT(""),Cancel,E));
    TestTrue(TEXT("simulate crash before completion marker publish"),IFileManager::Get().Delete(*(P->Root/TEXT("complete.json")),false,true));
    TestTrue(TEXT("recreate completion with already-confirmed checksum list"),Restart.Finish(F.Id,0,true,TEXT(""),Cancel,E));
    FString Again;FFileHelper::LoadFileToString(Again,*(P->Root/TEXT("complete.json")));TestEqual(TEXT("recreated marker bytes"),Again,Marker);
    SaveOwnedText(P->Root/TEXT("complete.json"),TEXT("conflicting immutable marker"));TestFalse(TEXT("conflicting final never replaced"),Restart.Finish(F.Id,0,true,TEXT(""),Cancel,E));FFileHelper::LoadFileToString(Again,*(P->Root/TEXT("complete.json")));TestEqual(TEXT("conflicting bytes retained"),Again,FString(TEXT("conflicting immutable marker")));
    FFixture Other;FTrainBatchStorage OtherStore;OtherStore.Receive(Other.Job(),E);const FString OtherOriginal=TrainBatchCodec::JobDirectory(Other.Id)/TEXT("input/case-001/original.json");SaveOwnedText(OtherOriginal,TEXT("conflicting original"));TestFalse(TEXT("original conflict refused"),OtherStore.Receive(Other.Item(),E));FFileHelper::LoadFileToString(Again,*OtherOriginal);TestEqual(TEXT("original conflict bytes retained"),Again,FString(TEXT("conflicting original")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainAdoptRollbackTest,"MA0T10.TrainBatch.AdoptTransactional",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainAdoptRollbackTest::RunTest(const FString&)
{
    FFixture F;FTrainBatchStorage S;FString E;std::atomic<bool> Cancel(false);if(!S.Receive(F.Job(),E)||!S.Receive(F.Item(),E))return false;
    auto P=S.Prepare(F.Id,0,FTransform::Identity,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,1,1024,E);if(!P||!WriteRecoveryDataset(*P)||!S.Finish(F.Id,0,true,TEXT(""),Cancel,E))return false;
    S.Jobs[0].Items[0].State=ETrainBatchItemState::Waiting;const FString Lease=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);S.MarkLeased(F.Id,0,Lease,E);const auto Before=S.Jobs[0].Items[0];
    const FString Worker=LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("TrainBatchTestWorker"));
    TestFalse(TEXT("unverified source rejected"),S.Adopt(F.Id,0,Worker,FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),E));
    TestEqual(TEXT("failure keeps lease id"),S.Jobs[0].Items[0].RunId,Before.RunId);TestEqual(TEXT("failure keeps lease state"),S.Jobs[0].Items[0].State,Before.State);TestEqual(TEXT("failure keeps run directory"),S.Jobs[0].Items[0].RunDirectory,Before.RunDirectory);TestEqual(TEXT("failure keeps message"),S.Jobs[0].Items[0].Message,Before.Message);
    const FString State=TrainBatchCodec::JobDirectory(F.Id)/TEXT("job.json"),Backup=State+TEXT(".test-backup");
    if(!IFileManager::Get().Move(*Backup,*State,false,true)||!IFileManager::Get().MakeDirectory(*State,true))return false;
    TestFalse(TEXT("snapshot publication failure rejected"),S.Adopt(F.Id,0,Worker,P->RunId,E));TestEqual(TEXT("snapshot failure also keeps lease"),S.Jobs[0].Items[0].RunId,Before.RunId);TestEqual(TEXT("snapshot failure keeps running state"),S.Jobs[0].Items[0].State,Before.State);
    IFileManager::Get().DeleteDirectory(*State,false,false);IFileManager::Get().Move(*State,*Backup,false,true);
    TestTrue(TEXT("verified destination can be adopted on retry"),S.Adopt(F.Id,0,Worker,P->RunId,E));TestEqual(TEXT("committed run id"),S.Jobs[0].Items[0].RunId,P->RunId);TestTrue(TEXT("committed completed state"),S.Jobs[0].Items[0].State==ETrainBatchItemState::Completed);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainRestoreIntegrityTest,"MA0T10.TrainBatch.RestoreSameMetadataTamper",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainRestoreIntegrityTest::RunTest(const FString&)
{
    FFixture F;FTrainBatchStorage S;FString E;std::atomic<bool> Cancel(false);if(!S.Receive(F.Job(),E)||!S.Receive(F.Item(),E))return false;
    auto P=S.Prepare(F.Id,0,FTransform::Identity,ESlabInputUnit::Millimeters,ESlabInputUnit::Centimeters,1,1024,E);if(!P||!WriteRecoveryDataset(*P)||!S.Finish(F.Id,0,true,TEXT(""),Cancel,E))return false;
    FTrainBatchStorage Good;TestTrue(TEXT("untouched results restore"),Good.Restore(F.Id,E));TestTrue(TEXT("untouched completed"),!Good.Jobs.IsEmpty()&&Good.Jobs[0].Items[0].State==ETrainBatchItemState::Completed);
    const FString Path=P->Root/TEXT("scene_00000/lidar.pcd");const auto Time=IFileManager::Get().GetTimeStamp(*Path);TArray<uint8> Bytes;FFileHelper::LoadFileToArray(Bytes,*Path);if(Bytes.IsEmpty())return false;Bytes.Last()^=1;FFileHelper::SaveArrayToFile(Bytes,*Path);IFileManager::Get().SetTimeStamp(*Path,Time);
    FTrainBatchStorage Bad;TestTrue(TEXT("job remains resumable after output corruption"),Bad.Restore(F.Id,E));TestTrue(TEXT("same size/mtime cannot bypass SHA256"),!Bad.Jobs.IsEmpty()&&Bad.Jobs[0].Items[0].State!=ETrainBatchItemState::Completed);return true;
}
#endif
