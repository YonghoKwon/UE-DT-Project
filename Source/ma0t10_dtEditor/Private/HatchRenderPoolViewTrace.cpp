#include "HatchRenderPoolViewTrace.h"
#include "SceneViewExtension.h"
#include "SceneInterface.h"
#include "Engine/World.h"
#include "RenderTargetPool.h"
#include "Misc/ScopeLock.h"
#include "Trace/Trace.inl"

UE_TRACE_CHANNEL_DEFINE(HatchPoolChannel);
UE_TRACE_EVENT_BEGIN(HatchPool,ViewFamily,NoSync)
    UE_TRACE_EVENT_FIELD(uint64,Qpc)
    UE_TRACE_EVENT_FIELD(uint64,Family)
    UE_TRACE_EVENT_FIELD(uint64,Scene)
    UE_TRACE_EVENT_FIELD(uint8,Begin)
    UE_TRACE_EVENT_FIELD(uint32,Width)
    UE_TRACE_EVENT_FIELD(uint32,Height)
    UE_TRACE_EVENT_FIELD(UE::Trace::WideString,World)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(HatchPool,PoolEntry,NoSync)
    UE_TRACE_EVENT_FIELD(uint64,Qpc)
    UE_TRACE_EVENT_FIELD(uint64,Family)
    UE_TRACE_EVENT_FIELD(uint64,Rhi)
    UE_TRACE_EVENT_FIELD(uint64,Element)
    UE_TRACE_EVENT_FIELD(uint64,Flags)
    UE_TRACE_EVENT_FIELD(uint32,Width)
    UE_TRACE_EVENT_FIELD(uint32,Height)
    UE_TRACE_EVENT_FIELD(uint32,Depth)
    UE_TRACE_EVENT_FIELD(uint32,ArraySize)
    UE_TRACE_EVENT_FIELD(uint32,Mips)
    UE_TRACE_EVENT_FIELD(uint32,Samples)
    UE_TRACE_EVENT_FIELD(uint32,Format)
    UE_TRACE_EVENT_FIELD(uint32,Hash)
    UE_TRACE_EVENT_FIELD(uint32,Refs)
    UE_TRACE_EVENT_FIELD(uint32,Unused)
    UE_TRACE_EVENT_FIELD(UE::Trace::WideString,Name)
UE_TRACE_EVENT_END()
namespace
{
class FHatchPoolViewTrace final:public FSceneViewExtensionBase
{
    FCriticalSection Mutex;TMap<const FSceneInterface*,FString> Worlds;
    TMap<uint64,uint32> Previous;
    void Marker(FSceneViewFamily& Family,bool Begin)
    {
        FString World;{FScopeLock Lock(&Mutex);if(const auto* Name=Worlds.Find(Family.Scene))World=*Name;}
        const auto Size=Family.RenderTarget?Family.RenderTarget->GetSizeXY():FIntPoint::ZeroValue;
        UE_TRACE_LOG(HatchPool,ViewFamily,HatchPoolChannel)<<ViewFamily.Qpc(FPlatformTime::Cycles64())<<ViewFamily.Family(uint64(&Family))<<ViewFamily.Scene(uint64(Family.Scene))<<ViewFamily.Begin(Begin)<<ViewFamily.Width(Size.X)<<ViewFamily.Height(Size.Y)<<ViewFamily.World(*World);
    }
public:
    explicit FHatchPoolViewTrace(const FAutoRegister& Auto):FSceneViewExtensionBase(Auto){}
    virtual void SetupViewFamily(FSceneViewFamily& Family) override
    {if(Family.Scene){const auto* World=Family.Scene->GetWorld();FScopeLock Lock(&Mutex);Worlds.Add(Family.Scene,World?World->GetPathName():TEXT("<null>"));}}
    virtual void SetupView(FSceneViewFamily&,FSceneView&) override{}
    virtual void BeginRenderViewFamily(FSceneViewFamily&) override{}
    virtual void PreRenderViewFamily_RenderThread(FRDGBuilder&,FSceneViewFamily& Family) override{Marker(Family,true);}
    virtual void PostRenderViewFamily_RenderThread(FRDGBuilder&,FSceneViewFamily& Family) override
    {
        TMap<uint64,uint32> Current;Current.Reserve(GRenderTargetPool.GetElementCount());
        for(uint32 Index=0;Index<GRenderTargetPool.GetElementCount();++Index)if(auto* Entry=GRenderTargetPool.GetElementById(Index))if(auto* Rhi=Entry->GetRHI())
        {
            const auto Desc=Translate(Entry->GetDesc());const uint32 Hash=GetTypeHash(Desc);const uint64 Key=uint64(Rhi);Current.Add(Key,Hash);
            const auto* Old=Previous.Find(Key);if(Old&&*Old==Hash)continue;
            UE_TRACE_LOG(HatchPool,PoolEntry,HatchPoolChannel)<<PoolEntry.Qpc(FPlatformTime::Cycles64())<<PoolEntry.Family(uint64(&Family))<<PoolEntry.Rhi(Key)<<PoolEntry.Element(uint64(Entry))<<PoolEntry.Flags(uint64(Desc.Flags))<<PoolEntry.Width(Desc.Extent.X)<<PoolEntry.Height(Desc.Extent.Y)<<PoolEntry.Depth(Desc.Depth)<<PoolEntry.ArraySize(Desc.ArraySize)<<PoolEntry.Mips(Desc.NumMips)<<PoolEntry.Samples(Desc.NumSamples)<<PoolEntry.Format(uint32(Desc.Format))<<PoolEntry.Hash(Hash)<<PoolEntry.Refs(Entry->GetRefCount())<<PoolEntry.Unused(Entry->GetUnusedForNFrames())<<PoolEntry.Name(Entry->GetDesc().DebugName);
        }
        Previous=MoveTemp(Current);Marker(Family,false);
    }
};
}
TSharedPtr<ISceneViewExtension,ESPMode::ThreadSafe> CreateHatchRenderPoolViewTrace()
{return FSceneViewExtensions::NewExtension<FHatchPoolViewTrace>();}
