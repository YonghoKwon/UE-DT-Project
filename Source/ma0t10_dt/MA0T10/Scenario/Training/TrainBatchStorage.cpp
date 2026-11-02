#include "TrainBatchStorage.h"
#include "TrainBatchProcessProtocol.h"
#include "ma0t10_dt/MA0T10/Contracts/LocalCapturePath.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Json.h"

namespace
{
FString Encode(const TSharedRef<FJsonObject>& O)
{FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
FString JournalJson(const TSharedRef<FJsonObject>& O)
{FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&S));return S;}
const TCHAR* StateName(ETrainBatchItemState S)
{switch(S){case ETrainBatchItemState::Waiting:return TEXT("waiting");case ETrainBatchItemState::Running:return TEXT("running");case ETrainBatchItemState::Completed:return TEXT("completed");case ETrainBatchItemState::Failed:return TEXT("failed");default:return TEXT("receiving");}}
FString InputPath(const FTrainBatchJobView& Job,int32 Index)
{return TrainBatchCodec::JobDirectory(Job.Spec.Id)/TEXT("input")/Job.Items[Index].Spec.Id/TEXT("original.json");}
}
bool FTrainBatchStorage::ReadUtf8(const FString& Path,int64 Limit,FString& Out)
{
    const int64 Size=IFileManager::Get().FileSize(*Path);if(Size<=0||Size>Limit)return false;
    TArray<uint8> B;if(!FFileHelper::LoadFileToArray(B,*Path)||B.Num()!=Size)return false;
    FUTF8ToTCHAR U(reinterpret_cast<const ANSICHAR*>(B.GetData()),B.Num());Out=FString(U.Length(),U.Get());
    FTCHARToUTF8 RoundTrip(*Out);return RoundTrip.Length()==B.Num()&&FMemory::Memcmp(RoundTrip.Get(),B.GetData(),B.Num())==0;
}
bool FTrainBatchStorage::AtomicText(const FString& Path,const FString& Text,bool Replace)
{
    if(!LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),Path))return false;
    if(!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true))return false;
    const FString Pending=Path+TEXT(".pending");
    TUniquePtr<FArchive> File(IFileManager::Get().CreateFileWriter(*Pending,FILEWRITE_NoReplaceExisting));
    if(!File)return false;FTCHARToUTF8 U(*Text);File->Serialize((void*)U.Get(),U.Length());File->Flush();
    const bool Written=!File->IsError();File.Reset();if(!Written)return false;
    if(Replace)return UVirtualSensorFileSaveSubsystem::CommitLocalSceneManifest(TrainBatchCodec::JobsRoot(),Path,Pending);
    // The physical move does not replace an existing destination (Windows MVP).
    return IPlatformFile::GetPlatformPhysical().MoveFile(*Path,*Pending);
}
FTrainBatchJobView* FTrainBatchStorage::Find(const FString& Id)
{return Jobs.FindByPredicate([&](const auto& J){return J.Spec.Id==Id;});}
bool FTrainBatchStorage::HasDiskSpace(uint64 Required,FString& Error)
{
    FString Path=TrainBatchCodec::JobsRoot();
    while(!Path.IsEmpty()&&!IFileManager::Get().DirectoryExists(*Path))Path=FPaths::GetPath(Path);
    uint64 Total=0,Free=0;const uint64 Reserve=FMath::Max<uint64>(10ull*1024*1024*1024,Required/10);
    if(Path.IsEmpty()||!FPlatformMisc::GetDiskTotalAndFreeSpace(Path,Total,Free)||Required>MAX_uint64-Reserve||Free<Required+Reserve)
    {Error=TEXT("학습 디스크 용량/여유 10GiB를 확인하세요");return false;}return true;
}
bool FTrainBatchStorage::Snapshot(FTrainBatchJobView& Job,const FString& Event,FString& Error)
{
    Job.StorageHealthy=false;
    const FString Root=TrainBatchCodec::JobDirectory(Job.Spec.Id);
    if(!LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),Root)){Error=TEXT("학습 저장 경로 거절");return false;}
    auto E=MakeShared<FJsonObject>();const int64 Sequence=Sequences.FindRef(Job.Spec.Id)+1;
    E->SetStringField(TEXT("schema"),TEXT("ma0t10.train.journal.v1"));E->SetNumberField(TEXT("sequence"),double(Sequence));E->SetStringField(TEXT("previous_sha256"),PreviousHashes.FindRef(Job.Spec.Id));
    E->SetStringField(TEXT("event"),Event);E->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());
    FString Hash;{const FString Body=JournalJson(E);FTCHARToUTF8 U(*Body);if(!TrainBatchCodec::Sha256(U.Get(),U.Length(),Hash))return false;}
    E->SetStringField(TEXT("sha256"),Hash);
    if(!FFileHelper::SaveStringToFile(JournalJson(E)+TEXT("\n"),*(Root/TEXT("journal.ndjson")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append))
    {Error=TEXT("학습 journal 저장 실패");return false;}
    Sequences.Add(Job.Spec.Id,Sequence);PreviousHashes.Add(Job.Spec.Id,Hash);
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("schema"),TEXT("ma0t10.train.state.v1"));O->SetStringField(TEXT("job_id"),Job.Spec.Id);
    const FString Commit=TEXT(MA0T10_PROJECT_COMMIT);
    O->SetStringField(TEXT("manifest_sha256"),Job.Spec.ManifestSha256);O->SetStringField(TEXT("project_commit"),Commit.IsEmpty()?TEXT("unknown"):Commit);
    O->SetStringField(TEXT("project_commit_source"),Commit.IsEmpty()?TEXT("unavailable"):TEXT("build"));
    O->SetNumberField(TEXT("sequence"),double(Sequence));O->SetStringField(TEXT("journal_sha256"),Hash);O->SetNumberField(TEXT("concurrent_scenarios"),1);O->SetNumberField(TEXT("worker_processes"),0);O->SetStringField(TEXT("execution_host"),TEXT("coordinator_baseline"));O->SetBoolField(TEXT("ready"),!Job.Items.ContainsByPredicate([](const auto& I){return I.State==ETrainBatchItemState::Receiving;}));
    TArray<TSharedPtr<FJsonValue>> Items;
    for(const auto& I:Job.Items)
    {
        auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("scenario_id"),I.Spec.Id);V->SetStringField(TEXT("state"),StateName(I.State));V->SetStringField(TEXT("message"),I.Message);
        V->SetStringField(TEXT("run_uuid"),I.RunId);V->SetNumberField(TEXT("completed_frames"),I.CompletedFrames);Items.Add(MakeShared<FJsonValueObject>(V));
    }
    O->SetArrayField(TEXT("scenarios"),Items);
    if(!AtomicText(Root/TEXT("job.json"),Encode(O),true)){Error=TEXT("학습 상태 manifest 확정 실패");return false;}Job.StorageHealthy=true;return true;
}
bool FTrainBatchStorage::StoreInput(FTrainBatchJobView& Job,int32 Index,const FString& Raw,FString& Error)
{
    auto& I=Job.Items[Index];FSlabScenarioDataPtr Parsed;
    if(!TrainBatchCodec::ValidateScenario(I.Spec,Raw,Parsed,Error))return false;
    if(I.State!=ETrainBatchItemState::Receiving)return Job.StorageHealthy||Snapshot(Job,TEXT("input_confirmed"),Error); // No duplicate writes/journal churn.
    if(!AtomicText(InputPath(Job,Index),Raw,false)){Error=TEXT("원본 입력 저장 실패 · 기존 파일을 덮지 않습니다");return false;}
    I.State=ETrainBatchItemState::Waiting;I.Message=TEXT("대기");const bool Saved=Snapshot(Job,TEXT("input_stored"),Error);
    Job.Message=Job.IsReady()?TEXT("생성 준비 완료"):TEXT("입력 수신 중");return Saved;
}
bool FTrainBatchStorage::Receive(const FTrainBatchMessage& Message,FString& Error)
{
    if(Message.Kind==FTrainBatchMessage::EKind::Job)
    {
        if(auto* Existing=Find(Message.JobId))
        {if(Existing->Spec.ManifestSha256==Message.Job.ManifestSha256)return Existing->StorageHealthy||Snapshot(*Existing,TEXT("manifest_confirmed"),Error);Error=TEXT("JOB_CONFLICT · 같은 ID의 manifest가 다릅니다");return false;}
        if(Jobs.Num()>=TrainBatchContract::MaxSessionJobs){Error=TEXT("세션 Job 16개 한도");return false;}
        const FString Root=TrainBatchCodec::JobDirectory(Message.JobId);
        uint64 InputBytes=0;for(const auto& Spec:Message.Job.Scenarios)InputBytes+=Spec.InputBytes;
        if(!HasDiskSpace(InputBytes,Error))return false;
        if(!LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),Root)||IFileManager::Get().DirectoryExists(*Root))
        {Error=TEXT("기존 Job 폴더 존재 또는 경로 거절 · 디스크 resume는 1B 대상");return false;}
        FTrainBatchJobView J;J.Spec=Message.Job;J.Message=TEXT("입력 수신 중");
        for(const auto& S:J.Spec.Scenarios){FTrainBatchItemView I;I.Spec=S;I.Message=TEXT("원본 대기");J.Items.Add(MoveTemp(I));}
        if(!AtomicText(Root/TEXT("input/job.json"),J.Spec.OriginalJson,false)){Error=TEXT("Job 원본 manifest 저장 실패");return false;}
        Jobs.Add(MoveTemp(J));return Snapshot(Jobs.Last(),TEXT("job_admitted"),Error);
    }
    auto* J=Find(Message.JobId);if(!J){Error=TEXT("먼저 Job manifest를 접수하세요");return false;}
    if(Message.Kind==FTrainBatchMessage::EKind::Seal)
    {if(!J->IsReady()){Error=TEXT("누락 원본이 있어 Job을 실행할 수 없습니다");return false;}return true;}
    const int32 Index=J->Items.IndexOfByPredicate([&](const auto& I){return I.Spec.Id==Message.ScenarioId;});
    if(Index==INDEX_NONE||!Message.Scenario.IsValid()||J->Items[Index].Spec.InputSha256!=Message.InputSha256||J->Items[Index].Spec.InputBytes!=Message.InputBytes)
    {Error=TEXT("Job에 없는 항목 또는 크기/hash 불일치");return false;}
    return StoreInput(*J,Index,Message.Scenario->OriginalJson,Error);
}
bool FTrainBatchStorage::Import(const FString& ManifestFile,const std::atomic<bool>& Cancel,FString& Error)
{
    if(FPaths::IsRelative(ManifestFile)){Error=TEXT("로컬 manifest 절대 경로가 필요합니다");return false;}
    const FString Base=LocalCapturePath::Full(FPaths::GetPath(ManifestFile));FString Json;
    if(!LocalCapturePath::SafeDescendant(Base,ManifestFile)||!ReadUtf8(ManifestFile,TrainBatchContract::ManifestBytes,Json)){Error=TEXT("로컬 manifest UTF-8/경로/용량 오류");return false;}
    FTrainBatchMessage M;if(!TrainBatchCodec::Parse(Json,M,Error)||M.Kind!=FTrainBatchMessage::EKind::Job||Cancel.load())return false;
    for(const auto& Spec:M.Job.Scenarios)
    {
        FString Name=Spec.InputFile;Name.ReplaceInline(TEXT("\\"),TEXT("/"));TArray<FString> Parts;Name.ParseIntoArray(Parts,TEXT("/"));
        if(Name.IsEmpty()||Name.Contains(TEXT(":"))||!FPaths::IsRelative(Name)||Parts.Contains(TEXT(".."))||!LocalCapturePath::SafeDescendant(Base,Base/Name)){Error=TEXT("input_file은 manifest 폴더 내부의 상대 경로여야 합니다");return false;}
    }
    if(!Receive(M,Error))return false;
    for(const auto& Spec:M.Job.Scenarios)
    {
        if(Cancel.load()){Error=TEXT("로컬 가져오기 취소");return false;}FString Raw;
        if(!ReadUtf8(Base/Spec.InputFile,TrainBatchContract::ScenarioBytes,Raw)){Error=TEXT("시나리오 원본 파일 읽기/UTF-8/용량 오류");return false;}
        auto* J=Find(M.JobId);const int32 Index=J->Items.IndexOfByPredicate([&](const auto& I){return I.Spec.Id==Spec.Id;});
        if(!StoreInput(*J,Index,Raw,Error))return false;
    }
    return true;
}
TSharedPtr<FVirtualDatasetFramePlan,ESPMode::ThreadSafe> FTrainBatchStorage::Prepare(const FString& JobId,int32 Index,const FTransform& Track,ESlabInputUnit Dimension,ESlabInputUnit Position,uint32 Configuration,int64 EstimatedFrameBytes,FString& Error)
{
    auto* J=Find(JobId);if(!J||!J->Items.IsValidIndex(Index)||J->Items[Index].State!=ETrainBatchItemState::Waiting){Error=TEXT("생성할 대기 항목이 없습니다");return nullptr;}
    auto& I=J->Items[Index];FString Raw;FSlabScenarioDataPtr D;
    uint64 RemainingFrames=0;for(const auto& Item:J->Items)if(Item.State==ETrainBatchItemState::Waiting){int32 N=0;if(!TrainBatchCodec::SampleCount(Item.Spec.Duration,N,Error))return nullptr;RemainingFrames+=N;}
    if(EstimatedFrameBytes<=0||uint64(EstimatedFrameBytes)>MAX_uint64/FMath::Max<uint64>(1,RemainingFrames)||!HasDiskSpace(RemainingFrames*uint64(EstimatedFrameBytes),Error))return nullptr;
    auto P=MakeShared<FVirtualDatasetFramePlan,ESPMode::ThreadSafe>();
    if(!ReadUtf8(InputPath(*J,Index),TrainBatchContract::ScenarioBytes,Raw)||!TrainBatchCodec::ValidateScenario(I.Spec,Raw,D,Error)||!TrainBatchCodec::Plan(*D,I.Spec.Duration,Track,Dimension,Position,*P,Error))return nullptr;
    P->BatchJobId=JobId;P->ScenarioId=I.Spec.Id;P->RunId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    P->LocalStorageRoot=TrainBatchCodec::JobsRoot();P->Root=TrainBatchCodec::RunDirectory(JobId,I.Spec.Id,P->RunId);P->SlabConfiguration=Configuration;P->CaptureCamera=J->Spec.CaptureCamera;P->Pipeline=true;
    if(!IsValidDatasetPlanRoot(*P)){Error=TEXT("학습 실행 출력 경로 거절");return nullptr;}
    I.RunId=P->RunId;I.RunDirectory=P->Root;I.State=ETrainBatchItemState::Running;I.Message=TEXT("생성 중");
    if(!Snapshot(*J,TEXT("scenario_started"),Error)){I.State=ETrainBatchItemState::Failed;return nullptr;}return P;
}
bool FTrainBatchStorage::Verify(FTrainBatchJobView& Job,int32 Index,const std::atomic<bool>& Cancel,FString& Error)
{
    auto& I=Job.Items[Index];const FString Root=I.RunDirectory;int32 Frames=0;if(!TrainBatchCodec::SampleCount(I.Spec.Duration,Frames,Error))return false;
    FString Text;TSharedPtr<FJsonObject> O;double Done=0;
    FString Schema;
    if(!ReadUtf8(Root/TEXT("dataset.json"),2*1024*1024,Text)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O)||!O->TryGetStringField(TEXT("schema_version"),Schema)||Schema!=TEXT("ma0t10.dataset.v1")||!O->TryGetNumberField(TEXT("completed_scenes"),Done)||Done!=Frames){Error=TEXT("dataset 완료 수/schema 불일치");return false;}
    FString Checksums;
    auto HashFile=[&](const FString& Relative)->bool
    {
        const FString Path=Root/Relative;const int64 Size=IFileManager::Get().FileSize(*Path);TArray<uint8> B;FString Hash;
        if(Cancel.load()||Size<0||Size>64ll*1024*1024||!LocalCapturePath::SafeDescendant(Root,Path)||!FFileHelper::LoadFileToArray(B,*Path)||B.Num()!=Size||!TrainBatchCodec::Sha256(B.GetData(),B.Num(),Hash))return false;
        Checksums+=Hash+TEXT("  ")+Relative+TEXT("\n");return true;
    };
    if(!HashFile(TEXT("dataset.json")))return false;
    for(int32 K=0;K<Frames;++K)
    {
        if(Cancel.load()){Error=TEXT("결과 검증 취소");return false;}
        const FString Frame=FString::Printf(TEXT("scene_%05d"),K);const FString Dir=Root/Frame;TArray<uint8> Bytes;FVirtualPcdDocument Doc;FString DecodeError;
        if(!LocalCapturePath::SafeDescendant(Root,Dir/TEXT("lidar.pcd"))){Error=TEXT("결과 경로 거절");return false;}
        const int64 PcdSize=IFileManager::Get().FileSize(*(Dir/TEXT("lidar.pcd")));
        if(PcdSize<0||PcdSize>16ll*1024*1024||!FFileHelper::LoadFileToArray(Bytes,*(Dir/TEXT("lidar.pcd")))||!FVirtualPcdCodec::Decode(Bytes,Doc,DecodeError)||Doc.Encoding!=TEXT("binary")||Doc.PointBytes!=33||Doc.PointCount>250000||IFileManager::Get().FileSize(*(Dir/TEXT("lidar_labels.bin")))!=int64(Doc.PointCount)*5){Error=TEXT("PCD/점별 라벨 결과 검증 실패");return false;}
        FString Scenario,Run;double T=0;
        if(!ReadUtf8(Dir/TEXT("label.json"),1024*1024,Text)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O)||!O->TryGetStringField(TEXT("schema_version"),Schema)||Schema!=TEXT("ma0t10.dataset.scene.v1")||!O->TryGetStringField(TEXT("scenario_id"),Scenario)||Scenario!=I.Spec.Id||!O->TryGetStringField(TEXT("run_uuid"),Run)||Run!=I.RunId||!O->TryGetNumberField(TEXT("simulation_elapsed_s"),T)||!FMath::IsFinite(T)||FMath::Abs(T-double(K)/20)>1.e-6){Error=TEXT("확정 프레임 ID/시간축/schema 불일치");return false;}
        for(const TCHAR* Name:{TEXT("lidar.pcd"),TEXT("lidar_labels.bin"),TEXT("label.json")})if(!HashFile(Frame/Name))return false;
        if(Job.Spec.CaptureCamera&&!HashFile(Frame/TEXT("rgb.png")))return false;
        if(Job.Spec.CameraTruth)for(const TCHAR* Name:{TEXT("depth.exr"),TEXT("seg.png"),TEXT("instance.png")})if(!HashFile(Frame/Name))return false;
    }
    auto Complete=MakeShared<FJsonObject>();FTCHARToUTF8 U(*Checksums);FString Hash;if(!TrainBatchCodec::Sha256(U.Get(),U.Length(),Hash))return false;
    Complete->SetStringField(TEXT("schema"),TEXT("ma0t10.train.complete.v1"));Complete->SetStringField(TEXT("job_id"),Job.Spec.Id);Complete->SetStringField(TEXT("scenario_id"),I.Spec.Id);Complete->SetStringField(TEXT("run_uuid"),I.RunId);Complete->SetNumberField(TEXT("frames"),Frames);Complete->SetStringField(TEXT("checksums_sha256"),Hash);
    if(!AtomicText(Root/TEXT("checksums.sha256"),Checksums,false)||!AtomicText(Root/TEXT("complete.json"),Encode(Complete),false)){Error=TEXT("완료 marker 확정 실패");return false;}
    I.CompletedFrames=Frames;return true;
}
bool FTrainBatchStorage::Finish(const FString& JobId,int32 Index,bool Success,const FString& Reason,const std::atomic<bool>& Cancel,FString& Error)
{
    auto* J=Find(JobId);if(!J||!J->Items.IsValidIndex(Index))return false;auto& I=J->Items[Index];
    const bool Valid=Success&&Verify(*J,Index,Cancel,Error);
    I.State=Valid?ETrainBatchItemState::Completed:ETrainBatchItemState::Failed;I.Message=Valid?TEXT("완료 · 저장 결과 검증됨"):!Error.IsEmpty()?Error:Reason.IsEmpty()?TEXT("결과 파일 검증 실패"):Reason;
    J->Message=J->CompleteCount()==J->Items.Num()?TEXT("Job 완료"):TEXT("대기/실패 항목 확인");
    if(!Snapshot(*J,Valid?TEXT("scenario_completed"):TEXT("scenario_failed"),Error))return false;
    return Valid;
}

namespace
{
bool ConfirmedFiles(const FString& Root,const FString& JobId,const FTrainBatchItemView& Item,FString& Error)
{
    int32 ExpectedFrames=0;if(!TrainBatchCodec::SampleCount(Item.Spec.Duration,ExpectedFrames,Error))return false;
    const auto Marker=TrainBatchProcess::Read(Root/TEXT("complete.json"));FString Id,Scenario,Run,Expected,Text;double Frames=0;
    if(!Marker||!Marker->TryGetStringField(TEXT("job_id"),Id)||Id!=JobId||!Marker->TryGetStringField(TEXT("scenario_id"),Scenario)||Scenario!=Item.Spec.Id||!Marker->TryGetStringField(TEXT("run_uuid"),Run)||Run!=Item.RunId||!Marker->TryGetNumberField(TEXT("frames"),Frames)||Frames!=ExpectedFrames||!Marker->TryGetStringField(TEXT("checksums_sha256"),Expected)||!FTrainBatchStorage::ReadUtf8(Root/TEXT("checksums.sha256"),16*1024*1024,Text))
    {Error=TEXT("완료 marker ID/프레임/체크섬 목록 오류");return false;}
    FTCHARToUTF8 U(*Text);FString Actual;if(!TrainBatchCodec::Sha256(U.Get(),U.Length(),Actual)||Actual!=Expected){Error=TEXT("체크섬 목록 손상");return false;}
    TArray<FString> Lines;Text.ParseIntoArrayLines(Lines);if(Lines.IsEmpty()||Lines.Num()>100000)return false;TSet<FString> Seen;
    for(const auto& Line:Lines)
    {
        if(Line.Len()<67||Line.Mid(64,2)!=TEXT("  ")){Error=TEXT("체크섬 행 형식 오류");return false;}
        const FString Relative=Line.Mid(66),Path=Root/Relative;if(Relative.Contains(TEXT("\\"))||Relative.Contains(TEXT(":"))||!FPaths::IsRelative(Relative)||Seen.Contains(Relative.ToLower())||!LocalCapturePath::SafeDescendant(Root,Path))return false;
        Seen.Add(Relative.ToLower());const int64 Size=IFileManager::Get().FileSize(*Path);TArray<uint8> Bytes;
        if(Size<0||Size>64ll*1024*1024||!FFileHelper::LoadFileToArray(Bytes,*Path)||!TrainBatchCodec::Sha256(Bytes.GetData(),Bytes.Num(),Actual)||Actual!=Line.Left(64))
        {Error=TEXT("결과 파일 SHA256 불일치");return false;}
    }
    return true;
}
}
bool FTrainBatchStorage::Restore(const FString& JobId,FString& Error)
{
    if(!TrainBatchCodec::CanonicalGuid(JobId)||Find(JobId)||Jobs.Num()>=TrainBatchContract::MaxSessionJobs){Error=TEXT("복원 Job ID/세션 상태 오류");return false;}
    const FString Root=TrainBatchCodec::JobDirectory(JobId);FString Raw;
    if(!LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),Root)||!ReadUtf8(Root/TEXT("input/job.json"),TrainBatchContract::ManifestBytes,Raw))return false;
    FTrainBatchMessage M;if(!TrainBatchCodec::Parse(Raw,M,Error)||M.Kind!=FTrainBatchMessage::EKind::Job||M.Job.Id!=JobId)return false;
    const auto State=TrainBatchProcess::Read(Root/TEXT("job.json"),2*1024*1024);FString Hash,Commit,JournalHash;double Sequence=0;
    if(!State||!State->TryGetStringField(TEXT("manifest_sha256"),Hash)||Hash!=M.Job.ManifestSha256||!State->TryGetStringField(TEXT("project_commit"),Commit)||Commit!=(FString(TEXT(MA0T10_PROJECT_COMMIT)).IsEmpty()?FString(TEXT("unknown")):FString(TEXT(MA0T10_PROJECT_COMMIT)))||!State->TryGetNumberField(TEXT("sequence"),Sequence)||!State->TryGetStringField(TEXT("journal_sha256"),JournalHash))
    {Error=TEXT("복원 manifest/hash/실행 버전 불일치");return false;}
    FString Journal;if(!ReadUtf8(Root/TEXT("journal.ndjson"),16*1024*1024,Journal))return false;TArray<FString> Lines;Journal.ParseIntoArrayLines(Lines);FString Previous;int64 Seq=0;bool SnapshotFound=Sequence==0;
    for(const auto& Line:Lines)
    {
        TSharedPtr<FJsonObject> Event;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line),Event)||!Event)return false;
        FString Prev,Stored;double Number=0;if(!Event->TryGetStringField(TEXT("previous_sha256"),Prev)||Prev!=Previous||!Event->TryGetNumberField(TEXT("sequence"),Number)||Number!=Seq+1||!Event->TryGetStringField(TEXT("sha256"),Stored))return false;
        Event->RemoveField(TEXT("sha256"));const FString Body=JournalJson(Event.ToSharedRef());FTCHARToUTF8 B(*Body);FString H;if(!TrainBatchCodec::Sha256(B.Get(),B.Length(),H)||H!=Stored){Error=TEXT("journal chain 손상");return false;}
        ++Seq;Previous=Stored;if(Seq==Sequence){if(Stored!=JournalHash)return false;SnapshotFound=true;}
    }
    if(!SnapshotFound){Error=TEXT("snapshot이 journal chain에 없습니다");return false;}
    const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;if(!State->TryGetArrayField(TEXT("scenarios"),Items)||Items->Num()!=M.Job.Scenarios.Num())return false;
    FTrainBatchJobView J;J.Spec=M.Job;
    for(int32 N=0;N<J.Spec.Scenarios.Num();++N)
    {
        FTrainBatchItemView I;I.Spec=J.Spec.Scenarios[N];FSlabScenarioDataPtr D;FString Input;
        if(ReadUtf8(Root/TEXT("input")/I.Spec.Id/TEXT("original.json"),TrainBatchContract::ScenarioBytes,Input)&&TrainBatchCodec::ValidateScenario(I.Spec,Input,D,Error)){I.State=ETrainBatchItemState::Waiting;I.Message=TEXT("재개 대기");}
        else {Error.Reset();I.State=ETrainBatchItemState::Receiving;I.Message=TEXT("원본 보충 필요");}
        const auto O=(*Items)[N]->AsObject();FString Id,Status;
        if(!O||!O->TryGetStringField(TEXT("scenario_id"),Id)||Id!=I.Spec.Id||!O->TryGetStringField(TEXT("state"),Status))return false;
        if(Status==TEXT("completed"))
        {
            O->TryGetStringField(TEXT("run_uuid"),I.RunId);if(!TrainBatchCodec::CanonicalGuid(I.RunId))return false;
            I.RunDirectory=TrainBatchCodec::RunDirectory(JobId,I.Spec.Id,I.RunId);
            if(ConfirmedFiles(I.RunDirectory,JobId,I,Error)){I.State=ETrainBatchItemState::Completed;if(!TrainBatchCodec::SampleCount(I.Spec.Duration,I.CompletedFrames,Error))return false;I.Message=TEXT("완료 결과 복원됨");}
            else {Error.Reset();I.RunId.Reset();I.RunDirectory.Reset();}
        }
        else if(Status==TEXT("failed")){I.State=ETrainBatchItemState::Failed;O->TryGetStringField(TEXT("message"),I.Message);}
        J.Items.Add(MoveTemp(I));
    }
    Sequences.Add(JobId,Seq);PreviousHashes.Add(JobId,Previous);J.Message=TEXT("디스크 Job 복원됨");Jobs.Add(MoveTemp(J));return Snapshot(Jobs.Last(),TEXT("job_restored"),Error);
}
bool FTrainBatchStorage::MarkLeased(const FString& JobId,int32 Index,const FString& Lease,FString& Error)
{
    auto* J=Find(JobId);if(!J||!J->Items.IsValidIndex(Index)||!TrainBatchCodec::CanonicalGuid(Lease))return false;
    auto& I=J->Items[Index];if(I.State==ETrainBatchItemState::Completed)return false;I.State=ETrainBatchItemState::Running;I.RunId=Lease;I.RunDirectory.Reset();I.Message=TEXT("외부 워커 실행 중");return Snapshot(*J,TEXT("worker_leased"),Error);
}
bool FTrainBatchStorage::ReleaseLease(const FString& JobId,int32 Index,FString& Error)
{
    auto* J=Find(JobId);if(!J||!J->Items.IsValidIndex(Index)||J->Items[Index].State==ETrainBatchItemState::Completed)return false;
    auto& I=J->Items[Index];I.State=ETrainBatchItemState::Waiting;I.RunId.Reset();I.RunDirectory.Reset();I.Message=TEXT("재할당 대기");return Snapshot(*J,TEXT("worker_requeued"),Error);
}
bool FTrainBatchStorage::Adopt(const FString& JobId,int32 Index,const FString& ApprovedWorkerRoot,const FString& RunId,FString& Error)
{
    auto* J=Find(JobId);if(!J||!J->Items.IsValidIndex(Index)||!TrainBatchCodec::CanonicalGuid(RunId))return false;auto& I=J->Items[Index];
    I.RunId=RunId;const FString Source=ApprovedWorkerRoot/JobId/TEXT("scenarios")/I.Spec.Id/(TEXT("run_")+RunId),Destination=TrainBatchCodec::RunDirectory(JobId,I.Spec.Id,RunId);
    if(!LocalCapturePath::SafeDescendant(ApprovedWorkerRoot,Source)||!LocalCapturePath::SafeDescendant(TrainBatchCodec::JobsRoot(),Destination))return false;
    if(!IFileManager::Get().DirectoryExists(*Destination))
    {
        if(!ConfirmedFiles(Source,JobId,I,Error))return false;
        // Same-volume rename preserves closed, verified files and has no partial destination.
        if(!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination),true))return false;
        if(!IPlatformFile::GetPlatformPhysical().MoveFile(*Destination,*Source))
        {Error=TEXT("워커 결과 이동 실패 · 같은 볼륨의 Saved root가 필요합니다");return false;}
    }
    if(!ConfirmedFiles(Destination,JobId,I,Error))return false;
    I.RunDirectory=Destination;I.State=ETrainBatchItemState::Completed;if(!TrainBatchCodec::SampleCount(I.Spec.Duration,I.CompletedFrames,Error))return false;I.Message=TEXT("워커 결과 검증 완료");return Snapshot(*J,TEXT("worker_result_adopted"),Error);
}
