#pragma once
#include "CoreMinimal.h"

namespace SensorNativeSafety
{
    enum class EStatus : uint8 { None, Recovery, Banned };
    /** Read the prepared session's decision without opening files or starting RHI work. */
    MA0T10_DTBOOTSTRAP_API EStatus GetStatus();
    struct FSnapshot {EStatus Status=EStatus::None;int32 Failures=0;bool Faulted=false;};
    /** Read-only prepared guard/device evidence; does not begin a session. */
    MA0T10_DTBOOTSTRAP_API FSnapshot GetSnapshot();
    /** Current adapter/driver key, shared with isolated worker guard readers. */
    MA0T10_DTBOOTSTRAP_API FString StateFileName();
}
