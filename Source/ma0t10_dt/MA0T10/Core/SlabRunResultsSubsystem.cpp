#include "SlabRunResultsSubsystem.h"
#include "VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "JsonObjectConverter.h"
#include "Async/Async.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
namespace { constexpr uint8 Accepted=1,Submitted=2,Receipt=4,Failed=8,Consumed=16,Invalid=32; FCriticalSection SlabReportWriterMutex; }
void USlabRunResultsSubsystem::Initialize(FSubsystemCollectionBase& C)
{
    Super::Initialize(C);C.InitializeDependency<UVirtualSensorHighThroughputTransportSubsystem>();
    auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
    ObservationHandle=Raw->OnTransportObservation.AddUObject(this,&ThisClass::ObserveTransport);
    ReceiveHandle=Raw->OnReceived.AddUObject(this,&ThisClass::HandleReceive);
}
void USlabRunResultsSubsystem::Deinitialize()
{
    bEnding=true;
    if(auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()){Raw->OnTransportObservation.Remove(ObservationHandle);Raw->OnReceived.Remove(ReceiveHandle);}
    if(BoundTransport.IsValid())BoundTransport->OnDataSent.RemoveDynamic(this,&ThisClass::HandleEngineResult);
    // Workers perform only file I/O; they never await a GameThread callback.
    for(auto& W:Writes)ApplyWriteResult(W.Get());Writes.Reset();Requests.Reset();Super::Deinitialize();
}
void USlabRunResultsSubsystem::Tick(float)
{
    for(int32 I=Writes.Num()-1;I>=0;--I)if(Writes[I].IsReady()){ApplyWriteResult(Writes[I].Get());Writes.RemoveAt(I);}
}
TStatId USlabRunResultsSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(USlabRunResultsSubsystem,STATGROUP_Tickables);}
FSlabRunDeliverySummary* USlabRunResultsSubsystem::Find(const FString& Run){return Results.FindByPredicate([&](const auto& R){return R.RunUUID==Run;});}
bool USlabRunResultsSubsystem::GetRunResult(const FString& Run,FSlabRunDeliverySummary& Out) const
{if(const auto* R=Results.FindByPredicate([&](const auto& Item){return Item.RunUUID==Run;})){Out=*R;return true;}return false;}
void USlabRunResultsSubsystem::BeginRun(const FString& Run,const FString& Scenario,ESlabExecutionPolicy Policy,const FVirtualSlabSensorOutputSelection& Outputs)
{
    if(bEnding||Run.IsEmpty()||Find(Run))return;
    ActiveRun=Run;Requests.Reset();auto& R=Results.AddDefaulted_GetRef();R.RunUUID=Run;R.ScenarioUUID=Scenario;R.Policy=Policy;R.Outputs=Outputs;R.PreparedUtc=FDateTime::UtcNow();
    while(Results.Num()>20)Results.RemoveAt(0);OnResultsChanged.Broadcast();
}
void USlabRunResultsSubsystem::SetSensors(const FString& Run,const TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>>& Sensors,UVirtualSensorTransportComponent* Transport)
{
    if(auto* R=Find(Run))
    {
        R->Sensors.Reset();for(const auto& Pair:Sensors)if(auto* S=Pair.Value.Get())
        {
            FVirtualSensorEditableState State;S->ReadEditableState(State);State.ActorTransform=FTransform::Identity;
            auto& Item=R->Sensors.AddDefaulted_GetRef();Item.SensorId=Pair.Key;Item.SettingsRevision=S->GetConfigurationRevision();
            FJsonObjectConverter::UStructToJsonObjectString(State,Item.Configuration);
            Item.RequestedHz=1.0f/FMath::Max(.001f,S->GetSensorKind()==EVirtualSensorKind::Camera?State.CameraCaptureInterval:State.LidarScanInterval);
        }
    }
    if(BoundTransport.Get()!=Transport)
    {if(BoundTransport.IsValid())BoundTransport->OnDataSent.RemoveDynamic(this,&ThisClass::HandleEngineResult);BoundTransport=Transport;if(Transport)Transport->OnDataSent.AddUniqueDynamic(this,&ThisClass::HandleEngineResult);}
}
void USlabRunResultsSubsystem::RecordAcquisition(const FString& Run,bool Success)
{if(auto* R=Find(Run);R&&!R->bFinalized){if(Success)++R->Acquired;else++R->AcquisitionFailures;}}
void USlabRunResultsSubsystem::MarkStopped(const FString& Run){if(auto* R=Find(Run);R&&R->StoppedUtc.GetTicks()==0)R->StoppedUtc=FDateTime::UtcNow();}
void USlabRunResultsSubsystem::SetAppliedOutputs(const FString& Run,const FVirtualSlabSensorOutputSelection& Outputs){if(auto* R=Find(Run);R&&!R->bFinalized)R->AppliedOutputs=Outputs;}
void USlabRunResultsSubsystem::SetMeasuredRates(const FString& Run,const TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>>& Sensors)
{if(auto* R=Find(Run))for(auto& S:R->Sensors)if(const auto* P=Sensors.Find(S.SensorId);P&&P->IsValid()){S.MeasuredHz=P->Get()->GetSensorRuntimeStatus().MeasuredAcquisitionRateHz;if(S.RequestedHz>0&&S.MeasuredHz<S.RequestedHz*.95f)R->Warning=TEXT("측정률 미달이 있습니다. 승인 프레임의 Broker 수락 결과와 구분하세요.");}}
int64 USlabRunResultsSubsystem::GetUnconfirmedCount(const FString& Run) const {const auto* R=Results.FindByPredicate([&](const auto& S){return S.RunUUID==Run;});return R?R->Unfinished:0;}
void USlabRunResultsSubsystem::UpdateProgress(const FString& Run,const FString& Mtl,int64 Frame,double Elapsed)
{if(auto* R=Find(Run);R&&!R->bFinalized){R->MtlNo=Mtl;R->LastSlabFrame=Frame;R->ElapsedSec=Elapsed;if(R->StartedUtc.GetTicks()==0)R->StartedUtc=FDateTime::UtcNow();}}
void USlabRunResultsSubsystem::ObserveTransport(const FVirtualSensorTransportObservation& E)
{
    auto* R=Find(E.RunId);if(!R||R->bFinalized||E.RunId!=ActiveRun||E.SensorId.IsEmpty())return;
    const FString Key=LexToString(static_cast<int32>(E.Kind))+TEXT("|")+E.SensorId+TEXT("|")+LexToString(E.FrameId);
    if(!Requests.Contains(Key)&&Requests.Num()>=65536){R->Warning=TEXT("실행별 진단 식별자 상한 65536건 초과");R->Reason=R->Warning;R->DeliveryFailures=FMath::Max<int64>(1,R->DeliveryFailures);return;}
    auto& S=Requests.FindOrAdd(Key);if(S.RequestId.IsEmpty())S.RequestId=E.RequestId;const bool WasPending=(S.Flags&Accepted)&&!(S.Flags&(Receipt|Failed));
    uint8 Flag=E.Phase==EVirtualSensorTransportObservationPhase::Accepted?Accepted:E.Phase==EVirtualSensorTransportObservationPhase::Submitted?Submitted:
        E.Phase==EVirtualSensorTransportObservationPhase::Receipt?Receipt:E.Phase==EVirtualSensorTransportObservationPhase::Consumed?Consumed:
        E.Phase==EVirtualSensorTransportObservationPhase::ValidationFailed?Invalid:Failed;
    if(S.Flags&Flag)return;S.Flags|=Flag;
    const bool IsPending=(S.Flags&Accepted)&&!(S.Flags&(Receipt|Failed));R->Unfinished+=int64(IsPending)-int64(WasPending);
    if(Flag==Accepted)++R->Accepted;else if(Flag==Submitted)++R->Submitted;else if(Flag==Receipt)++R->Receipts;
    else if(Flag==Consumed)++R->ConsumerValidated;else if(Flag==Invalid)++R->ConsumerInvalid;else {++R->DeliveryFailures;if(R->Reason.IsEmpty())R->Reason=E.Message;}
}
void USlabRunResultsSubsystem::RecordOutputFailure(const FString& Run,const FString& Sensor,int32 Kind,int64 Frame,const FString& Reason)
{FVirtualSensorTransportObservation E;E.RunId=Run;E.SensorId=Sensor;E.Kind=static_cast<EVirtualSensorStreamKind>(Kind);E.FrameId=Frame;E.Phase=EVirtualSensorTransportObservationPhase::Failed;E.Message=Reason;ObserveTransport(E);}
void USlabRunResultsSubsystem::HandleEngineResult(const FVirtualSensorTransportResult& R)
{
    if(R.RunId.IsEmpty()||R.bConsumerAckReceived||R.DataKind.IsEmpty())return;
    FVirtualSensorTransportObservation E;E.RunId=R.RunId;E.SensorId=R.SensorId;E.FrameId=R.FrameId;E.RequestId=R.RequestId;
    E.Kind=R.DataKind.Contains(TEXT("pointcloud"))?EVirtualSensorStreamKind::PointCloud:R.SensorType==TEXT("camera")?EVirtualSensorStreamKind::CameraImage:EVirtualSensorStreamKind::LidarPayload;
    E.Message=R.Message;
    if(R.bReceiptReceived)E.Phase=EVirtualSensorTransportObservationPhase::Receipt;
    else if(R.bSubmitted&&!R.bReceiptCompleted)
    {E.Phase=EVirtualSensorTransportObservationPhase::Accepted;ObserveTransport(E);E.Phase=EVirtualSensorTransportObservationPhase::Submitted;}
    else E.Phase=EVirtualSensorTransportObservationPhase::Failed;
    ObserveTransport(E);
}
void USlabRunResultsSubsystem::HandleReceive(const TSharedPtr<FVirtualSensorTopicReceivedDataBase>& R)
{
    if(!R.IsValid()||R->bFiltered)return;
    FVirtualSensorTransportObservation E;E.RunId=R->RunId;E.SensorId=R->SensorId;E.FrameId=R->FrameId;E.Kind=static_cast<EVirtualSensorStreamKind>(R->Kind);
    E.RequestId=R->RequestId;E.Phase=R->bValid?EVirtualSensorTransportObservationPhase::Consumed:EVirtualSensorTransportObservationPhase::ValidationFailed;ObserveTransport(E);
}
bool USlabRunResultsSubsystem::HasLedgerFailure(const FString& Run) const
{const auto* R=Results.FindByPredicate([&](const auto& S){return S.RunUUID==Run;});return R&&R->DeliveryFailures>0;}
void USlabRunResultsSubsystem::Finalize(const FVirtualSlabSessionStatus& S,bool PreparationFailed)
{
    auto* R=Find(S.RunId);if(!R||R->bFinalized)return;
    R->bFinalized=true;R->FinishedUtc=FDateTime::UtcNow();if(R->StoppedUtc.GetTicks()==0)R->StoppedUtc=R->FinishedUtc;
    R->AcquisitionFailures=FMath::Max<int64>(R->AcquisitionFailures,S.AcquisitionFailures);R->DeliveryFailures=FMath::Max<int64>(R->DeliveryFailures,S.StreamFailures);
    R->Unfinished=FMath::Max(R->Unfinished,S.UnfinishedFrames);
    if(!S.RequiredDataError.IsEmpty())R->Reason=S.RequiredDataError;else if(R->Reason.IsEmpty())R->Reason=S.Message;
    R->Outcome=PreparationFailed?ESlabRunDeliveryOutcome::PreparationFailed:
        !S.RequiredDataError.IsEmpty()||S.State==EVirtualSlabSessionState::Incomplete||R->DeliveryFailures||R->AcquisitionFailures||R->Unfinished?ESlabRunDeliveryOutcome::Partial:
        S.bAborted?ESlabRunDeliveryOutcome::UserStopped:!R->Outputs.HasAnyOutput()?ESlabRunDeliveryOutcome::ObservationCompleted:
        R->Accepted>0&&R->Receipts==R->Accepted?ESlabRunDeliveryOutcome::BrokerAccepted:ESlabRunDeliveryOutcome::Partial;
    if(R->ConsumerInvalid)R->Warning=TEXT("자체 수신 검증 실패가 있습니다. Broker 수락과 별도 결과입니다.");
    R->SaveState=TEXT("저장 요청 접수");QueueReport(*R);OnResultsChanged.Broadcast();
}
FString USlabRunResultsSubsystem::SerializeSummary(const FSlabRunDeliverySummary& S)
{
    TSharedPtr<FJsonObject> Root=FJsonObjectConverter::UStructToJsonObject(S);
    Root->SetStringField(TEXT("schema"),TEXT("ma0t10.slab-run.v1"));Root->SetStringField(TEXT("completionBoundary"),TEXT("broker-receipt; consumer processing not verified"));
    Root->SetStringField(TEXT("saveState"),TEXT("저장 완료"));
    FString Text;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return Text;
}
void USlabRunResultsSubsystem::QueueReport(const FSlabRunDeliverySummary& S)
{
    if(Writes.Num()>=2){Tick(0);if(Writes.Num()>=2){if(auto* R=Find(S.RunUUID))R->SaveState=TEXT("요약 저장 대기 상한 초과");return;}}
    const auto Run=S.RunUUID,Json=SerializeSummary(S),Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/ScenarioRuns"));
    Writes.Add(Async(EAsyncExecution::ThreadPool,[Run,Json,Dir](){return WriteReport(Run,Json,Dir,100);}));
}
USlabRunResultsSubsystem::FWriteResult USlabRunResultsSubsystem::WriteReport(FString Run,FString Json,FString Directory,int32 MaxFiles)
{
    FScopeLock Lock(&SlabReportWriterMutex);
    FWriteResult Result;Result.Run=Run;FGuid G;if(!FGuid::Parse(Run,G)||!G.IsValid()){Result.Error=TEXT("결과 UUID가 유효하지 않습니다.");return Result;}
    IFileManager::Get().MakeDirectory(*Directory,true);Result.Path=Directory/(TEXT("slab-run-")+G.ToString(EGuidFormats::DigitsWithHyphensLower)+TEXT(".json"));
    const FString Temporary=Result.Path+TEXT(".writing");
    Result.Success=FFileHelper::SaveStringToFile(Json,*Temporary,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)&&IFileManager::Get().Move(*Result.Path,*Temporary,true,true);
    if(!Result.Success&&FPaths::IsUnderDirectory(Temporary,Directory))IFileManager::Get().Delete(*Temporary);
    if(!Result.Success){Result.Error=TEXT("결과 JSON 쓰기 실패");return Result;}
    TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(Directory/TEXT("slab-run-*.json")),true,false);
    Files.RemoveAll([&](const FString& Name){FString Text,Schema;TSharedPtr<FJsonObject> O;return !FFileHelper::LoadFileToString(Text,*(Directory/Name))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O)||!O.IsValid()||!O->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("ma0t10.slab-run.v1");});
    Files.Sort([&](const FString& A,const FString& B){return IFileManager::Get().GetTimeStamp(*(Directory/A))<IFileManager::Get().GetTimeStamp(*(Directory/B));});
    while(Files.Num()>MaxFiles){const FString Path=FPaths::ConvertRelativePathToFull(Directory/Files[0]);if(FPaths::IsUnderDirectory(Path,Directory))IFileManager::Get().Delete(*Path);Files.RemoveAt(0);}
    return Result;
}
void USlabRunResultsSubsystem::ApplyWriteResult(const FWriteResult& S)
{if(auto* R=Find(S.Run)){R->SaveState=S.Success?TEXT("저장 완료"):S.Error;if(S.Success)R->ReportPath=S.Path;}if(!bEnding)OnResultsChanged.Broadcast();}
bool USlabRunResultsSubsystem::OpenRunReport(const FString& Run) const
{FSlabRunDeliverySummary R;if(!GetRunResult(Run,R)||R.ReportPath.IsEmpty()||!IFileManager::Get().FileExists(*R.ReportPath))return false;FPlatformProcess::ExploreFolder(*FPaths::GetPath(R.ReportPath));return true;}
FString USlabRunResultsSubsystem::Describe(const FSlabRunDeliverySummary& R)
{
    const TCHAR* Outcome=R.Outcome==ESlabRunDeliveryOutcome::PreparationFailed?TEXT("준비 실패"):R.Outcome==ESlabRunDeliveryOutcome::UserStopped?TEXT("사용자 중단"):
        R.Outcome==ESlabRunDeliveryOutcome::Partial?TEXT("부분 전송"):R.Outcome==ESlabRunDeliveryOutcome::BrokerAccepted?TEXT("Broker 수락 완료"):
        R.Outcome==ESlabRunDeliveryOutcome::ObservationCompleted?TEXT("관찰 완료"):TEXT("진행 중");
    return FString::Printf(TEXT("%s · %s\n승인/제출/receipt %lld/%lld/%lld · 오류 %lld · 미완료 %lld\n자체 검증(기록 시점) %lld · 검증 오류 %lld\n%s%s%s"),
        Outcome,*R.MtlNo,R.Accepted,R.Submitted,R.Receipts,R.DeliveryFailures,R.Unfinished,R.ConsumerValidated,R.ConsumerInvalid,
        *R.Reason,*FString(R.Warning.IsEmpty()?TEXT(""):TEXT("\n주의: ")+R.Warning),*FString(R.SaveState.IsEmpty()?TEXT(""):TEXT("\n요약 파일: ")+R.SaveState));
}
