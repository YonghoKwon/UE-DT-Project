#include "Memory26Diagnostics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include <atomic>

namespace Memory26
{
namespace {std::atomic<uint64> Counters[int32(ECounter::Count)]{};}
ECase GetCase()
{
    static const ECase Case=[]()
    {
        FString Name;if(!FParse::Value(FCommandLine::Get(),TEXT("Memory26Case="),Name))return ECase::None;
        if(!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryExperiment")))return ECase::Invalid;
        const TCHAR* Names[]={TEXT("baseline"),TEXT("preview-reuse"),TEXT("scene-reuse"),TEXT("readback-reuse"),TEXT("point-leases4"),TEXT("pcd-scratch-reuse")};
        for(int32 I=0;I<UE_ARRAY_COUNT(Names);++I)if(Name==Names[I])return ECase(I+1);
        return ECase::Invalid;
    }();return Case;
}
const TCHAR* CaseName()
{
    const TCHAR* Names[]={TEXT("none"),TEXT("baseline"),TEXT("preview-reuse"),TEXT("scene-reuse"),TEXT("readback-reuse"),TEXT("point-leases4"),TEXT("pcd-scratch-reuse"),TEXT("invalid")};
    return Names[int32(GetCase())];
}
void Count(ECounter C){if(GetCase()!=ECase::None)Counters[int32(C)].fetch_add(1,std::memory_order_relaxed);}
uint64 Value(ECounter C){return Counters[int32(C)].load(std::memory_order_relaxed);}
const TCHAR* CounterName(ECounter C)
{
    const TCHAR* Names[]={TEXT("tlas_build"),TEXT("tlas_reuse"),TEXT("blas_build"),TEXT("readback_create"),TEXT("readback_reuse"),TEXT("hit_array_create"),TEXT("hit_array_reuse"),TEXT("preview_array_create"),TEXT("preview_array_reuse"),TEXT("pcd_scratch_create"),TEXT("pcd_scratch_reuse"),TEXT("point_pool_create"),TEXT("point_pool_fallback"),TEXT("preview_fixed_proxy"),TEXT("preview_legacy_proxy")};
    static_assert(UE_ARRAY_COUNT(Names)==int32(ECounter::Count));return Names[int32(C)];
}
}
