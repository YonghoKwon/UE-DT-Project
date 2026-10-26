#include "SensorOwnedRayTracing.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
class FBatchParity final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds();
    int32 Phase=0,Repeats=0;
    TArray<TArray<FSensorRtRay>> Rays;
    FSensorRtMesh Mesh;
    TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe> Reference;
    TArray<TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe>> Scans;
    TArray<FSensorRtHit> Expected;
    TArray<TArray<FSensorRtHit>> Actual;
    SensorOwnedRayTracing::FSubmissionStats Stable;
    void Submit(bool Fresh)
    {
        if(Fresh)
        {
            FSensorRtRequest R;R.Meshes.Add(Mesh);R.bFreshResourcesForTests=true;
            for(const auto& Part:Rays)R.Rays.Append(Part);Reference=SensorOwnedRayTracing::Submit(MoveTemp(R));
        }
        Scans.Reset();Actual.SetNum(3);for(auto& A:Actual)A.Reset();
        for(int32 I=0;I<3;++I){FSensorRtRequest R;R.Meshes.Add(Mesh);R.Rays=Rays[I];R.OwnerToken=I+1;Scans.Add(SensorOwnedRayTracing::Submit(MoveTemp(R)));}
    }
public:
    explicit FBatchParity(FAutomationTestBase* T):Test(T){}
    bool Update()override
    {
        if(FPlatformTime::Seconds()-Started>40){Test->AddError(TEXT("batched ray parity timeout"));for(auto& S:Scans)if(S)S->Cancel();if(Reference)Reference->Cancel();return true;}
        if(Phase==0)
        {
            if(!Test->TestTrue(TEXT("actual HWRT required; no skip"),SensorOwnedRayTracing::IsAvailable()))return true;
            auto V=MakeShared<TArray<FVector3f>,ESPMode::ThreadSafe>();V->Append({FVector3f(100,-100,-100),FVector3f(100,100,-100),FVector3f(100,0,100)});
            auto N=MakeShared<TArray<FVector4f>,ESPMode::ThreadSafe>();N->Add(FVector4f(-1,0,0,0));Mesh.Revision=0x264000000001;Mesh.Vertices=V;Mesh.NormalsAndSurfaces=N;
            Rays.SetNum(3);for(int32 I=0;I<3;++I)for(int32 J=0;J<65536;++J)
                Rays[I].Add({FVector3f(I*10+J%7,I+(J%31)-15,(J/31)%31-15),J%17==0?20.f:1000.f,FVector3f(1,0,0),0});
            Submit(true);for(const auto& S:Scans)if(!Test->TestTrue(TEXT("all three admitted"),S.IsValid()))return true;
            if(!Test->TestTrue(TEXT("fresh reference admitted"),Reference.IsValid()))return true;Phase=1;return false;
        }
        FString Error;
        if(Reference&&Expected.IsEmpty()){if(!Reference->Poll(Expected,Error))return false;if(!Error.IsEmpty()){Test->AddError(Error);return true;}Reference.Reset();}
        for(int32 I=0;I<3;++I)if(Actual[I].IsEmpty())
        {if(!Scans[I]->Poll(Actual[I],Error))return false;if(!Error.IsEmpty()){Test->AddError(Error);return true;}}
        int32 Offset=0;
        for(int32 I=0;I<3;++I)
        {
            const int32 Count=Rays[I].Num();Test->TestEqual(TEXT("per-sensor result cardinality"),Actual[I].Num(),Count);
            if(Actual[I].Num()==Count&&Expected.Num()>=Offset+Count)
                Test->TestTrue(TEXT("batched persistent result bytes equal fresh independent dispatch"),FMemory::Memcmp(Actual[I].GetData(),Expected.GetData()+Offset,Count*sizeof(FSensorRtHit))==0);
            bool Plane=true;for(int32 J=0;J<Actual[I].Num();++J)
            {const auto& Ray=Rays[I][J];const float T=100-Ray.Origin.X;const float Want=Ray.MaxDistance<T?-1:T;Plane&=FMath::Abs(Actual[I][J].NormalAndDistance.W-Want)<.001f;}
            Test->TestTrue(TEXT("all65536 per-sensor varied analytic distances and misses"),Plane);
            Offset+=Count;SensorOwnedRayTracing::RecycleHitArray(MoveTemp(Actual[I]));
        }
        Scans.Reset();
        const auto Now=SensorOwnedRayTracing::GetSubmissionStats();
        if(Phase==1){Stable=Now;Test->TestTrue(TEXT("three requests shared a dispatch"),Now.Dispatches<Now.Requests);Phase=2;}
        else
        {
            Test->TestEqual(TEXT("steady packets persistent"),Now.PacketCreates,Stable.PacketCreates);
            Test->TestEqual(TEXT("steady SRV/UAV persistent"),Now.ViewCreates,Stable.ViewCreates);
            Test->TestEqual(TEXT("identical rays not reuploaded"),Now.RayUploads,Stable.RayUploads);
            // The geometry result must remain identical, but UE5.3's scene and
            // its residency snapshot are single-frame. Packet/view/ray-upload
            // reuse remains valid; scene/SBT counts must not enforce an unsafe
            // cross-frame scene lifetime.
            if(SensorOwnedRayTracing::UsesNativeBackend())
            {
                Test->TestTrue(TEXT("native compute actually dispatched"),Now.NativeDispatches>Stable.NativeDispatches);
                Test->TestEqual(TEXT("native TLAS retained safely across frames"),Now.NativeSceneBuilds,Stable.NativeSceneBuilds);
                Test->TestEqual(TEXT("native BLAS retained safely across frames"),Now.NativeBlasBuilds,Stable.NativeBlasBuilds);
                Test->TestEqual(TEXT("native live bytes bounded and stable"),Now.NativeBytes,Stable.NativeBytes);
            }
            else Test->TestTrue(TEXT("single-frame scene refreshed"),Now.SceneBuilds>Stable.SceneBuilds);
            if(++Repeats==3)return true;
        }
        Submit(false);return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorBatchParityTest,"MA0T10.GpuLidar.BatchReuseByteParityRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorBatchParityTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FBatchParity(this));return true;}
#endif
