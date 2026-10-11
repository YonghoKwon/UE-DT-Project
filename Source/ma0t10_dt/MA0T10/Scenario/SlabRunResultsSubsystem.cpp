#include "SlabRunResultsSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "SlabScenarioReplaySubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/SceneComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "JsonObjectConverter.h"
#include "Async/Async.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
namespace { constexpr uint8 LedgerAccepted=1,LedgerSubmitted=2,LedgerReceipt=4,LedgerFailed=8,LedgerConsumed=16,LedgerInvalid=32; FCriticalSection SlabReportWriterMutex; }
void USlabRunResultsSubsystem::Initialize(FSubsystemCollectionBase& C)
{
    Super::Initialize(C);C.InitializeDependency<UVirtualSensorHighThroughputTransportSubsystem>();C.InitializeDependency<UVirtualSensorPlacementSubsystem>();
    auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
    ObservationHandle=Raw->OnTransportObservation.AddUObject(this,&ThisClass::ObserveTransport);
    ReceiveHandle=Raw->OnReceived.AddUObject(this,&ThisClass::HandleReceive);
}
void USlabRunResultsSubsystem::Deinitialize()
{
    bEnding=true;ClearPlacementBindings();
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
void USlabRunResultsSubsystem::Changed(bool Broadcast){++ResultsRevision;if(Broadcast&&!bEnding)OnResultsChanged.Broadcast();}
#if WITH_DEV_AUTOMATION_TESTS
FVirtualSensorResourceSnapshot USlabRunResultsSubsystem::GetResourceSnapshotForTesting() const
{
    FVirtualSensorResourceSnapshot S;S.Owner=TEXT("slab-results");
    int64 Strings=0;for(const auto& P:Requests)Strings+=P.Key.GetAllocatedSize()+P.Value.RequestId.GetAllocatedSize();
    S.Counts={{TEXT("results"),Results.Num()},{TEXT("requests"),Requests.Num()},{TEXT("writers"),Writes.Num()},
        {TEXT("observation_delegate"),ObservationHandle.IsValid()},{TEXT("receive_delegate"),ReceiveHandle.IsValid()}};
    S.OwnedCpuBytes.Add(TEXT("ledger_strings"),Strings);S.OwnedCpuBytes.Add(TEXT("ledger_container"),Requests.GetAllocatedSize());
    S.OwnedCpuBytes.Add(TEXT("result_container"),Results.GetAllocatedSize());return S;
}
TArray<USlabRunResultsSubsystem::FFrameAudit> USlabRunResultsSubsystem::GetRunFrameAuditForTesting(const FString& Run) const
{
    TArray<FFrameAudit> Audit;if(Run==ActiveRun){Audit.Reserve(Requests.Num());for(const auto& P:Requests)if(P.Value.Flags&LedgerAccepted)Audit.Add({P.Key,P.Value.RequestId,P.Value.Flags});}return Audit;
}
#endif
FSlabRunDeliverySummary* USlabRunResultsSubsystem::Find(const FString& Run){return Results.FindByPredicate([&](const auto& R){return R.RunUUID==Run;});}
bool USlabRunResultsSubsystem::GetRunResult(const FString& Run,FSlabRunDeliverySummary& Out) const
{if(const auto* R=Results.FindByPredicate([&](const auto& Item){return Item.RunUUID==Run;})){Out=*R;return true;}return false;}
bool USlabRunResultsSubsystem::GetRunComparison(const FString& AId,const FString& BId,FSlabRunComparison& Comparison,FString& Reason) const
{
    FSlabRunDeliverySummary A,B;
    if(!GetRunResult(AId,A)||!GetRunResult(BId,B)){Comparison=FSlabRunComparison();Reason=TEXT("비교 대상 보관 종료 또는 실행을 찾을 수 없습니다.");return false;}
    return BuildRunComparison(A,B,Comparison,Reason);
}
void USlabRunResultsSubsystem::BeginRun(const FString& Run,const FString& Scenario,ESlabExecutionPolicy Policy,const FVirtualSlabSensorOutputSelection& Outputs)
{
    if(bEnding||Run.IsEmpty()||Find(Run))return;
    ClearPlacementBindings();ActiveRun=Run;Requests.Reset();auto& R=Results.AddDefaulted_GetRef();R.RunUUID=Run;R.ScenarioUUID=Scenario;R.Policy=Policy;R.Outputs=Outputs;R.PreparedUtc=FDateTime::UtcNow();
    R.bHasStreamDetails=true;
    if(auto* GI=GetWorld()->GetGameInstance())if(auto* Archive=GI->GetSubsystem<USlabScenarioReplaySubsystem>())if(const auto Data=Archive->GetValidatedScenario(Scenario))R.ScenarioContentHash=Data->OriginalContentSha1;
    while(Results.Num()>20)Results.RemoveAt(0);Changed(true);
}
void USlabRunResultsSubsystem::SetSensors(const FString& Run,const TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>>& Sensors,UVirtualSensorTransportComponent* Transport)
{
    if(const auto* R=Find(Run);!R||R->bFinalized||Run!=ActiveRun||R->StartedUtc.GetTicks()>0)return;
    ClearPlacementBindings();
    if(auto* R=Find(Run))
    {
        R->Sensors.Reset();for(const auto& Pair:Sensors)if(auto* S=Pair.Value.Get())
        {
            FVirtualSensorEditableState State;S->ReadEditableState(State);State.ActorTransform=FTransform::Identity;
            auto& Item=R->Sensors.AddDefaulted_GetRef();Item.SensorId=Pair.Key;Item.SettingsRevision=S->GetConfigurationRevision();
            Item.bHasAcquisitionCounts=true;
            FJsonObjectConverter::UStructToJsonObjectString(State,Item.Configuration);
            Item.RequestedHz=1.0f/FMath::Max(.001f,S->GetSensorKind()==EVirtualSensorKind::Camera?State.CameraCaptureInterval:State.LidarScanInterval);
            Item.Placement=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->GetCurrentPlacementSnapshot(S);
            if(auto* Root=S->GetRootComponent())
            {
                const FString Id=Pair.Key;TWeakObjectPtr<AVirtualSensorActorBase> WeakSensor(S);
                auto Handle=Root->TransformUpdated.AddWeakLambda(this,[this,Run,Id,WeakSensor](USceneComponent*,EUpdateTransformFlags,ETeleportType)
                {
                    auto* Result=Find(Run);if(!Result||Result->bFinalized||Run!=ActiveRun||!WeakSensor.IsValid())return;
                    auto* Entry=Result->Sensors.FindByPredicate([&](const auto& E){return E.SensorId==Id;});if(!Entry||Entry->Placement.bChangedDuringRun)return;
                    const auto Current=WeakSensor->GetActorTransform();
                    if(!UVirtualSensorPlacementSubsystem::SamePose(Entry->Placement.WorldTransform,Current)||!Entry->Placement.WorldTransform.GetScale3D().Equals(Current.GetScale3D(),.001))
                    {Entry->Placement.bChangedDuringRun=true;Changed();}
                });
                PlacementBindings.Add({Root,Handle});
            }
        }
    }
    if(BoundTransport.Get()!=Transport)
    {if(BoundTransport.IsValid())BoundTransport->OnDataSent.RemoveDynamic(this,&ThisClass::HandleEngineResult);BoundTransport=Transport;if(Transport)Transport->OnDataSent.AddUniqueDynamic(this,&ThisClass::HandleEngineResult);}
}
void USlabRunResultsSubsystem::ClearPlacementBindings()
{for(const auto& B:PlacementBindings)if(auto* Root=B.Component.Get())Root->TransformUpdated.Remove(B.Handle);PlacementBindings.Reset();}
void USlabRunResultsSubsystem::RecordAcquisition(const FString& Run,bool Success)
{if(auto* R=Find(Run);R&&!R->bFinalized){if(Success){++R->Acquired;++R->UnattributedAcquired;}else{++R->AcquisitionFailures;++R->UnattributedAcquisitionFailures;}Changed();}}
void USlabRunResultsSubsystem::RecordAcquisition(const FString& Run,const FString& Id,int64 Frame,bool Success)
{
    auto* R=Find(Run);if(!R||R->bFinalized||Run!=ActiveRun)return;
    const FString Key=TEXT("acq|")+Id+TEXT("|")+LexToString(Frame);
    if(!Requests.Contains(Key)&&Requests.Num()>=65536){R->Warning=TEXT("실행별 진단 식별자 상한 65536건 초과");return;}
    auto& Entry=Requests.FindOrAdd(Key);if(Entry.Flags&192)return;Entry.Flags|=Success?64:128;
    if(Success)++R->Acquired;else++R->AcquisitionFailures;
    if(auto* Sensor=R->Sensors.FindByPredicate([&](const auto& S){return S.SensorId==Id;}))
    {Sensor->bHasAcquisitionCounts=true;if(Success)++Sensor->Acquired;else++Sensor->AcquisitionFailures;}
    else {if(Success)++R->UnattributedAcquired;else++R->UnattributedAcquisitionFailures;}
    Changed();
}
void USlabRunResultsSubsystem::MarkStopped(const FString& Run){if(auto* R=Find(Run);R&&!R->bFinalized&&R->StoppedUtc.GetTicks()==0)R->StoppedUtc=FDateTime::UtcNow();}
void USlabRunResultsSubsystem::SetAppliedOutputs(const FString& Run,const FVirtualSlabSensorOutputSelection& Outputs){if(auto* R=Find(Run);R&&!R->bFinalized)R->AppliedOutputs=Outputs;}
void USlabRunResultsSubsystem::SetMeasuredRates(const FString& Run,const TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>>& Sensors)
{if(auto* R=Find(Run);R&&!R->bFinalized&&Run==ActiveRun)for(auto& S:R->Sensors)if(const auto* P=Sensors.Find(S.SensorId);P&&P->IsValid()){S.MeasuredHz=P->Get()->GetSensorRuntimeStatus().MeasuredAcquisitionRateHz;const bool Camera=P->Get()->GetSensorKind()==EVirtualSensorKind::Camera;if(!Camera){S.bHasCompatibilityJsonRate=true;S.CompatibilityJsonHz=P->Get()->GetSensorRuntimeStatus().MeasuredOutputRateHz;}const bool Selected=Camera?R->AppliedOutputs.bCameraImage:(R->AppliedOutputs.bPointCloud||R->AppliedOutputs.bLidarTelemetry);if(Selected&&S.RequestedHz>0&&S.MeasuredHz<S.RequestedHz*.95f)R->Warning=TEXT("측정률 미달이 있습니다. 승인 프레임의 Broker 수락 결과와 구분하세요.");}}
int64 USlabRunResultsSubsystem::GetUnconfirmedCount(const FString& Run) const {const auto* R=Results.FindByPredicate([&](const auto& S){return S.RunUUID==Run;});return R?R->Unfinished:0;}
void USlabRunResultsSubsystem::GatherUnconfirmedKeys(const FString& Run,TSet<FString>& Outputs,TSet<FString>& SensorFrames) const
{
    if(Run!=ActiveRun)return;
    for(const auto& P:Requests)if((P.Value.Flags&LedgerAccepted)&&!(P.Value.Flags&(LedgerReceipt|LedgerFailed)))
    {Outputs.Add(P.Key);int32 Separator;if(P.Key.FindChar(TEXT('|'),Separator))SensorFrames.Add(P.Key.Mid(Separator+1));}
}
void USlabRunResultsSubsystem::RecordTerminalSnapshot(const FString& Run,const FSlabRunTerminalSnapshot& Snapshot)
{if(auto* R=Find(Run);R&&!R->bFinalized){R->TerminalSnapshot=Snapshot;R->Unfinished=Snapshot.UniqueUnfinished;Changed();}}
void USlabRunResultsSubsystem::UpdateProgress(const FString& Run,const FString& Mtl,int64 Frame,double Elapsed)
{if(auto* R=Find(Run);R&&!R->bFinalized){R->MtlNo=Mtl;R->LastSlabFrame=Frame;R->ElapsedSec=Elapsed;if(R->StartedUtc.GetTicks()==0)R->StartedUtc=FDateTime::UtcNow();Changed();}}
void USlabRunResultsSubsystem::ObserveTransport(const FVirtualSensorTransportObservation& E)
{
    auto* R=Find(E.RunId);if(!R||R->bFinalized||E.RunId!=ActiveRun)return;
    if(E.SensorId.IsEmpty()||int32(E.Kind)<0||int32(E.Kind)>2)
    {if(E.Phase==EVirtualSensorTransportObservationPhase::Failed){if(!E.RequestId.IsEmpty()){const FString K=TEXT("unknown|")+E.RequestId;if(!Requests.Contains(K)&&Requests.Num()>=65536)return;auto& U=Requests.FindOrAdd(K);if(U.Flags&LedgerFailed)return;U.Flags|=LedgerFailed;}++R->DeliveryFailures;++R->UnattributedDeliveryFailures;Changed();}return;}
    const FString Key=LexToString(static_cast<int32>(E.Kind))+TEXT("|")+E.SensorId+TEXT("|")+LexToString(E.FrameId);
    if(!Requests.Contains(Key)&&Requests.Num()>=65536){R->Warning=TEXT("실행별 진단 식별자 상한 65536건 초과");R->Reason=R->Warning;R->DeliveryFailures=FMath::Max<int64>(1,R->DeliveryFailures);return;}
    auto& S=Requests.FindOrAdd(Key);if(S.RequestId.IsEmpty())S.RequestId=E.RequestId;const bool WasPending=(S.Flags&LedgerAccepted)&&!(S.Flags&(LedgerReceipt|LedgerFailed));
    uint8 Flag=E.Phase==EVirtualSensorTransportObservationPhase::Accepted?LedgerAccepted:E.Phase==EVirtualSensorTransportObservationPhase::Submitted?LedgerSubmitted:
        E.Phase==EVirtualSensorTransportObservationPhase::Receipt?LedgerReceipt:E.Phase==EVirtualSensorTransportObservationPhase::Consumed?LedgerConsumed:
        E.Phase==EVirtualSensorTransportObservationPhase::ValidationFailed?LedgerInvalid:LedgerFailed;
    if(S.Flags&Flag)return;S.Flags|=Flag;
    const bool IsPending=(S.Flags&LedgerAccepted)&&!(S.Flags&(LedgerReceipt|LedgerFailed));R->Unfinished+=int64(IsPending)-int64(WasPending);
    R->bHasStreamDetails=true;auto* Detail=R->StreamDetails.FindByPredicate([&](const auto& D){return D.SensorId==E.SensorId&&D.Kind==E.Kind;});
    if(!Detail){Detail=&R->StreamDetails.AddDefaulted_GetRef();Detail->SensorId=E.SensorId;Detail->Kind=E.Kind;}
    Detail->AcceptedUnconfirmed+=int64(IsPending)-int64(WasPending);
    if(Flag==LedgerAccepted)++Detail->Accepted;else if(Flag==LedgerSubmitted)++Detail->Submitted;else if(Flag==LedgerReceipt)++Detail->Receipts;
    else if(Flag==LedgerConsumed)++Detail->ConsumerValidated;else if(Flag==LedgerInvalid)++Detail->ConsumerInvalid;else ++Detail->Failures;
    const double At=E.MonotonicSeconds>0?E.MonotonicSeconds:FPlatformTime::Seconds();
    auto Rate=[&](int64 Count,double& First,double& Last,double& Hz,bool& Known)
    {if(First<=0)First=At;Last=FMath::Max(Last,At);Known=Count>1&&Last>First;if(Known)Hz=double(Count-1)/(Last-First);};
    if(Flag==LedgerSubmitted)Rate(Detail->Submitted,Detail->FirstSubmitted,Detail->LastSubmitted,Detail->SubmittedHz,Detail->bHasSubmittedRate);
    if(Flag==LedgerReceipt)Rate(Detail->Receipts,Detail->FirstReceipt,Detail->LastReceipt,Detail->ReceiptHz,Detail->bHasReceiptRate);
    if(Flag==LedgerConsumed)Rate(Detail->ConsumerValidated,Detail->FirstConsumer,Detail->LastConsumer,Detail->ConsumerValidatedHz,Detail->bHasConsumerRate);
    if(Flag==LedgerAccepted)++R->Accepted;else if(Flag==LedgerSubmitted)++R->Submitted;else if(Flag==LedgerReceipt)++R->Receipts;
    else if(Flag==LedgerConsumed)++R->ConsumerValidated;else if(Flag==LedgerInvalid)++R->ConsumerInvalid;else {++R->DeliveryFailures;if(R->Reason.IsEmpty())R->Reason=E.Message;}
    Changed();
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
    R->bFinalized=true;R->FinishedUtc=FDateTime::UtcNow();R->ConsumerSnapshotUtc=R->FinishedUtc;if(R->StoppedUtc.GetTicks()==0)R->StoppedUtc=R->FinishedUtc;
    R->AcquisitionFailures=FMath::Max<int64>(R->AcquisitionFailures,S.AcquisitionFailures);R->DeliveryFailures=FMath::Max<int64>(R->DeliveryFailures,S.StreamFailures);
    int64 AttributedAcquisitionFailures=0;for(const auto& Sensor:R->Sensors)AttributedAcquisitionFailures+=Sensor.AcquisitionFailures;
    R->UnattributedAcquisitionFailures=FMath::Max<int64>(R->UnattributedAcquisitionFailures,R->AcquisitionFailures-AttributedAcquisitionFailures);
    int64 Attributed=0;for(const auto& D:R->StreamDetails)Attributed+=D.Failures;R->UnattributedDeliveryFailures=FMath::Max<int64>(R->UnattributedDeliveryFailures,R->DeliveryFailures-Attributed);
    R->Unfinished=FMath::Max(R->Unfinished,S.UnfinishedFrames);
    if(!S.RequiredDataError.IsEmpty())R->Reason=S.RequiredDataError;else if(R->Reason.IsEmpty())R->Reason=S.Message;
    R->Outcome=PreparationFailed?ESlabRunDeliveryOutcome::PreparationFailed:
        !S.RequiredDataError.IsEmpty()||S.State==EVirtualSlabSessionState::Incomplete||R->DeliveryFailures||R->AcquisitionFailures||R->Unfinished?ESlabRunDeliveryOutcome::Partial:
        S.bAborted?ESlabRunDeliveryOutcome::UserStopped:(!R->Outputs.HasAnyOutput()||S.bObservationOnly||!S.Outputs.HasAnyOutput())?ESlabRunDeliveryOutcome::ObservationCompleted:
        R->Accepted>0&&R->Receipts==R->Accepted?ESlabRunDeliveryOutcome::BrokerAccepted:ESlabRunDeliveryOutcome::Partial;
    if(R->Outcome==ESlabRunDeliveryOutcome::ObservationCompleted&&R->Outputs.HasAnyOutput())R->Warning=TEXT("요청한 출력은 미송신이며 관찰만 완료했습니다.");
    if(R->ConsumerInvalid){if(!R->Warning.IsEmpty())R->Warning+=TEXT("\n");R->Warning+=TEXT("자체 수신 검증 실패가 있습니다. Broker 수락과 별도 결과입니다.");}
    if(R->Sensors.ContainsByPredicate([](const auto& Entry){return Entry.Placement.bChangedDuringRun;}))
    {if(!R->Warning.IsEmpty())R->Warning+=TEXT("\n");R->Warning+=TEXT("실행 중 외부에서 센서 배치가 변경됐습니다. 시작 배치와 프레임별 측정 자세를 확인하세요.");}
    ClearPlacementBindings();
    R->SaveState=TEXT("저장 요청 접수");QueueReport(*R);Changed(true);
}
FString USlabRunResultsSubsystem::DescribeStreamDetails(const FSlabRunDeliverySummary& R)
{
    FString T=TEXT("센서별 측정 완료/실패");
    for(const auto& S:R.Sensors)T+=S.bHasAcquisitionCounts?FString::Printf(TEXT("\n%s: %lld/%lld"),*S.SensorId,S.Acquired,S.AcquisitionFailures):TEXT("\n")+S.SensorId+TEXT(": 확인 불가");
    T+=TEXT("\n센서·출력별 승인/제출/receipt/실패/승인후 미확인/자체검증/invalid");
    if(!R.bHasStreamDetails)T+=TEXT("\n확인 불가 · 이전 기록에 상세 집계 없음");
    for(const auto& D:R.StreamDetails)
    {
        const TCHAR* K=D.Kind==EVirtualSensorStreamKind::PointCloud?TEXT("PCD"):D.Kind==EVirtualSensorStreamKind::CameraImage?TEXT("Camera"):TEXT("LiDAR 정보");
        T+=FString::Printf(TEXT("\n%s / %s: %lld/%lld/%lld/%lld/%lld/%lld/%lld"),*D.SensorId,K,D.Accepted,D.Submitted,D.Receipts,D.Failures,D.AcceptedUnconfirmed,D.ConsumerValidated,D.ConsumerInvalid);
        auto Hz=[](bool Known,double H){return Known?FString::Printf(TEXT("%.2f"),H):FString(TEXT("N/A"));};
        T+=TEXT("\n  기록 이벤트 간격 Hz · 제출/receipt/자체검증 ")+Hz(D.bHasSubmittedRate,D.SubmittedHz)+TEXT(" / ")+Hz(D.bHasReceiptRate,D.ReceiptHz)+TEXT(" / ")+Hz(D.bHasConsumerRate,D.ConsumerValidatedHz);
    }
    T+=FString::Printf(TEXT("\n귀속 불가 · 측정/측정실패/전송오류 %lld/%lld/%lld"),R.UnattributedAcquired,R.UnattributedAcquisitionFailures,R.UnattributedDeliveryFailures);
    return T;
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
    if(Writes.Num()>=2){Tick(0);if(Writes.Num()>=2){if(auto* R=Find(S.RunUUID))R->SaveState=TEXT("요약 저장 대기 상한 초과");Changed();return;}}
    const auto Run=S.RunUUID,Json=SerializeSummary(S);
    FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/ScenarioRuns"));
#if WITH_DEV_AUTOMATION_TESTS
    if(!ReportDirectoryOverrideForTests.IsEmpty())Dir=ReportDirectoryOverrideForTests;
#endif
    WritingRuns.Add(Run);Writes.Add(Async(EAsyncExecution::ThreadPool,[Run,Json,Dir](){return WriteReport(Run,Json,Dir,100);}));
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
    while(Files.Num()>MaxFiles)
    {
        const int32 Index=Files.IndexOfByPredicate([&](const FString& Name){return FPaths::ConvertRelativePathToFull(Directory/Name)!=Result.Path;});
        if(Index==INDEX_NONE)break;
        const FString Path=FPaths::ConvertRelativePathToFull(Directory/Files[Index]);
        if(!FPaths::IsUnderDirectory(Path,Directory)||!IFileManager::Get().Delete(*Path)){Result.Error=TEXT("요약은 저장됐지만 오래된 결과 파일 정리에 실패했습니다.");break;}
        Files.RemoveAt(Index);
    }
    return Result;
}
void USlabRunResultsSubsystem::ApplyWriteResult(const FWriteResult& S)
{WritingRuns.Remove(S.Run);if(auto* R=Find(S.Run)){R->SaveState=S.Success?(S.Error.IsEmpty()?TEXT("저장 완료"):S.Error):S.Error;if(S.Success)R->ReportPath=S.Path;}Changed(true);}
bool USlabRunResultsSubsystem::CanRetryRunReportSave(const FString& Run,FString& Reason) const
{
    Reason.Reset();FSlabRunDeliverySummary R;
    if(bEnding){Reason=TEXT("실행 월드 종료 중입니다.");return false;}
    if(!GetRunResult(Run,R)){Reason=TEXT("현재 PIE 보관 목록에 없는 실행입니다.");return false;}
    if(!R.bFinalized){Reason=TEXT("실행 결과가 아직 확정되지 않았습니다.");return false;}
    if(WritingRuns.Contains(Run)){Reason=TEXT("이 결과의 요약 저장이 진행 중입니다.");return false;}
    if(R.SaveState==TEXT("저장 완료")){Reason=TEXT("이미 저장된 결과입니다.");return false;}
    if(Writes.Num()>=2){Reason=TEXT("요약 저장 작업이 가득 찼습니다. 잠시 후 다시 시도하세요.");return false;}
    return true;
}
bool USlabRunResultsSubsystem::RetryRunReportSave(const FString& Run,FString& Reason)
{
    if(!IsInGameThread()||!CanRetryRunReportSave(Run,Reason))return false;
    auto* R=Find(Run);R->SaveState=TEXT("저장 요청 접수");QueueReport(*R);Changed(true);return WritingRuns.Contains(Run);
}
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
