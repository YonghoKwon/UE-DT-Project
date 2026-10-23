#include "SensorOwnedRayTracing.h"
#include "GlobalShader.h"
#include "BuiltInRayTracingShaders.h"
#include "RayTracingPayloadType.h"
#include "ShaderParameterStruct.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderCore.h"
#include "Misc/Paths.h"
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
constexpr int32 MaxActiveScans=4;
FDelegateHandle EndFrameHandle;
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
    TResourceArray<T> Resource;Resource.Append(Data);
    FRHIResourceCreateInfo Info(Name);Info.ResourceArray=&Resource;
    return Cmd.CreateStructuredBuffer(sizeof(T),Resource.GetResourceDataSize(),Flags|BUF_ShaderResource,Info);
}
TSharedPtr<FBlas,ESPMode::ThreadSafe> GetBlas(FRHICommandListImmediate& Cmd,const FSensorRtMesh& Mesh)
{
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
    Cmd.Transition(FRHITransitionInfo(B->Geometry.GetReference(),ERHIAccess::BVHWrite,ERHIAccess::BVHRead));
    BlasCache.Add(Mesh.Revision,B);return B;
}
#endif
class FScan;
TArray<TSharedPtr<FScan,ESPMode::ThreadSafe>> RetiredScans; // RT ownership until fence ready
class FScan final : public ISensorRtScan, public TSharedFromThis<FScan,ESPMode::ThreadSafe>
{
public:
    FSensorRtRequest Request;
    FCriticalSection Mutex;
    TArray<FSensorRtHit> Hits;
    FString Failure;
    std::atomic<bool> Finished{false},Cancelled{false},PollQueued{false};
#if RHI_RAYTRACING
    TUniquePtr<FRHIGPUBufferReadback> Readback;
    TArray<TSharedPtr<FBlas,ESPMode::ThreadSafe>> Blases;
    FRayTracingSceneRHIRef Scene;
    FBufferRHIRef SceneMemory,Scratch,InstanceUpload,TransformUpload,RayBuffer,ResultBuffer;
    FShaderResourceViewRHIRef SceneView,InstanceUploadView,TransformUploadView,RayView;
    FUnorderedAccessViewRHIRef ResultView;
    FRWBufferStructured Instances;
    FByteAddressBuffer Addresses;
#endif
    ~FScan(){--ActiveScans;}
    void Cancel() override
    {
        if(Cancelled.exchange(true)||Finished.load(std::memory_order_acquire))return;
        auto Self=AsShared();
        ENQUEUE_RENDER_COMMAND(RetireSensorRt)([Self](FRHICommandListImmediate& Cmd)
        {Self->Request.OnReadback=nullptr;RetiredScans.AddUnique(Self);});
    }
    bool CanRetire() const
    {
#if RHI_RAYTRACING
        return Finished.load(std::memory_order_acquire)||(Readback&&Readback->IsReady());
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
    bool Poll(TArray<FSensorRtHit>& Out,FString& Error) override
    {
        if(Finished.load(std::memory_order_acquire)){Out=MoveTemp(Hits);Error=Failure;return true;}
#if RHI_RAYTRACING
        if(!PollQueued.exchange(true))
        {
            auto Self=AsShared();
            ENQUEUE_RENDER_COMMAND(PollSensorRt)([Self](FRHICommandListImmediate& Cmd)
            {
                if(!Self->Finished.load(std::memory_order_acquire)&&Self->Readback&&Self->Readback->IsReady())
                {
                    const uint32 Bytes=Self->Request.Rays.Num()*sizeof(FSensorRtHit);
                    if(!Self->Cancelled.load())
                    {
                        const void* Data=Self->Readback->Lock(Bytes);
                        if(Data){Self->Hits.SetNumUninitialized(Self->Request.Rays.Num());FMemory::Memcpy(Self->Hits.GetData(),Data,Bytes);Self->Readback->Unlock();}
                        else Self->Failure=TEXT("GPU readback lock failed");
                    }
                    Self->Complete();
                }
                Self->PollQueued.store(false,std::memory_order_release);
            });
        }
#endif
        return false;
    }
    void Dispatch(FRHICommandListImmediate& Cmd)
    {
#if RHI_RAYTRACING
        if(Cancelled.load()){Fail(TEXT("cancelled before GPU submission"));return;}
        if(!SensorOwnedRayTracing::IsAvailable()){Fail(TEXT("HWRT pipeline unavailable"));return;}
        TArray<FRayTracingGeometryInstance> Descriptors;TArray<FMatrix> Poses;for(const auto& M:Request.Meshes)Poses.Add(M.LocalToWorld.ToMatrixWithScale());
        for(int32 I=0;I<Request.Meshes.Num();++I)
        {
            const auto& Mesh=Request.Meshes[I];if(!Mesh.Vertices||Mesh.Vertices->IsEmpty())continue;
            auto B=GetBlas(Cmd,Mesh);Blases.Add(B);
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
        RayBuffer=Upload(Cmd,TEXT("SensorRtRays"),Request.Rays,BUF_Static);RayView=Cmd.CreateShaderResourceView(RayBuffer);
        FRHIResourceCreateInfo OutputInfo(TEXT("SensorRtResults"));
        ResultBuffer=Cmd.CreateStructuredBuffer(sizeof(FSensorRtHit),Request.Rays.Num()*sizeof(FSensorRtHit),BUF_UnorderedAccess|BUF_ShaderResource,OutputInfo);
        ResultView=Cmd.CreateUnorderedAccessView(ResultBuffer,false,false);
        FLidarRGS::FParameters P;P.LidarTLAS=SceneView;P.LidarRays=RayView;P.LidarHits=ResultView;P.RayCount=Request.Rays.Num();
        FShaderResourceViewRHIRef Views[4]={Blases[0]->AttributeView,Blases[0]->AttributeView,Blases[0]->AttributeView,Blases[0]->AttributeView};
        for(int I=0;I<Descriptors.Num();++I)Views[Descriptors[I].DefaultUserData]=Blases[I]->AttributeView;
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
        Cmd.SetRayTracingHitGroup(Scene,0,0,0,Pipeline,0,0,nullptr,0,nullptr,0);
        Cmd.SetRayTracingMissShader(Scene,0,Pipeline,0,0,nullptr,0);
        FRayTracingShaderBindingsWriter Bindings;SetShaderParameters(Bindings,Shader,P);
        Cmd.RayTraceDispatch(Pipeline,Shader.GetRayTracingShader(),Scene,Bindings,Request.Rays.Num(),1);
        Cmd.Transition(FRHITransitionInfo(ResultBuffer,ERHIAccess::UAVCompute,ERHIAccess::CopySrc));
        Readback=MakeUnique<FRHIGPUBufferReadback>(TEXT("SensorOwnedLidar"));Readback->EnqueueCopy(Cmd,ResultBuffer,Request.Rays.Num()*sizeof(FSensorRtHit));
#else
        Fail(TEXT("RHI compiled without ray tracing"));
#endif
    }
};
}
uint64 SensorOwnedRayTracing::GetBlasBuildCount(){return BlasBuildCount.load();}
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
    if(!IsAvailable()||Request.Rays.IsEmpty()||Request.Rays.Num()>1048576||Request.Meshes.IsEmpty()||Request.Meshes.Num()>4)return {};
    uint64 GeometryBytes=0;
    for(const auto& Mesh:Request.Meshes)if(Mesh.Vertices&&!Mesh.Vertices->IsEmpty())
    {
        if(!Mesh.Revision||Mesh.Vertices->Num()%3||!Mesh.NormalsAndSurfaces||Mesh.NormalsAndSurfaces->Num()!=Mesh.Vertices->Num()/3)return {};
        GeometryBytes+=uint64(Mesh.Vertices->Num())*sizeof(FVector3f)+uint64(Mesh.NormalsAndSurfaces->Num())*sizeof(FVector4f);
        if(GeometryBytes>256ull*1024*1024)return {};
    }
    if(ActiveScans.fetch_add(1)>=MaxActiveScans){--ActiveScans;return {};}
    auto Scan=MakeShared<FScan,ESPMode::ThreadSafe>();Scan->Request=MoveTemp(Request);
    ENQUEUE_RENDER_COMMAND(DispatchSensorRt)([Scan](FRHICommandListImmediate& Cmd){Scan->Dispatch(Cmd);});
    return Scan;
}
void SensorOwnedRayTracing::RegisterShaders()
{
    AddShaderSourceDirectoryMapping(TEXT("/MA0T10"),FPaths::Combine(FPaths::ProjectDir(),TEXT("Shaders")));
    EndFrameHandle=FCoreDelegates::OnEndFrameRT.AddLambda([]()
    {
        for(int I=RetiredScans.Num()-1;I>=0;--I)if(RetiredScans[I]->CanRetire())RetiredScans.RemoveAtSwap(I);
    });
}

void SensorOwnedRayTracing::Shutdown()
{
    FCoreDelegates::OnEndFrameRT.Remove(EndFrameHandle);
    ENQUEUE_RENDER_COMMAND(ReleaseOwnedSensorRt)([](FRHICommandListImmediate& Cmd)
    {
        RetiredScans.Reset();
#if RHI_RAYTRACING
        BlasCache.Reset();
#endif
    });
    // Module teardown only, never acquisition/polling. RHI resource deletion
    // retains the engine's deferred GPU lifetime; no GPU-idle wait is introduced.
    FlushRenderingCommands();
}
