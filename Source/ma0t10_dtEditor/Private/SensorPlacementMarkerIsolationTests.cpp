#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "RHIGlobals.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"

/** Disposable scene: the real workspace pick marker is placed between sensors and a wall. */
class FPlacementMarkerIsolation : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TUniquePtr<FPreviewScene> Scene;
    AVirtualCameraSensorActor* PickTarget=nullptr;
    AVirtualLidarSensorActor* Lidar=nullptr;
    USceneCaptureComponent2D* Capture=nullptr;
    TArray<FColor> PixelsOff;
    TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> Off;
    TArray<FVector> NiagaraOff;
    TArray<FTransform> IsmOff;
    int32 Phase=0,PointsOff=0;
    bool Requested=false;
    double Started=FPlatformTime::Seconds();
    int64 LastFrame=-1;
    int32 CameraWarmup=0;
    TArray<FColor> WarmupPixels;
public:
    explicit FPlacementMarkerIsolation(FAutomationTestBase* T):Test(T){}
    ~FPlacementMarkerIsolation(){if(Lidar)Lidar->StopSensor();if(Capture&&Scene)Scene->RemoveComponent(Capture);Scene.Reset();}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>90){Test->AddError(TEXT("Placement marker RHI isolation timed out"));return true;}
        if(!Scene)
        {
            Scene=MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues());auto* W=Scene->GetWorld();
            auto* Wall=W->SpawnActor<AStaticMeshActor>();auto* Mesh=Wall->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Wall->SetActorLocation(FVector(900,0,0));Wall->SetActorScale3D(FVector(.1,30,30));
            auto* Material=NewObject<UMaterial>();Material->SetShadingModel(MSM_Unlit);auto* Color=NewObject<UMaterialExpressionConstant3Vector>(Material);Color->Constant=FLinearColor(.1,.4,.7);Material->GetExpressionCollection().AddExpression(Color);Material->GetEditorOnlyData()->EmissiveColor.Expression=Color;Material->PostEditChange();Mesh->SetMaterial(0,Material);
            PickTarget=W->SpawnActor<AVirtualCameraSensorActor>();PickTarget->StopSensor();PickTarget->SetActorLocation(FVector(300,0,0));
            Lidar=W->SpawnActor<AVirtualLidarSensorActor>();Lidar->StopSensor();auto* S=Lidar->ScanComponent.Get();
            S->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);S->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
            S->HorizontalSamples=64;S->VerticalChannels=16;S->HorizontalFov=60;S->MaxDistance=1500;S->bEnableRangeNoise=false;S->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;
            S->CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;
            S->bEnableSemanticClassification=true;S->ViewMode=EVirtualLidarViewMode::ActorClassColor;S->SemanticClassRules.Reset();
            FVirtualLidarSemanticClassRule Rule;Rule.Label=TEXT("Wall");Rule.ActorClassNames.Add(AStaticMeshActor::StaticClass()->GetFName());S->SemanticClassRules.Add(Rule);
            Rule.Label=TEXT("PickTarget");Rule.ActorClassNames={AVirtualCameraSensorActor::StaticClass()->GetFName()};S->SemanticClassRules.Add(Rule);
            S->SetPointCloudPreviewEnabled(true);S->SetPreviewPolicy(1,0,true);Lidar->VisualizationComponent->BindScanComponent(S);
            Lidar->VisualizationComponent->SetWorldPointCloudEnabled(true);
            Capture=NewObject<USceneCaptureComponent2D>();Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;
            Capture->ShowFlags.SetEyeAdaptation(false);Capture->ShowFlags.SetPostProcessing(false);Capture->ShowFlags.SetAntiAliasing(false);Capture->ShowFlags.SetTemporalAA(false);
            Capture->TextureTarget=NewObject<UTextureRenderTarget2D>(Capture);Capture->TextureTarget->InitCustomFormat(128,128,PF_B8G8R8A8,false);Capture->TextureTarget->UpdateResourceImmediate();
            Scene->AddComponent(Capture,FTransform::Identity);FAssetCompilingManager::Get().FinishAllCompilation();
            if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
            return false;
        }
        auto* W=Scene->GetWorld();auto* S=Lidar->ScanComponent.Get();
        if(CameraWarmup<3)
        {
            W->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();TArray<FColor> Pixels;
            Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
            CameraWarmup=Pixels==WarmupPixels?CameraWarmup+1:0;WarmupPixels=MoveTemp(Pixels);return false;
        }
        if(!Requested)
        {
            PickTarget->SetPlacementPickingEnabled(Phase%2==1);
            W->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();TArray<FColor> Pixels;Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
            if(Phase==0){PixelsOff=Pixels;Test->TestEqual(TEXT("Camera has real pixels"),Pixels.Num(),128*128);Test->TestTrue(TEXT("Camera renders wall"),Pixels.ContainsByPredicate([](const FColor& P){return P.B>20;}));}
            else
            {
                int32 Changed=0;for(int32 I=0;I<FMath::Min(Pixels.Num(),PixelsOff.Num());++I)Changed+=Pixels[I]!=PixelsOff[I];
                Test->AddInfo(FString::Printf(TEXT("Camera phase %d: changed pixels %d; first OFF=%s current=%s"),Phase,Changed,*PixelsOff[0].ToString(),*Pixels[0].ToString()));
                Test->TestTrue(TEXT("marker ON/OFF leaves Camera SceneCapture pixels identical"),Pixels==PixelsOff);
            }
            S->AcquisitionBackend=Phase<2?EVirtualLidarAcquisitionBackend::AccurateCpuTrace:EVirtualLidarAcquisitionBackend::GpuDepthProjection;
            S->StartScan();S->RequestImmediateScheduledScan();S->PrepareScheduledScan(0);Requested=true;
        }
        S->ProcessScheduledScanChunk(2048);const auto Frame=S->GetLastFrameSnapshot();
        if(!Frame.IsValid()||!Frame->Points.IsValid()||Frame->FrameId==LastFrame)return false;
        LastFrame=Frame->FrameId;S->StopScan();Lidar->VisualizationComponent->SetPointCloudRenderPolicy(ELidarPointCloudRenderPolicy::ForceCpu);Lidar->VisualizationComponent->RefreshLatestFrame();FlushRenderingCommands();
        FVirtualSensorFrameEnvelope Envelope;Envelope.PointSnapshot=Frame->Points;Envelope.SensorId=TEXT("PICK-ISOLATION");Envelope.SensorKind=EVirtualSensorKind::Lidar;
        FVirtualSensorStreamConfig Config;Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;
        FString Extension,Error;TArray<uint8> Bytes;int32 Points=0;
        Test->TestTrue(TEXT("actual PCD serializer accepts measured snapshot"),UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Envelope,Config,Extension,Bytes,Points,Error));
        Test->TestTrue(TEXT("physical wall produces PCD points"),Points>100);
        TArray<FTransform> Instances;TArray<UInstancedStaticMeshComponent*> Isms;Lidar->GetComponents(Isms);
        for(auto* Ism:Isms)if(Ism->GetFName()==TEXT("VirtualLidarPointCloudPreview"))for(int32 I=0;I<Ism->GetInstanceCount();++I){FTransform T;Ism->GetInstanceTransform(I,T,true);Instances.Add(T);}
        Test->TestTrue(TEXT("ISM preview has measured points"),!Instances.IsEmpty());
        Lidar->VisualizationComponent->SetPointCloudRenderPolicy(ELidarPointCloudRenderPolicy::ForceNiagara);Lidar->VisualizationComponent->RefreshLatestFrame();FlushRenderingCommands();
        TArray<FVector> Niagara;TArray<UNiagaraComponent*> Components;Lidar->GetComponents(Components);
        for(auto* N:Components)Niagara.Append(UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayPosition(N,TEXT("User.PointPositions")));
        Test->TestTrue(TEXT("Niagara preview has measured points"),!Niagara.IsEmpty());
        if(Phase%2==0){Off=Frame;PointsOff=Points;IsmOff=Instances;NiagaraOff=Niagara;}
        else
        {
            Test->TestEqual(TEXT("marker leaves PCD point count unchanged"),Points,PointsOff);
            Test->TestEqual(TEXT("marker leaves measured ray count unchanged"),Frame->Points->Num(),Off->Points->Num());
            int32 Depth=0,Semantic=0;for(int32 I=0;I<FMath::Min(Frame->Points->Num(),Off->Points->Num());++I){const auto& A=(*Off->Points)[I];const auto& B=(*Frame->Points)[I];Depth+=A.bHit!=B.bHit||!A.WorldLocation.Equals(B.WorldLocation,.01);Semantic+=A.SemanticLabel!=B.SemanticLabel;}
            Test->TestEqual(TEXT("CPU/GPU depth unaffected by pick sphere"),Depth,0);Test->TestEqual(TEXT("CPU/GPU semantic proxies exclude pick sphere"),Semantic,0);
            Test->TestEqual(TEXT("ISM preview count unchanged"),Instances.Num(),IsmOff.Num());for(int32 I=0;I<FMath::Min(Instances.Num(),IsmOff.Num());++I)Test->TestTrue(TEXT("ISM preview positions unchanged"),Instances[I].Equals(IsmOff[I],.01));
            Test->TestTrue(TEXT("Niagara preview positions unchanged"),Niagara==NiagaraOff);
            Test->AddInfo(FString::Printf(TEXT("%s: PCD %d, ISM %d, Niagara %d, changed depth %d, semantic %d"),Phase<2?TEXT("CPU"):TEXT("GPU"),Points,Instances.Num(),Niagara.Num(),Depth,Semantic));
        }
        ++Phase;Requested=false;
        if(Phase<4)return false;
        // Positive control: a visible, blocking sphere on the same ray must be detected.
        TArray<UStaticMeshComponent*> Meshes;PickTarget->GetComponents(Meshes);for(auto* M:Meshes)if(M->GetFName()==TEXT("SensorPlacementPickMarker")){M->SetRenderInMainPass(true);M->SetRenderInDepthPass(true);M->SetHiddenInSceneCapture(false);M->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);}
        W->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();TArray<FColor> Pixels;Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
        Test->TestTrue(TEXT("negative control detects visible marker in camera"),Pixels!=PixelsOff);
        FHitResult Hit;W->LineTraceSingleByChannel(Hit,FVector::ZeroVector,FVector(800,0,0),ECC_Visibility);Test->TestTrue(TEXT("negative control detects blocking marker in CPU trace"),Hit.GetActor()==PickTarget);
        return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementMarkerIsolationTest,"MA0T10.UXR02.PlacementMarkerIsolationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlacementMarkerIsolationTest::RunTest(const FString&)
{
    if(GUsingNullRHI||!FApp::CanEverRender()){AddInfo(TEXT("SKIP: placement marker isolation requires actual RHI"));return true;}
    ADD_LATENT_AUTOMATION_COMMAND(FPlacementMarkerIsolation(this));return true;
}
#endif
