#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "RHIGlobals.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"

class FLidarFidelityRhiCommand : public IAutomationLatentCommand
{
public:
    explicit FLidarFidelityRhiCommand(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        if (!Scene)
        {
            Started=FPlatformTime::Seconds();
            Scene=MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
            auto* World=Scene->GetWorld();
            if(auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId")))Material->CacheShaders(EMaterialShaderPrecompileMode::Default);
            Wall=World->SpawnActor<AStaticMeshActor>();Wall->Tags.Add(TEXT("Slab"));
            auto* Mesh=Wall->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Motion=NewObject<USlabMotionComponent>(Wall);Wall->AddInstanceComponent(Motion);Motion->RegisterComponent();
            auto Data=MakeShared<FSlabScenarioData,ESPMode::ThreadSafe>();FSlabScenarioRow A,B;A.CenterX=1000;B.CenterX=1200;B.ElapsedSec=1;Data->Rows={A,B};Data->DurationSec=1;
            Motion->OnPoseApplied.BindLambda([](const FSlabScenarioRow&,int32,double,bool){});
            Motion->StartPlayback(Data,FTransform::Identity,FVector(100,4000,2000),ESlabInputUnit::Centimeters);
            Wall->SetActorScale3D(FVector(1,40,20));
            Response=NewObject<UVirtualLidarSurfaceResponseComponent>(Wall);Wall->AddInstanceComponent(Response);Response->RegisterComponent();Response->Reflectivity940Nm=.35;
            Sensor=World->SpawnActor<AVirtualLidarSensorActor>();Sensor->SetActorLocation(FVector(0,0,1000));
            Scan=Sensor->ScanComponent.Get();Scan->StopScan();Scan->DeviceProfile=EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;
            Scan->HorizontalSamples=64;Scan->VerticalChannels=16;Scan->HorizontalFov=10;Scan->MinVerticalAngle=-2;Scan->MaxVerticalAngle=2;Scan->ScanInterval=.05;
            Scan->MaxDistance=5000;Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;
            Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;Scan->bEnableRangeNoise=false;
            Scan->bUseRollingPointTimestamps=true;Scan->bEnableMotionDistortion=true;
            Scene->GetWorld()->SendAllEndOfFrameUpdates();
            Request();return false;
        }
        if(FPlatformTime::Seconds()-Started>35){Test->AddError(TEXT("fidelity GPU acquisition timeout"));return Finish();}
        Scan->ProcessScheduledScanChunk(1024);
        const auto Frame=Scan->GetLastFrameSnapshot();
        if(!Frame || Frame->FrameId<=LastFrame)return false;
        LastFrame=Frame->FrameId;
        int32 Matched=0;double Error=0,Intensity=0;
        for(const auto& P:*Frame->Points)if(P.bHit&&P.SemanticLabel==TEXT("Slab"))
        {
            ++Matched;Intensity+=P.NormalizedIntensity;
            Error=FMath::Max(Error,FMath::Abs(P.WorldLocation.X-(950+200*P.PointTimeOffsetNanoseconds*1.e-9)));
        }
        if(Matched<100){Request();return false;}
        Test->AddInfo(FString::Printf(TEXT("real RHI phase=%d matches=%d maxPlaneErrorCm=%.6f intensitySum=%.3f backend=%d"),Phase,Matched,Error,Intensity,int32(Frame->AcquisitionBackend)));
        Test->TestEqual(TEXT("actual GPU backend used"),Frame->AcquisitionBackend,EVirtualLidarAcquisitionBackend::GpuDepthProjection);
        Test->TestTrue(TEXT("real depth + Slab motion agrees within 1cm"),Error<1);
        if(Phase==0){Scan->bEnableMaterialResponse=true;++Phase;Request();return false;}
        if(Phase==1){Bright=Intensity;Response->Reflectivity940Nm=.08;++Phase;Request();return false;}
        Test->TestTrue(TEXT("real GPU material reflectivity lowers return signal"),Bright>Intensity*2);
        return Finish();
    }
private:
    void Request(){Scan->StartScan();Scan->RequestImmediateScheduledScan();Scan->PrepareScheduledScan(0.0);}
    bool Finish(){if(Scan)Scan->StopScan();if(Motion)Motion->StopPlayback();if(Sensor)Sensor->GpuDepthProjectionComponent->UnregisterComponent();Scene.Reset();return true;}
    FAutomationTestBase* Test;TUniquePtr<FPreviewScene> Scene;AStaticMeshActor* Wall=nullptr;AVirtualLidarSensorActor* Sensor=nullptr;
    UVirtualLidarScanComponent* Scan=nullptr;USlabMotionComponent* Motion=nullptr;UVirtualLidarSurfaceResponseComponent* Response=nullptr;
    double Started=0,Bright=0;int64 LastFrame=-1;int32 Phase=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarFidelityRhiTest,"MA0T10.LidarFidelity.GpuAcquisitionRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarFidelityRhiTest::RunTest(const FString&)
{
    if(GUsingNullRHI||!FApp::CanEverRender()){AddWarning(TEXT("SKIP: fidelity GPU acquisition requires RHI"));return true;}
    ADD_LATENT_AUTOMATION_COMMAND(FLidarFidelityRhiCommand(this));return true;
}
#endif
