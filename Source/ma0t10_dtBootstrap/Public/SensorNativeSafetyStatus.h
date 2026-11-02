#pragma once
#include "CoreMinimal.h"

namespace SensorNativeSafety
{
    enum class EStatus : uint8 { None, Recovery, Banned };
    /** Read the prepared session's decision without opening files or starting RHI work. */
    MA0T10_DTBOOTSTRAP_API EStatus GetStatus();
}
