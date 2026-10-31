#pragma once
#include "SensorOwnedRayTracing.h"

// Render-thread entry points. No engine RT scene or engine-owned residency
// handle is retained. All native GPU work and terminal fences use one owned
// queue fence. The default uses an independent list on the graphics queue;
// it never records into an engine command list or inherits its binding state.
namespace SensorNativeRayTracing
{
    class ITicket
    {
    public:
        virtual ~ITicket()=default;
        virtual bool Poll(TArray<FSensorRtHit>& Hits,FString& Error,bool Cancelled)=0;
    };
    struct FStats { uint64 Dispatches=0,BlasBuilds=0,SceneBuilds=0,PacketCreates=0,PacketReuses=0,Bytes=0,PeakBytes=0,ValidationErrors=0; bool ValidationAvailable=false; uint64 BufferCreates=0,BufferReuses=0; };
    bool IsSupported();
    bool IsHealthy();
    TSharedPtr<ITicket,ESPMode::ThreadSafe> Submit(const TArray<FSensorRtMesh>& Meshes,const TArray<TConstArrayView<FSensorRtRay>>& Parts,FString& Error,bool& BudgetLimited);
    FStats GetStats();
    bool Shutdown();
}
