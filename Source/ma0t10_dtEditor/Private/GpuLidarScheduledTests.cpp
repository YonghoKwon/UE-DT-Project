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

namespace
{
double Average(const TArray<double>& A){double S=0;for(double V:A)S+=V;return A.IsEmpty()?0:S/A.Num();}
double P95(TArray<double> A){A.Sort();return A.IsEmpty()?0:A[FMath::Clamp(FMath::CeilToInt(A.Num()*.95)-1,0,A.Num()-1)];}
class FGpuScheduledRun final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int SensorCount;bool OS1,Operating;TWeakObjectPtr<AVirtualCameraSensorActor> OperatingCamera;
    int64 CameraBaseline=0;int Phase=0;double Started=FPlatformTime::Seconds(),Last=0,Measured=0;
    TArray<TWeakObjectPtr<AVirtualLidarSensorActor>> Sensors;TArray<int64> Baselines;
    TArray<double> Engine,Wall,Game,Gpu;uint64 FirstFrame=0;int Slow=0;
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
            if(OperatingCamera.IsValid())CameraBaseline=OperatingCamera->GetSensorRuntimeStatus().FrameId;
            Measured=Last=Now;FirstFrame=GFrameCounter;Phase=2;return false;
        }
        Wall.Add((Now-Last)*1000);Last=Now;Engine.Add(FApp::GetDeltaTime()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Gpu.Add(FPlatformTime::ToMilliseconds(GGPUFrameTime));
        Slow+=FApp::GetDeltaTime()>.15;
        if(Now-Measured<30)return false;
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("seconds"),Now-Measured);R->SetNumberField(TEXT("engine_fps"),1000/FMath::Max(.001,Average(Engine)));
        R->SetNumberField(TEXT("callback_wall_fps"),1000/FMath::Max(.001,Average(Wall)));R->SetNumberField(TEXT("engine_counter_fps"),(GFrameCounter-FirstFrame)/(Now-Measured));
        R->SetNumberField(TEXT("engine_frame_p95_ms"),P95(Engine));R->SetNumberField(TEXT("game_thread_mean_ms"),Average(Game));R->SetNumberField(TEXT("gpu_frame_mean_ms"),Average(Gpu));R->SetNumberField(TEXT("frames_over_150ms"),Slow);
        R->SetNumberField(TEXT("rss_bytes"),FPlatformMemory::GetStats().UsedPhysical);R->SetNumberField(TEXT("sensor_count"),Sensors.Num());
        R->SetStringField(TEXT("display"),TEXT("offscreen real D3D12"));R->SetStringField(TEXT("map"),Operating?TEXT("SensorTestMap existing user worktree; opened only, never saved"):TEXT("ShipHatchTestMap runtime fixture; never saved"));
        R->SetBoolField(TEXT("ray_tracing_disabled_by_commandline"),FParse::Param(FCommandLine::Get(),TEXT("noraytracing")));
        if(OperatingCamera.IsValid())R->SetNumberField(TEXT("camera_acquisition_hz"),(OperatingCamera->GetSensorRuntimeStatus().FrameId-CameraBaseline)/(Now-Measured));
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(int I=0;I<Sensors.Num();++I)
        {
            auto* Scan=Sensors[I]->ScanComponent.Get();const auto& Status=Scan->GetRuntimeStatus();const double Hz=(Status.FrameId-Baselines[I])/(Now-Measured);
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("sensor"),Scan->SensorId);Row->SetStringField(TEXT("backend"),Status.ActiveAcquisitionBackend);Row->SetStringField(TEXT("backend_message"),Status.AcquisitionBackendMessage);Row->SetNumberField(TEXT("hz"),Hz);
            Row->SetNumberField(TEXT("horizontal_samples"),Scan->HorizontalSamples);Row->SetNumberField(TEXT("vertical_channels"),Scan->VerticalChannels);Row->SetNumberField(TEXT("frame_count"),Status.FrameId-Baselines[I]);
            Row->SetNumberField(TEXT("acquisition_ms"),Status.LastAcquisitionDurationMs);Rows.Add(MakeShared<FJsonValueObject>(Row));
            if(!Operating)Test->TestEqual(TEXT("active acquisition uses hardware collision tracing"),Scan->GetActiveAcquisitionBackend(),EVirtualLidarAcquisitionBackend::HardwareRayTracing);
            if(!Operating)Test->TestTrue(FString::Printf(TEXT("sensor %d completed acquisition >=10Hz"),I),Hz>=10);
            else Test->TestTrue(TEXT("existing operating-map sensor completed real frames"),Status.FrameId>Baselines[I]);
            const auto Frame=Scan->GetLastFrameSnapshot();Test->TestTrue(TEXT("normalized snapshot retains original full beam count"),Frame&&Frame->Points&&(Operating?Frame->Points->Num()>0:Frame->Points->Num()==Scan->HorizontalSamples*Scan->VerticalChannels));
            AActor* Actor=Sensors[I].Get();if(Actor)Sensors[I]->StopSensor();
        }
        if(OperatingCamera.IsValid())OperatingCamera->StopSensor();
        R->SetArrayField(TEXT("sensors"),Rows);R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FString Root;if(FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Root))Root/=TEXT("measurements");else Root=FPaths::ProjectSavedDir()/TEXT("Reports/GPU-LIDAR-01/progress/scheduled");IFileManager::Get().MakeDirectory(*Root,true);
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
