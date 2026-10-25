#pragma once
#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"

// Process-only experiments. No CVar, saved setting, quality or cadence change.
namespace Memory26
{
enum class ECase { None, Baseline, Preview, Scene, Readback, Points, Pcd, Invalid };
enum class ECounter { SceneBuild, SceneReuse, BlasBuild, ReadbackCreate, ReadbackReuse, HitCreate, HitReuse,
    PreviewCreate, PreviewReuse, PcdCreate, PcdReuse, PointPoolCreate, PointFallback, PreviewFixedProxy, PreviewLegacyProxy, Count };
MA0T10_DTBOOTSTRAP_API ECase GetCase();
MA0T10_DTBOOTSTRAP_API const TCHAR* CaseName();
MA0T10_DTBOOTSTRAP_API void Count(ECounter Counter);
MA0T10_DTBOOTSTRAP_API uint64 Value(ECounter Counter);
MA0T10_DTBOOTSTRAP_API const TCHAR* CounterName(ECounter Counter);
inline bool Enabled(ECase C){return GetCase()==C;}

// A lease is owned by one caller until explicitly returned. Exhaustion creates
// an ordinary temporary; it never waits, evicts a live lease or aliases payloads.
// Both retained count and total capacity are bounded, across all worker threads.
template<class T,int32 MaxItems=4,uint64 MaxBytes=32ull*1024*1024>
class TPool
{
    FCriticalSection Mutex;
    TArray<TUniquePtr<T>> Free;
    uint64 Bytes=0;
public:
    TUniquePtr<T> Take(ECounter Created,ECounter Reused)
    {
        {FScopeLock Lock(&Mutex);if(!Free.IsEmpty()){auto P=Free.Pop(false);Bytes-=P->Bytes();Count(Reused);return P;}}
        Count(Created);return MakeUnique<T>();
    }
    void Return(TUniquePtr<T> P)
    {
        if(!P)return;const uint64 Size=P->Bytes();
        FScopeLock Lock(&Mutex);if(Free.Num()<MaxItems&&Size<=MaxBytes-Bytes){Bytes+=Size;Free.Add(MoveTemp(P));}
    }
    uint64 RetainedBytes(){FScopeLock Lock(&Mutex);return Bytes;}
    int32 RetainedCount(){FScopeLock Lock(&Mutex);return Free.Num();}
};
}
