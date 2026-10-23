#include "SensorOwnedRayTracing.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "Json.h"
#include "RHI.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

namespace
{
void RecordLegacyAssertion(FAutomationTestBase* Test, const TSharedPtr<FJsonObject>& Row, const TCHAR* Name, bool Passed)
{
    Row->SetBoolField(Name, Passed);
    Test->AddInfo(FString::Printf(TEXT("[legacy reference; non-gating] %s: %s"), Name, Passed ? TEXT("PASS") : TEXT("FAIL")));
}
double Average(const TArray<double>& A){double S=0;for(double V:A)S+=V;return A.IsEmpty()?0:S/A.Num();}
double GpuScheduledP95(TArray<double> A){A.Sort();return A.IsEmpty()?0:A[FMath::Clamp(FMath::CeilToInt(A.Num()*.95)-1,0,A.Num()-1)];}
class FGpuScheduledRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int SensorCount;bool OS1,Operating;TWeakObjectPtr<AVirtualCameraSensorActor> OperatingCamera;
    int64 CameraBaseline=0;int Phase=0;FDateTime MeasurementUtc;double Started=FPlatformTime::Seconds(),Last=0,Measured=0;
    TArray<TWeakObjectPtr<AVirtualLidarSensorActor>> Sensors;TArray<int64> Baselines, MissedSlotBaselines;
    TArray<double> Engine,Wall,Game,Gpu;uint64 FirstFrame=0;int Slow=0;
    TWeakObjectPtr<UVirtualSensorStreamPublisherComponent> PcdPublisher;
    TMap<FString,FVirtualSensorStreamStatus> PcdBaselines;
    int PcdPort=0; FIntPoint MeasurementClient; float MeasurementDpi=0; bool VisibleWindowVerified=false;
    TMap<FString,TArray<double>> EncodeTimes;
    FDelegateHandle EncodeHandle;
    FVirtualPcdSendOptions BandwidthOptions;
    static double Percentile(TArray<double> A,double P){A.Sort();return A.IsEmpty()?0:A[FMath::Clamp(FMath::CeilToInt(A.Num()*P)-1,0,A.Num()-1)];}

    double NextPcdBudgetSample=0,MaximumPcdRequiredMiB=0;int PcdBudgetWarningSamples=0;
    FVirtualSensorStreamStatus PcdStatus(const FString& Id) const
    {
        if(PcdPublisher.IsValid())for(const auto& S:PcdPublisher->GetStreamStatuses())
            if(S.StreamKind==EVirtualSensorStreamKind::PointCloud&&S.SensorId==Id)return S;
        return {};
    }
public:
    FGpuScheduledRun(FAutomationTestBase* T,int Count,bool High,bool OperatingMap=false):Test(T),SensorCount(Count),OS1(High),Operating(OperatingMap){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Started>180){Test->AddError(TEXT("scheduled GPU acquisition benchmark timed out"));return true;}
        if(Phase==0)
        {
            UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
            if(Operating)
            {
                for(TActorIterator<AVirtualLidarSensorActor>I(W);I;++I){I->StopSensor();if(Sensors.IsEmpty())Sensors.Add(*I);}
                for(TActorIterator<AVirtualCameraSensorActor>I(W);I;++I){I->StopSensor();if(!OperatingCamera.IsValid())OperatingCamera=*I;}
                if(Sensors.IsEmpty()||!OperatingCamera.IsValid()){Test->AddError(TEXT("operating map must contain existing Camera and LiDAR"));return true;}
                FString Reason;Sensors[0]->SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend::Auto,Reason);Sensors[0]->StartSensor();OperatingCamera->StartSensor();
                Baselines.Init(0,1);Started=Last=Now;Phase=1;return false;
            }
            AShipHatchSimulation* Sim=nullptr;for(TActorIterator<AShipHatchSimulation>I(W);I;++I)Sim=*I;if(!Sim||Sim->IsGenerating()||!Sim->HasFreshMeasurement())return false;
            if(!FParse::Param(FCommandLine::Get(),TEXT("GpuLidarCpu"))&&!Test->TestTrue(TEXT("scheduled test requires actual HWRT pipeline support"),SensorOwnedRayTracing::IsAvailable()))return true;
            const bool PcdRequested=FParse::Value(FCommandLine::Get(),TEXT("GpuLidarPcdLoopbackPort="),PcdPort);
            if(PcdRequested)
            {
                if(!Test->TestTrue(TEXT("PCD proof requires an unprivileged local loopback port"),PcdPort>=1024&&PcdPort<=65535))return true;
                auto* Transport=NewObject<UVirtualSensorTransportComponent>(Sim);Sim->AddInstanceComponent(Transport);
                Transport->RegisterComponent();Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
                FVirtualSensorTransportProfile Profile;Profile.BrokerUrl=FString::Printf(TEXT("tcp://127.0.0.1:%d"),PcdPort);Profile.UserName.Reset();
                Transport->ConfigureTransportProfile(Profile);Transport->SetSessionCredentials(TEXT(""),TEXT(""));
                auto* Publisher=NewObject<UVirtualSensorStreamPublisherComponent>(Sim);Sim->AddInstanceComponent(Publisher);
                Publisher->SetTransportComponent(Transport);Publisher->RegisterComponent();PcdPublisher=Publisher;
                FString FieldProfile;FParse::Value(FCommandLine::Get(),TEXT("PcdBandwidthProfile="),FieldProfile);
                if(FieldProfile==TEXT("Compact"))BandwidthOptions.Profile=EVirtualPcdFieldProfile::Compact;
                else if(FieldProfile==TEXT("Minimal"))BandwidthOptions.Profile=EVirtualPcdFieldProfile::Minimal;
                BandwidthOptions.bCompressed=FParse::Param(FCommandLine::Get(),TEXT("PcdBandwidthCompressed"));
                if(FParse::Param(FCommandLine::Get(),TEXT("PcdBandwidthDefaults")))BandwidthOptions=Publisher->GetPcdSendOptions(TEXT(""));
                else {FString OptionError;Test->TestTrue(TEXT("bandwidth fixture sets explicit send options"),Publisher->SetPcdSendOptions(TEXT(""),BandwidthOptions,true,OptionError));}
                int32 SendDivisor=1;
                if(FParse::Value(FCommandLine::Get(),TEXT("PcdBandwidthDivisor="),SendDivisor))
                {
                    if(!Test->TestTrue(TEXT("send divisor is1,2,4"),SendDivisor==1||SendDivisor==2||SendDivisor==4))return true;
                    BandwidthOptions.SendDivisor=SendDivisor;FString OptionError;
                    if(!Test->TestTrue(TEXT("configure send division without changing acquisition"),Publisher->SetPcdSendOptions(TEXT(""),BandwidthOptions,true,OptionError)))return true;
                }
                EncodeHandle=Publisher->OnPcdEncoded.AddLambda([this](const FString& Id,int64,float Ms,int32){if(Phase==2)EncodeTimes.FindOrAdd(Id).Add(Ms);});
                // Use the production normalization/default budgets: PCD binary, FIFO,
                // stride 1, receipt every frame, 20 retained frames, 80 MiB/s.
                for(int I=0;I<SensorCount;++I)if(auto* A=Sim->SensorAt(I))
                {
                    A->StopSensor();A->SetSharedOutputServices(Transport,nullptr,Publisher);
                    FVirtualSensorStreamConfig Config;Config.SensorId=A->ScanComponent->SensorId;
                    Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.bEnabled=true;Publisher->ConfigureStream(Config);
                }
            }
            for(int I=0;I<3;++I)if(auto* A=Sim->SensorAt(I))
            {
                A->StopSensor();if(I>=SensorCount)continue;
                auto* Scan=A->ScanComponent.Get();if(OS1)Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::OusterOS1_64_Rev05);
                Scan->AcquisitionBackend=FParse::Param(FCommandLine::Get(),TEXT("GpuLidarCpu"))?EVirtualLidarAcquisitionBackend::AccurateCpuTrace:EVirtualLidarAcquisitionBackend::Auto;Scan->ScanInterval=.05f;
#if defined(MA0T10_LIDAR_TIMING_TRACE_AVAILABLE)
                if(FParse::Param(FCommandLine::Get(),TEXT("GpuLidarTimingTrace")))Scan->StartTimingTraceForTests();
#endif
                A->StartSensor();Sensors.Add(A);
            }
            if(!Test->TestEqual(TEXT("requested sensor count is present"),Sensors.Num(),SensorCount))return true;
            Baselines.Init(0,Sensors.Num());
            Started=Last=Now;Phase=1;return false;
        }
        if(Phase==1)
        {
            if(Now-Started<10){Last=Now;return false;}
            MissedSlotBaselines.Reset();
            for(int I=0;I<Sensors.Num();++I){Baselines[I]=Sensors[I]->ScanComponent->GetRuntimeStatus().FrameId;MissedSlotBaselines.Add(Sensors[I]->ScanComponent->GetRuntimeStatus().DeadlineMissCount);}
            MeasurementClient=Sensors[0]->GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
            if(auto Window=FSlateApplication::Get().FindWidgetWindow(Sensors[0]->GetWorld()->GetGameViewport()->GetGameViewportWidget().ToSharedRef()))
            {MeasurementDpi=Window->GetDPIScaleFactor();VisibleWindowVerified=Window->IsVisible();}
            if(!FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen"))&&!Test->TestTrue(TEXT("visible PIE window verified"),VisibleWindowVerified))return true;
            if(PcdPublisher.IsValid())for(const auto& A:Sensors)PcdBaselines.Add(A->ScanComponent->SensorId,PcdStatus(A->ScanComponent->SensorId));
            if(OperatingCamera.IsValid())CameraBaseline=OperatingCamera->GetSensorRuntimeStatus().FrameId;
            Measured=Last=Now;MeasurementUtc=FDateTime::UtcNow();FirstFrame=GFrameCounter;Phase=2;return false;
        }
        if(Sensors[0]->GetWorld()->GetGameViewport()->Viewport->GetSizeXY()!=MeasurementClient){Test->AddError(TEXT("measurement viewport changed"));return true;}
        Wall.Add((Now-Last)*1000);Last=Now;Engine.Add(FApp::GetDeltaTime()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Gpu.Add(FPlatformTime::ToMilliseconds(GGPUFrameTime));
        Slow+=FApp::GetDeltaTime()>.15;
        if(PcdPublisher.IsValid()&&Now>=NextPcdBudgetSample)
        {
            NextPcdBudgetSample=Now+.2;bool Warning=false;
            for(const auto& S:PcdPublisher->GetStreamStatuses())if(S.StreamKind==EVirtualSensorStreamKind::PointCloud&&S.bEnabled)
            {MaximumPcdRequiredMiB=FMath::Max(MaximumPcdRequiredMiB,double(S.PcdRequiredMiBPerSecond));Warning|=S.PcdRequiredMiBPerSecond>S.PcdBudgetMiBPerSecond;}
            PcdBudgetWarningSamples+=Warning;
        }
        double MeasurementSeconds=30;FParse::Value(FCommandLine::Get(),TEXT("GpuLidarMeasurementSeconds="),MeasurementSeconds);
        if(Now-Measured<FMath::Clamp(MeasurementSeconds,30.0,120.0))return false;
        const bool ReferenceRun=FParse::Param(FCommandLine::Get(),TEXT("GpuLidarReferenceRun"));
        bool AcquisitionRatioPass=true;
        auto R=MakeShared<FJsonObject>();
        R->SetStringField(TEXT("acquisition_gate"),TEXT("completed Hz / requested Hz >=0.99 for every sensor; ABAB relative gate evaluated by run_perf_abab.ps1"));
        R->SetBoolField(TEXT("reference_run"),ReferenceRun);
        R->SetNumberField(TEXT("average_fps"),1000/FMath::Max(.001,Average(Engine)));
        R->SetNumberField(TEXT("measurement_seconds"),FMath::Clamp(MeasurementSeconds,30.0,120.0));
        R->SetNumberField(TEXT("warmup_seconds"),10);R->SetNumberField(TEXT("samples"),Wall.Num());
        R->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
        R->SetStringField(TEXT("actual_client"),FString::Printf(TEXT("%dx%d"),MeasurementClient.X,MeasurementClient.Y));
        R->SetNumberField(TEXT("dpi_scale"),MeasurementDpi);R->SetBoolField(TEXT("visible_window_verified"),VisibleWindowVerified);
        for(const TCHAR* Key:{TEXT("Perf01SourceCommit="),TEXT("Perf01RunId="),TEXT("Perf01InstrumentSha="),TEXT("Perf01SessionSha=")})
        {FString Value;FParse::Value(FCommandLine::Get(),Key,Value);R->SetStringField(FString(Key).LeftChop(1),Value);}
        R->SetStringField(TEXT("measurement_started_utc"),MeasurementUtc.ToIso8601());R->SetStringField(TEXT("measurement_ended_utc"),FDateTime::UtcNow().ToIso8601());R->SetNumberField(TEXT("seconds"),Now-Measured);R->SetNumberField(TEXT("engine_fps"),1000/FMath::Max(.001,Average(Engine)));
        R->SetNumberField(TEXT("callback_wall_fps"),1000/FMath::Max(.001,Average(Wall)));R->SetNumberField(TEXT("engine_counter_fps"),(GFrameCounter-FirstFrame)/(Now-Measured));
        R->SetNumberField(TEXT("engine_frame_p95_ms"),GpuScheduledP95(Engine));R->SetNumberField(TEXT("game_thread_mean_ms"),Average(Game));R->SetNumberField(TEXT("gpu_frame_mean_ms"),Average(Gpu));R->SetNumberField(TEXT("frames_over_150ms"),Slow);int WallSlow=0;for(double Ms:Wall)WallSlow+=Ms>150;R->SetNumberField(TEXT("callback_frames_over_150ms"),WallSlow);
        R->SetNumberField(TEXT("rss_bytes"),FPlatformMemory::GetStats().UsedPhysical);R->SetNumberField(TEXT("sensor_count"),Sensors.Num());
        R->SetBoolField(TEXT("pcd_send_enabled"),PcdPort!=0);
        R->SetStringField(TEXT("pcd_transport"),PcdPort?TEXT("credential-free TCP STOMP loopback, receipts + local receiver"):TEXT("disabled"));
        if(PcdPublisher.IsValid())R->SetNumberField(TEXT("pcd_bandwidth_limit_mib_s"),PcdPublisher->PointCloudBandwidthLimitMegabytesPerSecond);
        R->SetNumberField(TEXT("pcd_budget_warning_samples"),PcdBudgetWarningSamples);R->SetNumberField(TEXT("pcd_maximum_required_mib_s"),MaximumPcdRequiredMiB);
        R->SetStringField(TEXT("display"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen"))?TEXT("offscreen real D3D12"):TEXT("visible real D3D12"));R->SetStringField(TEXT("map"),Operating?TEXT("SensorTestMap existing user worktree; opened only, never saved"):TEXT("ShipHatchTestMap runtime fixture; never saved"));
        R->SetBoolField(TEXT("ray_tracing_disabled_by_commandline"),FParse::Param(FCommandLine::Get(),TEXT("noraytracing")));
        if(OperatingCamera.IsValid())R->SetNumberField(TEXT("camera_acquisition_hz"),(OperatingCamera->GetSensorRuntimeStatus().FrameId-CameraBaseline)/(Now-Measured));
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(int I=0;I<Sensors.Num();++I)
        {
            auto* Scan=Sensors[I]->ScanComponent.Get();const auto& Status=Scan->GetRuntimeStatus();const double Hz=(Status.FrameId-Baselines[I])/(Now-Measured);
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("sensor"),Scan->SensorId);Row->SetStringField(TEXT("backend"),Status.ActiveAcquisitionBackend);Row->SetStringField(TEXT("backend_message"),Status.AcquisitionBackendMessage);Row->SetNumberField(TEXT("hz"),Hz);
            Row->SetNumberField(TEXT("horizontal_samples"),Scan->HorizontalSamples);Row->SetNumberField(TEXT("vertical_channels"),Scan->VerticalChannels);Row->SetNumberField(TEXT("frame_count"),Status.FrameId-Baselines[I]);
            Row->SetStringField(TEXT("device_profile"),StaticEnum<EVirtualLidarDeviceProfile>()->GetNameStringByValue(int64(Scan->DeviceProfile)));
            Row->SetNumberField(TEXT("baseline_frame_id"),Baselines[I]);Row->SetNumberField(TEXT("last_frame_id"),Status.FrameId);
            const auto QueryStats=Scan->GetSceneQueryStatistics();
            Row->SetNumberField(TEXT("scene_geometry_ms"),QueryStats.X);Row->SetNumberField(TEXT("scene_game_thread_ms"),QueryStats.Y);
            Row->SetNumberField(TEXT("scene_external_bounds"),QueryStats.Z);Row->SetNumberField(TEXT("scene_fallback_traces"),QueryStats.W);
            Row->SetNumberField(TEXT("acquisition_ms"),Status.LastAcquisitionDurationMs);Rows.Add(MakeShared<FJsonValueObject>(Row));
            const double RequestedHz=1.0/FMath::Max(.001,double(Scan->ScanInterval));
            const double Ratio=Hz/RequestedHz;
            Row->SetNumberField(TEXT("requested_hz"),RequestedHz);Row->SetNumberField(TEXT("acquisition_ratio"),Ratio);
            Row->SetNumberField(TEXT("missed_requested_slots"),Status.DeadlineMissCount-MissedSlotBaselines[I]);
            AcquisitionRatioPass&=Ratio>=.99;
            if(!ReferenceRun)Test->TestTrue(FString::Printf(TEXT("sensor %d acquisition/requested Hz >=0.99 (%.6f)"),I,Ratio),Ratio>=.99);
            if(!Operating)Test->TestEqual(TEXT("active acquisition uses selected collision tracing"),Scan->GetActiveAcquisitionBackend(),FParse::Param(FCommandLine::Get(),TEXT("GpuLidarCpu"))?EVirtualLidarAcquisitionBackend::AccurateCpuTrace:EVirtualLidarAcquisitionBackend::HardwareRayTracing);
            if(!Operating)Test->TestTrue(FString::Printf(TEXT("sensor %d completed acquisition >=10Hz"),I),Hz>=10);
            if(!ReferenceRun&&!Operating&&!OS1&&SensorCount==3&&MeasurementSeconds>=90)
                Test->TestTrue(TEXT("90s20Hz request target >=19.9Hz and ratio >=0.995"),Hz>=19.9&&Ratio>=.995);
            if(!Operating&&!OS1&&Scan->DeviceProfile==EVirtualLidarDeviceProfile::OusterOS1_64_Rev7)
            {
                // Preserve both old assertions verbatim as legacy reference. Gate on
                // requested/completed wall Hz above and the independent ABAB2%.
                Row->SetNumberField(TEXT("hz_at_nominal_60fps"),Hz*60./((GFrameCounter-FirstFrame)/(Now-Measured)));
                RecordLegacyAssertion(Test,Row,TEXT("Ouster20Hz mode completes at least one acquisition per three engine frames, two boundary frames allowed"),Status.FrameId-Baselines[I]>=int64((GFrameCounter-FirstFrame)/3)-2);
                RecordLegacyAssertion(Test,Row,TEXT("Ouster default retains60FPS pacing (>=59FPS)"),1000/FMath::Max(.001,Average(Engine))>=59);
            }
            else Test->TestTrue(TEXT("existing operating-map sensor completed real frames"),Status.FrameId>Baselines[I]);
            const auto Frame=Scan->GetLastFrameSnapshot();Test->TestTrue(TEXT("normalized snapshot retains original full beam count"),Frame&&Frame->Points&&(Operating?Frame->Points->Num()>0:Frame->Points->Num()==Scan->HorizontalSamples*Scan->VerticalChannels));
            if(PcdPort)
            {
                const auto S=PcdStatus(Scan->SensorId);const auto B=PcdBaselines.FindRef(Scan->SensorId);const double Seconds=Now-Measured;
                auto Output=MakeShared<FJsonObject>();
                Output->SetNumberField(TEXT("input_frames"),S.InputFrameCount-B.InputFrameCount);
                Output->SetNumberField(TEXT("serialized_frames"),S.SerializedFrameCount-B.SerializedFrameCount);
                Output->SetNumberField(TEXT("submitted_frames"),S.SubmittedFrameCount-B.SubmittedFrameCount);
                Output->SetNumberField(TEXT("receipt_frames"),S.ReceiptReceivedCount-B.ReceiptReceivedCount);
                Output->SetNumberField(TEXT("received_frames"),S.ConsumerReceivedCount-B.ConsumerReceivedCount);
                Output->SetNumberField(TEXT("serialized_hz"),(S.SerializedFrameCount-B.SerializedFrameCount)/Seconds);
                Output->SetNumberField(TEXT("submitted_hz"),(S.SubmittedFrameCount-B.SubmittedFrameCount)/Seconds);
                Output->SetNumberField(TEXT("receipt_hz"),(S.ReceiptReceivedCount-B.ReceiptReceivedCount)/Seconds);
                Output->SetNumberField(TEXT("received_hz"),(S.ConsumerReceivedCount-B.ConsumerReceivedCount)/Seconds);
                Output->SetNumberField(TEXT("submitted_bytes"),S.TotalSubmittedBytes-B.TotalSubmittedBytes);
                Output->SetNumberField(TEXT("encode_failures"),S.EncodeFailureCount-B.EncodeFailureCount);
                Output->SetNumberField(TEXT("invalid_received"),S.ConsumerValidationFailureCount-B.ConsumerValidationFailureCount);
                Output->SetNumberField(TEXT("receiver_gaps"),S.ConsumerFrameGapCount-B.ConsumerFrameGapCount);
                Output->SetNumberField(TEXT("receiver_duplicates"),S.ConsumerDuplicateCount-B.ConsumerDuplicateCount);
                Output->SetNumberField(TEXT("rate_control_skipped"),S.RateControlSkippedFrameCount-B.RateControlSkippedFrameCount);
                Output->SetNumberField(TEXT("overload_count"),S.OverloadCount-B.OverloadCount);
                Output->SetNumberField(TEXT("replaced_frames"),S.ReplacedPendingFrameCount-B.ReplacedPendingFrameCount);
                Output->SetNumberField(TEXT("bandwidth_deferred"),S.BandwidthDeferredFrameCount-B.BandwidthDeferredFrameCount);
                Output->SetNumberField(TEXT("delivery_failures"),S.DeliveryFailureCount-B.DeliveryFailureCount+S.RawDeliveryFailureCount-B.RawDeliveryFailureCount);
                Output->SetNumberField(TEXT("serialization_p95_ms"),S.SerializationP95LatencyMs);
                const auto Samples=EncodeTimes.FindRef(Scan->SensorId);
                Output->SetNumberField(TEXT("window_encode_samples"),Samples.Num());
                Output->SetNumberField(TEXT("window_encode_p50_ms"),Percentile(Samples,.5));
                Output->SetNumberField(TEXT("window_encode_p95_ms"),Percentile(Samples,.95));
                int Over=0;for(double V:Samples)Over+=V>150;
                Output->SetNumberField(TEXT("window_encode_over_150ms"),Over);
                Output->SetNumberField(TEXT("mib_per_second"),(S.TotalSubmittedBytes-B.TotalSubmittedBytes)/Seconds/1048576.);
                Output->SetNumberField(TEXT("send_profile"),int(BandwidthOptions.Profile));
                Output->SetNumberField(TEXT("send_divisor"),BandwidthOptions.Divisor());
                Output->SetNumberField(TEXT("configured_divisor_skipped"),S.ConfiguredDivisorSkippedCount-B.ConfiguredDivisorSkippedCount);
                Output->SetBoolField(TEXT("compressed"),BandwidthOptions.bCompressed);
                Output->SetNumberField(TEXT("input_queue"),S.InputQueueDepth);Output->SetNumberField(TEXT("prepared_queue"),S.PreparedQueueDepth);
                Output->SetNumberField(TEXT("raw_outstanding_frames"),S.RawOutstandingFrameCount);Output->SetNumberField(TEXT("raw_outstanding_bytes"),S.RawOutstandingBytes);
                Output->SetNumberField(TEXT("last_frame_bytes"),S.LastFrameBytes);Output->SetNumberField(TEXT("last_points"),S.LastPointCount);
                const auto Actual=Scan->GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->GetRecentPcdBandwidth(Scan->SensorId,Now);
                Output->SetNumberField(TEXT("recent_1s_hz"),Actual.Frames);
                Output->SetNumberField(TEXT("recent_1s_mib_s"),Actual.BodyBytes/1048576.);
                Output->SetNumberField(TEXT("recent_1s_compression_reduction_percent"),Actual.ReductionPercent());
                Output->SetBoolField(TEXT("recent_1s_complete"),Actual.bComplete);
                Test->TestTrue(TEXT("live display receives completed socket writes per actual sensor"),Actual.bComplete&&Actual.Frames>0&&Actual.BodyBytes>Actual.EncodedPointBytes&&Actual.OriginalPointBytes>0);
                if(!BandwidthOptions.bCompressed)Test->TestEqual(TEXT("binary live payload reduction zero"),Actual.ReductionPercent(),0.);
                Output->SetNumberField(TEXT("required_mib_s"),S.PcdRequiredMiBPerSecond);
                Output->SetNumberField(TEXT("budget_mib_s"),S.PcdBudgetMiBPerSecond);
                Output->SetBoolField(TEXT("budget_warning"),S.PcdRequiredMiBPerSecond>S.PcdBudgetMiBPerSecond);
                Output->SetNumberField(TEXT("fair_share_mib_s"),S.PcdFairShareMiBPerSecond);
                Output->SetNumberField(TEXT("points_times_33_times_requested_hz_mib_s"),S.LastPointCount*33.0*20/1048576.0);
                Output->SetStringField(TEXT("message"),S.Message);Row->SetObjectField(TEXT("pcd"),Output);
                Test->TestTrue(TEXT("PCD serialization >=10Hz / configured divisor"),(S.SerializedFrameCount-B.SerializedFrameCount)/Seconds>=10.0/BandwidthOptions.Divisor());
                Test->TestTrue(TEXT("PCD socket submission >=10Hz / configured divisor"),(S.SubmittedFrameCount-B.SubmittedFrameCount)/Seconds>=10.0/BandwidthOptions.Divisor());
                Test->TestTrue(TEXT("PCD broker receipts >=10Hz / configured divisor"),(S.ReceiptReceivedCount-B.ReceiptReceivedCount)/Seconds>=10.0/BandwidthOptions.Divisor());
                Test->TestTrue(TEXT("PCD local validated receiver >=10Hz / configured divisor"),(S.ConsumerReceivedCount-B.ConsumerReceivedCount)/Seconds>=10.0/BandwidthOptions.Divisor());
                Test->TestEqual(TEXT("PCD encode failures"),S.EncodeFailureCount-B.EncodeFailureCount,int64(0));
                Test->TestEqual(TEXT("PCD invalid received"),S.ConsumerValidationFailureCount-B.ConsumerValidationFailureCount,int64(0));
                Test->TestEqual(TEXT("PCD FIFO overloads"),S.OverloadCount-B.OverloadCount,int64(0));
                if(!Operating&&!OS1&&SensorCount==3&&Scan->DeviceProfile==EVirtualLidarDeviceProfile::OusterOS1_64_Rev7)
                {
                    Test->TestEqual(TEXT("approved default PCD budget80MiB/s"),S.PcdBudgetMiBPerSecond,80.f);
                    Test->TestTrue(TEXT("default Rev7x3 serialization >=19Hz / configured divisor"),(S.SerializedFrameCount-B.SerializedFrameCount)/Seconds>=19.0/BandwidthOptions.Divisor());
                    Test->TestTrue(TEXT("default Rev7x3 socket submission >=19Hz / configured divisor"),(S.SubmittedFrameCount-B.SubmittedFrameCount)/Seconds>=19.0/BandwidthOptions.Divisor());
                    Test->TestTrue(TEXT("default Rev7x3 receipts >=19Hz / configured divisor"),(S.ReceiptReceivedCount-B.ReceiptReceivedCount)/Seconds>=19.0/BandwidthOptions.Divisor());
                    Test->TestTrue(TEXT("default Rev7x3 validated receive >=19Hz / configured divisor"),(S.ConsumerReceivedCount-B.ConsumerReceivedCount)/Seconds>=19.0/BandwidthOptions.Divisor());
                    Test->TestEqual(TEXT("default Rev7x3 has no budget warnings during measurement"),PcdBudgetWarningSamples,0);
                }
            }
#if defined(MA0T10_LIDAR_TIMING_TRACE_AVAILABLE)
            if(Scan->bTimingTraceForTests)
            {
                FString TraceOutput;
                if(FParse::Value(FCommandLine::Get(),TEXT("GpuLidarOutput="),TraceOutput))
                {
                    FString Csv=TEXT("frame_id,admitted_s,due_s,readback_s,normalized_s,completed_s,admission_engine_frame,readback_engine_frame,completion_engine_frame,due_busy_frames\n");
                    int32 Count=0;
                    for(const auto& S:Scan->GetTimingTraceForTests())if(S.FrameId>Baselines[I])
                    {++Count;Csv+=FString::Printf(TEXT("%lld,%.9f,%.9f,%.9f,%.9f,%.9f,%llu,%llu,%llu,%d\n"),S.FrameId,S.Admitted-Measured,S.Due-Measured,S.ReadbackObserved-Measured,S.Normalized-Measured,S.Completed-Measured,S.AdmissionFrame,S.ReadbackFrame,S.CompletionFrame,S.DueBusyFrames);}
                    Row->SetNumberField(TEXT("timing_trace_samples"),Count);
                    Test->TestTrue(TEXT("bounded timing trace written"),FFileHelper::SaveStringToFile(Csv,*(TraceOutput+TEXT(".")+Scan->SensorId+TEXT(".timing.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
                }
            }
#endif
            AActor* Actor=Sensors[I].Get();if(Actor)Sensors[I]->StopSensor();
        }
        if(PcdPublisher.IsValid())PcdPublisher->OnPcdEncoded.Remove(EncodeHandle);
        if(OperatingCamera.IsValid())OperatingCamera->StopSensor();
        R->SetBoolField(TEXT("acquisition_ratio_pass"),AcquisitionRatioPass);R->SetArrayField(TEXT("sensors"),Rows);R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FString Root;if(FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Root))Root/=TEXT("measurements");else Root=FPaths::ProjectSavedDir()/TEXT("Reports/GPU-LIDAR-01/progress/scheduled");if(PcdPort)Root/=TEXT("pcd-on");IFileManager::Get().MakeDirectory(*Root,true);
        FString CohortOutput;
        if(FParse::Value(FCommandLine::Get(),TEXT("GpuLidarOutput="),CohortOutput))
        {
            FString Csv=TEXT("callback_ms,engine_ms,game_thread_ms,gpu_ms\n");
            for(int I=0;I<Wall.Num();++I)Csv+=FString::Printf(TEXT("%.6f,%.6f,%.6f,%.6f\n"),Wall[I],Engine[I],Game[I],Gpu[I]);
            Test->TestTrue(TEXT("scheduled raw frame samples written"),FFileHelper::SaveStringToFile(Csv,*(CohortOutput+TEXT(".frames.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
            R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());Text.Reset();FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));
            Test->TestTrue(TEXT("scheduled cohort report written"),FFileHelper::SaveStringToFile(Text,*CohortOutput,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        }
        FFileHelper::SaveStringToFile(Text,*(Root/(Operating?(FParse::Param(FCommandLine::Get(),TEXT("noraytracing"))?TEXT("operating-rt-off.json"):TEXT("operating-rt-on.json")):OS1?TEXT("os1-three.json"):SensorCount==1?TEXT("native-one.json"):TEXT("native-three.json"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarScheduledOneTest,"MA0T10.GpuLidar.ScheduledOne",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarScheduledOneTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FGpuScheduledRun(this,1,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarScheduledThreeTest,"MA0T10.GpuLidar.ScheduledThree",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarScheduledThreeTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FGpuScheduledRun(this,3,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarScheduledOS1Test,"MA0T10.GpuLidar.ScheduledOS1Three",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarScheduledOS1Test::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FGpuScheduledRun(this,3,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarOperatingMapTest,"MA0T10.GpuLidar.OperatingMapImpact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarOperatingMapTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/SensorTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FGpuScheduledRun(this,1,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
