#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"

namespace
{
class FCoilOusterPcdRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Port,Phase=0;double Started=FPlatformTime::Seconds(),Warm=0,Measured=0,NextSample=0;
    TWeakObjectPtr<AVirtualLidarSensorActor> Sensor;TWeakObjectPtr<UVirtualSensorStreamPublisherComponent> Publisher;
    int64 BaselineFrame=0;FVirtualSensorStreamStatus Baseline;
    FDateTime MeasurementUtc;double MaximumRequired=0,MaxRawBytes=0,MaxGlobalBytes=0;int MaxQueue=0,WarningSamples=0;
    FVirtualSensorStreamStatus Status() const
    {if(Publisher.IsValid()&&Sensor.IsValid())for(const auto& S:Publisher->GetStreamStatuses())if(S.StreamKind==EVirtualSensorStreamKind::PointCloud&&S.SensorId==Sensor->GetSensorId())return S;return {};}
public:
    FCoilOusterPcdRun(FAutomationTestBase* T,int P):Test(T),Port(P){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Started>120){Test->AddError(TEXT("COIL-OUSTER PCD fixture timeout"));return true;}
        if(Phase==0)
        {
            for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&FinalAcceptanceViewportReady(C.World()))
            for(TActorIterator<ACoilLoadingMonitorHost> H(C.World());H;++H)if(H->Simulation&&H->Monitor&&H->Coordinator&&H->Simulation->Lidar)
            {
                auto& Sim=*H->Simulation;Sensor=Sim.Lidar.Get();
                if(!Test->TestTrue(TEXT("PCD proof uses actual default Rev7, not a test profile override"),Sensor->ScanComponent->DeviceProfile==EVirtualLidarDeviceProfile::OusterOS1_64_Rev7))return true;
                Test->TestTrue(TEXT("representative base row"),Sim.AddTier());for(int I=0;I<3;++I)Test->TestTrue(TEXT("supported second tier"),Sim.PlaceNext(false));
                H->Coordinator->StreamPublisherComponent->StopAllStreams(Sensor->GetSensorId());Sensor->StopSensor();
                auto* Transport=NewObject<UVirtualSensorTransportComponent>(&Sim);Sim.AddInstanceComponent(Transport);Transport->RegisterComponent();Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
                FVirtualSensorTransportProfile Profile;Profile.BrokerUrl=FString::Printf(TEXT("tcp://127.0.0.1:%d"),Port);Profile.UserName.Reset();Transport->ConfigureTransportProfile(Profile);Transport->SetSessionCredentials(TEXT(""),TEXT(""));
                auto* P=NewObject<UVirtualSensorStreamPublisherComponent>(&Sim);Sim.AddInstanceComponent(P);P->SetTransportComponent(Transport);P->RegisterComponent();Publisher=P;
                FVirtualPcdSendOptions Options;Options.Profile=EVirtualPcdFieldProfile::Full;Options.bCompressed=false;Options.SendDivisor=1;FString Error;
                Test->TestTrue(TEXT("full uncompressed single-frame proof options"),P->SetPcdSendOptions(Sensor->GetSensorId(),Options,false,Error));
                Sensor->SetSharedOutputServices(Transport,nullptr,P);FVirtualSensorStreamConfig Config;Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.SensorId=Sensor->GetSensorId();Config.bEnabled=true;P->ConfigureStream(Config);
                Test->TestEqual(TEXT("production default budget remains80MiB/s"),P->PointCloudBandwidthLimitMegabytesPerSecond,80.f);
                Sensor->StartSensor();H->Monitor->SelectView(0);Warm=Now;Phase=1;return false;
            }
            return false;
        }
        if(!Sensor.IsValid()||!Publisher.IsValid()){Test->AddError(TEXT("owned PCD fixture disappeared"));return true;}
        if(Phase==1)
        {if(Now-Warm<10)return false;Baseline=Status();BaselineFrame=Sensor->GetSensorRuntimeStatus().FrameId;Measured=Now;MeasurementUtc=FDateTime::UtcNow();Phase=2;return false;}
        const auto S=Status();
        if(Now>=NextSample)
        {
            NextSample=Now+.2;MaximumRequired=FMath::Max(MaximumRequired,double(S.PcdRequiredMiBPerSecond));WarningSamples+=S.PcdRequiredMiBPerSecond>S.PcdBudgetMiBPerSecond;
            MaxRawBytes=FMath::Max(MaxRawBytes,double(S.RawOutstandingBytes));MaxGlobalBytes=FMath::Max(MaxGlobalBytes,double(S.RawGlobalOutstandingBytes));MaxQueue=FMath::Max(MaxQueue,S.InputQueueDepth+S.PreparedQueueDepth+S.ReceiptQueueDepth);
        }
        const double Seconds=Now-Measured;if(Seconds<30)return false;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("measurement_started_utc"),MeasurementUtc.ToIso8601());R->SetStringField(TEXT("measurement_ended_utc"),FDateTime::UtcNow().ToIso8601());R->SetNumberField(TEXT("seconds"),Seconds);
        const int64 Acquired=Sensor->GetSensorRuntimeStatus().FrameId-BaselineFrame;
        R->SetNumberField(TEXT("acquired_frames"),Acquired);R->SetNumberField(TEXT("acquisition_hz"),Acquired/Seconds);
        struct FCounter{const TCHAR* Name;int64 Value;};
        for(const auto& C:TArray<FCounter>{{TEXT("input_frames"),S.InputFrameCount-Baseline.InputFrameCount},{TEXT("serialized_frames"),S.SerializedFrameCount-Baseline.SerializedFrameCount},{TEXT("submitted_frames"),S.SubmittedFrameCount-Baseline.SubmittedFrameCount},{TEXT("receipts"),S.ReceiptReceivedCount-Baseline.ReceiptReceivedCount},{TEXT("internally_validated_frames"),S.ConsumerReceivedCount-Baseline.ConsumerReceivedCount}})
        {R->SetNumberField(C.Name,C.Value);R->SetNumberField(FString(C.Name)+TEXT("_hz"),C.Value/Seconds);Test->TestTrue(*FString::Printf(TEXT("Rev7 %s >=19Hz, same default-device throughput strength"),C.Name),C.Value/Seconds>=19.);}
        for(const auto& C:TArray<FCounter>{{TEXT("encode_failures"),S.EncodeFailureCount-Baseline.EncodeFailureCount},{TEXT("invalid_received"),S.ConsumerValidationFailureCount-Baseline.ConsumerValidationFailureCount},{TEXT("receiver_gaps"),S.ConsumerFrameGapCount-Baseline.ConsumerFrameGapCount},{TEXT("duplicates"),S.ConsumerDuplicateCount-Baseline.ConsumerDuplicateCount},{TEXT("overloads"),S.OverloadCount-Baseline.OverloadCount},{TEXT("replaced_frames"),S.ReplacedPendingFrameCount-Baseline.ReplacedPendingFrameCount},{TEXT("rate_control_skipped"),S.RateControlSkippedFrameCount-Baseline.RateControlSkippedFrameCount},{TEXT("delivery_failures"),S.DeliveryFailureCount-Baseline.DeliveryFailureCount+S.RawDeliveryFailureCount-Baseline.RawDeliveryFailureCount}})
        {R->SetNumberField(C.Name,C.Value);Test->TestEqual(*FString::Printf(TEXT("no %s"),C.Name),C.Value,int64(0));}
        Test->TestTrue(TEXT("actual default acquisition >=19Hz"),Acquired/Seconds>=19.);
        Test->TestTrue(TEXT("actual full PCD need remains within approved80MiB/s"),MaximumRequired>0&&MaximumRequired<=80&&WarningSamples==0);
        const double ActualMiB=(S.TotalSubmittedBytes-Baseline.TotalSubmittedBytes)/Seconds/1048576.;Test->TestTrue(TEXT("actual socket throughput is positive and within80MiB/s"),ActualMiB>0&&ActualMiB<=80);
        R->SetNumberField(TEXT("wire_mib_s"),ActualMiB);R->SetNumberField(TEXT("maximum_required_mib_s"),MaximumRequired);R->SetNumberField(TEXT("budget_mib_s"),80);R->SetNumberField(TEXT("budget_warning_samples"),WarningSamples);
        R->SetNumberField(TEXT("last_points"),S.LastPointCount);R->SetNumberField(TEXT("last_frame_bytes"),S.LastFrameBytes);R->SetNumberField(TEXT("max_sampled_raw_bytes"),MaxRawBytes);R->SetNumberField(TEXT("max_sampled_global_bytes"),MaxGlobalBytes);R->SetNumberField(TEXT("max_sampled_queue"),MaxQueue);
        R->SetStringField(TEXT("scope"),TEXT("credential-free TCP STOMP loopback, Full33-byte binary, no compression/division; receipt is not external business ACK; counts use measured wall window; queue peaks sampled5Hz"));
        R->SetStringField(TEXT("backend"),Sensor->GetSensorRuntimeStatus().ActiveAcquisitionBackend);R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/COIL-OUSTER/pcd");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("sender.json")));
        Sensor->StopSensor();return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilOusterPcdTest,"MA0T10.CoilLoading.Ouster.PcdLoopbackRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilOusterPcdTest::RunTest(const FString&)
{
    int Port=0;if(!FParse::Value(FCommandLine::Get(),TEXT("CoilOusterPcdPort="),Port)){AddInfo(TEXT("[OPT-IN SKIP] -CoilOusterPcdPort=<credential-free loopback port> required"));return true;}
    if(!TestTrue(TEXT("unprivileged loopback port"),Port>=1024&&Port<=65535))return false;
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilOusterPcdRun(this,Port));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
