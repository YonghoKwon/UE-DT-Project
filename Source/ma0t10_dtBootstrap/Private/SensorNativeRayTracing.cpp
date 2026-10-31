#include "SensorNativeRayTracing.h"
#include "SensorNativeSafety.h"
#include "SensorUploadDiagnostics.h"
#include "RenderingThread.h"
#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include <atomic>
#include <initializer_list>
#if PLATFORM_WINDOWS && RHI_RAYTRACING
#include "ID3D12DynamicRHI.h"
#include <d3d12sdklayers.h>
#include "SensorNativeRayQueryDxil.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/HideWindowsPlatformTypes.h"

namespace SensorNativeRayTracing
{
namespace
{
constexpr uint64 MaxBytes=512ull*1024*1024;
constexpr uint64 MaxFreeBytes=64ull*1024*1024;
std::atomic<uint64> Bytes{0},PeakBytes{0},Dispatches{0},BlasBuilds{0},SceneBuilds{0},PacketCreates{0},PacketReuses{0};
std::atomic<uint64> ValidationErrors{0};
std::atomic<bool> ValidationAvailable{false};
std::atomic<uint64> BufferCreates{0},BufferReuses{0};
bool CapacityHit=false; // render thread only, one synchronous Submit transaction
std::atomic<uint64> LastCompleted{0};
bool LedgerEnabled(){static const bool Enabled=FParse::Param(FCommandLine::Get(),TEXT("SensorNativeLedger"));return Enabled;}
bool GeometryPoolEnabled(){static const bool Enabled=!FParse::Param(FCommandLine::Get(),TEXT("SensorNativeNoGeometryPool"));return Enabled;}
bool Succeeded(HRESULT Result,FString& Error,const TCHAR* Operation)
{
    if(SUCCEEDED(Result))return true;
    Error=FString::Printf(TEXT("native LiDAR %s failed (0x%08x)"),Operation,uint32(Result));return false;
}
struct FBuffer
{
    TRefCountPtr<ID3D12Resource> Resource;
    uint64 Size=0;
    uint64 LastUseFence=0;
    const TCHAR* Purpose=TEXT("Buffer");
    TRefCountPtr<ID3D12Device> LedgerDevice;
    void* Mapped=nullptr;
    ~FBuffer(){if(Resource&&LedgerEnabled())UE_LOG(LogTemp,Display,TEXT("[SensorNativeLedger] release VA=0x%llx bytes=%llu role=%s last_use=%llu completed=%llu device=0x%08x"),uint64(Resource->GetGPUVirtualAddress()),Size,Purpose,LastUseFence,LastCompleted.load(),LedgerDevice?uint32(LedgerDevice->GetDeviceRemovedReason()):0);if(Resource&&Mapped)Resource->Unmap(0,nullptr);if(Size)Bytes.fetch_sub(Size);}
    bool Create(ID3D12Device* Device,uint64 InSize,D3D12_HEAP_TYPE Heap,D3D12_RESOURCE_STATES State,bool Uav,FString& Error)
    {
        // Dedicated committed allocations cannot be relocated by the engine's
        // pool or BLAS compactor. They are created resident and never evicted by
        // this backend; the entire live + retired + cached ledger is bounded.
        InSize=Align(InSize,uint64(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
        if(!InSize||Bytes.load()+InSize>MaxBytes){CapacityHit=true;Error=TEXT("native LiDAR live allocation budget exceeded");return false;}
        D3D12_HEAP_PROPERTIES Properties={};Properties.Type=Heap;Properties.CreationNodeMask=Properties.VisibleNodeMask=1;
        D3D12_RESOURCE_DESC Desc={};Desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;Desc.Width=InSize;
        Desc.Height=1;Desc.DepthOrArraySize=1;Desc.MipLevels=1;Desc.SampleDesc.Count=1;Desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        Desc.Flags=Uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;
        if(!Succeeded(Device->CreateCommittedResource(&Properties,D3D12_HEAP_FLAG_NONE,&Desc,State,nullptr,IID_PPV_ARGS(Resource.GetInitReference())),Error,TEXT("CreateCommittedResource")))return false;
        ++BufferCreates;
        Size=InSize;Bytes.fetch_add(Size);PeakBytes.store(FMath::Max(PeakBytes.load(),Bytes.load()));
        Resource->SetName(L"SensorOwnedPersistent");
        if(LedgerEnabled())Resource->SetName(*FString::Printf(TEXT("SensorOwned/%s/0x%llx/%llu"),Purpose,uint64(Resource->GetGPUVirtualAddress()),Size));
        if(LedgerEnabled())LedgerDevice=Device;
        if(LedgerEnabled())UE_LOG(LogTemp,Display,TEXT("[SensorNativeLedger] create VA=0x%llx bytes=%llu heap=%d state=%u role=%s"),uint64(Resource->GetGPUVirtualAddress()),Size,int32(Heap),uint32(State),Purpose);
        if(Heap!=D3D12_HEAP_TYPE_DEFAULT)
        {
            D3D12_RANGE Range={0,Heap==D3D12_HEAP_TYPE_READBACK?SIZE_T(Size):0};
            if(!Succeeded(Resource->Map(0,&Range,&Mapped),Error,TEXT("Map")))return false;
        }
        return true;
    }
    D3D12_GPU_VIRTUAL_ADDRESS Address()const{return Resource->GetGPUVirtualAddress();}
    bool BeginWrite(FString& Error)
    {if(Mapped)return true;D3D12_RANGE Empty={0,0};return Succeeded(Resource->Map(0,&Empty,&Mapped),Error,TEXT("upload Map"));}
    void EndWrite(uint64 WrittenBytes)
    {FPlatformMisc::MemoryBarrier();D3D12_RANGE Written={0,SIZE_T(WrittenBytes)};Resource->Unmap(0,&Written);Mapped=nullptr;}
};
using FBufferPtr=TSharedPtr<FBuffer,ESPMode::ThreadSafe>;
FBufferPtr NewBuffer(ID3D12Device* Device,uint64 Size,D3D12_HEAP_TYPE Heap,D3D12_RESOURCE_STATES State,bool Uav,FString& Error,const TCHAR* Purpose=TEXT("Buffer"))
{
    auto Result=MakeShared<FBuffer,ESPMode::ThreadSafe>();Result->Purpose=Purpose;if(!Result->Create(Device,Size,Heap,State,Uav,Error))return {};
    if(SensorUploadDiagnostics::IsEnabled())
    {
        using namespace SensorUploadDiagnostics;ESite Site=ESite::NativeGeometry;
        if(FCString::Stristr(Purpose,TEXT("Rays")))Site=ESite::NativeRays;
        else if(FCString::Stristr(Purpose,TEXT("Constants")))Site=ESite::NativeConstants;
        else if(FCString::Stristr(Purpose,TEXT("Instances")))Site=ESite::NativeInstances;
        else if(FCString::Stristr(Purpose,TEXT("Readback")))Site=ESite::NativeReadback;
        RecordAllocation(Site,0,Result->Size);
    }
    return Result;
}
void UavBarrier(ID3D12GraphicsCommandList4* List)
{D3D12_RESOURCE_BARRIER B={};B.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;List->ResourceBarrier(1,&B);}
struct FBlas
{
    uint64 Revision=0,LastUse=0;
    FBufferPtr Vertices,Surfaces,Storage,Scratch;
};
struct FScene
{
    TArray<TSharedPtr<FBlas,ESPMode::ThreadSafe>> Blases;
    TArray<uint64> Revisions;
    TArray<FTransform> Poses;
    FBufferPtr Instances,Storage,Scratch;
    FMatrix44f Normals[4];
    uint64 LastUse=0;
    bool Matches(const TArray<FSensorRtMesh>& Meshes)const
    {
        if(Meshes.Num()!=Revisions.Num())return false;
        for(int32 I=0;I<Meshes.Num();++I)if(Meshes[I].Revision!=Revisions[I]||!Meshes[I].LocalToWorld.Equals(Poses[I],0))return false;
        return true;
    }
};
struct FJob
{
    TRefCountPtr<ID3D12CommandAllocator> Allocator;
    TRefCountPtr<ID3D12GraphicsCommandList4> List;
    FBufferPtr Rays,Results,Readback,Constants;
    TSharedPtr<FScene,ESPMode::ThreadSafe> Scene;
    TArray<FBufferPtr> KeepAlive;
    int32 Count=0;
    uint64 FenceValue=0;
    uint64 BufferBytes()const{return Rays->Size+Results->Size+Readback->Size+Constants->Size;}
};
class FBackend
{
public:
    TRefCountPtr<ID3D12Device5> Device;
    TRefCountPtr<ID3D12CommandQueue> Queue;
    TRefCountPtr<ID3D12Fence> Fence;
    TRefCountPtr<ID3D12RootSignature> Root;
    TRefCountPtr<ID3D12PipelineState> Pipeline;
    TRefCountPtr<ID3D12InfoQueue> InfoQueue;
    TArray<TSharedPtr<FBlas,ESPMode::ThreadSafe>> Blases;
    TArray<TSharedPtr<FScene,ESPMode::ThreadSafe>> Scenes;
    TArray<TSharedPtr<FJob,ESPMode::ThreadSafe>> Free,Retired;
    uint64 NextFence=0,Clock=0;
    uint64 LastMessage=0;
    bool Faulted=false;
    bool DredReported=false;
    bool ScopedGbv=false;
    D3D12_COMMAND_LIST_TYPE ListType=D3D12_COMMAND_LIST_TYPE_DIRECT;
    bool Initialize(FString& Error)
    {
        if(!IsRHID3D12()||GNumExplicitGPUsForRendering!=1){Error=TEXT("native LiDAR requires a single D3D12 GPU");return false;}
        auto* Native=GetID3D12DynamicRHI()->RHIGetDevice(0);
        if(GetID3D12DynamicRHI()->RHIGetDeviceNodeMask(0)!=1){Error=TEXT("native LiDAR unsupported GPU node mask");return false;}
        if(!Succeeded(Native->QueryInterface(IID_PPV_ARGS(Device.GetInitReference())),Error,TEXT("ID3D12Device5")))return false;
        ScopedGbv=FParse::Param(FCommandLine::Get(),TEXT("SensorNativeScopedGBV"));
        if(ScopedGbv)
        {
            TRefCountPtr<ID3D12DebugDevice1> Debug;
            if(!Succeeded(Device->QueryInterface(IID_PPV_ARGS(Debug.GetInitReference())),Error,TEXT("GBV debug device")))return false;
            D3D12_DEBUG_DEVICE_GPU_BASED_VALIDATION_SETTINGS Settings={};Settings.MaxMessagesPerCommandList=256;
            Settings.DefaultShaderPatchMode=D3D12_GPU_BASED_VALIDATION_SHADER_PATCH_MODE_NONE;
            Settings.PipelineStateCreateFlags=D3D12_GPU_BASED_VALIDATION_PIPELINE_STATE_CREATE_FLAG_FRONT_LOAD_CREATE_UNGUARDED_VALIDATION_SHADERS;
            if(!Succeeded(Debug->SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_GPU_BASED_VALIDATION_SETTINGS,&Settings,sizeof(Settings)),Error,TEXT("GBV diagnostic default patch mode")))return false;
            UE_LOG(LogTemp,Display,TEXT("[SensorNativeGBV] diagnostic process: default shader patch NONE; native lists override UNGUARDED; device GBV must be enabled before creation"));
        }
        if(SUCCEEDED(Native->QueryInterface(IID_PPV_ARGS(InfoQueue.GetInitReference()))))
        {ValidationAvailable.store(true);LastMessage=FParse::Param(FCommandLine::Get(),TEXT("SensorNativeStrictValidation"))?0:InfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();}
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 Options={};
        if(!Succeeded(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&Options,sizeof(Options)),Error,TEXT("DXR capability")))return false;
        if(Options.RaytracingTier<D3D12_RAYTRACING_TIER_1_1){Error=TEXT("native LiDAR requires DXR 1.1 inline queries");return false;}
        if(FParse::Param(FCommandLine::Get(),TEXT("SensorNativeComputeQueue")))
        {
            ListType=D3D12_COMMAND_LIST_TYPE_COMPUTE;
            D3D12_COMMAND_QUEUE_DESC Q={};Q.Type=ListType;Q.NodeMask=1;
            if(!Succeeded(Device->CreateCommandQueue(&Q,IID_PPV_ARGS(Queue.GetInitReference())),Error,TEXT("CreateCommandQueue")))return false;
            Queue->SetName(L"SensorOwnedLidarCompute");
        }
        else
        {
            // Use an independent native command list on the existing graphics
            // queue. No engine list, descriptor heap, state cache or residency
            // handle is touched. Native resources remain strongly owned through
            // our own fence on this queue; command-list state is self-contained.
            Queue=GetID3D12DynamicRHI()->RHIGetCommandQueue();
        }
        if(!Succeeded(Device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(Fence.GetInitReference())),Error,TEXT("CreateFence")))return false;
        D3D12_ROOT_PARAMETER Parameters[8]={};
        for(uint32 I=0;I<6;++I){Parameters[I].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;Parameters[I].Descriptor.ShaderRegister=I;}
        Parameters[6].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
        Parameters[7].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
        D3D12_ROOT_SIGNATURE_DESC RootDesc={};RootDesc.NumParameters=8;RootDesc.pParameters=Parameters;
        TRefCountPtr<ID3DBlob> Serialized,Errors;
        if(!Succeeded(D3D12SerializeRootSignature(&RootDesc,D3D_ROOT_SIGNATURE_VERSION_1,Serialized.GetInitReference(),Errors.GetInitReference()),Error,TEXT("SerializeRootSignature")))return false;
        if(!Succeeded(Device->CreateRootSignature(1,Serialized->GetBufferPointer(),Serialized->GetBufferSize(),IID_PPV_ARGS(Root.GetInitReference())),Error,TEXT("CreateRootSignature")))return false;
        D3D12_COMPUTE_PIPELINE_STATE_DESC Desc={};Desc.pRootSignature=Root;Desc.CS={SensorNativeRayQueryDxil,sizeof(SensorNativeRayQueryDxil)};Desc.NodeMask=1;
        if(!Succeeded(Device->CreateComputePipelineState(&Desc,IID_PPV_ARGS(Pipeline.GetInitReference())),Error,TEXT("CreateComputePipelineState")))return false;
        return SensorNativeSafety::BeginSession(Error);
    }
    void ReportDred()
    {
        if(DredReported||!LedgerEnabled())return;DredReported=true;
        TRefCountPtr<ID3D12DeviceRemovedExtendedData1> Dred;
        if(FAILED(Device->QueryInterface(IID_PPV_ARGS(Dred.GetInitReference()))))return;
        D3D12_DRED_PAGE_FAULT_OUTPUT1 Page={};const HRESULT PageResult=Dred->GetPageFaultAllocationOutput1(&Page);
        UE_LOG(LogTemp,Error,TEXT("[SensorNativeDRED] result=0x%08x faultVA=0x%llx removed=0x%08x"),uint32(PageResult),uint64(Page.PageFaultVA),uint32(Device->GetDeviceRemovedReason()));
        if(SUCCEEDED(PageResult))for(int32 Deleted=0;Deleted<2;++Deleted)
        {
            auto* Node=Deleted?Page.pHeadRecentFreedAllocationNode:Page.pHeadExistingAllocationNode;
            for(int32 I=0;Node&&I<256;++I,Node=Node->pNext)
                UE_LOG(LogTemp,Error,TEXT("[SensorNativeDREDAllocation] deleted=%d type=%d name=%s"),Deleted,int32(Node->AllocationType),Node->ObjectNameW?Node->ObjectNameW:TEXT("<unnamed>"));
        }
        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 Crumbs={};
        if(SUCCEEDED(Dred->GetAutoBreadcrumbsOutput1(&Crumbs)))
            for(auto* N=Crumbs.pHeadAutoBreadcrumbNode;N;N=N->pNext)
            {
                const uint32 Last=N->pLastBreadcrumbValue?*N->pLastBreadcrumbValue:0;
                UE_LOG(LogTemp,Error,TEXT("[SensorNativeDREDBreadcrumb] list=%s queue=%s last=%u count=%u"),N->pCommandListDebugNameW?N->pCommandListDebugNameW:TEXT("<unnamed>"),N->pCommandQueueDebugNameW?N->pCommandQueueDebugNameW:TEXT("<unnamed>"),Last,N->BreadcrumbCount);
            }
    }
    bool Complete(uint64 Value){const uint64 Done=Fence->GetCompletedValue();LastCompleted.store(Done);if(Done==MAX_uint64)ReportDred();return Done>=Value;}
    bool Validate(FString& Error)
    {
        if(!InfoQueue)return true;
        const uint64 Count=InfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
        if(Count<LastMessage)LastMessage=0;
        for(uint64 I=LastMessage;I<Count;++I)
        {
            SIZE_T Size=0;InfoQueue->GetMessage(I,nullptr,&Size);if(!Size)continue;
            TArray<uint8> Storage;Storage.SetNumUninitialized(Size);auto* Message=reinterpret_cast<D3D12_MESSAGE*>(Storage.GetData());
            if(SUCCEEDED(InfoQueue->GetMessage(I,Message,&Size))&&(Message->Severity==D3D12_MESSAGE_SEVERITY_ERROR||Message->Severity==D3D12_MESSAGE_SEVERITY_CORRUPTION))
            {
                const FString Description=UTF8_TO_TCHAR(Message->pDescription);
                const bool KnownEngineWarmup=Description.Contains(TEXT("ReflectionCubeTexture"))||Description.Contains(TEXT("LongTaskRenderTarget"));
                const bool KnownEngineProbe=Description.Contains(TEXT("CreateDescriptorHeap"))&&Description.Contains(TEXT("2000000"))&&Description.Contains(TEXT("1000000"));
                const bool ProvenEngineDepth=Message->Severity==D3D12_MESSAGE_SEVERITY_ERROR&&SensorNativeSafety::IsProvenEngineDepthBarrier(uint32(Message->ID),Description);
                if(ScopedGbv&&(KnownEngineWarmup||KnownEngineProbe||ProvenEngineDepth))
                {UE_LOG(LogTemp,Warning,TEXT("[SensorNativeExcludedEngineDebug] id=%d %s"),int32(Message->ID),*Description);}
                else {++ValidationErrors;Error=FString::Printf(TEXT("D3D12 validation during native acquisition: %s"),*Description);Faulted=true;SensorNativeSafety::DisableForSession(Error);}
            }
        }
        LastMessage=Count;return !Faulted;
    }
    void Return(TSharedPtr<FJob,ESPMode::ThreadSafe> Job)
    {
        check(Complete(Job->FenceValue));Job->KeepAlive.Reset();Job->Scene.Reset();uint64 Total=Job->BufferBytes();for(const auto& P:Free)Total+=P->BufferBytes();
        if(Free.Num()<4&&Total<=MaxFreeBytes)Free.Add(MoveTemp(Job));
    }
    void Collect()
    {for(int32 I=Retired.Num()-1;I>=0;--I)if(Complete(Retired[I]->FenceValue)){auto Job=Retired[I];Retired.RemoveAtSwap(I);Return(MoveTemp(Job));}}
    TSharedPtr<FJob,ESPMode::ThreadSafe> Take(int32 Count,FString& Error)
    {
        Collect();
        for(int32 I=0;I<Free.Num();++I)if(Free[I]->Count==Count){auto Job=Free[I];Free.RemoveAtSwap(I);++PacketReuses;return Job;}
        auto Job=MakeShared<FJob,ESPMode::ThreadSafe>();Job->Count=Count;
        if(!Succeeded(Device->CreateCommandAllocator(ListType,IID_PPV_ARGS(Job->Allocator.GetInitReference())),Error,TEXT("CreateCommandAllocator")))return {};
        if(!Succeeded(Device->CreateCommandList(1,ListType,Job->Allocator,nullptr,IID_PPV_ARGS(Job->List.GetInitReference())),Error,TEXT("CreateCommandList")))return {};
        if(ScopedGbv)
        {
            TRefCountPtr<ID3D12DebugCommandList1> Debug;
            if(!Succeeded(Job->List->QueryInterface(IID_PPV_ARGS(Debug.GetInitReference())),Error,TEXT("GBV native list")))return {};
            D3D12_DEBUG_COMMAND_LIST_GPU_BASED_VALIDATION_SETTINGS Settings={D3D12_GPU_BASED_VALIDATION_SHADER_PATCH_MODE_UNGUARDED_VALIDATION};
            if(!Succeeded(Debug->SetDebugParameter(D3D12_DEBUG_COMMAND_LIST_PARAMETER_GPU_BASED_VALIDATION_SETTINGS,&Settings,sizeof(Settings)),Error,TEXT("GBV native patch mode")))return {};
        }
        if(!Succeeded(Job->List->Close(),Error,TEXT("initial Close")))return {};
        Job->List->SetName(L"SensorOwnedLidarList");Job->Allocator->SetName(L"SensorOwnedLidarAllocator");
        Job->Rays=NewBuffer(Device,uint64(Count)*sizeof(FSensorRtRay),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,false,Error,TEXT("Job.Rays"));
        Job->Results=NewBuffer(Device,uint64(Count)*sizeof(FSensorRtHit),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true,Error,TEXT("Job.Results"));
        Job->Readback=NewBuffer(Device,uint64(Count)*sizeof(FSensorRtHit),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,false,Error,TEXT("Job.Readback"));
        Job->Constants=NewBuffer(Device,512,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,false,Error,TEXT("Job.Constants"));
        if(!Job->Rays||!Job->Results||!Job->Readback||!Job->Constants)return {};
        ++PacketCreates;return Job;
    }
    bool Idle(std::initializer_list<FBufferPtr> Buffers)
    {
        const uint64 Done=Fence->GetCompletedValue();LastCompleted.store(Done);
        if(Done==MAX_uint64)return false;
        for(const auto& Buffer:Buffers)if(Buffer&&Buffer->LastUseFence>Done)return false;
        return true;
    }
    bool EnsureBuffer(FBufferPtr& Buffer,uint64 Size,D3D12_HEAP_TYPE Heap,D3D12_RESOURCE_STATES State,bool Uav,FString& Error,const TCHAR* Purpose)
    {
        if(GeometryPoolEnabled()&&Buffer&&Buffer->Size>=Size&&Idle({Buffer})){++BufferReuses;return true;}
        Buffer=NewBuffer(Device,Size,Heap,State,Uav,Error,Purpose);return Buffer.IsValid();
    }
    TSharedPtr<FBlas,ESPMode::ThreadSafe> GetBlas(const FSensorRtMesh& Mesh,const TArray<FSensorRtMesh>& DesiredMeshes,ID3D12GraphicsCommandList4* List,uint64 MinSurfaceBytes,FString& Error)
    {
        // The compiled ternary eagerly loads all four metadata SRVs before
        // selecting a mesh. Root SRVs have no descriptor bounds, so every
        // binding must cover the largest primitive-index domain in the scene.
        // Grow by replacement only: old submitted roots stay pinned by jobs.
        const uint64 SurfaceBytes=FMath::Max(MinSurfaceBytes,uint64(Mesh.NormalsAndSurfaces->Num())*sizeof(FVector4f));
        auto EnsureSurfaces=[&](FBlas& B,bool Refresh)
        {
            if(!Refresh&&B.Surfaces&&B.Surfaces->Size>=SurfaceBytes)return true;
            FBufferPtr Surfaces;
            if(Refresh&&GeometryPoolEnabled()&&B.Surfaces&&B.Surfaces->Size>=SurfaceBytes&&Idle({B.Surfaces})){Surfaces=B.Surfaces;++BufferReuses;}
            else Surfaces=NewBuffer(Device,SurfaceBytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,false,Error,TEXT("B.Surfaces"));
            if(!Surfaces)return false;
            if(!Surfaces->BeginWrite(Error))return false;
            FMemory::Memzero(Surfaces->Mapped,Surfaces->Size);
            FMemory::Memcpy(Surfaces->Mapped,Mesh.NormalsAndSurfaces->GetData(),Mesh.NormalsAndSurfaces->Num()*sizeof(FVector4f));
            SensorUploadDiagnostics::RecordCopy(SensorUploadDiagnostics::ESite::NativeGeometry,GFrameNumberRenderThread,uint64(Mesh.NormalsAndSurfaces->Num())*sizeof(FVector4f),Surfaces->Size);
            Surfaces->EndWrite(Surfaces->Size);B.Surfaces=MoveTemp(Surfaces);return true;
        };
        for(auto& B:Blases)if(B->Revision==Mesh.Revision){B->LastUse=++Clock;return EnsureSurfaces(*B,false)?B:TSharedPtr<FBlas,ESPMode::ThreadSafe>();}
        TSharedPtr<FBlas,ESPMode::ThreadSafe> B;
        int32 ReuseIndex=INDEX_NONE;
        if(GeometryPoolEnabled())for(int32 I=0;I<Blases.Num();++I)
        {
            // Cache-only ownership excludes every cached/in-flight TLAS and
            // recording job. Explicit fence checks also exclude GPU use.
            const auto& Candidate=Blases[I];
            // A later mesh in this same request may need a currently cache-only
            // static BLAS. Do not steal its slot and rebuild it unnecessarily.
            const bool Desired=DesiredMeshes.ContainsByPredicate([&](const FSensorRtMesh& M){return M.Revision==Candidate->Revision;});
            if(!Desired&&Candidate.GetSharedReferenceCount()==1&&Idle({Candidate->Vertices,Candidate->Surfaces,Candidate->Storage,Candidate->Scratch})&&(ReuseIndex==INDEX_NONE||Candidate->LastUse<Blases[ReuseIndex]->LastUse))ReuseIndex=I;
        }
        if(ReuseIndex!=INDEX_NONE){B=MoveTemp(Blases[ReuseIndex]);Blases.RemoveAt(ReuseIndex);}
        if(!B)B=MakeShared<FBlas,ESPMode::ThreadSafe>();
        else if(LedgerEnabled())UE_LOG(LogTemp,Display,TEXT("[SensorNativeReuse] role=BLAS old_revision=%llu new_revision=%llu completed=%llu vertex_last_use=%llu"),B->Revision,Mesh.Revision,LastCompleted.load(),B->Vertices?B->Vertices->LastUseFence:0);
        B->Revision=Mesh.Revision;B->LastUse=++Clock;
        if(!EnsureBuffer(B->Vertices,uint64(Mesh.Vertices->Num())*sizeof(FVector3f),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,false,Error,TEXT("B.Vertices"))||!EnsureSurfaces(*B,true)||!B->Vertices->BeginWrite(Error))return {};
        FMemory::Memcpy(B->Vertices->Mapped,Mesh.Vertices->GetData(),Mesh.Vertices->Num()*sizeof(FVector3f));
        SensorUploadDiagnostics::RecordCopy(SensorUploadDiagnostics::ESite::NativeGeometry,GFrameNumberRenderThread,uint64(Mesh.Vertices->Num())*sizeof(FVector3f),B->Vertices->Size);
        B->Vertices->EndWrite(Mesh.Vertices->Num()*sizeof(FVector3f));
        D3D12_RAYTRACING_GEOMETRY_DESC Geometry={};Geometry.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;Geometry.Flags=D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        Geometry.Triangles.VertexBuffer.StartAddress=B->Vertices->Address();Geometry.Triangles.VertexBuffer.StrideInBytes=sizeof(FVector3f);
        Geometry.Triangles.VertexCount=Mesh.Vertices->Num();Geometry.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs={};Inputs.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        Inputs.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;Inputs.NumDescs=1;Inputs.pGeometryDescs=&Geometry;
        Inputs.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO Size={};Device->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs,&Size);
        if(!EnsureBuffer(B->Storage,Size.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,true,Error,TEXT("B.Storage"))||!EnsureBuffer(B->Scratch,Size.ScratchDataSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true,Error,TEXT("B.Scratch")))return {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC Build={};Build.Inputs=Inputs;Build.DestAccelerationStructureData=B->Storage->Address();Build.ScratchAccelerationStructureData=B->Scratch->Address();
        // DXR AS buffers keep their AS state. Order previous traversal reads
        // before a full rebuild into reused storage, then publish build writes.
        if(GeometryPoolEnabled())UavBarrier(List);
        List->BuildRaytracingAccelerationStructure(&Build,0,nullptr);UavBarrier(List);++BlasBuilds;
        if(LedgerEnabled())UE_LOG(LogTemp,Display,TEXT("[SensorNativeBlasBuild] next_fence=%llu revision=%llu vertices=0x%llx vertex_bytes=%llu vertex_count=%d stride=%u surfaces=0x%llx result=0x%llx scratch=0x%llx sourceAS=0 refit=0 node_mask=1 created_resident=1 MakeResident=0 Evict=0 cpu_desc=%p"),NextFence+1,Mesh.Revision,uint64(B->Vertices->Address()),B->Vertices->Size,Mesh.Vertices->Num(),uint32(sizeof(FVector3f)),uint64(B->Surfaces->Address()),uint64(B->Storage->Address()),uint64(B->Scratch->Address()),&Geometry);
        if(Blases.Num()>=8){int32 Old=0;for(int32 I=1;I<Blases.Num();++I)if(Blases[I]->LastUse<Blases[Old]->LastUse)Old=I;Blases.RemoveAt(Old);}
        Blases.Add(B);return B;
    }
    TSharedPtr<FScene,ESPMode::ThreadSafe> GetScene(const TArray<FSensorRtMesh>& Meshes,ID3D12GraphicsCommandList4* List,FString& Error)
    {
        for(auto& S:Scenes)if(S->Matches(Meshes)){S->LastUse=++Clock;return S;}
        TSharedPtr<FScene,ESPMode::ThreadSafe> S;
        if(GeometryPoolEnabled())for(int32 I=0;I<Scenes.Num();++I)
        {
            const auto& Candidate=Scenes[I];
            if(Candidate.GetSharedReferenceCount()==1&&Idle({Candidate->Instances,Candidate->Storage,Candidate->Scratch}))
            {S=MoveTemp(Scenes[I]);Scenes.RemoveAt(I);S->Blases.Reset();S->Revisions.Reset();S->Poses.Reset();break;}
        }
        if(!S)S=MakeShared<FScene,ESPMode::ThreadSafe>();
        else if(LedgerEnabled())UE_LOG(LogTemp,Display,TEXT("[SensorNativeReuse] role=TLAS completed=%llu storage_last_use=%llu"),LastCompleted.load(),S->Storage?S->Storage->LastUseFence:0);
        S->LastUse=++Clock;
        uint64 MinSurfaceBytes=0;for(const auto& M:Meshes)if(M.Vertices&&!M.Vertices->IsEmpty())MinSurfaceBytes=FMath::Max(MinSurfaceBytes,uint64(M.Vertices->Num()/3)*sizeof(FVector4f));
        TArray<D3D12_RAYTRACING_INSTANCE_DESC> Instances;
        for(int32 I=0;I<Meshes.Num();++I)
        {
            const auto& Mesh=Meshes[I];S->Revisions.Add(Mesh.Revision);S->Poses.Add(Mesh.LocalToWorld);S->Blases.Add({});
            if(!Mesh.Vertices||Mesh.Vertices->IsEmpty())continue;
            auto B=GetBlas(Mesh,Meshes,List,MinSurfaceBytes,Error);if(!B)return {};S->Blases[I]=B;
            D3D12_RAYTRACING_INSTANCE_DESC Instance={};const FMatrix M=Mesh.LocalToWorld.ToMatrixWithScale();
            for(int32 Row=0;Row<3;++Row)for(int32 Column=0;Column<4;++Column)Instance.Transform[Row][Column]=float(M.M[Column][Row]);
            Instance.InstanceID=I;Instance.InstanceMask=0xff;Instance.AccelerationStructure=B->Storage->Address();Instances.Add(Instance);
            if(LedgerEnabled())UE_LOG(LogTemp,Display,TEXT("[SensorNativeTlasInstance] next_fence=%llu instance=%d revision=%llu BLAS=0x%llx vertices=0x%llx surfaces=0x%llx"),NextFence+1,I,Mesh.Revision,uint64(B->Storage->Address()),uint64(B->Vertices->Address()),uint64(B->Surfaces->Address()));
        }
        if(Instances.IsEmpty()){Error=TEXT("native LiDAR empty collision TLAS");return {};}
        if(!EnsureBuffer(S->Instances,Instances.Num()*sizeof(D3D12_RAYTRACING_INSTANCE_DESC),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,false,Error,TEXT("S.Instances"))||!S->Instances->BeginWrite(Error))return {};
        FMemory::Memcpy(S->Instances->Mapped,Instances.GetData(),Instances.Num()*sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
        SensorUploadDiagnostics::RecordCopy(SensorUploadDiagnostics::ESite::NativeInstances,GFrameNumberRenderThread,uint64(Instances.Num())*sizeof(D3D12_RAYTRACING_INSTANCE_DESC),S->Instances->Size);
        S->Instances->EndWrite(Instances.Num()*sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
        for(int32 I=0;I<4;++I)S->Normals[I]=I<Meshes.Num()?FMatrix44f(Meshes[I].LocalToWorld.ToInverseMatrixWithScale().GetTransposed()):FMatrix44f::Identity;
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs={};Inputs.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        Inputs.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;Inputs.NumDescs=Instances.Num();Inputs.InstanceDescs=S->Instances->Address();
        Inputs.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO Size={};Device->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs,&Size);
        if(!EnsureBuffer(S->Storage,Size.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,true,Error,TEXT("S.Storage"))||!EnsureBuffer(S->Scratch,Size.ScratchDataSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true,Error,TEXT("S.Scratch")))return {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC Build={};Build.Inputs=Inputs;Build.DestAccelerationStructureData=S->Storage->Address();Build.ScratchAccelerationStructureData=S->Scratch->Address();
        if(GeometryPoolEnabled())UavBarrier(List);
        List->BuildRaytracingAccelerationStructure(&Build,0,nullptr);UavBarrier(List);++SceneBuilds;
        if(LedgerEnabled())UE_LOG(LogTemp,Display,TEXT("[SensorNativeTlasBuild] next_fence=%llu instances=0x%llx count=%d result=0x%llx scratch=0x%llx sourceAS=0 refit=0 node_mask=1"),NextFence+1,uint64(S->Instances->Address()),Instances.Num(),uint64(S->Storage->Address()),uint64(S->Scratch->Address()));
        if(Scenes.Num()>=4){int32 Old=0;for(int32 I=1;I<Scenes.Num();++I)if(Scenes[I]->LastUse<Scenes[Old]->LastUse)Old=I;Scenes.RemoveAt(Old);}
        Scenes.Add(S);return S;
    }
    void Drain()
    {
        // Module teardown only. Every submitted native command retains its
        // allocator, scene/BLAS, buffers and device until this terminal fence.
        if(Queue&&Fence&&NextFence&&!Complete(NextFence))
        {
            HANDLE Event=CreateEvent(nullptr,0,0,nullptr);
            if(Event&&SUCCEEDED(Fence->SetEventOnCompletion(NextFence,Event)))WaitForSingleObject(Event,INFINITE);
            if(Event)CloseHandle(Event);
        }
        Retired.Reset();Free.Reset();Scenes.Reset();Blases.Reset();
    }
};
TUniquePtr<FBackend> Backend;
class FTicket final:public ITicket
{
    TSharedPtr<FJob,ESPMode::ThreadSafe> Job;
public:
    explicit FTicket(TSharedPtr<FJob,ESPMode::ThreadSafe> In):Job(MoveTemp(In)){}
    ~FTicket(){if(Job&&Backend)Backend->Retired.Add(MoveTemp(Job));}
    bool Poll(TArray<FSensorRtHit>& Hits,FString& Error,bool Cancelled)override
    {
        check(IsInRenderingThread());if(!Job)return true;
        if(!Backend->Complete(Job->FenceValue))return false;
        if(FAILED(Backend->Device->GetDeviceRemovedReason()))
        {
            Error=TEXT("native LiDAR device removed");Backend->Faulted=true;
            SensorNativeSafety::DisableForSession(Error);
        }
        else if(Backend->Validate(Error)&&!Cancelled)
        {
            Hits.SetNumUninitialized(Job->Count,false);FMemory::Memcpy(Hits.GetData(),Job->Readback->Mapped,Job->Count*sizeof(FSensorRtHit));
            SensorUploadDiagnostics::RecordCopy(SensorUploadDiagnostics::ESite::NativeReadback,GFrameNumberRenderThread,uint64(Job->Count)*sizeof(FSensorRtHit),Job->Readback->Size);
        }
        Backend->Return(MoveTemp(Job));return true;
    }
};
}
TSharedPtr<ITicket,ESPMode::ThreadSafe> Submit(const TArray<FSensorRtMesh>& Meshes,const TArray<TConstArrayView<FSensorRtRay>>& Parts,FString& Error,bool& BudgetLimited)
{
    check(IsInRenderingThread());
    CapacityHit=false;BudgetLimited=false;ON_SCOPE_EXIT{BudgetLimited=CapacityHit;};
    if(!Backend){auto New=MakeUnique<FBackend>();if(!New->Initialize(Error))return {};Backend=MoveTemp(New);}
    if(Backend->Faulted){Error=TEXT("native LiDAR backend stopped after GPU failure");return {};}
    if(!Succeeded(Backend->Device->GetDeviceRemovedReason(),Error,TEXT("device status")))return {};
    int32 Count=0;for(const auto& Part:Parts)Count+=Part.Num();
    auto Job=Backend->Take(Count,Error);if(!Job)return {};
    bool Submitted=false;
    ON_SCOPE_EXIT{if(!Submitted){Backend->Scenes.Reset();Backend->Blases.Reset();}};
    if(!Succeeded(Job->Allocator->Reset(),Error,TEXT("allocator Reset"))||!Succeeded(Job->List->Reset(Job->Allocator,Backend->Pipeline),Error,TEXT("list Reset")))return {};
    if(Backend->ScopedGbv)
    {
        TRefCountPtr<ID3D12DebugCommandList1> Debug;
        if(!Succeeded(Job->List->QueryInterface(IID_PPV_ARGS(Debug.GetInitReference())),Error,TEXT("GBV reset list")))return {};
        D3D12_DEBUG_COMMAND_LIST_GPU_BASED_VALIDATION_SETTINGS Settings={D3D12_GPU_BASED_VALIDATION_SHADER_PATCH_MODE_UNGUARDED_VALIDATION};
        if(!Succeeded(Debug->SetDebugParameter(D3D12_DEBUG_COMMAND_LIST_PARAMETER_GPU_BASED_VALIDATION_SETTINGS,&Settings,sizeof(Settings)),Error,TEXT("GBV after reset")))return {};
        D3D12_DEBUG_COMMAND_LIST_GPU_BASED_VALIDATION_SETTINGS Readback={};
        if(!Succeeded(Debug->GetDebugParameter(D3D12_DEBUG_COMMAND_LIST_PARAMETER_GPU_BASED_VALIDATION_SETTINGS,&Readback,sizeof(Readback)),Error,TEXT("GBV verify after reset")))return {};
        if(Readback.ShaderPatchMode!=Settings.ShaderPatchMode){Error=TEXT("native GBV patch mode differs after Reset");return {};}
        Job->List->SetPipelineState(Backend->Pipeline);
        if(Backend->NextFence==0)UE_LOG(LogTemp,Display,TEXT("[SensorNativeGBV] native list patch mode verified after Reset=%d"),int32(Readback.ShaderPatchMode));
    }
    Job->Scene=Backend->GetScene(Meshes,Job->List,Error);if(!Job->Scene)return {};
    if(!Job->Rays->BeginWrite(Error)||!Job->Constants->BeginWrite(Error))return {};
    uint32 Offset=0;for(const auto& Part:Parts){const uint32 Size=Part.Num()*sizeof(FSensorRtRay);FMemory::Memcpy(static_cast<uint8*>(Job->Rays->Mapped)+Offset,Part.GetData(),Size);Offset+=Size;}
    SensorUploadDiagnostics::RecordCopy(SensorUploadDiagnostics::ESite::NativeRays,GFrameNumberRenderThread,Offset,Job->Rays->Size);
    // The immutable matrices are shared; the count/CBV is per fenced job.
    // Never overwrite another in-flight dispatch's constant allocation.
    FMemory::Memcpy(Job->Constants->Mapped,Job->Scene->Normals,sizeof(Job->Scene->Normals));
    FMemory::Memcpy(static_cast<uint8*>(Job->Constants->Mapped)+256,&Count,sizeof(Count));
    SensorUploadDiagnostics::RecordCopy(SensorUploadDiagnostics::ESite::NativeConstants,GFrameNumberRenderThread,sizeof(Job->Scene->Normals)+sizeof(Count),Job->Constants->Size);
    Job->Rays->EndWrite(uint64(Count)*sizeof(FSensorRtRay));Job->Constants->EndWrite(256+sizeof(Count));
    Job->List->SetComputeRootSignature(Backend->Root);
    Job->List->SetComputeRootShaderResourceView(0,Job->Scene->Storage->Address());
    Job->List->SetComputeRootShaderResourceView(1,Job->Rays->Address());
    auto Fallback=Job->Scene->Blases.FindByPredicate([](const auto& B){return B.IsValid();});
    for(int32 I=0;I<4;++I){auto B=Job->Scene->Blases.IsValidIndex(I)&&Job->Scene->Blases[I]?Job->Scene->Blases[I]:*Fallback;Job->List->SetComputeRootShaderResourceView(2+I,B->Surfaces->Address());}
    Job->List->SetComputeRootUnorderedAccessView(6,Job->Results->Address());Job->List->SetComputeRootConstantBufferView(7,Job->Constants->Address());
    const uint32 Groups=FMath::DivideAndRoundUp(uint32(Count),64u);
    Job->List->Dispatch(FMath::Min(Groups,65535u),FMath::DivideAndRoundUp(Groups,65535u),1);
    D3D12_RESOURCE_BARRIER Transition={};Transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Transition.Transition.pResource=Job->Results->Resource;Transition.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;Transition.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;Transition.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    Job->List->ResourceBarrier(1,&Transition);Job->List->CopyBufferRegion(Job->Readback->Resource,0,Job->Results->Resource,0,uint64(Count)*sizeof(FSensorRtHit));
    Swap(Transition.Transition.StateBefore,Transition.Transition.StateAfter);Job->List->ResourceBarrier(1,&Transition);
    if(!Succeeded(Job->List->Close(),Error,TEXT("Close")))return {};
    // Explicitly pin every address recorded into this command list. This is
    // independent of cache ownership and includes BLAS build inputs/scratch.
    const uint64 Value=Backend->NextFence+1;
    auto Pin=[&](const FBufferPtr& B){if(B){B->LastUseFence=FMath::Max(B->LastUseFence,Value);Job->KeepAlive.Add(B);}};
    Pin(Job->Rays);Pin(Job->Results);Pin(Job->Readback);Pin(Job->Constants);
    Pin(Job->Scene->Instances);Pin(Job->Scene->Storage);Pin(Job->Scene->Scratch);
    for(const auto& B:Job->Scene->Blases)if(B){Pin(B->Vertices);Pin(B->Surfaces);Pin(B->Storage);Pin(B->Scratch);}
    if(LedgerEnabled())
    {
        UE_LOG(LogTemp,Display,TEXT("[SensorNativeSubmit] fence=%llu completed=%llu TLAS=0x%llx constants=0x%llx rays=0x%llx results=0x%llx count=%d SBT=none local_root=none native_inline=1"),Value,LastCompleted.load(),uint64(Job->Scene->Storage->Address()),uint64(Job->Constants->Address()),uint64(Job->Rays->Address()),uint64(Job->Results->Address()),Count);
        for(int32 I=0;I<Job->Scene->Blases.Num();++I)if(const auto& B=Job->Scene->Blases[I])UE_LOG(LogTemp,Display,TEXT("[SensorNativeBinding] fence=%llu mesh=%d surface=0x%llx blas=0x%llx surface_bytes=%llu"),Value,I,uint64(B->Surfaces->Address()),uint64(B->Storage->Address()),B->Surfaces->Size);
    }
    ID3D12CommandList* Lists[]={Job->List};Backend->Queue->ExecuteCommandLists(1,Lists);
    Submitted=true;
    Job->FenceValue=++Backend->NextFence;
    if(!Succeeded(Backend->Queue->Signal(Backend->Fence,Job->FenceValue),Error,TEXT("Signal"))){Backend->Faulted=true;Backend->Retired.Add(Job);return {};}
    ++Dispatches;return MakeShared<FTicket,ESPMode::ThreadSafe>(MoveTemp(Job));
}
bool IsSupported()
{
    if(!IsRHID3D12()||GNumExplicitGPUsForRendering!=1)return false;
    auto* Rhi=GetID3D12DynamicRHI();if(Rhi->RHIGetDeviceNodeMask(0)!=1)return false;
    static const bool Supported=[Rhi](){D3D12_FEATURE_DATA_D3D12_OPTIONS5 Options={};return SUCCEEDED(Rhi->RHIGetDevice(0)->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&Options,sizeof(Options)))&&Options.RaytracingTier>=D3D12_RAYTRACING_TIER_1_1;}();
    return Supported;
}
FStats GetStats(){return {Dispatches.load(),BlasBuilds.load(),SceneBuilds.load(),PacketCreates.load(),PacketReuses.load(),Bytes.load(),PeakBytes.load(),ValidationErrors.load(),ValidationAvailable.load(),BufferCreates.load(),BufferReuses.load()};}
bool IsHealthy(){return !Backend||(!Backend->Faulted&&SUCCEEDED(Backend->Device->GetDeviceRemovedReason()));}
bool Shutdown(){check(IsInRenderingThread());bool Healthy=true;if(Backend){Backend->Drain();Healthy=IsHealthy();Backend.Reset();}return Healthy;}
}
#else
namespace SensorNativeRayTracing
{
TSharedPtr<ITicket,ESPMode::ThreadSafe> Submit(const TArray<FSensorRtMesh>&,const TArray<TConstArrayView<FSensorRtRay>>&,FString& Error,bool& BudgetLimited){BudgetLimited=false;Error=TEXT("native LiDAR requires Windows DXR");return {};}
bool IsSupported(){return false;}
bool IsHealthy(){return true;}
FStats GetStats(){return {};}
bool Shutdown(){return true;}
}
#endif
