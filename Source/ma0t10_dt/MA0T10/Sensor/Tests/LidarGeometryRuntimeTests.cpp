#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "RHIGlobals.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "LidarGeometryValidationRig.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include <atomic>

struct FGeometryPixelEvidence { TArray<FColor> Pixels; std::atomic<bool> Ready{false}; };
class FGeometryRuntimeCheck : public IAutomationLatentCommand
{
public:
    explicit FGeometryRuntimeCheck(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Started==0) Started=Now;
        UWorld* World=nullptr;
        for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::PIE){World=C.World();break;}
        ALidarGeometryValidationRig* Rig=nullptr;
        if(World) for(TActorIterator<ALidarGeometryValidationRig> It(World);It;++It){Rig=*It;break;}
        if(!Rig || !Rig->Monitor || !Rig->Sensor)
        {
            if(Now-Started<20) return false;
            Test->AddError(TEXT("validation rig/monitor did not initialize")); return true;
        }
        if(Now-Started>60){Test->AddError(TEXT("geometry pixel readback timed out"));return true;}
        auto* V=Rig->Sensor->VisualizationComponent.Get();
        auto* Scan=Rig->Sensor->ScanComponent.Get();
        if(!bStopped)
        {
            Rig->bMoving=false; bStopped=true; PhaseStart=Now; return false;
        }
        if(Now-PhaseStart<1.5) return false;
        if(!Evidence)
        {
            const auto& D=V->GetGeometryDiagnostics();
            if(!D.bReliable || D.ProtrusionPoints<20)
            {
                if(Now-PhaseStart<15) return false;
                Test->AddError(FString::Printf(TEXT("No reliable geometry result: %s hits=%d points=%d pose=%s"),*D.Reason,Scan->GetLastHitPointCount(),Scan->GetLastPoints().Num(),*Scan->GetComponentTransform().ToString())); return true;
            }
            Test->TestTrue(TEXT("plate and object have NO tags"), Rig->Plate->Tags.IsEmpty() && Rig->MovingObject->Tags.IsEmpty());
            const auto Frame=Scan->GetLastFrameSnapshot();
            int32 Examined=0,Correct=0;
            for(const auto& P:*Frame->Points)
            {
                if(!P.bHit) continue;
                const double X=FMath::Abs(P.WorldLocation.X),Y=FMath::Abs(P.WorldLocation.Y-Rig->MovingObject->GetActorLocation().Y);
                const bool Object=X<30&&Y<30;
                const bool Plate=(X>80||Y>80)&&X<180&&FMath::Abs(P.WorldLocation.Y)<380;
                if(!Object&&!Plate) continue;
                ++Examined; Correct+=D.Classify(P.WorldLocation)==(Object?ELidarGeometryClass::Protrusion:ELidarGeometryClass::Plane);
            }
            Test->TestTrue(TEXT("tag-free interior classification >=99%"),Examined>0&&Correct>=Examined*.99);
            auto* Texture=V->GetPreviewTexture();
            if(!Texture||!Texture->GetResource()) return false;
            const FTextureRHIRef Rhi=Texture->GetResource()->TextureRHI;
            if(!Rhi.IsValid()) return false;
            Width=Texture->GetSizeX();Height=Texture->GetSizeY();
            Readback=MakeShared<FRHIGPUTextureReadback,ESPMode::ThreadSafe>(TEXT("GeometryFinalTexture"));
            Evidence=MakeShared<FGeometryPixelEvidence,ESPMode::ThreadSafe>();
            auto Copy=Readback;
            ENQUEUE_RENDER_COMMAND(GeometryTextureCopy)([Copy,Rhi](FRHICommandListImmediate& R){Copy->EnqueueCopy(R,Rhi);});
            Test->AddInfo(FString::Printf(TEXT("geometry phase=%d examined=%d correct=%d plane=%d protrusion=%d analysisMs=%.3f"),Phase,Examined,Correct,D.PlanePoints,D.ProtrusionPoints,D.ProcessingMs));
            return false;
        }
        if(!bMapping && Readback->IsReady())
        {
            bMapping=true; auto Copy=Readback; auto Out=Evidence; const int32 W=Width,H=Height;
            ENQUEUE_RENDER_COMMAND(GeometryTextureMap)([Copy,Out,W,H](FRHICommandListImmediate&)
            {
                int32 Pitch=0;const FColor* P=static_cast<const FColor*>(Copy->Lock(Pitch));
                if(P&&Pitch>=W){Out->Pixels.SetNumUninitialized(W*H);for(int Y=0;Y<H;++Y)FMemory::Memcpy(Out->Pixels.GetData()+Y*W,P+Y*Pitch,W*sizeof(FColor));}
                if(P)Copy->Unlock();Out->Ready=true;
            });
            return false;
        }
        if(!Evidence->Ready) return false;
        int32 Blue=0,Orange=0;
        for(const auto& P:Evidence->Pixels){Blue+=P.B>220&&P.R<100&&P.G>70&&P.G<160;Orange+=P.R>220&&P.G>70&&P.G<180&&P.B<60;}
        Test->AddInfo(FString::Printf(TEXT("actual GPU preview texture phase=%d blue=%d orange=%d pixels=%d"),Phase,Blue,Orange,Evidence->Pixels.Num()));
        Test->TestTrue(TEXT("actual texture contains plane blue (not legend)"),Blue>=20);
        Test->TestTrue(TEXT("actual texture contains object orange (not legend)"),Orange>=20);
        auto Release=MoveTemp(Readback);ENQUEUE_RENDER_COMMAND(GeometryRelease)([Release](FRHICommandListImmediate&){});
        Evidence.Reset();bMapping=false;
        if(++Phase>=2) return true;
        Rig->ToggleBackend();PhaseStart=Now;return false;
    }
private:
    FAutomationTestBase* Test;double Started=0,PhaseStart=0;int32 Phase=0,Width=0,Height=0;bool bStopped=false,bMapping=false;
    TSharedPtr<FRHIGPUTextureReadback,ESPMode::ThreadSafe> Readback;
    TSharedPtr<FGeometryPixelEvidence,ESPMode::ThreadSafe> Evidence;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeometryRuntimeTest,"MA0T10.LidarGeometry.ActualMapPixels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGeometryRuntimeTest::RunTest(const FString&)
{
    if(GUsingNullRHI){AddWarning(TEXT("SKIP: actual geometry pixels require RHI"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/LidarSemanticValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FGeometryRuntimeCheck(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
