#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "RHIGlobals.h"

class FLidarReadbackReuseCommand : public IAutomationLatentCommand
{
public:
    explicit FLidarReadbackReuseCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        if (!Scene)
        {
            Scene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
            auto* World = Scene->GetWorld();
            auto* Plate = World->SpawnActor<AStaticMeshActor>();
            Plate->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
            Plate->SetActorLocation(FVector(0,0,-5)); Plate->SetActorScale3D(FVector(20,20,.1));
            Sensor = World->SpawnActor<AVirtualLidarSensorActor>();
            Sensor->ScanComponent->StopScan();
            World->SendAllEndOfFrameUpdates();
            Started = FPlatformTime::Seconds();
            return false;
        }
        if (FPlatformTime::Seconds()-Started < 2) return false;
        auto* Backend = Sensor->GpuDepthProjectionComponent.Get();
        if (!bRequested)
        {
            FVirtualLidarDepthAcquisitionRequest Request;
            Request.FrameId = Phase + 1;
            Request.HorizontalSamples = Phase < 3 ? 32 : 64;
            Request.VerticalChannels = 8;
            Request.AcquisitionTransform = FTransform(FRotator(-90,0,0), FVector(Phase*10,0,1000+Phase*100));
            Expected = Request;
            if (!Test->TestTrue(TEXT("GPU acquisition accepted"), Backend->BeginAcquisition(Request))) return Finish();
            bRequested = true; Deadline = FPlatformTime::Seconds()+8;
            if (Phase == 4)
            {
                // Retire an in-flight generation; its callback must not be
                // applied or return the old staging buffer to the new frame.
                Backend->CancelAcquisition();
                Request.FrameId = 100;
                Expected = Request;
                Test->TestTrue(TEXT("restart after cancellation accepted"), Backend->BeginAcquisition(Request));
            }
        }
        FVirtualLidarDepthAcquisitionFrame Frame;
        const auto Result = Backend->PollAcquisition(Frame);
        if (Result != EVirtualSensorBackendPollResult::Completed)
        {
            if (FPlatformTime::Seconds() < Deadline && Result != EVirtualSensorBackendPollResult::Failed) return false;
            Test->AddError(TEXT("readback reuse frame timeout/failure")); return Finish();
        }
        Test->TestEqual(TEXT("matching acquisition identity"), Frame.Request.FrameId, Expected.FrameId);
        Test->TestTrue(TEXT("matching acquisition pose"), Frame.Request.AcquisitionTransform.Equals(Expected.AcquisitionTransform));
        Test->TestEqual(TEXT("matching resolution"), Frame.CaptureWidth, Expected.HorizontalSamples);
        Test->TestEqual(TEXT("complete depth buffer"), Frame.ForwardDepthCentimeters.Num(), Frame.CaptureWidth*Frame.CaptureHeight);
        if (Frame.ForwardDepthCentimeters.Num() > 0)
        {
            const float CenterDepth = Frame.ForwardDepthCentimeters[(Frame.CaptureHeight/2)*Frame.CaptureWidth+Frame.CaptureWidth/2];
            Test->TestTrue(TEXT("reused staging contains current capture, not previous pixels"), FMath::Abs(CenterDepth-Expected.AcquisitionTransform.GetLocation().Z) < 2);
        }
        if (Phase < 3) Test->TestEqual(TEXT("same-extent acquisitions reuse one staging object"), Backend->GetReadbackAllocationCount(), uint64(1));
        if (Phase == 3) Test->TestEqual(TEXT("extent change reallocates once"), Backend->GetReadbackAllocationCount(), uint64(2));
        if (Phase == 4) Test->TestEqual(TEXT("cancelled generation gets a fresh staging object"), Backend->GetReadbackAllocationCount(), uint64(3));
        bRequested = false;
        if (++Phase == 5) return Finish();
        return false;
    }
private:
    bool Finish() { if (Sensor) Sensor->GpuDepthProjectionComponent->UnregisterComponent(); Scene.Reset(); return true; }
    FAutomationTestBase* Test;
    TUniquePtr<FPreviewScene> Scene;
    AVirtualLidarSensorActor* Sensor = nullptr;
    FVirtualLidarDepthAcquisitionRequest Expected;
    double Started = 0, Deadline = 0;
    int32 Phase = 0;
    bool bRequested = false;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarReadbackReuseTest, "MA0T10.LidarRegression.ReadbackReuseRhi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLidarReadbackReuseTest::RunTest(const FString&)
{
    if (!FApp::CanEverRender() || GUsingNullRHI) { AddWarning(TEXT("SKIP: readback reuse requires real RHI")); return true; }
    ADD_LATENT_AUTOMATION_COMMAND(FLidarReadbackReuseCommand(this));
    return true;
}
#endif
