#include "SlabRunComparison.h"
#include "Json.h"
#include "SlabRunResultsSubsystem.h"

namespace
{
bool SameOutputs(const FVirtualSlabSensorOutputSelection& A,const FVirtualSlabSensorOutputSelection& B)
{return A.bPointCloud==B.bPointCloud&&A.bCameraImage==B.bCameraImage&&A.bLidarTelemetry==B.bLidarTelemetry;}
void JsonDifferences(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,const FString& Path,TArray<FString>& Out)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type){Out.Add(Path);return;}
    switch(A->Type)
    {
    case EJson::Object:
    {
        const auto AO=A->AsObject(),BO=B->AsObject();TSet<FString> Keys;
        for(const auto& P:AO->Values)Keys.Add(P.Key);for(const auto& P:BO->Values)Keys.Add(P.Key);
        auto Ordered=Keys.Array();Ordered.Sort();for(const auto& K:Ordered)JsonDifferences(AO->Values.FindRef(K),BO->Values.FindRef(K),Path.IsEmpty()?K:Path+TEXT(".")+K,Out);break;
    }
    case EJson::Array:
    {
        const auto& AA=A->AsArray();const auto& BA=B->AsArray();if(AA.Num()!=BA.Num()){Out.Add(Path);break;}
        for(int32 I=0;I<AA.Num();++I)JsonDifferences(AA[I],BA[I],Path+FString::Printf(TEXT("[%d]"),I),Out);break;
    }
    case EJson::Number:if(A->AsNumber()!=B->AsNumber())Out.Add(Path);break;
    case EJson::String:if(A->AsString()!=B->AsString())Out.Add(Path);break;
    case EJson::Boolean:if(A->AsBool()!=B->AsBool())Out.Add(Path);break;
    default:break;
    }
}
bool CompareConfiguration(const FString& A,const FString& B,TArray<FString>& Out)
{
    TSharedPtr<FJsonObject> AO,BO;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(A),AO)||!AO.IsValid()||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(B),BO)||!BO.IsValid())return false;
    JsonDifferences(MakeShared<FJsonValueObject>(AO),MakeShared<FJsonValueObject>(BO),FString(),Out);return true;
}
FString SensorKey(const FSlabRunSensorSnapshot& S)
{return FString::Printf(TEXT("%d|%s"),S.Placement.MapPackage.IsEmpty()?-1:int32(S.Placement.SensorKind),*S.SensorId);}
FString Ratio(const FSlabRunDeliverySummary& R)
{return R.Accepted>0?FString::Printf(TEXT("%.2f%%"),double(R.Receipts)*100/R.Accepted):TEXT("N/A");}
}
bool BuildRunComparison(const FSlabRunDeliverySummary& A,const FSlabRunDeliverySummary& B,FSlabRunComparison& Out,FString& Reason)
{
    Out=FSlabRunComparison();Reason.Reset();
    if(A.RunUUID.IsEmpty()||B.RunUUID.IsEmpty()||A.RunUUID==B.RunUUID){Reason=TEXT("서로 다른 두 실행을 선택하세요.");return false;}
    if(!A.bFinalized||!B.bFinalized){Reason=TEXT("송신 정리까지 확정된 실행만 비교할 수 있습니다.");return false;}
    if(A.ScenarioUUID.IsEmpty()||A.ScenarioUUID!=B.ScenarioUUID){Reason=TEXT("같은 시나리오 UUID의 실행을 선택하세요.");return false;}
    Out.A=A;Out.B=B;
    if(A.Policy!=B.Policy)Out.ConditionDifferences.Add(TEXT("실행 정책"));
    if(!SameOutputs(A.Outputs,B.Outputs))Out.ConditionDifferences.Add(TEXT("요청 출력"));
    if(!SameOutputs(A.AppliedOutputs,B.AppliedOutputs))Out.ConditionDifferences.Add(TEXT("실제 적용 출력"));
    if(A.ScenarioContentHash.IsEmpty()||B.ScenarioContentHash.IsEmpty())Out.UnknownConditions.Add(TEXT("시나리오 원문 hash 확인 불가"));
    else if(A.ScenarioContentHash!=B.ScenarioContentHash)Out.ConditionDifferences.Add(TEXT("시나리오 원문"));
    if(A.LastSlabFrame!=B.LastSlabFrame||!FMath::IsNearlyEqual(A.ElapsedSec,B.ElapsedSec,1.e-6))Out.ConditionDifferences.Add(TEXT("진행된 시나리오 구간"));
    if(A.Outcome!=B.Outcome)Out.ConditionDifferences.Add(TEXT("종료 결과"));
    TMap<FString,const FSlabRunSensorSnapshot*> AS,BS;TSet<FString> Keys;
    for(const auto& S:A.Sensors){const auto K=SensorKey(S);if(AS.Contains(K)){Reason=TEXT("A 실행의 센서 식별자가 중복되어 비교할 수 없습니다.");return false;}AS.Add(K,&S);Keys.Add(K);}
    for(const auto& S:B.Sensors){const auto K=SensorKey(S);if(BS.Contains(K)){Reason=TEXT("B 실행의 센서 식별자가 중복되어 비교할 수 없습니다.");return false;}BS.Add(K,&S);Keys.Add(K);}
    auto Ordered=Keys.Array();Ordered.Sort();
    for(const auto& Key:Ordered)
    {
        auto& C=Out.Sensors.AddDefaulted_GetRef();const auto SA=AS.FindRef(Key),SB=BS.FindRef(Key);
        C.bPresentA=SA!=nullptr;C.bPresentB=SB!=nullptr;const auto* Identity=SA?SA:SB;C.SensorId=Identity->SensorId;C.Kind=Identity->Placement.SensorKind;
        C.bKindKnown=!Identity->Placement.MapPackage.IsEmpty();
        if(SA)C.A=*SA;if(SB)C.B=*SB;
        if(!SA||!SB){Out.ConditionDifferences.Add(C.SensorId+TEXT(": 대상 센서 구성"));continue;}
        if(SA->Placement.MapPackage.IsEmpty()||SB->Placement.MapPackage.IsEmpty())Out.UnknownConditions.Add(C.SensorId+TEXT(": 시작 배치 확인 불가"));
        else if(SA->Placement.MapPackage!=SB->Placement.MapPackage)Out.ConditionDifferences.Add(C.SensorId+TEXT(": 맵"));
        else
        {
            const auto& PA=SA->Placement.WorldTransform;const auto& PB=SB->Placement.WorldTransform;
            if(PA.ContainsNaN()||PB.ContainsNaN()||!PA.GetRotation().IsNormalized()||!PB.GetRotation().IsNormalized())Out.UnknownConditions.Add(C.SensorId+TEXT(": 시작 자세 확인 불가"));
            else {C.bPoseDeltaAvailable=true;C.TranslationDeltaCm=FVector::Dist(PA.GetLocation(),PB.GetLocation());C.RotationDeltaDegrees=FMath::RadiansToDegrees(PA.GetRotation().AngularDistance(PB.GetRotation()));C.ScaleDelta=PB.GetScale3D()-PA.GetScale3D();}
        }
        if(!CompareConfiguration(SA->Configuration,SB->Configuration,C.ChangedSettings))Out.UnknownConditions.Add(C.SensorId+TEXT(": 설정 확인 불가"));
        else if(!C.ChangedSettings.IsEmpty())Out.ConditionDifferences.Add(C.SensorId+TEXT(": 설정"));
        if(!FMath::IsNearlyEqual(SA->RequestedHz,SB->RequestedHz))Out.ConditionDifferences.Add(C.SensorId+TEXT(": 요청 측정률"));
        if(SA->Placement.bChangedDuringRun||SB->Placement.bChangedDuringRun)Out.ConditionDifferences.Add(C.SensorId+TEXT(": 실행 중 외부 배치 변경"));
    }
    if(A.Sensors.IsEmpty()||B.Sensors.IsEmpty())Out.UnknownConditions.Add(TEXT("센서 시작 snapshot 없음"));
    if(A.Outcome==ESlabRunDeliveryOutcome::Partial||A.Outcome==ESlabRunDeliveryOutcome::UserStopped||A.Outcome==ESlabRunDeliveryOutcome::PreparationFailed||B.Outcome==ESlabRunDeliveryOutcome::Partial||B.Outcome==ESlabRunDeliveryOutcome::UserStopped||B.Outcome==ESlabRunDeliveryOutcome::PreparationFailed)
        Out.Warnings.Add(TEXT("중단·실패·부분 전송 결과가 포함되어 있습니다."));
    if(!A.Warning.IsEmpty())Out.Warnings.Add(TEXT("A: ")+A.Warning);if(!B.Warning.IsEmpty())Out.Warnings.Add(TEXT("B: ")+B.Warning);
    Out.bRecordedConditionsMatch=Out.ConditionDifferences.IsEmpty()&&Out.UnknownConditions.IsEmpty();
    Out.Status=Out.bRecordedConditionsMatch?TEXT("기록된 조건 일치"):!Out.ConditionDifferences.IsEmpty()?TEXT("조건 차이가 있는 참고 비교"):TEXT("일부 조건 확인 불가 · 참고 비교");return true;
}
FString DescribeRunComparison(const FSlabRunComparison& C)
{
    FString T=C.Status+TEXT("\nA: ")+C.A.RunUUID+TEXT("\nB: ")+C.B.RunUUID+TEXT("\n시나리오: ")+C.A.ScenarioUUID;
    auto Outputs=[](const FVirtualSlabSensorOutputSelection& O){return FString::Printf(TEXT("PCD %s / Camera %s / LiDAR %s"),O.bPointCloud?TEXT("ON"):TEXT("OFF"),O.bCameraImage?TEXT("ON"):TEXT("OFF"),O.bLidarTelemetry?TEXT("ON"):TEXT("OFF"));};
    auto Outcome=[](ESlabRunDeliveryOutcome O)->FString{switch(O){case ESlabRunDeliveryOutcome::BrokerAccepted:return TEXT("Broker 수락 완료");case ESlabRunDeliveryOutcome::ObservationCompleted:return TEXT("관찰 완료");case ESlabRunDeliveryOutcome::Partial:return TEXT("부분 전송");case ESlabRunDeliveryOutcome::UserStopped:return TEXT("사용자 중단");case ESlabRunDeliveryOutcome::PreparationFailed:return TEXT("준비 실패");default:return TEXT("대기");}};
    for(const auto* R:{&C.A,&C.B})
    {const TCHAR* Label=R==&C.A?TEXT("A"):TEXT("B");T+=FString::Printf(TEXT("\n%s 정책: %s · 결과: %s · frame %lld / %.2f초\n%s 요청: %s\n%s 적용: %s\n%s 종료 사유: %s\n%s 원문 hash: %s"),Label,R->Policy==ESlabExecutionPolicy::RequireData?TEXT("데이터 필수"):TEXT("관찰 허용"),*Outcome(R->Outcome),R->LastSlabFrame,R->ElapsedSec,Label,*Outputs(R->Outputs),Label,*Outputs(R->AppliedOutputs),Label,*R->Reason,Label,R->ScenarioContentHash.IsEmpty()?TEXT("확인 불가"):*R->ScenarioContentHash);}
    for(const auto& D:C.ConditionDifferences)T+=TEXT("\n조건 차이: ")+D;for(const auto& U:C.UnknownConditions)T+=TEXT("\n")+U;
    for(const auto& S:C.Sensors)
    {
        T+=TEXT("\n\n")+S.SensorId+TEXT(" · ")+(!S.bKindKnown?TEXT("종류 확인 불가"):S.Kind==EVirtualSensorKind::Camera?TEXT("Camera"):TEXT("LiDAR"));
        if(!S.bPresentA||!S.bPresentB){T+=S.bPresentA?TEXT(" · A에만 있음"):TEXT(" · B에만 있음");continue;}
        auto Pose=[](const FSlabRunSensorSnapshot& X){return X.Placement.MapPackage.IsEmpty()?FString(TEXT("확인 불가")):FString::Printf(TEXT("%s · %s cm · %s · scale %s · %s"),*X.Placement.MapPackage,*X.Placement.WorldTransform.GetLocation().ToString(),*X.Placement.WorldTransform.Rotator().ToString(),*X.Placement.WorldTransform.GetScale3D().ToString(),X.Placement.bDirectPlacement?TEXT("직접 배치"):*X.Placement.BookmarkName);};
        T+=TEXT("\nA 배치: ")+Pose(S.A)+TEXT("\nB 배치: ")+Pose(S.B);
        T+=S.bPoseDeltaAvailable?FString::Printf(TEXT("\nB−A: 이동 %.2fcm · 최단 회전 %.3f° · scale %s"),S.TranslationDeltaCm,S.RotationDeltaDegrees,*S.ScaleDelta.ToString()):TEXT("\n위치 차이: N/A");
        T+=FString::Printf(TEXT("\n요청 Hz A/B %.2f/%.2f · 기록 시점 측정 Hz %.2f/%.2f\n설정 revision A/B %d/%d"),S.A.RequestedHz,S.B.RequestedHz,S.A.MeasuredHz,S.B.MeasuredHz,S.A.SettingsRevision,S.B.SettingsRevision);
        if(!S.ChangedSettings.IsEmpty())T+=TEXT("\n변경 설정: ")+FString::Join(S.ChangedSettings,TEXT(", "));
    }
    T+=TEXT("\n\n실행 전체 카운터 (센서별 수치가 아님)");
    auto Count=[&](const TCHAR* Label,int64 A,int64 B){T+=FString::Printf(TEXT("\n%s A/B %lld/%lld · B−A %+lld"),Label,A,B,B-A);};
    Count(TEXT("측정 완료"),C.A.Acquired,C.B.Acquired);Count(TEXT("측정 실패"),C.A.AcquisitionFailures,C.B.AcquisitionFailures);Count(TEXT("승인"),C.A.Accepted,C.B.Accepted);Count(TEXT("제출"),C.A.Submitted,C.B.Submitted);Count(TEXT("receipt"),C.A.Receipts,C.B.Receipts);Count(TEXT("전송 실패"),C.A.DeliveryFailures,C.B.DeliveryFailures);Count(TEXT("미완료"),C.A.Unfinished,C.B.Unfinished);Count(TEXT("자체 검증"),C.A.ConsumerValidated,C.B.ConsumerValidated);Count(TEXT("invalid"),C.A.ConsumerInvalid,C.B.ConsumerInvalid);
    T+=TEXT("\nBroker 수락 비율 A/B: ")+Ratio(C.A)+TEXT(" / ")+Ratio(C.B);
    T+=TEXT("\n\nA 상세:\n")+USlabRunResultsSubsystem::DescribeStreamDetails(C.A)+TEXT("\n\nB 상세:\n")+USlabRunResultsSubsystem::DescribeStreamDetails(C.B);
    for(const auto& W:C.Warnings)T+=TEXT("\n주의: ")+W;
    T+=TEXT("\n배치 품질·가림률·점밀도·실행 평균 Hz는 이 비교에 포함되지 않습니다. Broker receipt는 소비자 업무 처리 완료가 아닙니다.");return T;
}
