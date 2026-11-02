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
    FString H,E;TestTrue(TEXT("Windows SHA256 available"),TrainBatchCodec::Sha256("abc",3,H));TestEqual(TEXT("independent SHA256 known vector"),H,FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    TestTrue(TEXT("empty hash"),TrainBatchCodec::Sha256(nullptr,0,H));TestEqual(TEXT("empty known vector"),H,FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
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
#endif
