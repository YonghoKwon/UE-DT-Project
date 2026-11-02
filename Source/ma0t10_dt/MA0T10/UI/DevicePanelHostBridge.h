#pragma once
#include "CoreMinimal.h"
/** Optional host services, never a widget manager. No Workspace/Slab/TrainBatch types. */
class IDevicePanelHostBridge
{
public:
    virtual ~IDevicePanelHostBridge() = default;
    virtual bool IsOwned() const = 0;
    virtual float PixelScale() const = 0;
    virtual bool NavigateBack() = 0;
    virtual FText BackLabel() const = 0;
};
