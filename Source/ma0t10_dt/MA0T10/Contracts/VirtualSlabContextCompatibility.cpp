#include "VirtualSlabContextCompatibility.h"

TMap<FString,FString> FVirtualSlabFrameContext::ToHeaders() const
{
    return static_cast<FVirtualSensorAcquisitionContext>(*this).ToHeaders();
}
