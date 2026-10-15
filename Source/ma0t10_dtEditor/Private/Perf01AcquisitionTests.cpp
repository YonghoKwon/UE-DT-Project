#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#if __has_include("ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h")
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#else
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#endif
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#if __has_include("ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h")
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
#define CAM_GT_PERF_AVAILABLE 1
#else
#define CAM_GT_PERF_AVAILABLE 0
#endif
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#if __has_include("ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h")
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
#define IMU_PERF_AVAILABLE 1
#else
#define IMU_PERF_AVAILABLE 0
#endif
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "EngineGlobals.h"
#include "RenderTimer.h"
#include "Json.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "UxrScenarioUiFixture.h"
#include "EngineUtils.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

// This exact test-only file can be overlaid on fda25e0. Only header locations vary.
// It creates additional sensors in PIE memory, never saves a map or sends frames.
class FPerf01AcquisitionCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Created=FPlatformTime::Seconds(),Started=0,Last=0,LastRate=0,MeasurementStart=0;
    int32 Pairs=1,Duration=60;
    TArray<TWeakObjectPtr<AVirtualSensorActorBase>> Sensors;
    TArray<double> Wall,Engine,Game,Gpu,RateSums;
    TArray<int64> StartFrames;
    int32 RateSamples=0;
    int32 NativeClientX=0,NativeClientY=0;
    float DpiScale=0;
    bool bVisibleVerified=false;
    bool bCameraTruthPerformance=false;
    bool bLidarFidelityPerformance=false;
public:
    explicit FPerf01AcquisitionCommand(FAutomationTestBase* In):Test(In)
    {
        FParse::Value(FCommandLine::Get(),TEXT("Perf01Pairs="),Pairs);
        FParse::Value(FCommandLine::Get(),TEXT("Perf01Seconds="),Duration);
        Pairs=FMath::Clamp(Pairs,1,2);Duration=FMath::Clamp(Duration,30,60);
        bCameraTruthPerformance=FParse::Param(FCommandLine::Get(),TEXT("CameraTruthPerformance"));
        bLidarFidelityPerformance=FParse::Param(FCommandLine::Get(),TEXT("LidarFidelityPerformance"));
    }
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Created>Duration+70){Test->AddError(TEXT("PERF-01 timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World)||Now-Created<3)return false;
        auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        auto* Coordinator=Workspace->GetCoordinator();if(!Coordinator)return false;
        if(!Started)
        {
            for(auto* A:Coordinator->GetSensorActors())if(A)Sensors.Add(A);
            if(Sensors.Num()!=2){Test->AddError(TEXT("PERF-01 map must contain exactly one camera and one lidar"));return true;}
            if(Pairs==2)
            {
                auto Originals=Sensors;
                for(const auto& Weak:Originals)if(auto* Original=Weak.Get())
                {
                    auto Pose=Original->GetActorTransform();Pose.AddToTranslation(FVector(0,20,0));
                    auto* Extra=World->SpawnActor<AVirtualSensorActorBase>(Original->GetClass(),Pose);
                    if(!Extra){Test->AddError(TEXT("Extra PIE sensor spawn failed"));return true;}
                    Extra->SetSharedOutputServices(Coordinator->SharedTransportComponent,Coordinator->SharedRecorderComponent,Coordinator->StreamPublisherComponent);
                    Coordinator->RegisterSensorActor(Extra);Sensors.Add(Extra);
                }
            }
            Coordinator->SharedTransportComponent->TransportMode=EVirtualSensorTransportMode::None;
            Coordinator->SharedRecorderComponent->StopRecording();
            Coordinator->StreamPublisherComponent->StopAllStreams(TEXT("*"));
            int32 Cameras=0,Lidars=0;
            for(int32 I=0;I<Sensors.Num();++I)if(auto* A=Sensors[I].Get())
            {
                A->EndInteractiveManipulation();A->StopSensor();
                FVirtualSensorEditableState S,Applied;A->ReadEditableState(S);
                S.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;
                S.CameraProfile=EVirtualCameraDeviceProfile::IntelRealSenseD455;
                S.LidarProfile=EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;
                FString Error;if(!A->ApplyProfileAndSimulationQuality(S,Applied,Error)){Test->AddError(Error);return true;}
                S=Applied;S.SensorId=FString::Printf(TEXT("PERF01-%s-%d"),A->GetSensorKind()==EVirtualSensorKind::Camera?TEXT("CAM"):TEXT("LIDAR"),I);
#if CAM_GT_PERF_AVAILABLE
                S.bCameraGroundTruth=bCameraTruthPerformance;
#endif
                S.CameraCaptureMode=Pairs==2?EVirtualCameraCaptureMode::PreviewOnly:EVirtualCameraCaptureMode::Payload;
                S.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;
                S.bExportCsvOnScan=S.bExportJsonLinesOnScan=S.bExportPcdOnScan=false;
                if(!A->ApplyEditableState(S,Error)){Test->AddError(Error);return true;}
                if(auto* L=Cast<AVirtualLidarSensorActor>(A))
                {
                    // Test-owned fixture backend; identical to the map's original lidar.
                    L->ScanComponent->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;
                    L->ScanComponent->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;
                    L->ScanComponent->bEnableRangeNoise=false;++Lidars;
                    if(bLidarFidelityPerformance)
                    {
                        // Reflection keeps this identical instrument compilable on the baseline.
                        for(const FName Name:{FName(TEXT("bUseRollingPointTimestamps")),FName(TEXT("bEnableMotionDistortion")),FName(TEXT("bEnableMaterialResponse"))})
                        {
                            auto* Property=FindFProperty<FBoolProperty>(L->ScanComponent->GetClass(),Name);
                            if(!Property){Test->AddError(TEXT("LIDAR-FID performance option missing on this revision"));return true;}
                            Property->SetPropertyValue_InContainer(L->ScanComponent,true);
                        }
                        L->ScanComponent->bEnableRangeNoise=true;
                    }
                }
                else ++Cameras;
                Coordinator->StreamPublisherComponent->StopAllStreams(A->GetSensorId());
                A->StartSensor();
            }
            if(Cameras!=Pairs||Lidars!=Pairs){Test->AddError(TEXT("PERF-01 wrong sensor kind counts"));return true;}
            Workspace->ResetOwnedWorkspaceLayout();Workspace->SetOwnedPanelFontScale(1);
            for(const auto Role:{ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabCharts,ESensorToolPanelRole::SlabProgress})Workspace->SetPanelOpen(Role,false);
            Workspace->SetPanelOpen(ESensorToolPanelRole::Placement,true);Workspace->SetPanelOpen(ESensorToolPanelRole::Monitor,true);
            Coordinator->SetViewMode(EVirtualSensorViewMode::Camera);
            if(FParse::Param(FCommandLine::Get(),TEXT("Ui02Performance")))
            {
                for(TActorIterator<ASlabActor> It(World);It;++It)
                {
                    Test->TestTrue(TEXT("UI02 performance observation-only scenario starts"),StartUxrScenarioUiFixture(*It));break;
                }
            }
            RateSums.Init(0,Sensors.Num());StartFrames.Init(0,Sensors.Num());
            #if IMU_PERF_AVAILABLE
            if(FParse::Param(FCommandLine::Get(),TEXT("ImuPerformance")))
                for(const auto& Weak:Sensors)if(auto* A=Weak.Get()){auto* Imu=UVirtualImuComponent::Attach(A);FVirtualImuConfig C;C.RateHz=400;FString Error;Test->TestTrue(TEXT("IMU perf configure"),Imu&&Imu->Configure(C,Error));}
#endif
            Started=Last=Now;return false;
        }
#if CAM_GT_PERF_AVAILABLE
        if(bCameraTruthPerformance)
            for(const auto& Weak:Sensors)if(auto* C=Cast<AVirtualCameraSensorActor>(Weak.Get()))C->SetGroundTruthPreviewDemand(true);
#endif
        const double Elapsed=Now-Started;
        if(Elapsed>=10&&Elapsed<10+Duration)
        {
            if(!MeasurementStart)
            {
                MeasurementStart=Now;TRACE_BEGIN_REGION(TEXT("PERF01Measurement"));
                for(int32 I=0;I<Sensors.Num();++I)StartFrames[I]=Sensors[I]->GetSensorRuntimeStatus().FrameId;
                if(auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef()))
                {
                    auto Native=Window->GetNativeWindow();
#if PLATFORM_WINDOWS
                    const HWND Handle=Native?static_cast<HWND>(Native->GetOSWindowHandle()):nullptr;
                    RECT Client{};if(Handle&&GetClientRect(Handle,&Client))
                    {
                        bVisibleVerified=IsWindowVisible(Handle)&&!IsIconic(Handle);
                        NativeClientX=Client.right;NativeClientY=Client.bottom;DpiScale=Window->GetDPIScaleFactor();
                        Test->AddInfo(FString::Printf(TEXT("PERF-01 native visible=%d client=%ldx%ld viewport=1920x1080 dpiScale=%.3f"),bVisibleVerified,Client.right,Client.bottom,DpiScale));
                    }
#endif
                }
                if(!FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")))Test->TestTrue(TEXT("visible native PIE client 1920x1080"),bVisibleVerified);
            }
            Wall.Add((Now-Last)*1000);Engine.Add(FApp::GetDeltaTime()*1000);
            Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Gpu.Add(FPlatformTime::ToMilliseconds(GGPUFrameTime));
            if(Now-LastRate>=.2){LastRate=Now;++RateSamples;for(int32 I=0;I<Sensors.Num();++I)RateSums[I]+=Sensors[I]->GetSensorRuntimeStatus().MeasuredAcquisitionRateHz;}
        }
        Last=Now;if(Elapsed<10+Duration)return false;
        if(MeasurementStart)TRACE_END_REGION(TEXT("PERF01Measurement"));
        auto Json=MakeShared<FJsonObject>();
        auto Mean=[](const TArray<double>& Values){double Total=0;for(double V:Values)Total+=V;return Values.Num()?Total/Values.Num():0;};
        auto P95=[](TArray<double> Values){Values.Sort();return Values.Num()?Values[FMath::Clamp(FMath::CeilToInt(Values.Num()*.95)-1,0,Values.Num()-1)]:0;};
        Test->TestTrue(TEXT("performance samples exist"),Wall.Num()>500);
        Json->SetNumberField(TEXT("average_fps"),Mean(Wall)>0?1000/Mean(Wall):0);
        Json->SetNumberField(TEXT("engine_average_fps"),Mean(Engine)>0?1000/Mean(Engine):0);
        Json->SetNumberField(TEXT("callback_p95_ms"),P95(Wall));Json->SetNumberField(TEXT("engine_frame_p95_ms"),P95(Engine));
        Json->SetNumberField(TEXT("game_thread_mean_ms"),Mean(Game));Json->SetNumberField(TEXT("gpu_mean_ms"),Mean(Gpu));
        Json->SetBoolField(TEXT("camera_ground_truth_enabled"),FParse::Param(FCommandLine::Get(),TEXT("CameraTruthPerformance")));
        Json->SetBoolField(TEXT("imu_enabled"),FParse::Param(FCommandLine::Get(),TEXT("ImuPerformance")));
        Json->SetNumberField(TEXT("samples"),Wall.Num());Json->SetNumberField(TEXT("pairs"),Pairs);
        Json->SetBoolField(TEXT("lidar_fidelity_enabled"),bLidarFidelityPerformance);
        Json->SetNumberField(TEXT("warmup_seconds"),10);Json->SetNumberField(TEXT("measurement_seconds"),Duration);
        Json->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
        Json->SetBoolField(TEXT("native_visible_client_verified"),bVisibleVerified);
        Json->SetNumberField(TEXT("native_client_x_including_slate_border"),NativeClientX);Json->SetNumberField(TEXT("native_client_y_including_slate_border"),NativeClientY);Json->SetNumberField(TEXT("dpi_scale"),DpiScale);
        Json->SetStringField(TEXT("actual_client"),TEXT("1920x1080"));
        Json->SetBoolField(TEXT("ui02_observation_scenario"),FParse::Param(FCommandLine::Get(),TEXT("Ui02Performance")));
        Json->SetStringField(TEXT("output_condition"),Pairs==2?TEXT("acquisition+preview only; camera PreviewOnly; no streams/recording/transport"):TEXT("camera Payload (encode), lidar RequestedOnly; no streams/recording/transport"));
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(int32 I=0;I<Sensors.Num();++I)
        {
            auto* A=Sensors[I].Get();if(!A){Test->AddError(TEXT("Sensor disappeared"));continue;}
            const auto S=A->GetSensorRuntimeStatus();auto Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("id"),A->GetSensorId());Row->SetStringField(TEXT("kind"),A->GetSensorKind()==EVirtualSensorKind::Camera?TEXT("Camera"):TEXT("Lidar"));
            Row->SetStringField(TEXT("backend"),S.ActiveAcquisitionBackend);
            Row->SetNumberField(TEXT("acquisition_hz_mean_including_zero"),RateSamples?RateSums[I]/RateSamples:0);
            Row->SetNumberField(TEXT("completed_frame_id_delta"),S.FrameId-StartFrames[I]);
            Row->SetNumberField(TEXT("failed_acquisition"),S.FailedAcquisitionFrameCount);Row->SetNumberField(TEXT("queue_overflow"),S.QueueOverflowCount);
            Row->SetNumberField(TEXT("budget_skipped"),S.BudgetSkippedAcquisitionFrameCount);
            Test->TestTrue(TEXT("each device still acquiring including zero-rate devices"),A->IsSensorRunning()&&RateSums[I]>0);
            Test->TestEqual(TEXT("acquisition failures"),S.FailedAcquisitionFrameCount,0);Test->TestEqual(TEXT("queue overflow"),S.QueueOverflowCount,0);
#if CAM_GT_PERF_AVAILABLE
            if(auto* C=Cast<AVirtualCameraSensorActor>(A);C&&C->CaptureComponent->GetGroundTruth())
            {
                const auto F=C->CaptureComponent->GetGroundTruth()->LastFrame;
                if(F){Row->SetNumberField(TEXT("truth_frame_id"),F->Acquisition.FrameId);Row->SetNumberField(TEXT("truth_submit_ms"),F->SubmitMs);Row->SetNumberField(TEXT("truth_readback_latency_ms"),F->ReadbackLatencyMs);Row->SetNumberField(TEXT("truth_decode_ms"),F->DecodeMs);}
                if(FParse::Param(FCommandLine::Get(),TEXT("CameraTruthPerformance")))Test->TestTrue(TEXT("enabled truth frame completed"),F.IsValid());
            }
#endif
            Rows.Add(MakeShared<FJsonValueObject>(Row));A->StopSensor();
        }
        Json->SetArrayField(TEXT("sensors"),Rows);Json->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
        FString Path;FParse::Value(FCommandLine::Get(),TEXT("Perf01Output="),Path);
        if(Path.IsEmpty()){Test->AddError(TEXT("Perf01Output is required"));return true;}
        FString Csv=TEXT("callback_ms,engine_ms,game_thread_ms,gpu_ms\n");
        for(int32 I=0;I<Wall.Num();++I)Csv+=FString::Printf(TEXT("%.6f,%.6f,%.6f,%.6f\n"),Wall[I],Engine[I],Game[I],Gpu[I]);
        Test->TestTrue(TEXT("raw frame samples written"),FFileHelper::SaveStringToFile(Csv,*(Path+TEXT(".frames.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        FString Text;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text));
        Test->TestTrue(TEXT("PERF-01 report written"),FFileHelper::SaveStringToFile(Text,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        Test->AddInfo(Text);return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerf01AcquisitionTest,"MA0T10.PERF01.AcquisitionAndPreview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPerf01AcquisitionTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("Perf01"))){AddInfo(TEXT("SKIP explicit PERF-01 only (-Perf01)"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPerf01AcquisitionCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
