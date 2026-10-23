#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "ShaderCompiler.h"
#include "RHIGlobals.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"

// Force only this private semantic scene to remain pending. Main depth capture,
// RHI readback, completion, frame IDs and subsequent PSO recovery remain real.
class FLidarSemanticPsoTimeoutCommand final : public IAutomationLatentCommand
{
public:
    explicit FLidarSemanticPsoTimeoutCommand(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(!Scene)
        {
            Started=Now;
            Scene=MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
            auto* World=Scene->GetWorld();
            auto* Target=World->SpawnActor<AStaticMeshActor>();Target->Tags.Add(TEXT("Slab"));
            Target->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Target->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Target->SetActorLocation(FVector(1000,0,0));Target->SetActorScale3D(FVector(.5,8,4));
            Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Lidar->ScanComponent->StopScan();
            Backend=Lidar->GpuDepthProjectionComponent.Get();Backend->SetSemanticPsoPendingForTests(true);
            Request.bRequestSemantics=true;Request.HorizontalSamples=64;Request.VerticalChannels=16;
            Request.HorizontalFovDegrees=30;Request.MinVerticalAngleDegrees=-8;Request.MaxVerticalAngleDegrees=8;Request.MaxDistanceCm=5000;
            World->SendAllEndOfFrameUpdates();return false;
        }
        if(Now-Started>30){Test->AddError(TEXT("PSO timeout/recovery regression exceeded30s"));return Finish();}
        if(Now-Started<2||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()))return false;
        if(!InFlight)
        {
            if(Phase==1&&Now<NextSubmission)return false;
            Request.FrameId++;
            if(!Test->TestTrue(TEXT("next depth acquisition is admitted"),Backend->BeginAcquisition(Request)))return Finish();
            Submitted=Now;InFlight=true;return false;
        }
        FVirtualLidarDepthAcquisitionFrame Frame;
        const auto Result=Backend->PollAcquisition(Frame);
        if(Result==EVirtualSensorBackendPollResult::Pending)return false;
        if(!Test->TestEqual(TEXT("pending PSO completes depth, without Failed/Idle"),Result,EVirtualSensorBackendPollResult::Completed))return Finish();
        InFlight=false;
        Test->TestEqual(TEXT("immutable frame identity retained"),Frame.Request.FrameId,Request.FrameId);
        Test->TestTrue(TEXT("real depth hits survive semantic timeout"),Frame.ForwardDepthCentimeters.ContainsByPredicate([](float Depth){return Depth>100&&Depth<2000;}));
        if(Phase<2)
        {
            Test->TestEqual(TEXT("pending PSO has no stale ID pixels"),Frame.SemanticIdDepth.Num(),0);
            Test->TestEqual(TEXT("pending PSO has no stale identities"),Frame.SemanticIdentities.Num(),0);
            Test->TestTrue(TEXT("unclassified reason reports PSO timeout"),Frame.SemanticStatus.Contains(TEXT("PSO")));
        }
        if(Phase==0)
        {
            Test->TestTrue(TEXT("initial grace expires near1s rather than hanging"),Now-Submitted>=1&&Now-Submitted<2);
            Phase=1;CadenceStarted=Now;NextSubmission=Now;return false;
        }
        if(Phase==1)
        {
            if(CadenceFrames==0)FirstCompletion=Now;
            ++CadenceFrames;NextSubmission=CadenceStarted+CadenceFrames*.05;
            if(CadenceFrames<60)return false;
            const double Hz=(CadenceFrames-1)/(Now-FirstCompletion);
            Test->AddInfo(FString::Printf(TEXT("forced PSO pending:60 completed depth frames, requested20Hz, actual%.6fHz"),Hz));
            Test->TestTrue(TEXT("after one grace period pending PSO retains >=99% of requested20Hz"),Hz/20>=.99);
            Backend->SetSemanticPsoPendingForTests(false);Phase=2;return false;
        }
        Test->TestTrue(TEXT("ready PSO restores semantic pixels"),Frame.SemanticIdDepth.ContainsByPredicate([](const FLinearColor& Pixel){return Pixel.R>0&&Pixel.A>100&&Pixel.A<2000;}));
        bool bSlab=false;for(const auto& Pair:Frame.SemanticIdentities)bSlab|=Pair.Value.Label==TEXT("Slab");
        Test->TestTrue(TEXT("ready PSO restores Slab identity"),bSlab);
        return Finish();
    }
private:
    bool Finish(){if(Backend)Backend->CancelAcquisition();Scene.Reset();return true;}
    FAutomationTestBase* Test;
    TUniquePtr<FPreviewScene> Scene;
    AVirtualLidarSensorActor* Lidar=nullptr;
    UVirtualLidarGpuDepthProjectionComponent* Backend=nullptr;
    FVirtualLidarDepthAcquisitionRequest Request;
    bool InFlight=false;
    int32 Phase=0,CadenceFrames=0;
    double Started=0,Submitted=0,CadenceStarted=0,FirstCompletion=0,NextSubmission=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarSemanticPsoTimeoutTest,"MA0T10.LidarRegression.SemanticPsoTimeoutRecoveryRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarSemanticPsoTimeoutTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddWarning(TEXT("SKIP: semantic PSO regression requires actual RHI"));return true;}
    ADD_LATENT_AUTOMATION_COMMAND(FLidarSemanticPsoTimeoutCommand(this));return true;
}
#endif
