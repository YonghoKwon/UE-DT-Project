#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "RHIGlobals.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "Misc/App.h"
#include "ShaderCompiler.h"
#include "HAL/PlatformMemory.h"
#include "HAL/MemoryMisc.h"
#include "UObject/UObjectArray.h"
#include "Engine/StaticMesh.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDatasetRandomTest,"MA0T10.Dataset.SeedRangesAndAdmission",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDatasetRandomTest::RunTest(const FString&)
{
    FVirtualDatasetSettings S;FString Error;
    TestFalse(TEXT("default off cannot start"),S.Validate(Error));S.Enabled=true;TestTrue(TEXT("explicit settings accepted"),S.Validate(Error));
    S.Yaw.Enabled=S.Pitch.Enabled=S.X.Enabled=S.Size.Enabled=S.Surface.Enabled=S.SensorPosition.Enabled=true;
    for(int32 I=0;I<1000;++I)
    {
        const auto A=FVirtualDatasetSample::Make(S,I),B=FVirtualDatasetSample::Make(S,I);
        if(A.ToJson()!=B.ToJson()){AddError(TEXT("seed/scene sample or label mismatch"));return false;}
        if(A.SlabRotation.Yaw<S.Yaw.Min||A.SlabRotation.Yaw>S.Yaw.Max||A.Size<S.Size.Min||A.Size>S.Size.Max||A.Surface<0||A.Surface>4||A.LidarOffset.GetAbsMax()>1){AddError(TEXT("sample out of range"));return false;}
    }
    const auto Before=FVirtualDatasetSample::Make(S,4);S.Oxidation.Enabled=true;const auto After=FVirtualDatasetSample::Make(S,4);
    TestEqual(TEXT("enabling unrelated parameter does not shift yaw"),Before.SlabRotation,After.SlabRotation);
    TestEqual(TEXT("enabling unrelated parameter does not shift installation"),Before.LidarOffset,After.LidarOffset);
    S.Seed++;TestNotEqual(TEXT("different seeds differ"),After.ToJson(),FVirtualDatasetSample::Make(S,4).ToJson());
    S.Size.Min=-1;TestFalse(TEXT("unsafe size rejected before work"),S.Validate(Error));S.Size.Min=.9;S.Name=TEXT("../escape");TestFalse(TEXT("path traversal rejected"),S.Validate(Error));return true;
}

class FDatasetRhiCommand final:public IAutomationLatentCommand
{
public:
    explicit FDatasetRhiCommand(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(!World)
        {
            UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){Candidate=C.World();break;}
            if(Candidate!=ObservedWorld.Get()){ObservedWorld=Candidate;ObservedAt=Now;return false;}
            if(!Candidate||Now-ObservedAt<3)return false;World=Candidate;Started=Now;
            for(TActorIterator<ASlabActor> It(World);It;++It)It->StopSimulation();
            for(TActorIterator<AVirtualSensorActorBase> It(World);It;++It)It->StopSensor();
            Slab=World->SpawnActor<ASlabActor>();Slab->Tags.Add(TEXT("Slab"));Slab->SetActorLocation(FVector(1500,0,200));Slab->ApplyDatasetGeometry(FVector(800,200,50));Slab->SetDiagnosticHelpersVisible(false);
            for(int32 I=0;I<5;++I){FString E;Slab->ApplySurfacePreset(ESlabSurfacePreset(I),false,E);}FString E;Slab->ApplySurfacePreset(ESlabSurfacePreset::Default,false,E);
            Slab->Roughness=.42f;Slab->Oxidation=.17f;Slab->EmissiveStrength=6.25f;Slab->bHotAppearance=true;Slab->RefreshDatasetAppearance();
            Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Lidar->StopSensor();Lidar->SetActorLocation(FVector(1500,-1000,1000));Lidar->SetActorRotation((Slab->GetActorLocation()-Lidar->GetActorLocation()).Rotation());
            auto* Scan=Lidar->ScanComponent.Get();Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Scan->SensorId=TEXT("DATASET-LIDAR");Scan->OutputMode=EVirtualLidarOutputMode::None;Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->bEnableRangeNoise=false;
            Camera=World->SpawnActor<AVirtualCameraSensorActor>();Camera->StopSensor();Camera->SetActorTransform(Lidar->GetActorTransform());Camera->CaptureComponent->ApplyDeviceProfile(EVirtualCameraDeviceProfile::IntelRealSenseD455);Camera->CaptureComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);Camera->CaptureComponent->SensorId=TEXT("DATASET-CAMERA");Camera->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;
            Light=World->SpawnActor<ADirectionalLight>();Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            Service=World->GetSubsystem<UVirtualDatasetSubsystem>();if(!Service){Test->AddError(TEXT("dataset service missing"));return Finish();}
            SlabBefore=Slab->GetActorTransform();LidarBefore=Lidar->GetActorTransform();CameraBefore=Camera->GetActorTransform();SizeBefore=Slab->SlabMesh->GetRelativeScale3D();
            Settings.Enabled=Settings.CameraTruth=Settings.LidarFidelity=true;Settings.Name=TEXT("DATASET01_final");Settings.Scenes=FMath::Clamp(FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_DATASET_SCENES"))),10,100);Settings.Seed=22015;
            Settings.Yaw.Enabled=Settings.Pitch.Enabled=Settings.Roll.Enabled=Settings.X.Enabled=Settings.Y.Enabled=Settings.Z.Enabled=Settings.Size.Enabled=Settings.Surface.Enabled=Settings.Roughness.Enabled=Settings.Oxidation.Enabled=Settings.LightIntensity.Enabled=Settings.LightPitch.Enabled=Settings.LightYaw.Enabled=Settings.SensorPosition.Enabled=Settings.SensorAngle.Enabled=true;
            Service->OnSceneSnapshotForTests=[this](const FJsonObject& Label)
            {
                ++Snapshots;const auto P=Label.GetObjectField(TEXT("slab_pose"));const auto A=P->GetArrayField(TEXT("pitch_yaw_roll_deg"));
                Test->TestTrue(TEXT("truth yaw equals actual Actor"),FMath::Abs(A[1]->AsNumber()-Slab->GetActorRotation().Yaw)<1.e-6);
                const auto Position=P->GetArrayField(TEXT("xyz_m"));const auto Actual=Slab->GetActorLocation()*.01;
                Test->TestTrue(TEXT("truth position equals actual Actor"),FVector(Position[0]->AsNumber(),Position[1]->AsNumber(),Position[2]->AsNumber()).Equals(Actual,1.e-6));
                const auto Size=Label.GetArrayField(TEXT("slab_size_m"));const FVector ActualSize=Slab->SlabMesh->GetComponentScale()*Slab->SlabMesh->GetStaticMesh()->GetBounds().BoxExtent*2.*.01;
                Test->TestTrue(TEXT("truth dimensions equal rendered geometry"),FVector(Size[0]->AsNumber(),Size[1]->AsNumber(),Size[2]->AsNumber()).Equals(ActualSize,1.e-6));
                Test->TestEqual(TEXT("actual GPU acquisition, no CPU fallback"),int32(Label.GetNumberField(TEXT("lidar_backend"))),int32(EVirtualLidarAcquisitionBackend::GpuDepthProjection));
                Test->TestTrue(TEXT("actual surface parameters in truth"),FMath::Abs(Label.GetNumberField(TEXT("roughness"))-Slab->Roughness)<1.e-6&&FMath::Abs(Label.GetNumberField(TEXT("oxidation"))-Slab->Oxidation)<1.e-6);
            };
            return false;
        }
        if(Now-Started>300){Test->AddError(TEXT("dataset workflow timeout: ")+Service->Message);return Finish();}
        if(Stage==0)
        {
            if(Now-Started<10||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()))return false;
            FVirtualDatasetSettings Off;FString E;const FString Root=FPaths::ProjectSavedDir()/TEXT("Datasets");TArray<FString> Before,After;IFileManager::Get().FindFilesRecursive(Before,*Root,TEXT("*"),true,false);
            Test->TestFalse(TEXT("disabled Start rejected"),Service->Start(Off,Slab,Lidar,Camera,Light,E));IFileManager::Get().FindFilesRecursive(After,*Root,TEXT("*"),true,false);
            Test->TestEqual(TEXT("disabled creates exactly zero files"),After.Num(),Before.Num());Test->TestTrue(TEXT("disabled has no session or GT resources"),!Service->IsActive()&&Service->Directory().IsEmpty()&&!Service->HasTruthResourcesForTests());
            if(!StartRun(Settings))return Finish();Stage=1;MeasurementStart=LastCallback=FPlatformTime::Seconds();return false;
        }
        if(Stage==1||Stage==2)
        {
            if(Service->IsActive()&&!Service->IsPaused()&&LastMetricFrame!=GFrameCounter)
            {LastMetricFrame=GFrameCounter;FrameMs.Add(FApp::GetDeltaTime()*1000.);GameMs.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));WallMs.Add((Now-LastCallback)*1000.);LastCallback=Now;}
            if(Stage==1&&Service->CompletedScenes()>=3&&!Paused){Service->Pause();Paused=true;}
            if(Service->IsPaused()&&!Resumed)
            {CheckRestored();FString E;Test->TestTrue(TEXT("pause resumes same run"),Service->Resume(E));Resumed=true;}
            if(Service->IsActive())return false;
            Test->TestEqual(TEXT("all requested GPU scenes completed"),Service->CompletedScenes(),Settings.Scenes);CheckRestored();Roots.Add(Service->Directory());RecordRun(Now);
            if(Stage==1){if(!StartRun(Settings))return Finish();Stage=2;MeasurementStart=LastCallback=FPlatformTime::Seconds();return false;}
            if(Stage==2&&Reports.Num()<3){if(!StartRun(Settings))return Finish();MeasurementStart=LastCallback=FPlatformTime::Seconds();return false;}
            CompareTruth();auto Rgb=Settings;Rgb.CameraTruth=false;Rgb.LidarFidelity=false;Rgb.Scenes=1;if(!StartRun(Rgb))return Finish();Stage=3;return false;
        }
        if(Stage==3)
        {
            if(Service->IsActive())return false;const FString D=Service->Directory()/TEXT("scene_00000");
            Test->TestTrue(TEXT("RGB-only dataset completed"),Service->CompletedScenes()==1&&FPaths::FileExists(D/TEXT("rgb.png")));
            Test->TestFalse(TEXT("GT-off has no internal truth targets"),Service->HasTruthResourcesForTests());
            TArray<uint8> Labels;FFileHelper::LoadFileToArray(Labels,*(D/TEXT("lidar_labels.bin")));bool HasSlab=false;for(int32 I=0;I<Labels.Num();I+=5)HasSlab|=Labels[I]==2;
            Test->TestTrue(TEXT("LiDAR labels remain present with fidelity options off"),HasSlab);
            for(const TCHAR* F:{TEXT("depth.exr"),TEXT("seg.png"),TEXT("instance.png")})Test->TestFalse(TEXT("GT-off writes no truth planes"),FPaths::FileExists(D/F));
            Roots.Add(Service->Directory());auto Cancel=Settings;Cancel.Scenes=10;if(!StartRun(Cancel))return Finish();Stage=4;return false;
        }
        if(Stage==4)
        {
            if(!Service->IsCapturingForTests())return false;Service->Stop();CancelledRoot=Service->Directory();StoppedAt=Now;Stage=5;return false;
        }
        if(Stage==5)
        {
            if(Service->IsActive()||Now-StoppedAt<.3)return false;Test->TestFalse(TEXT("capture cancellation produces zero files"),IFileManager::Get().DirectoryExists(*CancelledRoot));CheckRestored();
            Service->WorkerDelayForTests=.2f;auto Cancel=Settings;Cancel.Scenes=10;if(!StartRun(Cancel))return Finish();Stage=6;return false;
        }
        if(Stage==6)
        {
            if(!Service->IsWritingForTests())return false;Service->Stop();CancelledRoot=Service->Directory();Stage=7;return false;
        }
        if(Stage==7)
        {
            if(Service->IsActive())return false;Test->TestEqual(TEXT("write cancellation commits zero scenes"),Service->CompletedScenes(),0);
            TArray<FString> Files;IFileManager::Get().FindFilesRecursive(Files,*CancelledRoot,TEXT("*"),true,false);Test->TestEqual(TEXT("write cancellation leaves no partial files"),Files.Num(),0);CheckRestored();return Finish();
        }
        return false;
    }
private:
    bool StartRun(const FVirtualDatasetSettings& S)
    {FString E;FrameAtStart=Lidar->ScanComponent->GetRuntimeStatus().FrameId;const bool Ok=Service->Start(S,Slab,Lidar,Camera,Light,E);Test->TestTrue(TEXT("dataset start accepted"),Ok);if(!Ok)Test->AddError(E);else{FString Busy;Test->TestFalse(TEXT("single-scene admission rejects concurrent run"),Service->Start(S,Slab,Lidar,Camera,Light,Busy));}return Ok;}
    void CheckRestored()
    {
        Test->TestTrue(TEXT("Slab pose/geometry restored"),Slab->GetActorTransform().Equals(SlabBefore)&&Slab->SlabMesh->GetRelativeScale3D().Equals(SizeBefore));
        Test->TestTrue(TEXT("custom surface values restored"),Slab->bHotAppearance&&FMath::IsNearlyEqual(Slab->EmissiveStrength,6.25f)&&FMath::IsNearlyEqual(Slab->Roughness,.42f));
        Test->TestTrue(TEXT("sensor pose/stopped state restored"),Lidar->GetActorTransform().Equals(LidarBefore)&&Camera->GetActorTransform().Equals(CameraBefore)&&!Lidar->IsSensorRunning()&&!Camera->IsSensorRunning());
        Test->TestEqual(TEXT("original GPU backend preserved"),Lidar->ScanComponent->AcquisitionBackend,EVirtualLidarAcquisitionBackend::GpuDepthProjection);
    }
    void RecordRun(double Now)
    {
        Service->Stop();Service->Stop();Test->TestFalse(TEXT("stop after completion is idempotent"),Service->IsActive());
        Test->TestEqual(TEXT("exactly one LiDAR acquisition per scene"),Lidar->ScanComponent->GetRuntimeStatus().FrameId-FrameAtStart,int64(Settings.Scenes));
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("root"),Service->Directory());O->SetNumberField(TEXT("scene_count"),Service->CompletedScenes());O->SetNumberField(TEXT("wall_seconds"),Now-MeasurementStart);
        if(!FrameMs.IsEmpty())
        {
            double Sum=0,Game=0;for(double V:FrameMs)Sum+=V;for(double V:GameMs)Game+=V;auto Sorted=FrameMs;Sorted.Sort();
            O->SetNumberField(TEXT("engine_frame_mean_ms"),Sum/FrameMs.Num());O->SetNumberField(TEXT("engine_frame_p95_ms"),Sorted[FMath::Min(Sorted.Num()-1,FMath::FloorToInt((Sorted.Num()-1)*.95))]);O->SetNumberField(TEXT("engine_frame_max_ms"),Sorted.Last());O->SetNumberField(TEXT("game_thread_mean_ms"),Game/GameMs.Num());
        }
        auto Wall=WallMs;Wall.Sort();if(!Wall.IsEmpty()){O->SetNumberField(TEXT("callback_wall_p95_ms"),Wall[FMath::FloorToInt((Wall.Num()-1)*.95)]);O->SetNumberField(TEXT("callback_wall_max_ms"),Wall.Last());}
        O->SetNumberField(TEXT("truth_readback_allocations"),Service->GetReadbackAllocationsForTests());O->SetNumberField(TEXT("lidar_readback_allocations"),Lidar->GpuDepthProjectionComponent->GetResourceSnapshot().Counts.FindRef(TEXT("readback_allocations")));
        O->SetNumberField(TEXT("resident_mib_before_gc"),FPlatformMemory::GetStats().UsedPhysical/1048576.);O->SetNumberField(TEXT("objects_before_gc"),GUObjectArray.GetObjectArrayNumMinusAvailable());
        FlushRenderingCommands();CollectGarbage(RF_NoFlags);CollectGarbage(RF_NoFlags);FlushRenderingCommands();
        O->SetNumberField(TEXT("resident_mib_after_gc"),FPlatformMemory::GetStats().UsedPhysical/1048576.);O->SetNumberField(TEXT("objects_after_gc"),GUObjectArray.GetObjectArrayNumMinusAvailable());
        O->SetStringField(TEXT("allocator"),GMalloc->GetDescriptiveName());O->SetNumberField(TEXT("resident_mib_before_trim"),FPlatformMemory::GetStats().UsedPhysical/1048576.);GMalloc->Trim(true);O->SetNumberField(TEXT("resident_mib_after_trim"),FPlatformMemory::GetStats().UsedPhysical/1048576.);
        FGenericMemoryStats Allocator;GMalloc->GetAllocatorStats(Allocator);auto Stats=MakeShared<FJsonObject>();for(const auto& V:Allocator.Data)Stats->SetNumberField(V.Key,double(V.Value));O->SetObjectField(TEXT("allocator_stats"),Stats);
        Reports.Add(MakeShared<FJsonValueObject>(O));FrameMs.Reset();GameMs.Reset();WallMs.Reset();
        Test->AddInfo(FString::Printf(TEXT("DATASET-01 run%d scenes%d wall%.3f memoryAfterGcMiB%.3f objects%d"),Stage,Service->CompletedScenes(),Now-MeasurementStart,FPlatformMemory::GetStats().UsedPhysical/1048576.,GUObjectArray.GetObjectArrayNumMinusAvailable()));
    }
    void CompareTruth()
    {
        for(int32 I=0;I<Settings.Scenes;++I)
        {
            TSharedPtr<FJsonObject> A,B;FString T;const FString S=FString::Printf(TEXT("scene_%05d/label.json"),I);
            FFileHelper::LoadFileToString(T,*(Roots[0]/S));FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T),A);FFileHelper::LoadFileToString(T,*(Roots[1]/S));FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T),B);
            if(!A||!B){Test->AddError(TEXT("repeat label missing"));return;}
            for(const TCHAR* Key:{TEXT("slab_pose"),TEXT("slab_size_m"),TEXT("surface_preset"),TEXT("roughness"),TEXT("oxidation"),TEXT("hot_surface"),TEXT("emissive_strength"),TEXT("lidar_pose"),TEXT("camera_pose"),TEXT("camera_K"),TEXT("random_values")})
            {const auto* L=A->Values.Find(Key);const auto* R=B->Values.Find(Key);Test->TestTrue(TEXT("same seed produces identical ground truth"),L&&R&&FJsonValue::CompareEqual(**L,**R));}
        }
    }
    bool Finish()
    {
        if(Service){Service->OnSceneSnapshotForTests=nullptr;Service->WorkerDelayForTests=0;Service->Stop();}
        auto Root=MakeShared<FJsonObject>();Root->SetArrayField(TEXT("runs"),Reports);Root->SetNumberField(TEXT("validated_snapshots"),Snapshots);TArray<TSharedPtr<FJsonValue>> Paths;for(const auto& P:Roots)Paths.Add(MakeShared<FJsonValueString>(P));Root->SetArrayField(TEXT("dataset_roots"),Paths);
        FString T;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&T));FFileHelper::SaveStringToFile(T,*(FPaths::ProjectSavedDir()/TEXT("Reports/DATASET-01/resume/endurance.json")));
        FFileHelper::SaveStringToFile(T,*(FPaths::ProjectSavedDir()/TEXT("Reports/DATASET-01/resume")/(TEXT("endurance_")+FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"))+TEXT(".json"))));
        if(!Roots.IsEmpty())FFileHelper::SaveStringToFile(Roots[0],*(FPaths::ProjectSavedDir()/TEXT("Reports/DATASET-01/dataset-path.txt")));return true;
    }
    FAutomationTestBase* Test;TWeakObjectPtr<UWorld> ObservedWorld;double ObservedAt=0,Started=0,StoppedAt=0,MeasurementStart=0,LastCallback=0;UWorld* World=nullptr;
    ASlabActor* Slab=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;AVirtualCameraSensorActor* Camera=nullptr;ADirectionalLight* Light=nullptr;UVirtualDatasetSubsystem* Service=nullptr;
    FVirtualDatasetSettings Settings;FTransform SlabBefore,LidarBefore,CameraBefore;FVector SizeBefore;int64 FrameAtStart=0;int32 Stage=0,Snapshots=0;bool Paused=false,Resumed=false;uint64 LastMetricFrame=0;
    FString CancelledRoot;TArray<FString> Roots;TArray<double> FrameMs,GameMs,WallMs;TArray<TSharedPtr<FJsonValue>> Reports;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDatasetRhiTest,"MA0T10.Dataset.RealRhiGeneration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDatasetRhiTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_DATASET_SCENES")).IsEmpty()){AddInfo(TEXT("[OPT-IN SKIP] MA0T10_DATASET_SCENES unset; bounded dataset generation is explicit"));return true;}
    if(GUsingNullRHI){AddError(TEXT("DATASET-01 requires real RHI"));return false;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FDatasetRhiCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
