#include "VirtualSensorStreamPublisherComponent.h"
#include "VirtualSensorHighThroughputTransportSubsystem.h"

void UVirtualSensorStreamPublisherComponent::UpdatePointCloudFairBudgets(double Now)
{
    // Max-min frame-rate fairness, weighted by each measured body size. Slow
    // producers get their entire demand first; peers get the same Hz ceiling.
    // Wildcard routes retain
    // separate sensor buckets, so changing the route cannot multiply its share.
    if(Now>=NextFairBudgetUpdateSeconds)
    {
        NextFairBudgetUpdateSeconds=Now+.2;
        TArray<FStreamRuntime::FSensorSequence*> Active;
        double Required=0,Weight=0;
        for(auto& Pair:StreamRuntimes)
        {
            auto& R=Pair.Value;
            if(R.Config.StreamKind!=EVirtualSensorStreamKind::PointCloud)continue;
            for(auto& Item:R.SensorSequences)
            {
                auto& S=Item.Value;S.FairBytesPerSecond=0;
                const bool Pending=[&]()
                {
                    if(R.bSerializationInFlight&&R.SerializationSensorId==Item.Key)return true;
                    for(const auto& F:R.PendingFrameQueue)if(F.SensorId==Item.Key)return true;
                    for(const auto& M:R.PreparedMessageQueue)if(M.SensorId==Item.Key)return true;
                    return false;
                }();
                if(!R.Config.bEnabled||IsLocalCaptureSuspended(Item.Key)||S.FrameBytes<=0||S.InputPeriod<=0||
                    (!Pending&&Now-S.LastInputSeconds>FMath::Max(1.0,3*S.InputPeriod)))continue;
                const auto* Exact=StreamRuntimes.Find(MakeStreamKey(R.Config.StreamKind,Item.Key));
                if(!Pending&&R.Config.SensorId.IsEmpty()&&Exact&&Exact!=&R&&(Exact->Config.bEnabled||Exact->Status.bOverloadSuspended))continue;
                Required+=S.FrameBytes/S.InputPeriod;Weight+=S.FrameBytes;Active.Add(&S);
            }
        }
        Active.Sort([](const auto& A,const auto& B){return A.InputPeriod>B.InputPeriod;});
        double Remaining=FMath::Max(1.0f,PointCloudBandwidthLimitMegabytesPerSecond)*1048576.0;
        for(int32 I=0;I<Active.Num();++I)
        {
            auto& S=*Active[I];S.FairBytesPerSecond=S.FrameBytes*FMath::Min(1.0/S.InputPeriod,Remaining/FMath::Max(1.0,Weight));
            Remaining=FMath::Max(0.0,Remaining-S.FairBytesPerSecond);
            Weight-=S.FrameBytes;
        }
        for(auto& Pair:StreamRuntimes)
        {
            auto& R=Pair.Value;if(R.Config.StreamKind!=EVirtualSensorStreamKind::PointCloud)continue;
            if(R.RecentSubmitSampleSeconds<=0||R.Status.SubmittedFrameCount<R.RecentSubmitSampleCount)
            {
                R.RecentSubmitSampleSeconds=Now;R.RecentSubmitSampleCount=R.Status.SubmittedFrameCount;R.Status.RecentSubmittedHz=0;
            }
            else if(Now-R.RecentSubmitSampleSeconds>=1)
            {
                R.Status.RecentSubmittedHz=(R.Status.SubmittedFrameCount-R.RecentSubmitSampleCount)/(Now-R.RecentSubmitSampleSeconds);
                R.RecentSubmitSampleSeconds=Now;R.RecentSubmitSampleCount=R.Status.SubmittedFrameCount;
            }
            R.Status.PcdRequiredMiBPerSecond=Required/1048576.0;
            R.Status.PcdBudgetMiBPerSecond=FMath::Max(1.0f,PointCloudBandwidthLimitMegabytesPerSecond);
            double Share=0;for(const auto& Item:R.SensorSequences)Share+=Item.Value.FairBytesPerSecond;
            R.Status.PcdFairShareMiBPerSecond=Share/1048576.0;
        }
    }
    for(auto& Pair:StreamRuntimes)for(auto& Item:Pair.Value.SensorSequences)
    {
        auto& S=Item.Value;
        // A newer small serialization must not strand an older large FIFO head.
        // Only the eligible head raises its sensor cap; refill shares and the
        // global byte bucket remain unchanged, with no token debt or bypass.
        const auto* Head=Pair.Value.PreparedMessageQueue.IsEmpty()?nullptr:&Pair.Value.PreparedMessageQueue[0];
        const double HeadBytes=Head&&Head->bBinaryPcd&&Head->SensorId==Item.Key?Head->ByteCount:0;
        const double Capacity=FMath::Max(HeadBytes,FMath::Max(S.FrameBytes*2,S.FairBytesPerSecond*.1));
        const double Elapsed=S.BandwidthUpdated>0?FMath::Max(0.0,Now-S.BandwidthUpdated):0;
        S.BandwidthTokens=FMath::Min(Capacity,S.BandwidthTokens+Elapsed*S.FairBytesPerSecond);
        S.BandwidthUpdated=Now;
    }
}

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
    auto& Sequence=R.SensorSequences.FindChecked(Frame.SensorId);
    if(Frame.RequestedAcquisitionIntervalSeconds>0)
    {
        const double Period=Frame.RequestedAcquisitionIntervalSeconds;
        // This is an explicit configuration hint, not callback pacing. Even a
        // 0.05 -> 0.06 change should immediately reclaim the available margin.
        if(Sequence.InputPeriod>0&&Period>Sequence.InputPeriod+1e-6)
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
    if(!bAutoAdjustPointCloudSendRate||R.Config.TransportBackend!=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput||!CanUseHighThroughputTransport())
    {R.AdmissionRatio=1;R.Status.bRateControlActive=false;return false;}
    const double BudgetRatio=Sequence.FrameBytes>0&&Sequence.FairBytesPerSecond>0
        ?FMath::Min(1.0,Sequence.FairBytesPerSecond*Sequence.InputPeriod/Sequence.FrameBytes):1.0;
    const double Ratio=FMath::Min(R.AdmissionRatio,BudgetRatio);
    R.Status.bRateControlActive=Ratio<.9999;
    if(Sequence.InputPeriod>0)R.Status.RateControlHz=Ratio/Sequence.InputPeriod;
    // Fractional credit distributes omissions evenly in acquisition order;
    // there is no timer to duplicate or stale wall-clock deadline to restore.
    Sequence.AdmissionCredit+=Ratio;
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
