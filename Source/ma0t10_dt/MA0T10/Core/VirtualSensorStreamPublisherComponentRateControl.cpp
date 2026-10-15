#include "VirtualSensorStreamPublisherComponent.h"
#include "VirtualSensorHighThroughputTransportSubsystem.h"

void UVirtualSensorStreamPublisherComponent::LogPointCloudTransition(const FString& Key,FStreamRuntime& R,const FString& Message,double Now)
{
    if(Now<R.NextPcdConsoleLogSeconds){++R.PendingPcdConsoleLogs;++R.Status.SuppressedConsoleLogCount;return;}
    R.NextPcdConsoleLogSeconds=Now+1;
    UE_LOG(LogTemp,Display,TEXT("[PCD-RESUME] %s %s (console suppressed=%lld; transition counters retained)"),*Key,*Message,R.PendingPcdConsoleLogs);
    R.PendingPcdConsoleLogs=0;
}

void UVirtualSensorStreamPublisherComponent::UpdatePointCloudRateControl(FStreamRuntime& R,double Now,const FVirtualSensorHighThroughputProfile& Profile)
{
    if(R.Config.StreamKind!=EVirtualSensorStreamKind::PointCloud)return;
    if(!bAutoAdjustPointCloudSendRate||!R.Config.bEnabled||R.Config.TransportBackend!=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput)
    {R.AdmissionRatio=1;R.Status.bRateControlActive=false;R.Status.RateControlHz=0;return;}
    if(R.LastAdmissionUpdate>0&&Now-R.LastAdmissionUpdate<.25)return;
    R.LastAdmissionUpdate=Now;
    // The frame cap usually binds before 64MiB. Watch all retained stages;
    // byte/global pressure also covers larger bodies and other streams.
    double Pressure=FMath::Max3(R.Status.RawOutstandingFrameCount/20.0,
        R.Status.RawOutstandingBytes/double(FMath::Max<int64>(1,Profile.MaxOutstandingBytesPerStream)),
        R.Status.RawGlobalOutstandingBytes/double(FMath::Max<int64>(1,Profile.MaxOutstandingBytes)));
    Pressure=FMath::Max(Pressure,FMath::Max(R.PreparedMessageQueue.Num(),R.PendingFrameQueue.Num())/double(FMath::Max(1,R.Config.MaxBufferedFrames)));
    // AIMD, with hysteresis and an admission floor. Falling backlog must drain
    // before another multiplicative decrease; an empty path recovers promptly.
    if(Pressure>=.25&&Pressure>=R.PreviousPressure-.015)R.AdmissionRatio=FMath::Max(.02,R.AdmissionRatio*.9);
    else if(Pressure<=.10)R.AdmissionRatio=FMath::Min(1.0,R.AdmissionRatio+.02);
    R.PreviousPressure=Pressure;
    R.Status.bRateControlActive=R.AdmissionRatio<.9999;
}

bool UVirtualSensorStreamPublisherComponent::OmitPointCloudForRateControl(const FString& Key,FStreamRuntime& R,const FVirtualSensorFrameEnvelope& Frame,double Now)
{
    if(!bAutoAdjustPointCloudSendRate||R.Config.TransportBackend!=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput||!CanUseHighThroughputTransport())
    {R.AdmissionRatio=1;R.Status.bRateControlActive=false;return false;}
    auto& Sequence=R.SensorSequences.FindChecked(Frame.SensorId);
    if(Frame.RequestedAcquisitionIntervalSeconds>0)
    {
        const double Period=Frame.RequestedAcquisitionIntervalSeconds;
        if(Sequence.InputPeriod>0&&Period>Sequence.InputPeriod*1.25)
            R.AdmissionRatio=FMath::Min(1.0,R.AdmissionRatio*Period/Sequence.InputPeriod);
        Sequence.InputPeriod=Period;
    }
    else if(Sequence.LastInputSeconds>0)
    {
        const double Period=Now-Sequence.LastInputSeconds;
        // External producers without a hint use a smoothed period. Callback
        // jitter is not evidence of a sensor configuration change.
        if(Period>0&&Period<1)Sequence.InputPeriod=Sequence.InputPeriod>0?.9*Sequence.InputPeriod+.1*Period:Period;
    }
    Sequence.LastInputSeconds=Now;
    R.Status.bRateControlActive=R.AdmissionRatio<.9999;
    if(Sequence.InputPeriod>0)R.Status.RateControlHz=R.AdmissionRatio/Sequence.InputPeriod;
    // Fractional credit distributes omissions evenly in acquisition order;
    // there is no timer to duplicate or stale wall-clock deadline to restore.
    Sequence.AdmissionCredit+=R.AdmissionRatio;
    if(Sequence.AdmissionCredit>=1-1e-8){Sequence.AdmissionCredit=FMath::Max(0.0,Sequence.AdmissionCredit-1);return false;}
    ++R.Status.RateControlSkippedFrameCount;
    RecordSlabFailure(Frame.SlabContext.RunId,Frame.SensorId,R.Config.StreamKind,Frame.FrameId,TEXT("송신율 조절로 생략"));
    if(Now-R.LastRateLogSeconds>=2)
    {
        R.LastRateLogSeconds=Now;
        const FString Message=FString::Printf(TEXT("송신율 조절로 생략: %lld프레임 (누계 %lld), 목표 %.1fHz. 승인한 프레임은 FIFO로 배수합니다."),
            R.Status.RateControlSkippedFrameCount-R.LastRateLogSkipped,R.Status.RateControlSkippedFrameCount,R.Status.RateControlHz);
        R.LastRateLogSkipped=R.Status.RateControlSkippedFrameCount;
        AddLog(Key,TEXT("pcd-rate-control"),Message,nullptr,Frame.FrameId);
        UE_LOG(LogTemp,Display,TEXT("[PCD-OPT-RATE] %s %s"),*Key,*Message);
    }
    return true;
}
