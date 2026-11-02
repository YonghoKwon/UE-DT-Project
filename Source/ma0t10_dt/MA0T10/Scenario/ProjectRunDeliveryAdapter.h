#pragma once
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
/** Scenario UI/drain policy remains in the host; runtime only exposes neutral run accounting. */
namespace ProjectRunDeliveryAdapter
{
    inline int64 Pending(const UVirtualSensorStreamPublisherComponent& Source,const FString& RunId) {return Source.GetPendingRunCount(RunId);}
    inline int64 Failed(const UVirtualSensorStreamPublisherComponent& Source,const FString& RunId) {return Source.GetFailedRunCount(RunId);}
    inline void Gather(const UVirtualSensorStreamPublisherComponent& Source,const FString& RunId,TSet<FString>& Outputs,TSet<FString>& Frames,int64* Preparation=nullptr,int64* Receipts=nullptr)
    {Source.GatherPendingRunKeys(RunId,Outputs,Frames,Preparation,Receipts);}
}
