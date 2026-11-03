#include "TrainBatchCodec.h"
#include "TrainBatchArchive.h"
#include "Device/Contracts/LocalCapturePath.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"
#include "HAL/PlatformMisc.h"
#include "Json.h"

namespace
{
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double& V)
{return O->TryGetNumberField(Key,V)&&FMath::IsFinite(V);}
bool Integer(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,int64 Min,int64 Max,int64& V)
{double N=0;if(!Number(O,Key,N)||N<Min||N>Max||N!=FMath::FloorToDouble(N))return false;V=int64(N);return true;}
bool Hash(const FString& S)
{if(S.Len()!=64)return false;for(TCHAR C:S)if(!((C>=TEXT('0')&&C<=TEXT('9'))||(C>=TEXT('a')&&C<=TEXT('f'))))return false;return true;}
bool OptionalBool(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,bool& V)
{return !O->HasField(Key)||O->TryGetBoolField(Key,V);}
bool Keys(const TSharedPtr<FJsonObject>& O,std::initializer_list<const TCHAR*> Allowed)
{for(const auto& Field:O->Values){bool Known=false;for(const TCHAR* Key:Allowed)Known|=Field.Key==Key;if(!Known)return false;}return true;}
bool DurationMatches(const FSlabScenarioData& D,double Duration)
{return !D.Rows.IsEmpty()&&D.Rows[0].ElapsedSec==0&&FMath::Abs(D.DurationSec-Duration)<=1.e-6;}
}
bool TrainBatchCodec::SafeId(const FString& Id)
{
    if(Id.IsEmpty()||Id.Len()>64||Id==TEXT(".")||Id.Contains(TEXT("..")))return false;
    const FString Upper=Id.ToUpper();
    if(Upper==TEXT("CON")||Upper==TEXT("PRN")||Upper==TEXT("AUX")||Upper==TEXT("NUL")||
       (Upper.Len()==4&&(Upper.StartsWith(TEXT("COM"))||Upper.StartsWith(TEXT("LPT")))&&Upper[3]>=TEXT('1')&&Upper[3]<=TEXT('9')))return false;
    for(TCHAR C:Id)if(!((C>=TEXT('a')&&C<=TEXT('z'))||(C>=TEXT('A')&&C<=TEXT('Z'))||(C>=TEXT('0')&&C<=TEXT('9'))||C==TEXT('_')||C==TEXT('-')))return false;
    return true;
}
bool TrainBatchCodec::CanonicalGuid(const FString& Id)
{FGuid G;return FGuid::ParseExact(Id,EGuidFormats::DigitsWithHyphens,G)&&G.IsValid()&&G.ToString(EGuidFormats::DigitsWithHyphensLower)==Id;}
bool TrainBatchCodec::Sha256(const void* Bytes,uint32 Size,FString& Out)
{
    Out.Reset();
    if(Size&&!Bytes)return false;
    Out=TrainBatchArchive::Sha256Bytes(Bytes,Size);
    return Out.Len()==64;
}
bool TrainBatchCodec::SampleCount(double Duration,int32& Out,FString& Error)
{
    Out=0;Error.Reset();
    if(!FMath::IsFinite(Duration)||Duration<=0){Error=TEXT("sampled_timeline duration must be finite and positive");return false;}
    const double Samples=Duration*TrainBatchContract::SampleHz;
    if(!FMath::IsFinite(Samples)||Samples-1.e-9>TrainBatchContract::MaxFrames){Error=TEXT("sampled_timeline exceeds the per-scenario frame limit");return false;}
    Out=FMath::Max(1,int32(FMath::CeilToDouble(Samples-1.e-9)));return true;
}
FString TrainBatchCodec::JobsRoot(){return LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("TrainBatch/Jobs"));}
FString TrainBatchCodec::JobDirectory(const FString& JobId){return JobsRoot()/JobId;}
FString TrainBatchCodec::RunDirectory(const FString& JobId,const FString& Id,const FString& RunId)
{return JobDirectory(JobId)/TEXT("scenarios")/Id/(TEXT("run_")+RunId);}

bool TrainBatchCodec::Parse(const FString& Json,FTrainBatchMessage& Out,FString& Error)
{
    using namespace TrainBatchContract;Out=FTrainBatchMessage();Error.Reset();
    auto Reject=[&](const TCHAR* Why){Error=Why;return false;};
    if(Json.Len()>EnvelopeBytes)return Reject(TEXT("학습 envelope 용량 초과"));
    FTCHARToUTF8 Utf8(*Json);if(Utf8.Length()>EnvelopeBytes)return Reject(TEXT("학습 envelope UTF-8 용량 초과"));
    TSharedPtr<FJsonObject> O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),O)||!O)return Reject(TEXT("학습 JSON 구문 오류"));
    FString S,Id,Kind;
    if(!O->TryGetStringField(TEXT("schema"),S)||S!=Schema||!O->TryGetStringField(TEXT("MESSAGE_ID"),S)||S!=MessageId||
       !O->TryGetStringField(TEXT("job_id"),Id)||!CanonicalGuid(Id)||!O->TryGetStringField(TEXT("kind"),Kind))return Reject(TEXT("학습 schema/MESSAGE_ID/JobId/kind 오류"));
    Out.JobId=Id;
    if(Kind==TEXT("seal")){Out.Kind=FTrainBatchMessage::EKind::Seal;return Keys(O,{TEXT("schema"),TEXT("MESSAGE_ID"),TEXT("job_id"),TEXT("kind")})||Reject(TEXT("seal의 미지원 필드"));}
    if(Kind==TEXT("scenario"))
    {
        Out.Kind=FTrainBatchMessage::EKind::Scenario;FString Raw;
        if(!Keys(O,{TEXT("schema"),TEXT("MESSAGE_ID"),TEXT("job_id"),TEXT("kind"),TEXT("scenario_id"),TEXT("input_sha256"),TEXT("scenario_json")}))return Reject(TEXT("학습 항목의 미지원 필드"));
        if(!O->TryGetStringField(TEXT("scenario_id"),Out.ScenarioId)||!SafeId(Out.ScenarioId)||!O->TryGetStringField(TEXT("input_sha256"),Out.InputSha256)||!Hash(Out.InputSha256)||!O->TryGetStringField(TEXT("scenario_json"),Raw))return Reject(TEXT("학습 항목 ID/hash/원문 오류"));
        FTCHARToUTF8 U(*Raw);Out.InputBytes=U.Length();FString Actual;
        if(Out.InputBytes<=0||Out.InputBytes>ScenarioBytes||!Sha256(U.Get(),U.Length(),Actual)||Actual!=Out.InputSha256)return Reject(TEXT("학습 항목 원문 용량/SHA256 불일치"));
        return FSlabScenarioCodec::Parse(Raw,Out.Scenario,Error,false);
    }
    if(Kind!=TEXT("job"))return Reject(TEXT("알 수 없는 학습 메시지 종류"));
    if(!Keys(O,{TEXT("schema"),TEXT("MESSAGE_ID"),TEXT("job_id"),TEXT("kind"),TEXT("preset_id"),TEXT("dimension_unit"),TEXT("position_unit"),TEXT("scenario_count"),TEXT("worker_limit"),TEXT("execution"),TEXT("capture"),TEXT("scenarios"),TEXT("archive_format")}))return Reject(TEXT("Job의 미지원 필드"));
    if(Utf8.Length()>ManifestBytes)return Reject(TEXT("Job manifest 256KiB 초과"));
    auto& J=Out.Job;J.Id=Id;J.OriginalJson=Json;
    if(!Sha256(Utf8.Get(),Utf8.Length(),J.ManifestSha256))return Reject(TEXT("SHA256 제공자 사용 불가"));
    if(!O->TryGetStringField(TEXT("preset_id"),J.PresetId)||!SafeId(J.PresetId)||!O->TryGetStringField(TEXT("dimension_unit"),J.DimensionUnit)||!O->TryGetStringField(TEXT("position_unit"),J.PositionUnit))return Reject(TEXT("preset/입력 단위를 명시하세요"));
    for(const auto& U:{J.DimensionUnit,J.PositionUnit})if(U!=TEXT("mm")&&U!=TEXT("cm")&&U!=TEXT("m"))return Reject(TEXT("단위는 mm/cm/m 중 하나여야 합니다"));
    const TSharedPtr<FJsonObject>* E=nullptr;double Hz=0;
    if(!O->TryGetObjectField(TEXT("execution"),E)||!Keys(*E,{TEXT("mode"),TEXT("sample_hz")})||!(*E)->TryGetStringField(TEXT("mode"),S)||S!=TEXT("sampled_timeline")||!Number(*E,TEXT("sample_hz"),Hz)||Hz!=SampleHz)return Reject(TEXT("1A는 20Hz sampled_timeline만 지원합니다"));
    int64 Count=0,Workers=0;
    if(!Integer(O,TEXT("scenario_count"),1,MaxScenarios,Count)||!Integer(O,TEXT("worker_limit"),1,4,Workers))return Reject(TEXT("시나리오 개수/워커 상한 오류"));
    J.WorkerLimit=int32(Workers);
    if(O->HasField(TEXT("archive_format"))&&(!O->TryGetStringField(TEXT("archive_format"),J.ArchiveFormat)||(J.ArchiveFormat!=TEXT("zip")&&J.ArchiveFormat!=TEXT("tar.gz"))))return Reject(TEXT("압축 선택은 ZIP/tar.gz만 지원합니다 · tar.zst 보류"));
    const TSharedPtr<FJsonObject>* C=nullptr;
    if(O->HasField(TEXT("capture")))
    {
        bool Lidar=true,Imu=false,Fidelity=false;
        if(!O->TryGetObjectField(TEXT("capture"),C)||!Keys(*C,{TEXT("lidar"),TEXT("camera"),TEXT("camera_truth"),TEXT("imu"),TEXT("lidar_fidelity")})||!OptionalBool(*C,TEXT("lidar"),Lidar)||!Lidar||!OptionalBool(*C,TEXT("camera"),J.CaptureCamera)||!OptionalBool(*C,TEXT("camera_truth"),J.CameraTruth)||!OptionalBool(*C,TEXT("imu"),Imu)||!OptionalBool(*C,TEXT("lidar_fidelity"),Fidelity)||Imu||Fidelity||(!J.CaptureCamera&&J.CameraTruth))return Reject(TEXT("캡처 옵션 오류 · 1A IMU/fidelity 미지원"));
    }
    const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;
    if(!O->TryGetArrayField(TEXT("scenarios"),Items)||Items->Num()!=Count)return Reject(TEXT("scenario_count와 목록 수가 다릅니다"));
    TSet<FString> Ids;int64 Bytes=0,Frames=0;double Seconds=0;
    for(const auto& V:*Items)
    {
        const TSharedPtr<FJsonObject>* Item=nullptr;FTrainBatchScenarioSpec Spec;
        if(!V||!V->TryGetObject(Item)||!(*Item)->TryGetStringField(TEXT("scenario_id"),Spec.Id)||!SafeId(Spec.Id)||Ids.Contains(Spec.Id.ToLower())||!(*Item)->TryGetStringField(TEXT("input_sha256"),Spec.InputSha256)||!Hash(Spec.InputSha256)||!Integer(*Item,TEXT("input_bytes"),1,ScenarioBytes,Spec.InputBytes)||!Number(*Item,TEXT("duration_sec"),Spec.Duration)||Spec.Duration<=0||Spec.Duration>MaxDuration)return Reject(TEXT("시나리오 descriptor/중복/길이/용량 오류"));
        if(!Keys(*Item,{TEXT("scenario_id"),TEXT("input_sha256"),TEXT("input_bytes"),TEXT("duration_sec"),TEXT("input_file")}))return Reject(TEXT("시나리오 descriptor의 미지원 필드"));
        if((*Item)->HasField(TEXT("input_file"))&&!(*Item)->TryGetStringField(TEXT("input_file"),Spec.InputFile))return Reject(TEXT("input_file 형식 오류"));
        int32 N=0;if(!SampleCount(Spec.Duration,N,Error))return false;Frames+=N;Bytes+=Spec.InputBytes;Seconds+=Spec.Duration;
        if(N>MaxFrames||Frames>MaxJobFrames||Bytes>JobInputBytes||Seconds>MaxTotalDuration)return Reject(TEXT("Job 프레임/입력/총 길이 상한 초과"));
        Ids.Add(Spec.Id.ToLower());J.Scenarios.Add(MoveTemp(Spec));
    }
    return true;
}
bool TrainBatchCodec::ValidateScenario(const FTrainBatchScenarioSpec& Spec,const FString& Json,FSlabScenarioDataPtr& Out,FString& Error)
{
    FTCHARToUTF8 U(*Json);FString H;
    if(U.Length()!=Spec.InputBytes||U.Length()>TrainBatchContract::ScenarioBytes||!Sha256(U.Get(),U.Length(),H)||H!=Spec.InputSha256){Error=TEXT("보관 원문 크기/SHA256 불일치");return false;}
    if(!FSlabScenarioCodec::Parse(Json,Out,Error,false))return false;
    TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),O);
    const TSharedPtr<FJsonObject>* Meta=nullptr;double Duration=0;
    if(!O->TryGetObjectField(TEXT("_meta"),Meta)||!Number(*Meta,TEXT("duration_sec"),Duration)||FMath::Abs(Duration-Spec.Duration)>1.e-6||!DurationMatches(*Out,Spec.Duration))
    {Out.Reset();Error=TEXT("명시 duration_sec/첫 행 0초/Job 길이를 확인하세요");return false;}
    return true;
}
bool TrainBatchCodec::Plan(const FSlabScenarioData& Data,double Duration,const FTransform& Track,ESlabInputUnit Dimension,ESlabInputUnit Position,FVirtualDatasetFramePlan& Out,FString& Error)
{
    int32 Count=0;if(!SampleCount(Duration,Count,Error))return false;
    if(!FMath::IsFinite(Duration)||Duration<=0||Duration>TrainBatchContract::MaxDuration||!DurationMatches(Data,Duration)||Track.ContainsNaN())
    {Error=TEXT("시간축 길이/원본/배치 오류");return false;}
    const auto& First=Data.Rows[0];const FVector Size(FSlabScenarioCodec::ToCm(First.Length,Dimension),FSlabScenarioCodec::ToCm(First.Width,Dimension),FSlabScenarioCodec::ToCm(First.Thickness,Dimension));
    Out.Frames.Reset(Count);
    for(int32 K=0;K<Count;++K)
    {
        const double T=double(K)/TrainBatchContract::SampleHz;FSlabScenarioRow Row;int32 Lower=0;
        if(!FSlabScenarioCodec::Sample(Data,T,Row,Lower)){Error=TEXT("시간축 샘플링 실패");Out.Frames.Reset();return false;}
        FVirtualDatasetPlannedFrame Frame;Frame.SlabPose=USlabMotionComponent::BuildPose(Row,Track,Size,Position);Frame.SizeCm=Size;
        Frame.FrameNo=Row.FrameNo;Frame.ElapsedSec=T;Frame.SourceElapsedSec=Row.ElapsedSec;Frame.MtlNo=Row.MtlNo;Out.Frames.Add(MoveTemp(Frame));
    }
    Out.Source=TEXT("train_batch");Out.SourceUUID=Data.SourceUUID;Out.FramePolicy=TEXT("sampled_timeline_20hz");return true;
}
