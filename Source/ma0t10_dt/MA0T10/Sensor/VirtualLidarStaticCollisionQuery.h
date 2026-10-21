#pragma once
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorRayQuery.h"

/** Exact supported static collision only; an unsupported query body rejects the
 * whole GPU snapshot and retains native CPU behavior. No render LOD substitution. */
MA0T10_DT_API TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> CaptureLidarStaticCollision(UWorld* World,AActor* Ignore,ECollisionChannel Channel,FString& Reason);
