#include "VirtualSensorOutputPreflight.h"
#include "VirtualSensorLayoutSubsystem.h"
#include "VirtualSensorSchedulerSubsystem.h"
#include "VirtualSensorHighThroughputTransportSubsystem.h"
#include "VirtualSensorSlabContextSubsystem.h"
#include "VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
FVirtualSensorOutputPreflightResult UVirtualSensorOutputPreflight::Evaluate(FVirtualSensorOutputPreflightResult R,int64 MaxMessageBytes)
{
    R.MessagesPerSecond=0;R.PcdMegabytesPerSecond=0;
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
            R.PcdMegabytesPerSecond+=S.RequestedHz*S.EstimatedBytesPerFrame/(1024.0*1024);
            if(S.EstimatedBytesPerFrame>MaxMessageBytes)R.Warnings.AddUnique(S.SensorId+TEXT(": 보수 추정 PCD가 메시지 한도를 넘습니다. 실제 점 개수·한도를 확인하세요."));
        }
        if(S.bEstimateUncertain)R.Warnings.AddUnique(S.SensorId+TEXT(": backend/echo 또는 압축 크기는 추정값입니다."));
    }
    if(R.MessagesPerSecond>R.TheoreticalSubmissionHz)R.Warnings.AddUnique(TEXT("제출 예산 부족 가능성: MaxSubmissionsPerFrame을 명시적으로 조정하고 다시 검증하세요. 자동 변경하지 않습니다."));
    if(R.PcdMegabytesPerSecond>R.PcdBudgetMegabytesPerSecond)R.Warnings.AddUnique(TEXT("PCD 대역폭 예산 부족 가능성: 점 수·선택 대상과 PointCloudBandwidthLimitMegabytesPerSecond를 확인하세요."));
    R.Summary=FString::Printf(TEXT("PCD 사전 점검 · %.1f건/s · PCD 보수 추정 %.2fMiB/s / 예산 %.1fMiB/s\n제출 %d건/프레임 × %.1fFPS%s = 이론 %.1f건/s (성능 보장 아님) · 연결 %s"),R.MessagesPerSecond,R.PcdMegabytesPerSecond,R.PcdBudgetMegabytesPerSecond,R.SubmissionsPerFrame,R.FpsUsed,R.bAssumedFps?TEXT(" 가정"):TEXT(" 관측"),R.TheoreticalSubmissionHz,R.bTransportConnected?TEXT("완료"):TEXT("대기"));
    for(const auto& S:R.Streams)if(S.Kind==EVirtualSensorStreamKind::PointCloud)R.Summary+=FString::Printf(TEXT("\n%s · %s · %.1fHz · 추정 %lldbytes · 최근 동일 규격 %s"),*S.SensorId,*S.Specification,S.RequestedHz,S.EstimatedBytesPerFrame,S.ObservedBytesPerFrame>0?*FString::Printf(TEXT("%lldbytes"),S.ObservedBytesPerFrame):TEXT("확인 불가"));
    for(const auto& E:R.BlockingReasons)R.Summary+=TEXT("\n차단: ")+E;
    for(const auto& E:R.Warnings)R.Summary+=TEXT("\n안내: ")+E;
    return R;
}
FVirtualSensorOutputPreflightResult UVirtualSensorOutputPreflight::Inspect(const UObject* Context,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& Targets,double ObservedFps)
{
    FVirtualSensorOutputPreflightResult R;auto* World=Context?Context->GetWorld():nullptr;
    auto* Layout=World?World->GetSubsystem<UVirtualSensorLayoutSubsystem>():nullptr;auto* C=Layout?Layout->Coordinator():nullptr;
    if(!C||!C->StreamPublisherComponent||!C->SharedTransportComponent){R.BlockingReasons.Add(TEXT("센서 Coordinator와 전송 설정이 필요합니다."));return Evaluate(R);}
    auto* P=C->StreamPublisherComponent.Get();auto* T=C->SharedTransportComponent.Get();const auto& Profile=T->GetTransportProfile();
    R.SubmissionsPerFrame=P->MaxSubmissionsPerFrame;R.PcdBudgetMegabytesPerSecond=P->PointCloudBandwidthLimitMegabytesPerSecond;
    if(ObservedFps<=0)if(const auto* Scheduler=World->GetSubsystem<UVirtualSensorSchedulerSubsystem>())ObservedFps=Scheduler->GetTelemetry().AverageFps;
    R.bAssumedFps=!FMath::IsFinite(ObservedFps)||ObservedFps<=0;R.FpsUsed=R.bAssumedFps?60:ObservedFps;
    const auto* Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
    R.bTransportConnected=P->CanUseHighThroughputTransport()?(Raw&&Raw->IsTransportConnected()):T->IsStompConnected();
    const auto* SlabContext=World->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
    const auto Session=SlabContext?SlabContext->GetSlabSensorSessionStatus():FVirtualSlabSessionStatus();
    R.bSessionBusy=Session.State==EVirtualSlabSessionState::Preparing||Session.State==EVirtualSlabSessionState::Ready||Session.State==EVirtualSlabSessionState::Running||Session.State==EVirtualSlabSessionState::Paused||Session.State==EVirtualSlabSessionState::Draining;
    if(R.bSessionBusy)R.Warnings.Add(TEXT("시나리오 준비·실행·정리 중입니다. 새 실행의 시작 가능 여부를 다시 확인하세요."));
    if(Profile.BrokerUrl.TrimStartAndEnd().IsEmpty())R.BlockingReasons.Add(TEXT("Broker URL을 설정하세요."));
    TSet<FString> Ids,FoundTargets;
    for(auto* A:C->GetSensorActors())if(IsValid(A)&&!A->IsActorBeingDestroyed())
    {
        const FString Id=A->GetSensorId();if(Ids.Contains(Id))R.BlockingReasons.AddUnique(TEXT("중복 SensorId: ")+Id);Ids.Add(Id);
        const bool Target=Targets.IsEmpty()||Targets.Contains(Id);if(Target)FoundTargets.Add(Id);
        FVirtualSensorEditableState S;bool Preview=false;if(!A->ReadCollectionState(S,Preview)){R.BlockingReasons.Add(Id+TEXT(": 수집 설정을 읽을 수 없습니다."));continue;}
        const bool Camera=A->GetSensorKind()==EVirtualSensorKind::Camera;
        for(const auto Kind:{EVirtualSensorStreamKind::CameraImage,EVirtualSensorStreamKind::LidarPayload,EVirtualSensorStreamKind::PointCloud})
        {
            if((Kind==EVirtualSensorStreamKind::CameraImage)!=Camera)continue;
            const bool Selected=Target&&(Kind==EVirtualSensorStreamKind::PointCloud?Outputs.bPointCloud:Kind==EVirtualSensorStreamKind::CameraImage?Outputs.bCameraImage:Outputs.bLidarTelemetry);
            if(!Selected&&!P->IsStreamEnabled(Kind,Id))continue;
            auto& Row=R.Streams.AddDefaulted_GetRef();Row.SensorId=Id;Row.Kind=Kind;Row.ConfigurationRevision=A->GetConfigurationRevision();
            const auto Config=P->GetEffectiveStreamConfig(Kind,Id);Row.RequestedHz=1.0/(Camera?S.CameraCaptureInterval:S.LidarScanInterval)/FMath::Max(1,Selected?1:Config.FrameStride);
            Row.Specification=Camera?FString::Printf(TEXT("%d×%d"),S.CameraResolution.X,S.CameraResolution.Y):FString::Printf(TEXT("%d×%d"),S.LidarHorizontalSamples,S.LidarVerticalChannels);
            if(Kind==EVirtualSensorStreamKind::PointCloud)
            {
                const auto* L=Cast<AVirtualLidarSensorActor>(A);const auto* Scan=L?L->ScanComponent.Get():nullptr;
                int32 Echoes=1;if(Scan){if(Scan->bUseProfileEchoCapability&&Scan->GetDeviceSpec().MaxEchoesPerPixel>1)Echoes=FMath::Clamp(Scan->GetDeviceSpec().MaxEchoesPerPixel,1,2);else if(Scan->bUseMultiHit)Echoes=FMath::Clamp(Scan->MaxHitsPerRay,1,16);if(Scan->AcquisitionBackend==EVirtualLidarAcquisitionBackend::GpuDepthProjection)Echoes=1;else Row.bEstimateUncertain=Scan->AcquisitionBackend==EVirtualLidarAcquisitionBackend::Auto;}
                Row.EstimatedBytesPerFrame=int64(S.LidarHorizontalSamples)*S.LidarVerticalChannels*Echoes*33+4096;
                Row.Specification+=FString::Printf(TEXT(" · 최대 echo %d · 33bytes/점"),Echoes);
                const auto Frame=Scan?Scan->GetLastFrameSnapshot():nullptr;
                if(Frame&&Frame->PipelineGeneration==Scan->GetFileCaptureRevision()&&Frame->HorizontalSamples==S.LidarHorizontalSamples&&Frame->VerticalChannels==S.LidarVerticalChannels&&FMath::IsNearlyEqual(Frame->MaxDistanceCm,S.LidarMaxDistance))
                    for(const auto& Status:P->GetStreamStatuses())if(Status.SensorId==Id&&Status.StreamKind==Kind&&Status.LastSubmittedFrameId==Frame->FrameId)Row.ObservedBytesPerFrame=Status.LastFrameBytes;
            }
            else Row.bEstimateUncertain=true;
        }
    }
    for(const auto& Id:Targets)if(!FoundTargets.Contains(Id))R.BlockingReasons.AddUnique(TEXT("대상 SensorId 없음: ")+Id);
    if(Outputs.bPointCloud&&Profile.ExportTopic.IsEmpty())R.BlockingReasons.Add(TEXT("PCD Topic을 설정하세요."));
    return Evaluate(R,Profile.MaxMessageBytes);
}
