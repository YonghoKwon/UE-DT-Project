#pragma once

#include "HAL/LowLevelMemTracker.h"
#include "Misc/Optional.h"
#include "ProfilingDebugging/TagTrace.h"

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
MA0T10_DTBOOTSTRAP_API bool IsEnabled();
#if ENABLE_LOW_LEVEL_MEM_TRACKER
class FProjectScope
{
    TOptional<FLLMScope> Scope;
#if UE_MEMORY_TAGS_TRACE_ENABLED
    TOptional<FMemScope> TraceScope;
#endif
public:
    explicit FProjectScope(FLLMTagDeclaration& Tag)
    {
        if(IsEnabled())
        {
            const auto Name=Tag.GetUniqueName();Scope.Emplace(Name,false,ELLMTagSet::None,ELLMTracker::Default);
#if UE_MEMORY_TAGS_TRACE_ENABLED
            TraceScope.Emplace(Name);
#endif
        }
    }
};
#endif
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

#if ENABLE_LOW_LEVEL_MEM_TRACKER
#define MA0T10_LLM_SCOPE_BYTAG(Tag) MA0T10MemoryTags::FProjectScope PREPROCESSOR_JOIN(MA0T10Scope_,__LINE__)(PREPROCESSOR_JOIN(LLMTagDeclaration_,Tag))
#else
#define MA0T10_LLM_SCOPE_BYTAG(Tag)
#endif
