#pragma once

#include "HAL/LowLevelMemTracker.h"

// Shared by the existing Bootstrap, Runtime and Editor modules. No Runtime
// dependency is introduced into Bootstrap. UE 5.3 compiles these macros away
// when LLM is disabled at build time (including the default Shipping target).
LLM_DECLARE_TAG_API(MA0T10_LidarRtScene, MA0T10_DTBOOTSTRAP_API);
LLM_DECLARE_TAG_API(MA0T10_LidarReadback, MA0T10_DTBOOTSTRAP_API);
LLM_DECLARE_TAG_API(MA0T10_LidarPoints, MA0T10_DTBOOTSTRAP_API);
LLM_DECLARE_TAG_API(MA0T10_LidarPreview, MA0T10_DTBOOTSTRAP_API);
LLM_DECLARE_TAG_API(MA0T10_PcdEncode, MA0T10_DTBOOTSTRAP_API);
LLM_DECLARE_TAG_API(MA0T10_HatchAnalysis, MA0T10_DTBOOTSTRAP_API);
LLM_DECLARE_TAG_API(MA0T10_CameraGT, MA0T10_DTBOOTSTRAP_API);

namespace MA0T10MemoryTags
{
// LLM_DEFINE_TAG converts '_' in C++ identifiers to '/' in tracker names.
// Watch every leaf independently of top-N ranking. Never add parent/RHI totals
// to these amounts: scopes attribute allocations, they do not own GPU accounting.
inline constexpr const TCHAR* Names[] = {
    TEXT("MA0T10/LidarRtScene"),
    TEXT("MA0T10/LidarReadback"),
    TEXT("MA0T10/LidarPoints"),
    TEXT("MA0T10/LidarPreview"),
    TEXT("MA0T10/PcdEncode"),
    TEXT("MA0T10/HatchAnalysis"),
    TEXT("MA0T10/CameraGT")
};
inline constexpr int32 Count = sizeof(Names) / sizeof(Names[0]);
}
