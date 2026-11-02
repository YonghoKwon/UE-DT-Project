#pragma once
#include "CoreMinimal.h"

// Process-only opt-in instrumentation. No polling, logging, allocations or
// saved state in Record*. RHI request bytes are not driver heap reservation.
namespace SensorUploadDiagnostics
{
enum class EDomain:uint8 { RhiRequest,DirectMappedWrite,EngineManagedInput,Readback,Count };
enum class ESite:uint8
{
    PreviewPosition,PreviewColor,PreviewStatic,PreviewProxy,PreviewLegacy,
    NativeRays,NativeGeometry,NativeInstances,NativeConstants,NativeReadback,
    OwnedScene,OwnedRays,OwnedResults,WorldPreviewInstances,SemanticInstances,
    CsvInstances,StressInstances,ScanTexture,VisualizationTexture,PreviewUniform,SceneMeshInput,Count
};
constexpr uint32 SiteCount=uint32(ESite::Count),DomainCount=uint32(EDomain::Count);
struct FSiteStats
{
    uint64 Bytes=0,CopyCalls=0,Creations=0,Growths=0,Shrinks=0,Skipped=0;
    uint64 LastCapacity=0,MaxCapacity=0,PeakFrameBytes=0,PeakFrameCalls=0;
};
struct FSnapshot
{
    FSiteStats Sites[SiteCount];
    uint64 PeakFrameBytes[DomainCount]{},PeakFrameCalls[DomainCount]{};
    uint64 RecordCalls=0,RecordCpuCycles=0;
};
MA0T10_DTBOOTSTRAP_API const TCHAR* SiteName(ESite Site);
MA0T10_DTBOOTSTRAP_API const TCHAR* DomainName(EDomain Domain);
MA0T10_DTBOOTSTRAP_API EDomain Domain(ESite Site);
MA0T10_DTBOOTSTRAP_API bool IsEnabled();

// Separately instantiable ledger for deterministic counter tests. Each domain
// has a processing-frame ring, so GT producer estimates never add to RT writes.
class MA0T10_DTBOOTSTRAP_API FAccumulator
{
    static constexpr uint32 RingSize=256;
    struct FFrame
    {
        uint64 Key=0,Bytes=0,Calls=0;
        uint64 SiteBytes[SiteCount]{},SiteCalls[SiteCount]{};
        bool Valid=false;
    };
    mutable FCriticalSection Mutex;
    FFrame Frames[DomainCount][RingSize];
    FSnapshot Stats;
public:
    void Copy(ESite Site,uint64 Frame,uint64 Bytes,uint64 Capacity);
    void Allocation(ESite Site,uint64 OldCapacity,uint64 NewCapacity);
    void Skip(ESite Site);
    void Reset();
    FSnapshot Snapshot() const;
};
MA0T10_DTBOOTSTRAP_API void RecordCopy(ESite Site,uint64 Frame,uint64 Bytes,uint64 Capacity);
MA0T10_DTBOOTSTRAP_API void RecordAllocation(ESite Site,uint64 OldCapacity,uint64 NewCapacity);
MA0T10_DTBOOTSTRAP_API void RecordSkipped(ESite Site);
MA0T10_DTBOOTSTRAP_API void ResetWindow();
MA0T10_DTBOOTSTRAP_API FSnapshot Snapshot();
}
