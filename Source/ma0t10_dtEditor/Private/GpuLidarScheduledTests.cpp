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
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

namespace
{
double Average(const TArray<double>& A){double S=0;for(double V:A)S+=V;return A.IsEmpty()?0:S/A.Num();}
double GpuScheduledP95(TArray<double> A){A.Sort();return A.IsEmpty()?0:A[FMath::Clamp(FMath::CeilToInt(A.Num()*.95)-1,0,A.Num()-1)];}
class FGpuScheduledRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int SensorCount;bool OS1,Operating;TWeakObjectPtr<AVirtualCameraSensorActor> OperatingCamera;
    int64 CameraBaseline=0;int Phase=0;FDateTime MeasurementUtc;double Started=FPlatformTime::Seconds(),Last=0,Measured=0;
    TArray<TWeakObjectPtr<AVirtualLidarSensorActor>> Sensors;TArray<int64> Baselines;
    TArray<double> Engine,Wall,Game,Gpu;uint64 FirstFrame=0;int Slow=0;
    TWeakObjectPtr<UVirtualSensorStreamPublisherComponent> PcdPublisher;
    TMap<FString,FVirtualSensorStreamStatus> PcdBaselines;
    int PcdPort=0;
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
            if(!Test->TestTrue(TEXT("scheduled test requires actual HWRT pipeline support"),SensorOwnedRayTracing::IsAvailable()))return true;
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
                // Use the production normalization/default budgets: PCD binary, FIFO,
                // stride 1, receipt every frame, 20 retained frames, 64 MiB/s.
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
                Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::Auto;Scan->ScanInterval=.05f;A->StartSensor();Sensors.Add(A);
            }
            Test->TestEqual(TEXT("requested sensor count is present"),Sensors.Num(),SensorCount);Baselines.Init(0,Sensors.Num());
            Started=Last=Now;Phase=1;return false;
        }
        if(Phase==1)
        {
            if(Now-Started<10){Last=Now;return false;}
            for(int I=0;I<Sensors.Num();++I)Baselines[I]=Sensors[I]->ScanComponent->GetRuntimeStatus().FrameId;
            if(PcdPublisher.IsValid())for(const auto& A:Sensors)PcdBaselines.Add(A->ScanComponent->SensorId,PcdStatus(A->ScanComponent->SensorId));
            if(OperatingCamera.IsValid())CameraBaseline=OperatingCamera->GetSensorRuntimeStatus().FrameId;
            Measured=Last=Now;MeasurementUtc=FDateTime::UtcNow();FirstFrame=GFrameCounter;Phase=2;return false;
        }
        Wall.Add((Now-Last)*1000);Last=Now;Engine.Add(FApp::GetDeltaTime()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Gpu.Add(FPlatformTime::ToMilliseconds(GGPUFrameTime));
        Slow+=FApp::GetDeltaTime()>.15;
        double MeasurementSeconds=30;FParse::Value(FCommandLine::Get(),TEXT("GpuLidarMeasurementSeconds="),MeasurementSeconds);
        if(Now-Measured<FMath::Clamp(MeasurementSeconds,30.0,120.0))return false;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("measurement_started_utc"),MeasurementUtc.ToIso8601());R->SetStringField(TEXT("measurement_ended_utc"),FDateTime::UtcNow().ToIso8601());R->SetNumberField(TEXT("seconds"),Now-Measured);R->SetNumberField(TEXT("engine_fps"),1000/FMath::Max(.001,Average(Engine)));
        R->SetNumberField(TEXT("callback_wall_fps"),1000/FMath::Max(.001,Average(Wall)));R->SetNumberField(TEXT("engine_counter_fps"),(GFrameCounter-FirstFrame)/(Now-Measured));
        R->SetNumberField(TEXT("engine_frame_p95_ms"),GpuScheduledP95(Engine));R->SetNumberField(TEXT("game_thread_mean_ms"),Average(Game));R->SetNumberField(TEXT("gpu_frame_mean_ms"),Average(Gpu));R->SetNumberField(TEXT("frames_over_150ms"),Slow);int WallSlow=0;for(double Ms:Wall)WallSlow+=Ms>150;R->SetNumberField(TEXT("callback_frames_over_150ms"),WallSlow);
        R->SetNumberField(TEXT("rss_bytes"),FPlatformMemory::GetStats().UsedPhysical);R->SetNumberField(TEXT("sensor_count"),Sensors.Num());
        R->SetBoolField(TEXT("pcd_send_enabled"),PcdPort!=0);
        R->SetStringField(TEXT("pcd_transport"),PcdPort?TEXT("credential-free TCP STOMP loopback, receipts + local receiver"):TEXT("disabled"));
        if(PcdPublisher.IsValid())R->SetNumberField(TEXT("pcd_bandwidth_limit_mib_s"),PcdPublisher->PointCloudBandwidthLimitMegabytesPerSecond);
        R->SetStringField(TEXT("display"),TEXT("offscreen real D3D12"));R->SetStringField(TEXT("map"),Operating?TEXT("SensorTestMap existing user worktree; opened only, never saved"):TEXT("ShipHatchTestMap runtime fixture; never saved"));
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
            Row->SetNumberField(TEXT("acquisition_ms"),Status.LastAcquisitionDurationMs);Rows.Add(MakeShared<FJsonValueObject>(Row));
            if(!Operating)Test->TestEqual(TEXT("active acquisition uses hardware collision tracing"),Scan->GetActiveAcquisitionBackend(),EVirtualLidarAcquisitionBackend::HardwareRayTracing);
            if(!Operating)Test->TestTrue(FString::Printf(TEXT("sensor %d completed acquisition >=10Hz"),I),Hz>=10);
            if(!Operating&&!OS1&&Scan->DeviceProfile==EVirtualLidarDeviceProfile::OusterOS1_64_Rev7)
            {
                // The 60FPS limiter runs slightly below60 in wall time. Check
                // nominal20Hz against actual engine frames; keep wall Hz above
                // and the existing absolute10Hz assertion independently.
                Row->SetNumberField(TEXT("hz_at_nominal_60fps"),Hz*60./((GFrameCounter-FirstFrame)/(Now-Measured)));
                Test->TestTrue(TEXT("Ouster20Hz mode completes at least one acquisition per three engine frames, two boundary frames allowed"),Status.FrameId-Baselines[I]>=int64((GFrameCounter-FirstFrame)/3)-2);
                Test->TestTrue(TEXT("Ouster default retains60FPS pacing (>=59FPS)"),1000/FMath::Max(.001,Average(Engine))>=59);
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
                Output->SetNumberField(TEXT("input_queue"),S.InputQueueDepth);Output->SetNumberField(TEXT("prepared_queue"),S.PreparedQueueDepth);
                Output->SetNumberField(TEXT("raw_outstanding_frames"),S.RawOutstandingFrameCount);Output->SetNumberField(TEXT("raw_outstanding_bytes"),S.RawOutstandingBytes);
                Output->SetNumberField(TEXT("last_frame_bytes"),S.LastFrameBytes);Output->SetNumberField(TEXT("last_points"),S.LastPointCount);
                Output->SetNumberField(TEXT("required_mib_s"),S.PcdRequiredMiBPerSecond);
                Output->SetNumberField(TEXT("fair_share_mib_s"),S.PcdFairShareMiBPerSecond);
                Output->SetNumberField(TEXT("points_times_33_times_requested_hz_mib_s"),S.LastPointCount*33.0*20/1048576.0);
                Output->SetStringField(TEXT("message"),S.Message);Row->SetObjectField(TEXT("pcd"),Output);
                Test->TestTrue(TEXT("PCD serialization >=10Hz"),(S.SerializedFrameCount-B.SerializedFrameCount)/Seconds>=10);
                Test->TestTrue(TEXT("PCD socket submission >=10Hz"),(S.SubmittedFrameCount-B.SubmittedFrameCount)/Seconds>=10);
                Test->TestTrue(TEXT("PCD broker receipts >=10Hz"),(S.ReceiptReceivedCount-B.ReceiptReceivedCount)/Seconds>=10);
                Test->TestTrue(TEXT("PCD local validated receiver >=10Hz"),(S.ConsumerReceivedCount-B.ConsumerReceivedCount)/Seconds>=10);
                Test->TestEqual(TEXT("PCD encode failures"),S.EncodeFailureCount-B.EncodeFailureCount,int64(0));
                Test->TestEqual(TEXT("PCD invalid received"),S.ConsumerValidationFailureCount-B.ConsumerValidationFailureCount,int64(0));
                Test->TestEqual(TEXT("PCD FIFO overloads"),S.OverloadCount-B.OverloadCount,int64(0));
            }
            AActor* Actor=Sensors[I].Get();if(Actor)Sensors[I]->StopSensor();
        }
        if(OperatingCamera.IsValid())OperatingCamera->StopSensor();
        R->SetArrayField(TEXT("sensors"),Rows);R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FString Root;if(FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Root))Root/=TEXT("measurements");else Root=FPaths::ProjectSavedDir()/TEXT("Reports/GPU-LIDAR-01/progress/scheduled");if(PcdPort)Root/=TEXT("pcd-on");IFileManager::Get().MakeDirectory(*Root,true);
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
