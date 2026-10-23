#include "VirtualSensorOutputPreflight.h"
#include "VirtualSensorLayoutSubsystem.h"
#include "VirtualSensorSchedulerSubsystem.h"
#include "VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorAcquisitionContext.h"
#include "VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
namespace
{
FString FormatPreflightDetails(const FVirtualSensorOutputPreflightResult& R)
{
    FString Summary=FString::Printf(TEXT("PCD %.1f건/s · 전체 출력 %.1f건/s · PCD 보수 추정 %.2fMiB/s / 예산 %.1fMiB/s\n제출 %d건/프레임 × %.1fFPS%s = 이론 %.1f건/s (성능 보장 아님)"),R.PcdMessagesPerSecond,R.MessagesPerSecond,R.PcdMegabytesPerSecond,R.PcdBudgetMegabytesPerSecond,R.SubmissionsPerFrame,R.FpsUsed,R.bAssumedFps?TEXT(" 가정"):TEXT(" 관측"),R.TheoreticalSubmissionHz);
    if(R.bRequiresRaw)Summary+=FString(TEXT("\nRaw TCP 연결: "))+(R.bRawConnected?TEXT("완료"):TEXT("대기"));
    if(R.bRequiresEngine)Summary+=FString(TEXT("\nEngine STOMP 연결: "))+(R.bEngineConnected?TEXT("완료"):TEXT("대기"));
    if(R.Streams.IsEmpty())Summary+=TEXT("\n송신 대상 없음");
    for(const auto& S:R.Streams)
    {
        Summary+=FString::Printf(TEXT("\n%s · %s · %s · %.1fHz · %s · Topic %s"),*S.SensorId,S.Kind==EVirtualSensorStreamKind::PointCloud?TEXT("PCD"):S.Kind==EVirtualSensorStreamKind::CameraImage?TEXT("Camera"):TEXT("LiDAR 정보"),*S.Specification,S.RequestedHz,S.TransportBackend==EVirtualSensorStreamTransportBackend::TcpStompHighThroughput?TEXT("Raw TCP"):TEXT("Engine STOMP"),*S.Topic);
        if(S.Kind==EVirtualSensorStreamKind::PointCloud)Summary+=FString::Printf(TEXT(" · 추정 %lldbytes · 최근 동일 규격 %s"),S.EstimatedBytesPerFrame,S.ObservedBytesPerFrame>0?*FString::Printf(TEXT("%lldbytes"),S.ObservedBytesPerFrame):TEXT("확인 불가"));
    }
    for(const auto& E:R.BlockingReasons)Summary+=TEXT("\n차단: ")+E;
    for(const auto& E:R.Warnings)Summary+=TEXT("\n안내: ")+E;
    return Summary;
}
FVirtualSensorOutputPreflightResult EvaluatePreflight(FVirtualSensorOutputPreflightResult R,int64 MaxMessageBytes,bool bBuildDetails)
{
    R.MessagesPerSecond=0;R.PcdMessagesPerSecond=0;R.PcdMegabytesPerSecond=0;
    if(!FMath::IsFinite(R.FpsUsed)||R.FpsUsed<=0){R.FpsUsed=60;R.bAssumedFps=true;}
    R.TheoreticalSubmissionHz=FMath::Max(1,R.SubmissionsPerFrame)*R.FpsUsed;
    TSet<FString> Keys;
    for(const auto& S:R.Streams)
    {
        const FString Key=S.SensorId+TEXT("|")+FString::FromInt(int32(S.Kind));if(Keys.Contains(Key))continue;Keys.Add(Key);
        if(!FMath::IsFinite(S.RequestedHz)||S.RequestedHz<=0){R.BlockingReasons.AddUnique(S.SensorId+TEXT(": 측정 주기를 확인하세요."));continue;}
        R.MessagesPerSecond+=S.RequestedHz;
        if(S.Kind==EVirtualSensorStreamKind::PointCloud)
        {
            R.PcdMessagesPerSecond+=S.RequestedHz;
            R.PcdMegabytesPerSecond+=S.RequestedHz*S.EstimatedBytesPerFrame/(1024.0*1024);
            if(S.EstimatedBytesPerFrame>MaxMessageBytes)R.Warnings.AddUnique(S.SensorId+TEXT(": 보수 추정 PCD가 메시지 한도를 넘습니다. 실제 점 개수·한도를 확인하세요."));
            const double BucketBytes=FMath::Max(1.0,R.PcdBudgetMegabytesPerSecond)*1024.0*1024;
            if(S.EstimatedBytesPerFrame>BucketBytes)R.Warnings.AddUnique(S.SensorId+TEXT(": 단일 PCD 프레임 추정이 전송 bucket 용량보다 큽니다. 평균 MiB/s가 낮아도 제출이 계속 대기할 수 있습니다."));
        }
        if(S.bEstimateUncertain)R.Warnings.AddUnique(S.SensorId+TEXT(": backend/echo 또는 압축 크기는 추정값입니다."));
    }
    if(R.MessagesPerSecond>R.TheoreticalSubmissionHz)R.Warnings.AddUnique(TEXT("제출 예산 부족 가능성: MaxSubmissionsPerFrame을 명시적으로 조정하고 다시 검증하세요. 자동 변경하지 않습니다."));
    if(R.PcdMegabytesPerSecond>R.PcdBudgetMegabytesPerSecond)R.Warnings.AddUnique(TEXT("PCD 대역폭 예산 부족 가능성: 점 수·선택 대상과 PointCloudBandwidthLimitMegabytesPerSecond를 확인하세요."));
    if(!bBuildDetails){R.Summary.Reset();return R;}
    R.Summary=FormatPreflightDetails(R);
    return R;
}
}
FString UVirtualSensorOutputPreflight::FormatDetails(const FVirtualSensorOutputPreflightResult& R)
{return FormatPreflightDetails(R);}
FVirtualSensorOutputPreflightResult UVirtualSensorOutputPreflight::Evaluate(FVirtualSensorOutputPreflightResult R,int64 MaxMessageBytes)
{return EvaluatePreflight(MoveTemp(R),MaxMessageBytes,true);}
FVirtualSensorOutputPreflightResult UVirtualSensorOutputPreflight::Inspect(const UObject* Context,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& Targets,double ObservedFps)
{return InspectForPurpose(Context,Outputs,Targets,EVirtualSensorPreflightPurpose::LegacyUnion,ObservedFps);}

FVirtualSensorOutputPreflightResult UVirtualSensorOutputPreflight::InspectForPurpose(const UObject* Context,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& Targets,EVirtualSensorPreflightPurpose Purpose,double ObservedFps)
{return InspectForPresentation(Context,Outputs,Targets,Purpose,true,ObservedFps);}
FVirtualSensorOutputPreflightResult UVirtualSensorOutputPreflight::InspectForPresentation(const UObject* Context,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& Targets,EVirtualSensorPreflightPurpose Purpose,bool bBuildDetails,double ObservedFps)
{
    FVirtualSensorOutputPreflightResult R;auto* World=Context?Context->GetWorld():nullptr;
    auto* Layout=World?World->GetSubsystem<UVirtualSensorLayoutSubsystem>():nullptr;auto* C=Layout?Layout->Coordinator():nullptr;
    if(!C||!C->StreamPublisherComponent||!C->SharedTransportComponent){R.BlockingReasons.Add(TEXT("센서 Coordinator와 전송 설정이 필요합니다."));return EvaluatePreflight(MoveTemp(R),8388608,bBuildDetails);}
    auto* P=C->StreamPublisherComponent.Get();auto* T=C->SharedTransportComponent.Get();const auto& Profile=T->GetTransportProfile();
    R.SubmissionsPerFrame=P->MaxSubmissionsPerFrame;R.PcdBudgetMegabytesPerSecond=P->PointCloudBandwidthLimitMegabytesPerSecond;
    if(ObservedFps<=0)if(const auto* Scheduler=World->GetSubsystem<UVirtualSensorSchedulerSubsystem>())ObservedFps=Scheduler->GetTelemetry().AverageFps;
    R.bAssumedFps=!FMath::IsFinite(ObservedFps)||ObservedFps<=0;R.FpsUsed=R.bAssumedFps?60:ObservedFps;
    const auto* Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
    R.bRawConnected=Raw&&Raw->IsTransportConnected();R.bEngineConnected=T->IsStompConnected();
    if(Purpose==EVirtualSensorPreflightPurpose::ManualStreamStart&&Targets.IsEmpty())
        R.BlockingReasons.Add(TEXT("선택 센서 없음: 배치 창이나 상단 목록에서 송신할 LiDAR를 선택하세요."));
    const auto* SlabContext=FVirtualSensorAcquisitionContextProviders::Get(World);
    R.bSessionBusy=SlabContext&&SlabContext->IsAcquisitionSessionBusy();
    if(R.bSessionBusy)R.Warnings.Add(TEXT("시나리오 준비·실행·정리 중입니다. 새 실행의 시작 가능 여부를 다시 확인하세요."));
    TSet<FString> Ids,FoundTargets;bool HasCamera=false,HasLidar=false;
    for(auto* A:C->GetSensorActors())if(IsValid(A)&&!A->IsActorBeingDestroyed())
    {
        const FString Id=A->GetSensorId();if(Ids.Contains(Id))R.BlockingReasons.AddUnique(TEXT("중복 SensorId: ")+Id);Ids.Add(Id);
        const bool Target=(Targets.IsEmpty()&&Purpose!=EVirtualSensorPreflightPurpose::ManualStreamStart)||Targets.Contains(Id);if(Target)FoundTargets.Add(Id);
        FVirtualSensorEditableState S;bool Preview=false;if(!A->ReadCollectionState(S,Preview)){R.BlockingReasons.Add(Id+TEXT(": 수집 설정을 읽을 수 없습니다."));continue;}
        const bool Camera=A->GetSensorKind()==EVirtualSensorKind::Camera;
        if(Target){HasCamera|=Camera;HasLidar|=!Camera;}
        for(const auto Kind:{EVirtualSensorStreamKind::CameraImage,EVirtualSensorStreamKind::LidarPayload,EVirtualSensorStreamKind::PointCloud})
        {
            if((Kind==EVirtualSensorStreamKind::CameraImage)!=Camera)continue;
            const bool Selected=Target&&(Kind==EVirtualSensorStreamKind::PointCloud?Outputs.bPointCloud:Kind==EVirtualSensorStreamKind::CameraImage?Outputs.bCameraImage:Outputs.bLidarTelemetry);
            FVirtualSensorStreamConfig Config;
            if(Selected)
            {
                if(Purpose==EVirtualSensorPreflightPurpose::ManualStreamStart){Config.SensorId=Id;Config.StreamKind=Kind;Config=P->NormalizeLiveConfig(Config);}
                else Config=P->GetEffectiveStreamConfig(Kind,Id);
                Config.FrameStride=1;
            }
            else
            {
                if(Purpose==EVirtualSensorPreflightPurpose::ScenarioTransition&&Target)continue;
                if(!P->ResolveActiveStreamForSensor(Kind,Id,Config))continue;
            }
            auto& Row=R.Streams.AddDefaulted_GetRef();Row.SensorId=Id;Row.Kind=Kind;Row.ConfigurationRevision=A->GetConfigurationRevision();
            Row.bPlannedOutput=Selected;Row.RequestedHz=1.0/(Camera?S.CameraCaptureInterval:S.LidarScanInterval)/FMath::Max(1,Config.FrameStride);
            if(Kind==EVirtualSensorStreamKind::PointCloud)Row.RequestedHz/=P->GetPcdSendOptions(Id).Divisor();
            const bool UseRaw=Config.TransportBackend==EVirtualSensorStreamTransportBackend::TcpStompHighThroughput&&P->CanUseHighThroughputTransport();
            Row.TransportBackend=UseRaw?EVirtualSensorStreamTransportBackend::TcpStompHighThroughput:EVirtualSensorStreamTransportBackend::EngineStompCompatibility;
            R.bRequiresRaw|=UseRaw;R.bRequiresEngine|=!UseRaw;Row.bTransportConnected=UseRaw?R.bRawConnected:R.bEngineConnected;
            Row.Topic=Kind==EVirtualSensorStreamKind::PointCloud?Profile.ExportTopic:Camera?Profile.CameraTopic:Profile.LidarTopic;
            if(Row.Topic.TrimStartAndEnd().IsEmpty())
            {
                const FString Reason=Id+TEXT(": ")+(Kind==EVirtualSensorStreamKind::PointCloud?TEXT("PCD"):Camera?TEXT("Camera"):TEXT("LiDAR 정보"))+TEXT(" Topic을 설정하세요.");
                if(Selected)R.BlockingReasons.AddUnique(Reason);
                else R.Warnings.AddUnique(TEXT("기존 독립 출력 설정 확인: ")+Reason);
            }
            Row.Specification=Camera?FString::Printf(TEXT("%d×%d"),S.CameraResolution.X,S.CameraResolution.Y):FString::Printf(TEXT("%d×%d"),S.LidarHorizontalSamples,S.LidarVerticalChannels);
            if(Kind==EVirtualSensorStreamKind::PointCloud)
            {
                const auto* L=Cast<AVirtualLidarSensorActor>(A);const auto* Scan=L?L->ScanComponent.Get():nullptr;
                int32 Echoes=1;
                if(Scan)
                {
                    if(Scan->bUseProfileEchoCapability&&Scan->GetDeviceSpec().MaxEchoesPerPixel>1)Echoes=FMath::Clamp(Scan->GetDeviceSpec().MaxEchoesPerPixel,1,2);
                    else if(Scan->bUseMultiHit)Echoes=FMath::Clamp(Scan->MaxHitsPerRay,1,16);
                    FString BackendReason;const auto Resolved=Scan->GetResolvedAcquisitionBackend(BackendReason);
                    Row.bEstimateUncertain=Scan->AcquisitionBackend!=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
                    const TCHAR* Configured=Scan->AcquisitionBackend==EVirtualLidarAcquisitionBackend::GpuDepthProjection?TEXT("GPU"):Scan->AcquisitionBackend==EVirtualLidarAcquisitionBackend::Auto?TEXT("자동"):Scan->AcquisitionBackend==EVirtualLidarAcquisitionBackend::AccurateCpuTrace?TEXT("CPU"):TEXT("기타");
                    Row.Specification+=FString::Printf(TEXT(" · 설정 %s / 현재 %s"),Configured,Resolved==EVirtualLidarAcquisitionBackend::GpuDepthProjection?TEXT("GPU"):TEXT("CPU/외부"));
                    if(!BackendReason.IsEmpty())Row.Specification+=TEXT(" · ")+BackendReason;
                }
                const auto SendOptions=P->GetPcdSendOptions(Id);
                const int32 PointBytes=FVirtualPcdCodec::BytesPerPoint(SendOptions);
                Row.EstimatedBytesPerFrame=int64(S.LidarHorizontalSamples)*S.LidarVerticalChannels*Echoes*PointBytes+4096;
                Row.bEstimateUncertain|=SendOptions.bCompressed;
                Row.Specification+=FString::Printf(TEXT(" · fallback 포함 최대 echo %d · %dbytes/점 · 전송 1/%d · 비압축 환산"),Echoes,PointBytes,SendOptions.Divisor());
                const auto Frame=Scan?Scan->GetLastFrameSnapshot():nullptr;
                if(Frame&&Frame->PipelineGeneration==Scan->GetFileCaptureRevision()&&Frame->HorizontalSamples==S.LidarHorizontalSamples&&Frame->VerticalChannels==S.LidarVerticalChannels&&FMath::IsNearlyEqual(Frame->MaxDistanceCm,S.LidarMaxDistance))
                    for(const auto& Status:P->GetStreamStatuses())if(Status.SensorId==Id&&Status.StreamKind==Kind&&Status.LastSubmittedFrameId==Frame->FrameId)Row.ObservedBytesPerFrame=Status.LastFrameBytes;
            }
            else Row.bEstimateUncertain=true;
        }
    }
    for(const auto& Id:Targets)if(!FoundTargets.Contains(Id))R.BlockingReasons.AddUnique(TEXT("대상 SensorId 없음: ")+Id);
    if(Outputs.bPointCloud&&!HasLidar)R.BlockingReasons.Add(TEXT("PCD 측정 대상 LiDAR가 없습니다. 배치 패널에서 추가하거나 대상을 확인하세요."));
    if(Outputs.bCameraImage&&!HasCamera)R.BlockingReasons.Add(TEXT("이미지 송신 대상 Camera가 없습니다."));
    if(Outputs.bLidarTelemetry&&!HasLidar)R.BlockingReasons.Add(TEXT("정보 송신 대상 LiDAR가 없습니다."));
    if(!R.Streams.IsEmpty())
    {
        if(Profile.BrokerUrl.TrimStartAndEnd().IsEmpty())R.BlockingReasons.Add(TEXT("Broker URL을 설정하세요."));
        if(T->TransportMode!=EVirtualSensorTransportMode::StompWebSocket)R.BlockingReasons.Add(TEXT("실시간 Topic 송신에는 STOMP 전송 모드가 필요합니다."));
    }
    R.bTransportConnected=!R.Streams.IsEmpty()&&(!R.bRequiresRaw||R.bRawConnected)&&(!R.bRequiresEngine||R.bEngineConnected);
    return EvaluatePreflight(MoveTemp(R),Profile.MaxMessageBytes,bBuildDetails);
}
