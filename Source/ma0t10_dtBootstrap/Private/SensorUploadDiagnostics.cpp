#include "SensorUploadDiagnostics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"

namespace SensorUploadDiagnostics
{
const TCHAR* SiteName(ESite Site)
{
    static const TCHAR* Names[]={TEXT("preview_position"),TEXT("preview_color"),TEXT("preview_static"),TEXT("preview_proxy"),TEXT("preview_legacy"),
        TEXT("native_rays"),TEXT("native_geometry"),TEXT("native_instances"),TEXT("native_constants"),TEXT("native_readback"),
        TEXT("owned_scene"),TEXT("owned_rays"),TEXT("owned_results"),TEXT("world_preview_instances"),TEXT("semantic_instances"),
        TEXT("csv_instances"),TEXT("stress_instances"),TEXT("scan_texture"),TEXT("visualization_texture"),TEXT("preview_uniform_input"),TEXT("scene_mesh_position_index_input")};
    static_assert(UE_ARRAY_COUNT(Names)==SiteCount);return Names[uint32(Site)];
}
const TCHAR* DomainName(EDomain Value)
{
    static const TCHAR* Names[]={TEXT("rhi_request"),TEXT("direct_mapped_write"),TEXT("engine_managed_input"),TEXT("readback")};
    return Names[uint32(Value)];
}
EDomain Domain(ESite Site)
{
    switch(Site)
    {
    case ESite::NativeRays:case ESite::NativeGeometry:case ESite::NativeInstances:case ESite::NativeConstants:return EDomain::DirectMappedWrite;
    case ESite::NativeReadback:return EDomain::Readback;
    case ESite::PreviewStatic:case ESite::PreviewProxy:case ESite::PreviewLegacy:case ESite::WorldPreviewInstances:case ESite::SemanticInstances:
    case ESite::CsvInstances:case ESite::StressInstances:case ESite::ScanTexture:case ESite::VisualizationTexture:case ESite::PreviewUniform:case ESite::SceneMeshInput:return EDomain::EngineManagedInput;
    default:return EDomain::RhiRequest;
    }
}
bool IsEnabled(){static const bool Enabled=FParse::Param(FCommandLine::Get(),TEXT("SensorUploadStats"));return Enabled;}
static FAccumulator& Ledger(){static FAccumulator Value;return Value;}
void FAccumulator::Copy(ESite Site,uint64 Frame,uint64 Bytes,uint64 Capacity)
{
    const uint64 Start=FPlatformTime::Cycles64();FScopeLock Lock(&Mutex);
    auto& S=Stats.Sites[uint32(Site)];S.Bytes+=Bytes;++S.CopyCalls;S.LastCapacity=Capacity;S.MaxCapacity=FMath::Max(S.MaxCapacity,Capacity);
    const uint32 D=uint32(Domain(Site));auto& F=Frames[D][Frame%RingSize];
    if(!F.Valid||F.Key!=Frame){F=FFrame();F.Valid=true;F.Key=Frame;}
    F.Bytes+=Bytes;++F.Calls;F.SiteBytes[uint32(Site)]+=Bytes;++F.SiteCalls[uint32(Site)];
    S.PeakFrameBytes=FMath::Max(S.PeakFrameBytes,F.SiteBytes[uint32(Site)]);S.PeakFrameCalls=FMath::Max(S.PeakFrameCalls,F.SiteCalls[uint32(Site)]);
    Stats.PeakFrameBytes[D]=FMath::Max(Stats.PeakFrameBytes[D],F.Bytes);Stats.PeakFrameCalls[D]=FMath::Max(Stats.PeakFrameCalls[D],F.Calls);
    ++Stats.RecordCalls;Stats.RecordCpuCycles+=FPlatformTime::Cycles64()-Start;
}
void FAccumulator::Allocation(ESite Site,uint64 OldCapacity,uint64 NewCapacity)
{
    const uint64 Start=FPlatformTime::Cycles64();FScopeLock Lock(&Mutex);auto& S=Stats.Sites[uint32(Site)];
    if(!OldCapacity)++S.Creations;else if(NewCapacity>OldCapacity)++S.Growths;else if(NewCapacity<OldCapacity)++S.Shrinks;
    S.LastCapacity=NewCapacity;S.MaxCapacity=FMath::Max(S.MaxCapacity,NewCapacity);++Stats.RecordCalls;Stats.RecordCpuCycles+=FPlatformTime::Cycles64()-Start;
}
void FAccumulator::Skip(ESite Site){const uint64 Start=FPlatformTime::Cycles64();FScopeLock Lock(&Mutex);++Stats.Sites[uint32(Site)].Skipped;++Stats.RecordCalls;Stats.RecordCpuCycles+=FPlatformTime::Cycles64()-Start;}
void FAccumulator::Reset(){FScopeLock Lock(&Mutex);Stats=FSnapshot();FMemory::Memzero(Frames,sizeof(Frames));}
FSnapshot FAccumulator::Snapshot()const{FScopeLock Lock(&Mutex);return Stats;}
void RecordCopy(ESite Site,uint64 Frame,uint64 Bytes,uint64 Capacity){if(IsEnabled())Ledger().Copy(Site,Frame,Bytes,Capacity);}
void RecordAllocation(ESite Site,uint64 OldCapacity,uint64 NewCapacity){if(IsEnabled())Ledger().Allocation(Site,OldCapacity,NewCapacity);}
void RecordSkipped(ESite Site){if(IsEnabled())Ledger().Skip(Site);}
void ResetWindow(){if(IsEnabled())Ledger().Reset();}
FSnapshot Snapshot(){return IsEnabled()?Ledger().Snapshot():FSnapshot();}
}
