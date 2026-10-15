#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "ShaderCompiler.h"
#include "RHIGlobals.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"

/** Four independent reproductions: no dataset subsystem, writer, Slab business or UI. */
class FGpuCaptureLifetimeCommand final:public IAutomationLatentCommand
{
public:
    FGpuCaptureLifetimeCommand(FAutomationTestBase* In,int32 InMode):Test(In),Mode(InMode){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(!Scene)
        {
            Started=Now;Scene=MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(true));auto* W=Scene->GetWorld();
            auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId"));
            if(Material)Material->CacheShaders(EMaterialShaderPrecompileMode::Default);
            Target=W->SpawnActor<AStaticMeshActor>();Target->Tags.Add(TEXT("Slab"));Target->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Target->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Target->SetActorLocation(FVector(1000,0,0));Target->SetActorScale3D(FVector(.5,8,4));
            Lidar=W->SpawnActor<AVirtualLidarSensorActor>();Scan=Lidar->ScanComponent.Get();Scan->StopScan();Scan->DeviceProfile=EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;
            Scan->HorizontalSamples=64;Scan->VerticalChannels=16;Scan->HorizontalFov=30;Scan->MinVerticalAngle=-8;Scan->MaxVerticalAngle=8;Scan->ScanInterval=.05;
            Scan->MaxDistance=5000;Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->bEnableMaterialResponse=true;Scan->bEnableRangeNoise=false;Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;
            Camera=W->SpawnActor<AVirtualCameraSensorActor>();auto* C=Camera->CaptureComponent.Get();C->StopCapture();C->CaptureResolution=FIntPoint(128,64);C->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;C->FOVAngle=60;C->SetGroundTruthEnabled(true);
            Truth=NewObject<UVirtualCameraGroundTruth>(Camera);Camera->AddInstanceComponent(Truth);Truth->RegisterComponent();
            MemoryStart=FPlatformMemory::GetStats().UsedPhysical;return false;
        }
        if(Now-Started>120){Test->AddError(FString::Printf(TEXT("capture matrix timeout mode%d phase%d GT=%s"),Mode,Completed,*Truth->Status));return Finish();}
        if(Now-Started<2||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()))return false;
        if(!Pending)
        {
            if(GFrameCounter<NextFrame)return false;
            Target->SetActorLocation(FVector(1000,Completed%3,0));
            if(Mode==3)
            {
                FVirtualSensorEditableState State;Lidar->ReadEditableState(State);State.ActorTransform.SetLocation(FVector(0,Completed%2,0));State.LidarHorizontalSamples=Completed%2?96:64;
                FString Error;if(!Lidar->ApplyEditableState(State,Error)){Test->AddError(Error);return Finish();}
            }
            Scene->GetWorld()->SendAllEndOfFrameUpdates();Truth->LastFrame.Reset();LidarDone=Mode==1;CameraDone=Mode==0||Mode==3;
            if(!CameraDone)
            {
                auto* C=Camera->CaptureComponent.Get();C->CaptureAndSendImage();
                FVirtualCameraPayloadSnapshot S;S.SensorId=C->SensorId;S.FrameId=Completed+1;S.AcquisitionRevision=C->GetFileCaptureRevision();S.TimestampUtc=FDateTime::UtcNow();S.Width=128;S.Height=64;S.HorizontalFov=60;
                if(!Truth->Capture(*C,S,false)){Test->AddError(Truth->Status);return Finish();}
            }
            if(!LidarDone){Baseline=Scan->GetLastFrameSnapshot()?Scan->GetLastFrameSnapshot()->FrameId:-1;Scan->StartScan();Scan->RequestImmediateScheduledScan();Scan->PrepareScheduledScan(0);}
            Pending=true;return false;
        }
        if(!LidarDone)
        {
            Scan->ProcessScheduledScanChunk(1024);auto F=Scan->GetLastFrameSnapshot();
            if(F&&F->FrameId>Baseline)
            {Test->TestEqual(TEXT("actual GPU backend retained"),F->AcquisitionBackend,EVirtualLidarAcquisitionBackend::GpuDepthProjection);Test->TestTrue(TEXT("real measured hits"),F->ValidPointCount>20);Scan->StopScan();LidarDone=true;}
        }
        if(!CameraDone){Truth->Poll();if(Truth->LastFrame){Test->TestTrue(TEXT("GT mask contains measured slab"),Truth->LastFrame->Classes.Contains(2));CameraDone=true;}}
        if(!LidarDone||!CameraDone)return false;
        ++Completed;Pending=false;NextFrame=GFrameCounter+2;
        if(Completed%25==0)Test->AddInfo(FString::Printf(TEXT("GPU-CRASH-01 mode%d completed%d memoryMiB%.3f"),Mode,Completed,FPlatformMemory::GetStats().UsedPhysical/1048576.));
        return Completed>=100?Finish():false;
    }
private:
    bool Finish()
    {
        Test->AddInfo(FString::Printf(TEXT("GPU-CRASH-01 mode%d final%d wall%.3fs memoryStartMiB%.3f memoryEndMiB%.3f"),Mode,Completed,FPlatformTime::Seconds()-Started,MemoryStart/1048576.,FPlatformMemory::GetStats().UsedPhysical/1048576.));
        if(Truth)Truth->Cancel();if(Scan)Scan->StopScan();if(Camera)Camera->CaptureComponent->StopCapture();if(Lidar)Lidar->GpuDepthProjectionComponent->UnregisterComponent();Scene.Reset();return true;
    }
    FAutomationTestBase* Test;int32 Mode,Completed=0;double Started=0;uint64 NextFrame=0,MemoryStart=0;int64 Baseline=-1;
    bool Pending=false,LidarDone=false,CameraDone=false;
    TUniquePtr<FPreviewScene> Scene;AStaticMeshActor* Target=nullptr;AVirtualLidarSensorActor* Lidar=nullptr;UVirtualLidarScanComponent* Scan=nullptr;AVirtualCameraSensorActor* Camera=nullptr;UVirtualCameraGroundTruth* Truth=nullptr;
};
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGpuCaptureLifetimeTest,"MA0T10.GpuCaptureLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FGpuCaptureLifetimeTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{const TCHAR* Cases[]={TEXT("LidarRepeated"),TEXT("CameraTruthRepeated"),TEXT("ConcurrentRepeated"),TEXT("SettingsRepeated")};for(int32 I=0;I<4;++I){Names.Add(Cases[I]);Commands.Add(LexToString(I));}}
bool FGpuCaptureLifetimeTest::RunTest(const FString& Parameter)
{if(GUsingNullRHI){AddWarning(TEXT("SKIP: GPU capture lifetime requires actual RHI"));return true;}ADD_LATENT_AUTOMATION_COMMAND(FGpuCaptureLifetimeCommand(this,FCString::Atoi(*Parameter)));return true;}
#endif
