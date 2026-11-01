#include "LidarMemoryAttributionProbe.h"
#include "Core/DxWidgetSubsystem.h"
#include "Core/DxDataSubsystem.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Core/DxLogSubsystem.h"
#include "Player/DxPlayerControllerBase.h"
#include "UI/DxWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "UObject/UObjectIterator.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/CoreDelegates.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "HAL/LowLevelMemTracker.h"
#include "Trace/SlateTrace.h"
#include "SensorOwnedRayTracing.h"
#include "DynamicRHI.h"
#include "RHIStats.h"

#if !UE_BUILD_SHIPPING
// This engine-version-specific counter is read only in monolithic diagnostics.
// Editor DLLs do not export it; they report UMG counts without inventing a Slate total.
#if UE_SLATE_TRACE_ENABLED && IS_MONOLITHIC
namespace SlateTraceDetail { extern uint32 GWidgetCount; }
#endif

namespace
{
// Explicit-instantiation member access keeps DTCore sources/ABI unchanged.
// Only sizes/counters are read; no keys, message bodies or log text are serialized.
template<class Tag,typename Tag::Member Ptr>struct TReadMember
{friend typename Tag::Member ReadMember(Tag){return Ptr;}};
#define R36_MEMBER(Tag,Class,Type,Field) \
struct Tag{using Member=Type Class::*;friend Member ReadMember(Tag);}; \
template struct TReadMember<Tag,&Class::Field>;
using FOpenArray=TArray<TObjectPtr<UDxWidget>>;
using FStringSet=TSet<FString>;
using FLogArray=TArray<TPair<FString,FString>>;
R36_MEMBER(FOpen,UDxWidgetSubsystem,FOpenArray,OpenWidgets)
R36_MEMBER(FWarnApi,UDxDataSubsystem,FStringSet,WarnedApiKeys)
R36_MEMBER(FWarnWs,UDxDataSubsystem,FStringSet,WarnedWebSocketKeys)
R36_MEMBER(FUnknownMutex,UDxDataSubsystem,FCriticalSection,UnknownHandlerMutex)
R36_MEMBER(FRouteTopics,UDxWebSocketSubsystem,FStringSet,RouteLoggedTopics)
R36_MEMBER(FLogBuffer,UDxLogSubsystem,FLogArray,LogBuffer)
R36_MEMBER(FLogMutex,UDxLogSubsystem,FCriticalSection,BufferMutex)
R36_MEMBER(FHoverTimer,ADxPlayerControllerBase,float,LastHoverCheckTime)
#undef R36_MEMBER

TWeakObjectPtr<UWorld> ObservedWorld;
TWeakObjectPtr<ADxPlayerControllerBase> Controller;
FDelegateHandle BeginHandle,EndHandle;
double BeforeTimer=0;
bool HaveBefore=false;
uint64 ObserverFrames=0,HoverTimerResets=0,AmbiguousHoverFrames=0;
double ObserverCycles=0;
struct FClassCount{int64 Count=0,BodyBytes=0;};
TMap<UClass*,FClassCount> ClassCounts;

void BeforeFrame()
{
    const uint64 Start=FPlatformTime::Cycles64();HaveBefore=false;
    if(UWorld* World=ObservedWorld.Get())
    {
        Controller=Cast<ADxPlayerControllerBase>(World->GetFirstPlayerController());
        if(auto* PC=Controller.Get())
        {BeforeTimer=PC->*ReadMember(FHoverTimer{});HaveBefore=true;}
    }
    ObserverCycles+=double(FPlatformTime::Cycles64()-Start);
}
void AfterFrame()
{
    const uint64 Start=FPlatformTime::Cycles64();
    if(HaveBefore)if(auto* PC=Controller.Get())
    {
        const double After=PC->*ReadMember(FHoverTimer{});++ObserverFrames;
        if(After<BeforeTimer)++HoverTimerResets;
        else if(After==0&&BeforeTimer==0&&PC->GetWorld()->GetDeltaSeconds()>=.05f)++AmbiguousHoverFrames;
    }
    ObserverCycles+=double(FPlatformTime::Cycles64()-Start);
}
void SetSizes(const TSharedRef<FJsonObject>& Out,const TCHAR* Prefix,const FStringSet& Set)
{
    Out->SetNumberField(FString(Prefix)+TEXT("_count"),Set.Num());
    Out->SetNumberField(FString(Prefix)+TEXT("_container_bytes"),double(Set.GetAllocatedSize()));
    // Bounded read under the owner's lock (or GT-owned container); no strings copied.
    if(Set.Num()<=100000)
    {uint64 Bytes=0;for(const FString& Value:Set)Bytes+=Value.GetAllocatedSize();Out->SetNumberField(FString(Prefix)+TEXT("_string_bytes"),double(Bytes));}
    else Out->SetBoolField(FString(Prefix)+TEXT("_string_size_skipped_over_100k"),true);
}
}
#endif

bool LidarMemoryAttribution::IsEnabled()
{
#if !UE_BUILD_SHIPPING
    static const bool Enabled=FParse::Param(FCommandLine::Get(),TEXT("LidarMapMemoryAttribution"));return Enabled;
#else
    return false;
#endif
}
void LidarMemoryAttribution::Begin(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    if(!IsEnabled())return;
    check(IsInGameThread());ObservedWorld=World;
    BeginHandle=FCoreDelegates::OnBeginFrame.AddStatic(&BeforeFrame);
    EndHandle=FCoreDelegates::OnEndFrame.AddStatic(&AfterFrame);
    TRACE_BOOKMARK(TEXT("MEMORY26B_BEGIN"));
#endif
}
void LidarMemoryAttribution::Snapshot(UWorld* World,double Seconds,const TSharedRef<FJsonObject>& Row)
{
#if !UE_BUILD_SHIPPING
    if(!IsEnabled()||!World)return;
    check(IsInGameThread());LLM_SCOPE_BYNAME(TEXT("MA0T10/R36Diagnostics"));
    const double Start=FPlatformTime::Seconds();auto D=MakeShared<FJsonObject>();
    auto* ActualController=World->GetFirstPlayerController();
    const bool HoverObserverAvailable=Cast<ADxPlayerControllerBase>(ActualController)!=nullptr;
    D->SetBoolField(TEXT("hover_timer_observer_available"),HoverObserverAvailable);
    if(ActualController)
    {
        D->SetStringField(TEXT("player_controller_class"),ActualController->GetClass()->GetPathName());
        D->SetBoolField(TEXT("player_controller_tick_enabled"),ActualController->IsActorTickEnabled());
        D->SetBoolField(TEXT("player_controller_show_cursor"),ActualController->bShowMouseCursor);
    }
    D->SetNumberField(TEXT("observed_frames"),double(ObserverFrames));
    D->SetNumberField(TEXT("hover_timer_reset_calls"),double(HoverTimerResets));
    D->SetNumberField(TEXT("hover_ambiguous_hitch_frames"),double(AmbiguousHoverFrames));
    D->SetNumberField(TEXT("minimum_is_mouse_over_calls"),double(HoverTimerResets));
    D->SetNumberField(TEXT("minimum_prune_calls"),double(HoverTimerResets));
    if(!HoverObserverAvailable)
    {D->SetField(TEXT("minimum_is_mouse_over_calls"),MakeShared<FJsonValueNull>());D->SetField(TEXT("minimum_prune_calls"),MakeShared<FJsonValueNull>());}
    D->SetNumberField(TEXT("observer_total_gt_ms"),ObserverCycles*FPlatformTime::GetSecondsPerCycle64()*1000.);
    D->SetNumberField(TEXT("fname_entry_bytes"),FName::GetNameEntryMemorySize());
    D->SetNumberField(TEXT("fname_ansi_count"),FName::GetNumAnsiNames());
    D->SetNumberField(TEXT("fname_wide_count"),FName::GetNumWideNames());
    const auto Native=SensorOwnedRayTracing::GetSubmissionStats();
    D->SetNumberField(TEXT("native_rt_live_bytes"),double(Native.NativeBytes));
    D->SetNumberField(TEXT("native_rt_peak_bytes"),double(Native.NativePeakBytes));
    D->SetNumberField(TEXT("native_buffer_creates"),double(Native.NativeBufferCreates));
    D->SetNumberField(TEXT("native_blas_builds"),double(Native.NativeBlasBuilds));
    D->SetNumberField(TEXT("native_tlas_builds"),double(Native.NativeSceneBuilds));
    if(GDynamicRHI)
    {
        FTextureMemoryStats Rhi;RHIGetTextureMemoryStats(Rhi);auto J=MakeShared<FJsonObject>();
        J->SetNumberField(TEXT("streaming_texture_bytes"),double(Rhi.StreamingMemorySize));
        J->SetNumberField(TEXT("nonstreaming_texture_bytes"),double(Rhi.NonStreamingMemorySize));
        J->SetNumberField(TEXT("texture_pool_bytes"),double(Rhi.TexturePoolSize));
        J->SetNumberField(TEXT("total_graphics_budget_bytes"),double(Rhi.TotalGraphicsMemory));
        D->SetObjectField(TEXT("rhi_texture_memory"),J);
    }
    D->SetStringField(TEXT("call_count_method"),TEXT("Read-only pre/post controller hover timer reset observation; source CheckMouseHover->IsMouseOverAnyWidget->Prune once. Extra Blueprint/UI/input calls not claimed; trace allocation traffic is independent evidence."));
    if(auto* GI=World->GetGameInstance())
    {
        if(auto* W=GI->GetSubsystem<UDxWidgetSubsystem>())
        {
            const auto& Open=W->*ReadMember(FOpen{});
            D->SetNumberField(TEXT("dx_open_widgets"),Open.Num());D->SetNumberField(TEXT("dx_open_widgets_capacity_bytes"),double(Open.GetAllocatedSize()));
        }
        if(auto* Data=GI->GetSubsystem<UDxDataSubsystem>())
        {
            const auto S=Data->GetStatistics();auto J=MakeShared<FJsonObject>();
            J->SetNumberField(TEXT("processed_api"),double(S.ProcessedApi));J->SetNumberField(TEXT("processed_websocket"),double(S.ProcessedWebSocket));
            J->SetNumberField(TEXT("json_failures"),double(S.JsonParseFailures));J->SetNumberField(TEXT("missing_dispatch"),double(S.MissingDispatchKeys));
            J->SetNumberField(TEXT("unknown_handler"),double(S.UnknownHandlerKeys));J->SetNumberField(TEXT("queued_api"),double(S.QueuedApi));J->SetNumberField(TEXT("queued_websocket"),double(S.QueuedWebSocket));
            auto& Mutex=Data->*ReadMember(FUnknownMutex{});const bool Locked=Mutex.TryLock();J->SetBoolField(TEXT("warned_sets_read"),Locked);
            if(Locked){SetSizes(J,TEXT("warned_api_keys"),Data->*ReadMember(FWarnApi{}));SetSizes(J,TEXT("warned_websocket_keys"),Data->*ReadMember(FWarnWs{}));Mutex.Unlock();}
            D->SetObjectField(TEXT("dx_data"),J);
        }
        if(auto* Web=GI->GetSubsystem<UDxWebSocketSubsystem>())
        {
            auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("connection_state"),int32(Web->GetConnectionState()));J->SetNumberField(TEXT("reconnect_attempts"),Web->GetReconnectAttemptCount());
            J->SetNumberField(TEXT("last_subscribe_failures"),Web->GetLastSubscribeFailureCount());SetSizes(J,TEXT("route_logged_topics"),Web->*ReadMember(FRouteTopics{}));D->SetObjectField(TEXT("dx_websocket"),J);
        }
        if(auto* Log=GI->GetSubsystem<UDxLogSubsystem>())
        {
            auto J=MakeShared<FJsonObject>();auto& Mutex=Log->*ReadMember(FLogMutex{});const bool Locked=Mutex.TryLock();J->SetBoolField(TEXT("buffer_read"),Locked);
            if(Locked){const auto& Buffer=Log->*ReadMember(FLogBuffer{});uint64 Bytes=0;for(const auto& Pair:Buffer)Bytes+=Pair.Key.GetAllocatedSize()+Pair.Value.GetAllocatedSize();J->SetNumberField(TEXT("buffer_count"),Buffer.Num());J->SetNumberField(TEXT("buffer_container_bytes"),double(Buffer.GetAllocatedSize()));J->SetNumberField(TEXT("buffer_string_bytes"),double(Bytes));Mutex.Unlock();}
            D->SetObjectField(TEXT("dx_log"),J);
        }
    }
    ClassCounts.Reset();int64 Widgets=0,UserWidgets=0,Textures=0,RenderTargets=0;
    for(TObjectIterator<UObject> I;I;++I)
    {
        UObject* Object=*I;auto& C=ClassCounts.FindOrAdd(Object->GetClass());++C.Count;C.BodyBytes+=Object->GetClass()->GetStructureSize();
        Widgets+=Object->IsA<UWidget>();UserWidgets+=Object->IsA<UUserWidget>();Textures+=Object->IsA<UTexture>();RenderTargets+=Object->IsA<UTextureRenderTarget>();
    }
    auto Classes=MakeShared<FJsonObject>();auto Bodies=MakeShared<FJsonObject>();
    for(const auto& C:ClassCounts){const FString Name=C.Key->GetPathName();Classes->SetNumberField(Name,double(C.Value.Count));Bodies->SetNumberField(Name,double(C.Value.BodyBytes));}
    D->SetObjectField(TEXT("uobject_class_counts"),Classes);D->SetObjectField(TEXT("uobject_class_body_bytes_lower_bound"),Bodies);
    D->SetNumberField(TEXT("umg_widget_count"),double(Widgets));D->SetNumberField(TEXT("umg_user_widget_count"),double(UserWidgets));D->SetNumberField(TEXT("texture_object_count"),double(Textures));D->SetNumberField(TEXT("render_target_object_count"),double(RenderTargets));
#if UE_SLATE_TRACE_ENABLED && IS_MONOLITHIC
    D->SetBoolField(TEXT("slate_global_count_available"),true);D->SetNumberField(TEXT("slate_widget_count"),SlateTraceDetail::GWidgetCount);
#else
    D->SetBoolField(TEXT("slate_global_count_available"),false);
#endif
    D->SetNumberField(TEXT("snapshot_total_gt_ms"),(FPlatformTime::Seconds()-Start)*1000.);Row->SetObjectField(TEXT("attribution"),D);
    const int32 Minute=FMath::RoundToInt(Seconds/60.);
    if(Minute==10){TRACE_BOOKMARK(TEXT("R36_SNAPSHOT_10"));}
    if(Minute==60){TRACE_BOOKMARK(TEXT("R36_SNAPSHOT_60"));}
    if(Minute==120){TRACE_BOOKMARK(TEXT("R36_SNAPSHOT_120"));}
    if(Minute==180){TRACE_BOOKMARK(TEXT("R36_SNAPSHOT_180"));}
#endif
}
void LidarMemoryAttribution::End()
{
#if !UE_BUILD_SHIPPING
    if(!IsEnabled())return;TRACE_BOOKMARK(TEXT("MEMORY26B_END"));
    FCoreDelegates::OnBeginFrame.Remove(BeginHandle);FCoreDelegates::OnEndFrame.Remove(EndHandle);Controller.Reset();ObservedWorld.Reset();ClassCounts.Reset();
#endif
}
