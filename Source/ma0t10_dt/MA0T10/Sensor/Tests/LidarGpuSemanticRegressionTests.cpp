#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "ShaderCompiler.h"
#include "Components/StaticMeshComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "RHIGlobals.h"

class FLidarGpuSemanticCommand : public IAutomationLatentCommand
{
public:
    explicit FLidarGpuSemanticCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        if (!Scene)
        {
            Scene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
            auto* World = Scene->GetWorld();
            auto* IdMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId"));
            if (IdMaterial) IdMaterial->CacheShaders(EMaterialShaderPrecompileMode::Default);
            auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            auto MakeBox = [&](FName Tag, FVector Location, FVector Scale)
            {
                auto* Actor = World->SpawnActor<AStaticMeshActor>();
                Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
                Actor->SetActorLocation(Location); Actor->SetActorScale3D(Scale);
                Actor->Tags.Add(Tag); return Actor;
            };
            Plate = MakeBox(TEXT("RegressionPlate"), FVector(0,0,-5), FVector(20,20,0.1));
            Object = MakeBox(TEXT("RegressionObject"), FVector(0,0,12.5), FVector(3,3,0.25));
            Sensor = World->SpawnActor<AVirtualLidarSensorActor>();
            Sensor->SetActorLocation(FVector(0,0,1000));
            Sensor->SetActorRotation(FRotator(-90,0,0));
            auto* Scan = Sensor->ScanComponent.Get();
            Scan->StopScan();
            Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);
            Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
            Scan->AcquisitionBackend = EVirtualLidarAcquisitionBackend::GpuDepthProjection;
            Scan->FidelityMode = EVirtualSensorFidelityMode::IdealTruth;
            Scan->bEnableRangeNoise = false;
            Scan->ViewMode = EVirtualLidarViewMode::ActorClassColor;
            Scan->bEnableSemanticClassification = true;
            Scan->SemanticClassRules.Reset();
            for (FName Label : {FName(TEXT("RegressionPlate")), FName(TEXT("RegressionObject"))})
            {
                FVirtualLidarSemanticClassRule R; R.Label = Label; R.ActorTags.Add(Label); Scan->SemanticClassRules.Add(R);
            }
            Scan->SetInteractivePreviewMode(false, true);
            World->SendAllEndOfFrameUpdates();
            Started = FPlatformTime::Seconds();
            // Give assets/shader resources time to enter the render scene before first capture.
            return false;
        }
        const double Now = FPlatformTime::Seconds();
        if (Now - Started < 2.0) return false;
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling() && Now - Started < 60.0) return false;
        auto* Scan = Sensor->ScanComponent.Get();
        if (!bRequested)
        {
            Scan->StartScan(); Scan->RequestImmediateScheduledScan(); Scan->PrepareScheduledScan(0.0);
            bRequested = true;
        }
        Scan->ProcessScheduledScanChunk(1024);
        const auto Frame = Scan->GetLastFrameSnapshot();
        if (!Frame.IsValid() || (bWaitingSecond && Frame->FrameId == PreviousFrameId))
        {
            if (Now - Started < 20.0) return false;
            Test->AddError(TEXT("GPU frame timeout")); return Finish();
        }
        int32 Examined = 0, Correct = 0, ObjectHits = 0, PlateHits = 0;
        double MaxHeightError = 0;
        for (const auto& P : *Frame->Points)
        {
            if (!P.bHit) continue;
            const double X = FMath::Abs(P.WorldLocation.X - Object->GetActorLocation().X);
            const double Y = FMath::Abs(P.WorldLocation.Y);
            const bool bObjectInterior = X < 100 && Y < 100;
            const bool bPlateInterior = (X > 200 || Y > 200) && FMath::Abs(P.WorldLocation.X) < 850 && Y < 850;
            if (!bObjectInterior && !bPlateInterior) continue;
            ++Examined;
            if (P.SemanticLabel == (bObjectInterior ? TEXT("RegressionObject") : TEXT("RegressionPlate"))) ++Correct;
            if (bObjectInterior) ++ObjectHits; else ++PlateHits;
            MaxHeightError = FMath::Max(MaxHeightError, FMath::Abs(P.WorldLocation.Z - (bObjectInterior ? 25.0 : 0.0)));
        }
        Test->AddInfo(FString::Printf(TEXT("GPU semantic phase=%d interior=%d correct=%d object=%d plate=%d maxHeightErrorCm=%.6f status=%s"), Phase, Examined, Correct, ObjectHits, PlateHits, MaxHeightError, *Scan->GetRuntimeStatus().AcquisitionBackendMessage));
        Test->TestTrue(TEXT("both surfaces were acquired"), ObjectHits > 20 && PlateHits > 20);
        Test->TestTrue(TEXT("GPU semantic accuracy >=99%%"), Examined > 0 && Correct >= Examined * .99);
        Test->TestTrue(TEXT("25cm geometry accuracy <=1cm"), MaxHeightError <= 1.0);
        Test->TestFalse(TEXT("source stencil untouched"), Object->GetStaticMeshComponent()->bRenderCustomDepth);
        if (Phase++ == 0)
        {
            Scan->StopScan();
            Object->SetActorLocation(FVector(300,0,12.5));
            Scene->GetWorld()->SendAllEndOfFrameUpdates();
            PreviousFrameId = Frame->FrameId;
            Scan->StartScan(); Scan->RequestImmediateScheduledScan(); Scan->PrepareScheduledScan(0.0);
            bWaitingSecond = true;
            return false;
        }
        return Finish();
    }
private:
    bool Finish() { if (Sensor) { Sensor->ScanComponent->StopScan(); Sensor->GpuDepthProjectionComponent->UnregisterComponent(); } Scene.Reset(); return true; }
    FAutomationTestBase* Test;
    TUniquePtr<FPreviewScene> Scene;
    AStaticMeshActor* Plate = nullptr;
    AStaticMeshActor* Object = nullptr;
    AVirtualLidarSensorActor* Sensor = nullptr;
    double Started = 0;
    bool bRequested = false;
    bool bWaitingSecond = false;
    int64 PreviousFrameId = -1;
    int32 Phase = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarGpuSemanticRegression,
    "MA0T10.LidarRegression.GpuSemanticRhi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarGpuSemanticRegression::RunTest(const FString&)
{
    if (!FApp::CanEverRender() || GUsingNullRHI) { AddWarning(TEXT("SKIP: GPU semantic requires real RHI")); return true; }
    ADD_LATENT_AUTOMATION_COMMAND(FLidarGpuSemanticCommand(this));
    return true;
}
#endif
