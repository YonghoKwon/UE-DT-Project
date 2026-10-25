#include "SensorOwnedRayTracing.h"
#include "MA0T10MemoryTags.h"
#include "Memory26Diagnostics.h"
#include "GlobalShader.h"
#include "BuiltInRayTracingShaders.h"
#include "RayTracingPayloadType.h"
#include "ShaderParameterStruct.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderCore.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RHIUtilities.h"
#include "PipelineStateCache.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreDelegates.h"
#include "HAL/IConsoleManager.h"
#include <atomic>

namespace
{
TAutoConsoleVariable<int32> EnableOwnedLidar(TEXT("ma0t10.Lidar.HardwareRayTracing"),1,TEXT("Enable sensor-owned HWRT; zero retains CPU fallback."));
std::atomic<int32> ActiveScans{0};
std::atomic<uint64> BlasBuildCount{0};
struct FHitScratch{TArray<FSensorRtHit> Data;uint64 Bytes()const{return Data.GetAllocatedSize();}};
Memory26::TPool<FHitScratch> HitPool;
constexpr int32 MaxActiveScans=4;
FDelegateHandle EndFrameHandle,SubmitFrameHandle;
bool Optimized()
{static const bool Value=!FParse::Param(FCommandLine::Get(),TEXT("SensorRtLegacy"));return Value;}
struct FStats
{std::atomic<uint64> Batches{0},Dispatches{0},Requests{0},PacketCreates{0},PacketReuses{0},RayUploads{0},ViewCreates{0},SceneBuilds{0},BindingUpdates{0};} RtSubmissionCounters;
#if RHI_RAYTRACING
class FLidarRGS final : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FLidarRGS);
    SHADER_USE_ROOT_PARAMETER_STRUCT(FLidarRGS,FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_SRV(RaytracingAccelerationStructure,LidarTLAS)
        SHADER_PARAMETER_SRV(StructuredBuffer<FSensorRtRay>,LidarRays)
        SHADER_PARAMETER_SRV(StructuredBuffer<float4>,Surface0)
        SHADER_PARAMETER_SRV(StructuredBuffer<float4>,Surface1)
        SHADER_PARAMETER_SRV(StructuredBuffer<float4>,Surface2)
        SHADER_PARAMETER_SRV(StructuredBuffer<float4>,Surface3)
        SHADER_PARAMETER_UAV(RWStructuredBuffer<FSensorRtHit>,LidarHits)
        SHADER_PARAMETER(FMatrix44f,Normal0)
        SHADER_PARAMETER(FMatrix44f,Normal1)
        SHADER_PARAMETER(FMatrix44f,Normal2)
        SHADER_PARAMETER(FMatrix44f,Normal3)
        SHADER_PARAMETER(uint32,RayCount)
    END_SHADER_PARAMETER_STRUCT()
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P)
    {return IsRayTracingEnabledForProject(P.Platform)&&RHISupportsRayTracingShaders(P.Platform);}
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& P,FShaderCompilerEnvironment& E)
    {FGlobalShader::ModifyCompilationEnvironment(P,E);}
    static ERayTracingPayloadType GetRayTracingPayloadType(int32){return ERayTracingPayloadType::Default;}
};
IMPLEMENT_GLOBAL_SHADER(FLidarRGS,"/MA0T10/Private/SensorOwnedRayTracing.usf","MainRGS",SF_RayGen);
class FLidarCHS final:public FBuiltInRayTracingShader
{
    DECLARE_GLOBAL_SHADER(FLidarCHS);
public:
    FLidarCHS()=default;
    FLidarCHS(const ShaderMetaType::CompiledShaderInitializerType& Init):FBuiltInRayTracingShader(Init){}
    static ERayTracingPayloadType GetRayTracingPayloadType(int32){return ERayTracingPayloadType::Default;}
};
IMPLEMENT_GLOBAL_SHADER(FLidarCHS,"/MA0T10/Private/SensorOwnedRayTracing.usf","MainCHS",SF_RayHitGroup);

struct FBlas
{
    uint64 Revision=0,LastUse=0;
    FBufferRHIRef Vertices,Attributes;
    FShaderResourceViewRHIRef AttributeView;
    FRayTracingGeometryRHIRef Geometry;
};
TMap<uint64,TSharedPtr<FBlas,ESPMode::ThreadSafe>> BlasCache; // render thread only; bounded LRU
uint64 CacheClock=0;

template<typename T>
FBufferRHIRef Upload(FRHICommandListImmediate& Cmd,const TCHAR* Name,const TArray<T>& Data,EBufferUsageFlags Flags)
{
    MA0T10_LLM_SCOPE_BYTAG(MA0T10_LidarRtScene);
    TResourceArray<T> Resource;Resource.Append(Data);
    FRHIResourceCreateInfo Info(Name);Info.ResourceArray=&Resource;
    return Cmd.CreateStructuredBuffer(sizeof(T),Resource.GetResourceDataSize(),Flags|BUF_ShaderResource,Info);
}
TSharedPtr<FBlas,ESPMode::ThreadSafe> GetBlas(FRHICommandListImmediate& Cmd,const FSensorRtMesh& Mesh)
{
    MA0T10_LLM_SCOPE_BYTAG(MA0T10_LidarRtScene);
    if(auto* Existing=BlasCache.Find(Mesh.Revision)){(*Existing)->LastUse=++CacheClock;return *Existing;}
    while(BlasCache.Num()>=8){uint64 OldKey=0,OldUse=MAX_uint64;for(const auto& Entry:BlasCache)if(Entry.Value->LastUse<OldUse){OldKey=Entry.Key;OldUse=Entry.Value->LastUse;}BlasCache.Remove(OldKey);}
    auto B=MakeShared<FBlas,ESPMode::ThreadSafe>();B->Revision=Mesh.Revision;B->LastUse=++CacheClock;
    // Triangle-list float3 positions: same immutable triangles as CPU BVH.
    TResourceArray<FVector3f> Positions;Positions.Append(*Mesh.Vertices);
    FRHIResourceCreateInfo Info(TEXT("SensorRtVertices"));Info.ResourceArray=&Positions;
    B->Vertices=Cmd.CreateVertexBuffer(Positions.GetResourceDataSize(),BUF_Static|BUF_ShaderResource,Info);
    B->Attributes=Upload(Cmd,TEXT("SensorRtSurfaces"),*Mesh.NormalsAndSurfaces,BUF_Static);
    B->AttributeView=Cmd.CreateShaderResourceView(B->Attributes);
    FRayTracingGeometryInitializer Init;Init.DebugName=TEXT("SensorOwnedCollision");Init.GeometryType=RTGT_Triangles;
    Init.TotalPrimitiveCount=Mesh.Vertices->Num()/3;Init.bFastBuild=true;Init.bAllowUpdate=false;
    FRayTracingGeometrySegment Segment;Segment.VertexBuffer=B->Vertices;Segment.VertexBufferStride=sizeof(FVector3f);
    Segment.MaxVertices=Mesh.Vertices->Num();Segment.NumPrimitives=Init.TotalPrimitiveCount;Init.Segments.Add(Segment);
    B->Geometry=Cmd.CreateRayTracingGeometry(Init);Cmd.BuildAccelerationStructure(B->Geometry);++BlasBuildCount;
    Memory26::Count(Memory26::ECounter::BlasBuild);
    Cmd.Transition(FRHITransitionInfo(B->Geometry.GetReference(),ERHIAccess::BVHWrite,ERHIAccess::BVHRead));
    BlasCache.Add(Mesh.Revision,B);return B;
}

struct FSceneResources
{
    TArray<TSharedPtr<FBlas,ESPMode::ThreadSafe>> Blases;
    TArray<uint64> Revisions;
    TArray<int32> MeshIndices;
    TArray<FTransform> Poses;
    FRayTracingSceneRHIRef Scene;
    FBufferRHIRef SceneMemory,Scratch,InstanceUpload,TransformUpload;
    FShaderResourceViewRHIRef SceneView,InstanceUploadView,TransformUploadView;
    FRWBufferStructured Instances;
    FByteAddressBuffer Addresses;
    uint64 LastUse=0;
    uint64 RenderFrame=0;
    FRayTracingPipelineState* BoundPipeline=nullptr;
    double LastPipelineUseSeconds=0;
    bool Matches(const TArray<FSensorRtMesh>& Meshes)const
    {
        if(Meshes.Num()!=Revisions.Num())return false;
        for(int32 I=0;I<Meshes.Num();++I)if(Meshes[I].Revision!=Revisions[I]||!Meshes[I].LocalToWorld.Equals(Poses[I],0))return false;
        return true;
    }
};
TArray<TSharedPtr<FSceneResources,ESPMode::ThreadSafe>> SceneCache; // immutable key, bounded4, RT only
uint64 SceneClock=0;
struct FDispatchPacket
{
    uint64 OwnerToken=0;
    int32 RayCount=0;
    bool Used=false;
    TArray<FSensorRtRay> LastRays;
    FBufferRHIRef RayBuffer,ResultBuffer;
    FShaderResourceViewRHIRef RayView;
    FUnorderedAccessViewRHIRef ResultView;
    TUniquePtr<FRHIGPUBufferReadback> Readback;
    uint64 Bytes()const{return uint64(RayCount)*(sizeof(FSensorRtRay)+2*sizeof(FSensorRtHit))+LastRays.GetAllocatedSize();}
};
TArray<TUniquePtr<FDispatchPacket>> FreePackets; // RT only; no live/in-flight packet can enter here
uint64 FreePacketBytes=0;
constexpr uint64 MaxFreePacketBytes=64ull*1024*1024;
TUniquePtr<FDispatchPacket> TakePacket(int32 Count,uint64 Owner,bool Reuse)
{
    if(Reuse)
    {
        int32 Found=INDEX_NONE;
        for(int32 I=0;I<FreePackets.Num();++I)if(FreePackets[I]->RayCount==Count)
        {Found=I;if(FreePackets[I]->OwnerToken==Owner)break;}
        if(Found!=INDEX_NONE){auto P=MoveTemp(FreePackets[Found]);FreePackets.RemoveAtSwap(Found);FreePacketBytes-=P->Bytes();P->OwnerToken=Owner;++RtSubmissionCounters.PacketReuses;Memory26::Count(Memory26::ECounter::ReadbackReuse);return P;}
    }
    auto P=MakeUnique<FDispatchPacket>();P->RayCount=Count;P->OwnerToken=Owner;++RtSubmissionCounters.PacketCreates;Memory26::Count(Memory26::ECounter::ReadbackCreate);return P;
}
void ReturnPacket(TUniquePtr<FDispatchPacket> P)
{
    // Called only after the latest copy fence is ready and every Lock is unlocked.
    if(P&&FreePackets.Num()<4&&P->Bytes()<=MaxFreePacketBytes-FreePacketBytes)
    {FreePacketBytes+=P->Bytes();FreePackets.Add(MoveTemp(P));}
}
#endif
class FScan;
FCriticalSection SubmissionMutex;
TArray<TSharedPtr<FScan,ESPMode::ThreadSafe>> Submissions; // admission already bounded to4
TArray<TSharedPtr<FScan,ESPMode::ThreadSafe>> RetiredScans; // RT ownership until fence ready
TArray<TSharedPtr<FScan,ESPMode::ThreadSafe>> PendingReadbacks; // Same bounded scans; RT-only references
class FScan final : public ISensorRtScan, public TSharedFromThis<FScan,ESPMode::ThreadSafe>
{
public:
    FSensorRtRequest Request;
    bool bAutomaticReadback=false,bBatchMember=false,bCounted=true;
    TArray<TSharedPtr<FScan,ESPMode::ThreadSafe>> Members;
    int32 RayCount()const
    {int32 N=Request.Rays.Num();for(const auto& M:Members)N+=M->Request.Rays.Num();return N;}
    bool Reusable()const
    {
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        if(Request.bFreshResourcesForTests)return false;
#endif
        return Optimized();
    }
    FCriticalSection Mutex;
    TArray<FSensorRtHit> Hits;
    FString Failure;
    std::atomic<bool> Finished{false},Cancelled{false},PollQueued{false};
#if RHI_RAYTRACING
    TUniquePtr<FDispatchPacket> Packet;
    TSharedPtr<FSceneResources,ESPMode::ThreadSafe> Resources;
#endif
    ~FScan(){if(bCounted)--ActiveScans;}
    void Cancel() override
    {
        if(Cancelled.exchange(true)||Finished.load(std::memory_order_acquire))return;
        if(bBatchMember)return; // queued batch owns the request until its terminal GPU fence
        auto Self=AsShared();
        ENQUEUE_RENDER_COMMAND(RetireSensorRt)([Self](FRHICommandListImmediate& Cmd)
        {Self->Request.OnReadback=nullptr;RetiredScans.AddUnique(Self);});
    }
    bool CanRetire() const
    {
#if RHI_RAYTRACING
        return Finished.load(std::memory_order_acquire)||(Packet&&Packet->Readback&&Packet->Readback->IsReady());
#else
        return true;
#endif
    }
    void Complete()
    {
        auto Notify=MoveTemp(Request.OnReadback);
        if(Notify&&!Cancelled.load(std::memory_order_acquire))Notify(MoveTemp(Hits),Failure);
        Finished.store(true,std::memory_order_release);
    }
    void Fail(const TCHAR* Reason){Failure=Reason;Complete();}
    void PumpReadback()
    {
        MA0T10_LLM_SCOPE_BYTAG(MA0T10_LidarReadback);
#if RHI_RAYTRACING
        if(Finished.load(std::memory_order_acquire)||!Packet||!Packet->Readback||!Packet->Readback->IsReady())return;
        auto& Readback=Packet->Readback;
        const uint32 Bytes=RayCount()*sizeof(FSensorRtHit);
        if(!Cancelled.load(std::memory_order_acquire))
        {
            const void* Data=Readback->Lock(Bytes);
            if(Data)
            {
                if(Reusable()){auto Lease=HitPool.Take(Memory26::ECounter::HitCreate,Memory26::ECounter::HitReuse);Hits=MoveTemp(Lease->Data);}
                else Memory26::Count(Memory26::ECounter::HitCreate);
                Hits.SetNumUninitialized(RayCount());FMemory::Memcpy(Hits.GetData(),Data,Bytes);Readback->Unlock();
            }
            else Failure=TEXT("GPU readback lock failed");
        }
        if(Reusable())ReturnPacket(MoveTemp(Packet));
        Complete();
#endif
    }
    bool Poll(TArray<FSensorRtHit>& Out,FString& Error) override
    {
        if(Finished.load(std::memory_order_acquire)){Out=MoveTemp(Hits);Error=Failure;return true;}
#if RHI_RAYTRACING
        if(!bBatchMember&&!bAutomaticReadback&&!PollQueued.exchange(true))
        {
            auto Self=AsShared();
            ENQUEUE_RENDER_COMMAND(PollSensorRt)([Self](FRHICommandListImmediate& Cmd)
            {Self->PumpReadback();Self->PollQueued.store(false,std::memory_order_release);});
        }
#endif
        return false;
    }
    void Dispatch(FRHICommandListImmediate& Cmd)
    {
        MA0T10_LLM_SCOPE_BYTAG(MA0T10_LidarRtScene);
#if RHI_RAYTRACING
        if(Cancelled.load()){Fail(TEXT("cancelled before GPU submission"));return;}
        if(!SensorOwnedRayTracing::IsAvailable()){Fail(TEXT("HWRT pipeline unavailable"));return;}
        // UE5.3 D3D12 supports RTSL_SingleFrame only. A scene captures raw BLAS
        // residency handles at build time; never reuse that snapshot in a later
        // render frame, even when logical mesh revision and pose still match.
        if(Reusable())for(auto& Cached:SceneCache)if(Cached->RenderFrame==GFrameNumberRenderThread&&Cached->Matches(Request.Meshes))
        {Resources=Cached;Resources->LastUse=++SceneClock;Memory26::Count(Memory26::ECounter::SceneReuse);break;}
        if(!Resources)
        {
        Resources=MakeShared<FSceneResources,ESPMode::ThreadSafe>();
        Resources->RenderFrame=GFrameNumberRenderThread;
        for(const auto& Mesh:Request.Meshes){Resources->Revisions.Add(Mesh.Revision);Resources->Poses.Add(Mesh.LocalToWorld);}
        auto& Blases=Resources->Blases;auto& Scene=Resources->Scene;auto& SceneMemory=Resources->SceneMemory;auto& Scratch=Resources->Scratch;
        auto& Instances=Resources->Instances;auto& Addresses=Resources->Addresses;auto& SceneView=Resources->SceneView;
        auto& InstanceUpload=Resources->InstanceUpload;auto& InstanceUploadView=Resources->InstanceUploadView;
        auto& TransformUpload=Resources->TransformUpload;auto& TransformUploadView=Resources->TransformUploadView;
        TArray<FRayTracingGeometryInstance> Descriptors;TArray<FMatrix> Poses;for(const auto& M:Request.Meshes)Poses.Add(M.LocalToWorld.ToMatrixWithScale());
        for(int32 I=0;I<Request.Meshes.Num();++I)
        {
            const auto& Mesh=Request.Meshes[I];if(!Mesh.Vertices||Mesh.Vertices->IsEmpty())continue;
            auto B=GetBlas(Cmd,Mesh);Blases.Add(B);Resources->MeshIndices.Add(I);
            FRayTracingGeometryInstance D;D.GeometryRHI=B->Geometry;D.NumTransforms=1;D.Transforms=MakeArrayView(&Poses[I],1);D.DefaultUserData=I;D.Mask=0xff;Descriptors.Add(D);
        }
        if(Descriptors.IsEmpty()){Fail(TEXT("empty collision TLAS"));return;}
        auto Built=CreateRayTracingSceneWithGeometryInstances(Descriptors,1,1,1);Scene=Built.Scene;
        const auto& Init=Scene->GetInitializer();const uint32 Count=Init.NumNativeInstancesPerLayer[0];
        const auto Sizes=RHICalcRayTracingSceneSize(Count,ERayTracingAccelerationStructureFlags::FastTrace);
        FRHIResourceCreateInfo SceneInfo(TEXT("SensorOwnedTLAS")),ScratchInfo(TEXT("SensorOwnedTLASScratch"));
        SceneMemory=Cmd.CreateBuffer(Sizes.ResultSize,BUF_AccelerationStructure,0,ERHIAccess::BVHWrite,SceneInfo);
        Scratch=Cmd.CreateBuffer(Sizes.BuildScratchSize,BUF_UnorderedAccess,GRHIRayTracingScratchBufferAlignment,ERHIAccess::UAVCompute,ScratchInfo);
        Instances.Initialize(Cmd,TEXT("SensorOwnedInstances"),GRHIRayTracingInstanceDescriptorSize,Count);
        TArray<FRayTracingInstanceDescriptorInput> Inputs;Inputs.SetNumUninitialized(Count);
        TArray<FVector4f> Transforms;Transforms.SetNumUninitialized(Built.NumNativeCPUInstances*3);
        FillRayTracingInstanceUploadBuffer(Scene,FVector::ZeroVector,Descriptors,Built.InstanceGeometryIndices,Built.BaseUploadBufferOffsets,
            Built.NumNativeGPUSceneInstances,Built.NumNativeCPUInstances,Inputs,Transforms);
        InstanceUpload=Upload(Cmd,TEXT("SensorRtInstanceUpload"),Inputs,BUF_Volatile);InstanceUploadView=Cmd.CreateShaderResourceView(InstanceUpload);
        TransformUpload=Upload(Cmd,TEXT("SensorRtTransforms"),Transforms,BUF_Volatile);TransformUploadView=Cmd.CreateShaderResourceView(TransformUpload);
        Addresses.Initialize(Cmd,TEXT("SensorRtAddresses"),Init.ReferencedGeometries.Num()*sizeof(FRayTracingAccelerationStructureAddress),BUF_Volatile);
        auto AddressBuffer=Addresses.Buffer;auto Geometries=Init.ReferencedGeometries;
        Cmd.EnqueueLambda([AddressBuffer,Geometries](FRHICommandListImmediate& Rhi)
        {
            MA0T10_LLM_SCOPE_BYTAG(MA0T10_LidarRtScene);
            auto* Data=static_cast<FRayTracingAccelerationStructureAddress*>(Rhi.LockBuffer(AddressBuffer,0,Geometries.Num()*sizeof(FRayTracingAccelerationStructureAddress),RLM_WriteOnly));
            for(int I=0;I<Geometries.Num();++I)Data[I]=Geometries[I]->GetAccelerationStructureAddress(Rhi.GetGPUMask().ToIndex());
            Rhi.UnlockBuffer(AddressBuffer);
        });
        BuildRayTracingInstanceBuffer(Cmd,nullptr,FVector3f::ZeroVector,FVector3f::ZeroVector,Instances.UAV,InstanceUploadView,Addresses.SRV,TransformUploadView,
            Built.NumNativeGPUSceneInstances,Built.NumNativeCPUInstances,{},nullptr);
        Cmd.Transition(FRHITransitionInfo(Instances.Buffer,ERHIAccess::UAVCompute,ERHIAccess::SRVCompute));
        Cmd.BindAccelerationStructureMemory(Scene,SceneMemory,0);
        FRayTracingSceneBuildParams Build;Build.Scene=Scene;Build.ScratchBuffer=Scratch;Build.InstanceBuffer=Instances.Buffer;
        Cmd.BuildAccelerationStructure(Build);Cmd.Transition(FRHITransitionInfo(Scene.GetReference(),ERHIAccess::BVHWrite,ERHIAccess::BVHRead));
        SceneView=Cmd.CreateShaderResourceView(FShaderResourceViewInitializer(SceneMemory,Scene->GetLayerBufferOffset(0),0));
        Memory26::Count(Memory26::ECounter::SceneBuild);++RtSubmissionCounters.SceneBuilds;
        if(Reusable())
        {
            // Exact immutable geometry+pose key. Eviction never invalidates a
            // live scan: it holds shared ownership through its terminal fence.
            if(SceneCache.Num()>=4){int32 Old=0;for(int32 I=1;I<SceneCache.Num();++I)if(SceneCache[I]->LastUse<SceneCache[Old]->LastUse)Old=I;SceneCache.RemoveAt(Old);}
            Resources->LastUse=++SceneClock;SceneCache.Add(Resources);
        }
        }
        auto& Scene=Resources->Scene;auto& SceneView=Resources->SceneView;auto& Blases=Resources->Blases;
        const int32 Count=RayCount();
        Packet=TakePacket(Count,Request.OwnerToken,Reusable());
        bool Same=Packet->LastRays.Num()==Count;int32 Offset=0;
        auto Compare=[&](const TArray<FSensorRtRay>& Rays)
        {if(Same&&FMemory::Memcmp(Packet->LastRays.GetData()+Offset,Rays.GetData(),Rays.Num()*sizeof(FSensorRtRay)))Same=false;Offset+=Rays.Num();};
        if(Members.IsEmpty())Compare(Request.Rays);else for(const auto& M:Members)Compare(M->Request.Rays);
        if(!Same)
        {
            Packet->LastRays.SetNumUninitialized(Count);Offset=0;
            auto Copy=[&](const TArray<FSensorRtRay>& Rays){FMemory::Memcpy(Packet->LastRays.GetData()+Offset,Rays.GetData(),Rays.Num()*sizeof(FSensorRtRay));Offset+=Rays.Num();};
            if(Members.IsEmpty())Copy(Request.Rays);else for(const auto& M:Members)Copy(M->Request.Rays);
            if(!Packet->RayBuffer.IsValid())
            {Packet->RayBuffer=Upload(Cmd,TEXT("SensorRtRays"),Packet->LastRays,BUF_Static);Packet->RayView=Cmd.CreateShaderResourceView(Packet->RayBuffer);++RtSubmissionCounters.ViewCreates;}
            else
            {void* Dest=Cmd.LockBuffer(Packet->RayBuffer,0,Count*sizeof(FSensorRtRay),RLM_WriteOnly);FMemory::Memcpy(Dest,Packet->LastRays.GetData(),Count*sizeof(FSensorRtRay));Cmd.UnlockBuffer(Packet->RayBuffer);}
            ++RtSubmissionCounters.RayUploads;
        }
        if(!Packet->ResultBuffer.IsValid())
        {
            FRHIResourceCreateInfo Info(TEXT("SensorRtResults"));
            Packet->ResultBuffer=Cmd.CreateStructuredBuffer(sizeof(FSensorRtHit),Count*sizeof(FSensorRtHit),BUF_UnorderedAccess|BUF_ShaderResource,Info);
            Packet->ResultView=Cmd.CreateUnorderedAccessView(Packet->ResultBuffer,false,false);++RtSubmissionCounters.ViewCreates;
        }
        else Cmd.Transition(FRHITransitionInfo(Packet->ResultBuffer,ERHIAccess::CopySrc,ERHIAccess::UAVCompute));
        auto& RayView=Packet->RayView;auto& ResultView=Packet->ResultView;auto& ResultBuffer=Packet->ResultBuffer;
        FLidarRGS::FParameters P;P.LidarTLAS=SceneView;P.LidarRays=RayView;P.LidarHits=ResultView;P.RayCount=Count;
        FShaderResourceViewRHIRef Views[4]={Blases[0]->AttributeView,Blases[0]->AttributeView,Blases[0]->AttributeView,Blases[0]->AttributeView};
        for(int I=0;I<Resources->MeshIndices.Num();++I)Views[Resources->MeshIndices[I]]=Blases[I]->AttributeView;
        P.Surface0=Views[0];P.Surface1=Views[1];P.Surface2=Views[2];P.Surface3=Views[3];
        P.Normal0=FMatrix44f(Request.Meshes[0].LocalToWorld.ToInverseMatrixWithScale().GetTransposed());
        P.Normal1=Request.Meshes.Num()>1?FMatrix44f(Request.Meshes[1].LocalToWorld.ToInverseMatrixWithScale().GetTransposed()):FMatrix44f::Identity;
        P.Normal2=Request.Meshes.Num()>2?FMatrix44f(Request.Meshes[2].LocalToWorld.ToInverseMatrixWithScale().GetTransposed()):FMatrix44f::Identity;
        P.Normal3=Request.Meshes.Num()>3?FMatrix44f(Request.Meshes[3].LocalToWorld.ToInverseMatrixWithScale().GetTransposed()):FMatrix44f::Identity;
        auto Shader=GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FLidarRGS>();
        auto HitShader=GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FLidarCHS>();
        auto MissShader=GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FDefaultPayloadMS>();
        FRayTracingPipelineStateInitializer PipelineInit;PipelineInit.MaxPayloadSizeInBytes=24;PipelineInit.bAllowHitGroupIndexing=false;
        FRHIRayTracingShader* RayGenTable[]={Shader.GetRayTracingShader()};PipelineInit.SetRayGenShaderTable(RayGenTable);
        FRHIRayTracingShader* HitTable[]={HitShader.GetRayTracingShader()};PipelineInit.SetHitGroupTable(HitTable);
        FRHIRayTracingShader* MissTable[]={MissShader.GetRayTracingShader()};PipelineInit.SetMissShaderTable(MissTable);
        auto* Pipeline=PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Cmd,PipelineInit);
        const double PipelineNow=FPlatformTime::Seconds();
        // Local hit/miss records have no per-frame uniforms or loose data. The
        // exact scene key owns their immutable geometry; global raygen bindings
        // below still carry this dispatch's ray/result buffers. Rebind on a new
        // PSO or after an idle gap (the engine may evict inactive PSOs).
        if(!Reusable()||Resources->BoundPipeline!=Pipeline||PipelineNow-Resources->LastPipelineUseSeconds>1)
        {
            Cmd.SetRayTracingHitGroup(Scene,0,0,0,Pipeline,0,0,nullptr,0,nullptr,0);
            Cmd.SetRayTracingMissShader(Scene,0,Pipeline,0,0,nullptr,0);
            Resources->BoundPipeline=Pipeline;++RtSubmissionCounters.BindingUpdates;
        }
        Resources->LastPipelineUseSeconds=PipelineNow;
        FRayTracingShaderBindingsWriter Bindings;SetShaderParameters(Bindings,Shader,P);
        Cmd.RayTraceDispatch(Pipeline,Shader.GetRayTracingShader(),Scene,Bindings,Count,1);++RtSubmissionCounters.Dispatches;
        Cmd.Transition(FRHITransitionInfo(ResultBuffer,ERHIAccess::UAVCompute,ERHIAccess::CopySrc));
        {
            MA0T10_LLM_SCOPE_BYTAG(MA0T10_LidarReadback);
            if(!Packet->Readback)Packet->Readback=MakeUnique<FRHIGPUBufferReadback>(TEXT("SensorOwnedLidarBatch"));
            Packet->Readback->EnqueueCopy(Cmd,ResultBuffer,Count*sizeof(FSensorRtHit));Packet->Used=true;
        }
#else
        Fail(TEXT("RHI compiled without ray tracing"));
#endif
    }
};
bool SameScene(const FSensorRtRequest& A,const FSensorRtRequest& B)
{
    if(A.Meshes.Num()!=B.Meshes.Num())return false;
    for(int32 I=0;I<A.Meshes.Num();++I)if(A.Meshes[I].Revision!=B.Meshes[I].Revision||!A.Meshes[I].LocalToWorld.Equals(B.Meshes[I].LocalToWorld,0))return false;
    return true;
}
void FlushSubmissions()
{
    TArray<TSharedPtr<FScan,ESPMode::ThreadSafe>> Pending;
    {FScopeLock Lock(&SubmissionMutex);Swap(Pending,Submissions);}
    if(Pending.IsEmpty())return;
    ENQUEUE_RENDER_COMMAND(SensorLidarBatch)([Pending=MoveTemp(Pending)](FRHICommandListImmediate& Cmd) mutable
    {
        ++RtSubmissionCounters.Batches;
        while(!Pending.IsEmpty())
        {
            auto First=Pending[0];Pending.RemoveAt(0);
            if(First->Cancelled.load()){First->Complete();continue;}
            TArray<TSharedPtr<FScan,ESPMode::ThreadSafe>> Group;Group.Add(First);
            if(First->Reusable())for(int32 I=0;I<Pending.Num();)
            {
                if(Pending[I]->Reusable()&&SameScene(First->Request,Pending[I]->Request))
                {Group.Add(Pending[I]);Pending.RemoveAt(I);}else ++I;
            }
            TSharedPtr<FScan,ESPMode::ThreadSafe> Work=First;
            if(Group.Num()>1)
            {
                Work=MakeShared<FScan,ESPMode::ThreadSafe>();Work->bCounted=false;Work->Members=Group;
                Work->Request.Meshes=First->Request.Meshes;Work->Request.OwnerToken=First->Request.OwnerToken;
                for(const auto& M:Group)Work->Request.OwnerToken=HashCombineFast(Work->Request.OwnerToken,M->Request.OwnerToken);
                Work->Request.OnReadback=[Group](TArray<FSensorRtHit>&& Hits,const FString& Error)
                {
                    int32 Offset=0;
                    for(const auto& M:Group)
                    {
                        const int32 Count=M->Request.Rays.Num();M->Failure=Error;
                        if(Error.IsEmpty()&&!M->Cancelled.load())
                        {
                            auto Lease=HitPool.Take(Memory26::ECounter::HitCreate,Memory26::ECounter::HitReuse);M->Hits=MoveTemp(Lease->Data);
                            M->Hits.SetNumUninitialized(Count);FMemory::Memcpy(M->Hits.GetData(),Hits.GetData()+Offset,Count*sizeof(FSensorRtHit));
                        }
                        Offset+=Count;M->Complete();
                    }
                    SensorOwnedRayTracing::RecycleHitArray(MoveTemp(Hits));
                };
            }
            Work->bAutomaticReadback=true;Work->Dispatch(Cmd);
            if(!Work->Finished.load(std::memory_order_acquire))PendingReadbacks.Add(Work);
        }
    });
}
}
SensorOwnedRayTracing::FSubmissionStats SensorOwnedRayTracing::GetSubmissionStats()
{return {RtSubmissionCounters.Batches.load(),RtSubmissionCounters.Dispatches.load(),RtSubmissionCounters.Requests.load(),RtSubmissionCounters.PacketCreates.load(),RtSubmissionCounters.PacketReuses.load(),RtSubmissionCounters.RayUploads.load(),RtSubmissionCounters.ViewCreates.load(),RtSubmissionCounters.SceneBuilds.load(),RtSubmissionCounters.BindingUpdates.load()};}
void SensorOwnedRayTracing::FlushPendingSubmissions(){FlushSubmissions();}
uint64 SensorOwnedRayTracing::GetBlasBuildCount(){return BlasBuildCount.load();}
void SensorOwnedRayTracing::RecycleHitArray(TArray<FSensorRtHit>&& Hits)
{
    if(!Optimized())return;
    MA0T10_LLM_SCOPE_BYTAG(MA0T10_LidarReadback);
    auto Scratch=MakeUnique<FHitScratch>();Scratch->Data=MoveTemp(Hits);Scratch->Data.Reset();HitPool.Return(MoveTemp(Scratch));
}
bool SensorOwnedRayTracing::IsAvailable()
{
#if RHI_RAYTRACING
    return EnableOwnedLidar.GetValueOnAnyThread()!=0&&GIsRHIInitialized&&IsRayTracingEnabled()&&GRHISupportsRayTracingShaders&&GNumExplicitGPUsForRendering==1;
#else
    return false;
#endif
}
TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe> SensorOwnedRayTracing::Submit(FSensorRtRequest&& Request)
{
    MA0T10_LLM_SCOPE_BYTAG(MA0T10_LidarRtScene);
    if(!IsAvailable()||Request.Rays.IsEmpty()||Request.Rays.Num()>1048576||Request.Meshes.IsEmpty()||Request.Meshes.Num()>4)return {};
    uint64 GeometryBytes=0;
    for(const auto& Mesh:Request.Meshes)if(Mesh.Vertices&&!Mesh.Vertices->IsEmpty())
    {
        if(!Mesh.Revision||Mesh.Vertices->Num()%3||!Mesh.NormalsAndSurfaces||Mesh.NormalsAndSurfaces->Num()!=Mesh.Vertices->Num()/3)return {};
        GeometryBytes+=uint64(Mesh.Vertices->Num())*sizeof(FVector3f)+uint64(Mesh.NormalsAndSurfaces->Num())*sizeof(FVector4f);
        if(GeometryBytes>256ull*1024*1024)return {};
    }
    if(ActiveScans.fetch_add(1)>=MaxActiveScans){--ActiveScans;return {};}
    auto Scan=MakeShared<FScan,ESPMode::ThreadSafe>();Scan->bAutomaticReadback=bool(Request.OnReadback);Scan->Request=MoveTemp(Request);
    Scan->bBatchMember=true;++RtSubmissionCounters.Requests;
    {FScopeLock Lock(&SubmissionMutex);Submissions.Add(Scan);}
    return Scan;
}
void SensorOwnedRayTracing::RegisterShaders()
{
    AddShaderSourceDirectoryMapping(TEXT("/MA0T10"),FPaths::Combine(FPaths::ProjectDir(),TEXT("Shaders")));
    SubmitFrameHandle=FCoreDelegates::OnEndFrame.AddStatic(&FlushSubmissions);
    EndFrameHandle=FCoreDelegates::OnEndFrameRT.AddLambda([]()
    {
        for(int I=PendingReadbacks.Num()-1;I>=0;--I)
        {
            PendingReadbacks[I]->PumpReadback();
            if(PendingReadbacks[I]->Finished.load(std::memory_order_acquire))PendingReadbacks.RemoveAtSwap(I);
        }
        for(int I=RetiredScans.Num()-1;I>=0;--I)if(RetiredScans[I]->CanRetire())RetiredScans.RemoveAtSwap(I);
    });
}

void SensorOwnedRayTracing::Shutdown()
{
    FCoreDelegates::OnEndFrame.Remove(SubmitFrameHandle);
    {FScopeLock Lock(&SubmissionMutex);for(const auto& Scan:Submissions){Scan->Cancelled.store(true);Scan->Request.OnReadback=nullptr;}Submissions.Reset();}
    FCoreDelegates::OnEndFrameRT.Remove(EndFrameHandle);
    ENQUEUE_RENDER_COMMAND(ReleaseOwnedSensorRt)([](FRHICommandListImmediate& Cmd)
    {
        for(const auto& Scan:PendingReadbacks){Scan->Cancelled.store(true,std::memory_order_release);Scan->Request.OnReadback=nullptr;}
        PendingReadbacks.Reset();RetiredScans.Reset();
#if RHI_RAYTRACING
        SceneCache.Reset();FreePackets.Reset();FreePacketBytes=0;BlasCache.Reset();
#endif
    });
    // Module teardown only, never acquisition/polling. RHI resource deletion
    // retains the engine's deferred GPU lifetime; no GPU-idle wait is introduced.
    FlushRenderingCommands();
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && RHI_RAYTRACING
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemory26SceneKeyTest,"HATCH26.SceneReuseKey",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMemory26SceneKeyTest::RunTest(const FString&)
{
    FSceneResources Key;Key.Revisions.Add(42);Key.Poses.Add(FTransform::Identity);
    TArray<FSensorRtMesh> Meshes;Meshes.AddDefaulted();Meshes[0].Revision=42;
    TestTrue(TEXT("unchanged immutable geometry and pose matches"),Key.Matches(Meshes));
    Meshes[0].Revision=43;TestFalse(TEXT("revision invalidates"),Key.Matches(Meshes));Meshes[0].Revision=42;
    Meshes[0].LocalToWorld.SetTranslation(FVector(.000001,0,0));TestFalse(TEXT("even small pose change invalidates"),Key.Matches(Meshes));
    Meshes[0].LocalToWorld=FTransform::Identity;Meshes.AddDefaulted();TestFalse(TEXT("mesh count invalidates"),Key.Matches(Meshes));
    return true;
}
#endif
