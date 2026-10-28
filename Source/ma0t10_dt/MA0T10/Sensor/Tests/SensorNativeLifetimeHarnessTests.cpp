#include "SensorOwnedRayTracing.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Json.h"
#include "RenderingThread.h"
#include "Misc/CoreDelegates.h"

namespace
{
// One logical sensor, fixed poses, one immutable back plane and one edited
// cargo grid. Distinct mesh shapes/counts expose stale AS input addresses that
// identical-plane oracles cannot distinguish. No map or UObject is saved.
class FNativeLifetimeHarness final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds();
    int32 Iteration=0,Target=1000;
    bool Submitted=false,ReferenceReady=false,NativeReady=false;
    bool EngineReference=true;
    bool WideMetadata=false;
    TArray<FSensorRtMesh> Meshes;
    TArray<FSensorRtRay> Rays;
    TArray<FSensorRtHit> Hits,ReferenceHits;
    TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe> Scan,Reference;
    SensorOwnedRayTracing::FSubmissionStats Before;
    int32 Side=32,Cells=32*32;
    float FrontX=100;
    bool Removed(int32 Cell)const{return (Cell+Iteration*17)%Cells<(1+Iteration%13);}
    static FVector4f Surface(uint32 Index)
    {float Bits;FMemory::Memcpy(&Bits,&Index,sizeof(Bits));return FVector4f(-1,0,0,Bits);}
    void Build()
    {
        FrontX=100.f+float(Iteration%37)*.25f;Meshes.Reset();
        auto V=MakeShared<TArray<FVector3f>,ESPMode::ThreadSafe>();
        auto N=MakeShared<TArray<FVector4f>,ESPMode::ThreadSafe>();
        V->Reserve(Cells*6);N->Reserve(Cells*2);
        for(int32 Cell=0;Cell<Cells;++Cell)if(!Removed(Cell))
        {
            // First wide epoch caches only the two-triangle back plane.
            // The following epoch grows that same BLAS's metadata binding.
            if(WideMetadata&&Iteration==0)continue;
            const float Y=-128+(Cell%Side)*8,Z=-128+(Cell/Side)*8;
            V->Append({FVector3f(FrontX,Y,Z),FVector3f(FrontX,Y+8,Z),FVector3f(FrontX,Y+8,Z+8),FVector3f(FrontX,Y,Z),FVector3f(FrontX,Y+8,Z+8),FVector3f(FrontX,Y,Z+8)});
            N->Add(Surface(11));N->Add(Surface(11));
        }
        FSensorRtMesh Front;Front.Revision=(WideMetadata?0x26b800000000ull:0x26a800000000ull)+Iteration;Front.Vertices=V;Front.NormalsAndSurfaces=N;Meshes.Add(Front);
        if(WideMetadata)for(int32 I=0;I<2;++I){FSensorRtMesh Empty;Empty.Revision=0x26b700000001ull+I;Empty.Vertices=MakeShared<TArray<FVector3f>,ESPMode::ThreadSafe>();Empty.NormalsAndSurfaces=MakeShared<TArray<FVector4f>,ESPMode::ThreadSafe>();Meshes.Add(Empty);}
        auto B=MakeShared<TArray<FVector3f>,ESPMode::ThreadSafe>();
        const float Extent=WideMetadata?1024.f:256.f;
        B->Append({FVector3f(450,-Extent,-Extent),FVector3f(450,Extent,-Extent),FVector3f(450,Extent,Extent),FVector3f(450,-Extent,-Extent),FVector3f(450,Extent,Extent),FVector3f(450,-Extent,Extent)});
        auto BN=MakeShared<TArray<FVector4f>,ESPMode::ThreadSafe>();BN->Add(Surface(22));BN->Add(Surface(22));
        FSensorRtMesh Back;Back.Revision=WideMetadata?0x26b7ffffffffull:0x26a7ffffffffull;Back.Vertices=B;Back.NormalsAndSurfaces=BN;Meshes.Add(Back);
        if(Rays.IsEmpty())for(int32 I=0;I<65536;++I)
        {
            const int32 Cell=I%Cells;const float Y=-128+(Cell%Side)*8+2.96f,Z=-128+(Cell/Side)*8+5.68f;
            Rays.Add({FVector3f(0,Y,Z),(WideMetadata&&Iteration==0)||I%97==0?50.f:500.f,FVector3f(1,0,0),0});
        }
        if(WideMetadata&&Iteration==1)for(int32 I=0;I<Rays.Num();++I)Rays[I].MaxDistance=I%97==0?50.f:500.f;
    }
    void Save(bool Complete)
    {
        FString Root;if(!FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Root))Root=FPaths::ProjectSavedDir()/TEXT("Reports/LIDAR-NATIVE-LIFETIME-26h/harness");
        IFileManager::Get().MakeDirectory(*Root,true);
        const auto Stats=SensorOwnedRayTracing::GetSubmissionStats();auto J=MakeShared<FJsonObject>();
        J->SetBoolField(TEXT("complete"),Complete);J->SetBoolField(TEXT("valid"),Complete&&!Test->HasAnyErrors());
        J->SetNumberField(TEXT("completed_iterations"),Iteration);J->SetNumberField(TEXT("requested_iterations"),Target);
        J->SetNumberField(TEXT("rays_per_sensor"),Rays.Num());J->SetNumberField(TEXT("sensor_count"),1);
        J->SetNumberField(TEXT("grid_side"),Side);J->SetBoolField(TEXT("wide_unequal_metadata_sparse_slots"),WideMetadata);
        J->SetBoolField(TEXT("fresh_engine_byte_oracle_enabled"),EngineReference);
        J->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
        J->SetNumberField(TEXT("native_dispatch_delta"),Stats.NativeDispatches-Before.NativeDispatches);
        J->SetNumberField(TEXT("native_blas_build_delta"),Stats.NativeBlasBuilds-Before.NativeBlasBuilds);
        J->SetNumberField(TEXT("native_tlas_build_delta"),Stats.NativeSceneBuilds-Before.NativeSceneBuilds);
        J->SetNumberField(TEXT("native_bytes"),Stats.NativeBytes);J->SetNumberField(TEXT("native_peak_bytes"),Stats.NativePeakBytes);
        J->SetBoolField(TEXT("debug_interface_observed"),Stats.NativeValidationAvailable);
        if(Stats.NativeValidationAvailable)J->SetNumberField(TEXT("debug_error_delta"),Stats.NativeValidationErrors-Before.NativeValidationErrors);
        J->SetStringField(TEXT("scope"),TEXT("Synthetic fixed scene; one sensor; cargo geometry edits/cache eviction; full65536 analytic points plus periodic fresh-engine whole-byte oracle. Production Dig integration is a separate gate."));
        FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Root/(WideMetadata?TEXT("native-wide-metadata-harness.json"):TEXT("native-lifetime-harness.json"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
public:
    explicit FNativeLifetimeHarness(FAutomationTestBase* In,bool Wide=false):Test(In),WideMetadata(Wide)
    {Side=Wide?96:32;Cells=Side*Side;FParse::Value(FCommandLine::Get(),TEXT("SensorNativeHarnessIterations="),Target);Target=FMath::Clamp(Target,1,10000);EngineReference=!FParse::Param(FCommandLine::Get(),TEXT("SensorNativeNoEngineReference"));Before=SensorOwnedRayTracing::GetSubmissionStats();}
    bool Update()override
    {
        if(FPlatformTime::Seconds()-Started>1800){Test->AddError(TEXT("native lifetime harness timeout"));if(Scan)Scan->Cancel();if(Reference)Reference->Cancel();Save(false);return true;}
        if(!Submitted)
        {
            if(!SensorOwnedRayTracing::IsAvailable()||!SensorOwnedRayTracing::UsesNativeBackend())
            {Test->AddError(TEXT("native lifetime harness requires actual native HWRT (-SensorRtNative); no skip"));Save(false);return true;}
            Build();Hits.Reset();ReferenceHits.Reset();ReferenceReady=!EngineReference||Iteration%64!=0;NativeReady=false;
            if(!ReferenceReady){FSensorRtRequest R;R.Meshes=Meshes;R.Rays=Rays;R.bFreshResourcesForTests=true;Reference=SensorOwnedRayTracing::Submit(MoveTemp(R));}
            FSensorRtRequest R;R.Meshes=Meshes;R.Rays=Rays;R.OwnerToken=WideMetadata?0x26b8:0x26a8;Scan=SensorOwnedRayTracing::Submit(MoveTemp(R));
            if(!Scan||(!ReferenceReady&&!Reference)){Test->AddError(TEXT("harness admission failed"));Save(false);return true;}
            SensorOwnedRayTracing::FlushPendingSubmissions();Submitted=true;return false;
        }
        FString Error;
        if(!NativeReady){NativeReady=Scan->Poll(Hits,Error);if(!NativeReady)return false;if(!Error.IsEmpty()){Test->AddError(Error);Save(false);return true;}}
        if(!ReferenceReady){ReferenceReady=Reference->Poll(ReferenceHits,Error);if(!ReferenceReady)return false;if(!Error.IsEmpty()){Test->AddError(Error);Save(false);return true;}}
        bool Correct=Hits.Num()==Rays.Num();
        for(int32 I=0;Correct&&I<Hits.Num();++I)
        {
            const bool Miss=Rays[I].MaxDistance<FrontX;const bool Hole=Removed(I%Cells);const auto& H=Hits[I];
            Correct=Miss?(H.NormalAndDistance.W==-1&&H.Mesh==MAX_uint32&&H.Surface==MAX_uint32&&H.Triangle==MAX_uint32):
                (FMath::Abs(H.NormalAndDistance.W-(Hole?450.f:FrontX))<.001f&&H.Mesh==uint32(Hole?(WideMetadata?3:1):0)&&H.Surface==uint32(Hole?22:11)&&H.NormalAndDistance.X==-1&&H.NormalAndDistance.Y==0&&H.NormalAndDistance.Z==0&&H.Reserved==0);
        }
        if(!Test->TestTrue(FString::Printf(TEXT("all65536 analytic hit/miss/mesh/surface/normal values at edit%d"),Iteration),Correct)){Save(false);return true;}
        if(!ReferenceHits.IsEmpty()&&!Test->TestTrue(TEXT("whole native bytes equal independent fresh engine raygen"),ReferenceHits.Num()==Hits.Num()&&FMemory::Memcmp(Hits.GetData(),ReferenceHits.GetData(),Hits.Num()*sizeof(FSensorRtHit))==0)){Save(false);return true;}
        Scan.Reset();Reference.Reset();SensorOwnedRayTracing::RecycleHitArray(MoveTemp(Hits));++Iteration;Submitted=false;
        if(Iteration%100==0){UE_LOG(LogTemp,Display,TEXT("[NativeLifetimeHarness] completed=%d/%d"),Iteration,Target);Save(false);}
        if(Iteration==Target)
        {
            if(!FParse::Param(FCommandLine::Get(),TEXT("SensorNativeHarnessDiagnostic")))Test->TestTrue(TEXT("at least1000 edits for acceptance"),Iteration>=1000);
            const auto Now=SensorOwnedRayTracing::GetSubmissionStats();Test->TestTrue(TEXT("native dispatched for every edit"),Now.NativeDispatches-Before.NativeDispatches>=uint64(Target));
            if(FParse::Param(FCommandLine::Get(),TEXT("SensorNativeStrictValidation"))){Test->TestTrue(TEXT("debug layer interface observed"),Now.NativeValidationAvailable);Test->TestEqual(TEXT("debug layer errors zero"),Now.NativeValidationErrors-Before.NativeValidationErrors,uint64(0));}
            Save(true);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorNativeLifetimeHarnessTest,"HATCH26.NativeLifetimeHarnessRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FSensorNativeLifetimeHarnessTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FNativeLifetimeHarness(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorNativeWideMetadataTest,"HATCH26.NativeWideMetadataBoundsRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FSensorNativeWideMetadataTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FNativeLifetimeHarness(this,true));return true;}

// A commandlet drives the same diagnostic command without an Editor/game
// viewport or AutomationController's interactive-frame-rate/GPU warm-up.
// Only test execution uses this clock; acquisition scheduling is unchanged.
MA0T10_DT_API int32 RunNativeLifetimeHarnessCommandlet()
{
    FSensorNativeLifetimeHarnessTest Test(TEXT("HATCH26.NativeLifetimeCommandlet"));
    FNativeLifetimeHarness Harness(&Test);
    while(!Harness.Update())
    {
        ENQUEUE_RENDER_COMMAND(NativeLifetimeDiagnosticPump)([](FRHICommandListImmediate&){FCoreDelegates::OnEndFrameRT.Broadcast();});
        FlushRenderingCommands();FPlatformProcess::Sleep(.001f);
    }
    ENQUEUE_RENDER_COMMAND(NativeLifetimeDiagnosticFinalPump)([](FRHICommandListImmediate&){FCoreDelegates::OnEndFrameRT.Broadcast();});
    FlushRenderingCommands();
    return Test.HasAnyErrors()?1:0;
}
#endif
