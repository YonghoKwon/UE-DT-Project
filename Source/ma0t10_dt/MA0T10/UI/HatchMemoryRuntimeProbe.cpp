#include "HatchMemoryRuntimeProbe.h"
#include "Widgets/SWindow.h"
#include "LidarMapMemoryRuntimeProbe.h"
#include "HatchMemoryExperimentDiagnostics.h"
#include "MA0T10MemoryTags.h"
#include "SensorOwnedRayTracing.h"
#include "SensorUploadDiagnostics.h"

#include "ShipHatchMonitorHost.h"
#include "ShipHatchMonitorWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/MemoryBase.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Tickable.h"
#include "RenderingThread.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "UObject/UObjectArray.h"
#include "UnrealClient.h"
#include "Device/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "Device/Sensor/VirtualLidarScanComponent.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarVisualizationComponent.h"
#include "Device/Sensor/VirtualSensorEditableTypes.h"
#include "Device/Sensor/VirtualSensorTransportComponent.h"
#include "Device/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Device/Contracts/VirtualSensorRayQuery.h"

namespace
{
class FHatchMemoryRuntimeProbe final : public FTickableGameObject
{
    FString Directory, CancelFile, SourceCommit;
    bool ObserveReceive=false;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AShipHatchMonitorHost> Host;
    TWeakObjectPtr<UVirtualSensorStreamPublisherComponent> Publisher;
    TMap<FString,FVirtualSensorStreamStatus> PcdBefore;
    TMap<FString,int64> FramesBefore;
    TMap<FName,uint64> DefaultTags, PlatformTags;
    TUniquePtr<FArchive> File;
    TUniquePtr<TGuardValue<bool>> ScreenMessages;
    double Created=FPlatformTime::Seconds(), NextPoll=0, Changed=0, Started=0, LastSample=-1;
    double Seconds=3600, Interval=5, PreviousCost=0, CostSum=0, CostMax=0;
    int32 Port=0, Phase=0, WarmEdits=0, Samples=0, SensorCount=3, WarmDigTarget=17, RestoredAt=0;
    bool Finished=false, Failed=false, Llm=false;
    bool Investigation=false, TraceCanaryEnabled=false, DepthProjection=false, SkipWarmEdits=false, Isolation=false;
    bool AcquisitionOff=false, OutputsOff=false, CutApplied=false;
    double CutReadyAt=0;
    int32 CanaryStage=0;
    TArray<uint8> TraceCanary;
    TArray<uint8> CameraTagCanary;
    static FORCENOINLINE void ResizeMemoryTraceCanary(TArray<uint8>& Bytes,int32 Size){Bytes.SetNumZeroed(Size);}

    bool Write(const TSharedRef<FJsonObject>& Row)
    {
        if(!File)return false;
        FString Text;FJsonSerializer::Serialize(Row,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));Text+=TEXT("\n");
        FTCHARToUTF8 Utf8(*Text);File->Serialize((void*)Utf8.Get(),Utf8.Length());File->Flush();return !File->IsError();
    }
    static TSharedRef<FJsonObject> Top(const TMap<FName,uint64>& Values)
    {
        TArray<TPair<FName,uint64>,TInlineAllocator<16>> Best;
        for(const auto& P:Values)
        {
            int32 I=0;while(I<Best.Num()&&(Best[I].Value>P.Value||(Best[I].Value==P.Value&&Best[I].Key.LexicalLess(P.Key))))++I;
            if(I>=16)continue;if(Best.Num()==16)Best.Pop(false);Best.Insert(P,I);
        }
        auto R=MakeShared<FJsonObject>();for(const auto& P:Best)R->SetNumberField(P.Key.ToString(),double(P.Value));return R;
    }
    void SaveQualityOutsideMeasurement()
    {
        if(!DepthProjection||!Host.IsValid()||!Host->Simulation)return;
        auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("scope"),TEXT("After measurement footer only; up to256 uniformly spaced points/sensor vs static CPU collision snapshot; modeled/noisy returns retained; descriptive, not accuracy acceptance"));
        for(int32 I=0;I<SensorCount;++I)if(auto* A=Host->Simulation->SensorAt(I))
        {
            const auto Frame=A->ScanComponent->GetLastFrameSnapshot();const auto Query=Host->Simulation->CaptureRayQuery(A,ECC_Visibility);
            if(!Frame||!Frame->Points||!Query)continue;
            int32 Compared=0,MatchedHits=0,Disagree=0;double Sum=0,Max=0;
            const int32 Step=FMath::Max(1,Frame->Points->Num()/256);
            for(int32 J=0;J<Frame->Points->Num()&&Compared<256;J+=Step)
            {
                const auto& P=(*Frame->Points)[J];const FVector Start=Frame->AcquisitionTransform.GetLocation();
                const FVector End=Start+Frame->AcquisitionTransform.TransformVectorNoScale(P.LocalDirection)*Frame->MaxDistanceCm;
                FHitResult Hit;const bool CpuHit=Query->Trace(Start,End,Hit);++Compared;if(CpuHit!=P.bHit)++Disagree;
                if(CpuHit&&P.bHit){const double E=FMath::Abs(double(P.Distance)-FVector::Distance(Start,Hit.ImpactPoint));Sum+=E;Max=FMath::Max(Max,E);++MatchedHits;}
            }
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("compared"),Compared);Row->SetNumberField(TEXT("matched_hits"),MatchedHits);Row->SetNumberField(TEXT("hit_disagreements"),Disagree);Row->SetNumberField(TEXT("mean_abs_range_error_cm"),MatchedHits?Sum/MatchedHits:0);Row->SetNumberField(TEXT("max_abs_range_error_cm"),Max);Row->SetNumberField(TEXT("frame_id"),double(Frame->FrameId));Root->SetObjectField(A->GetSensorId(),Row);
        }
        FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("backend-quality.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
    TSharedRef<FJsonObject> Pcd(bool Check=false)
    {
        auto R=MakeShared<FJsonObject>();int32 Count=0;
        if(Publisher.IsValid())for(const auto& S:Publisher->GetStreamStatuses())if(S.StreamKind==EVirtualSensorStreamKind::PointCloud)
        {
            ++Count;const auto B=PcdBefore.FindRef(S.SensorId);auto J=MakeShared<FJsonObject>();
            J->SetNumberField(TEXT("input_frames"),S.InputFrameCount-B.InputFrameCount);
            J->SetNumberField(TEXT("serialized_frames"),S.SerializedFrameCount-B.SerializedFrameCount);
            J->SetNumberField(TEXT("submitted_frames"),S.SubmittedFrameCount-B.SubmittedFrameCount);
            J->SetNumberField(TEXT("receipt_frames"),S.ReceiptReceivedCount-B.ReceiptReceivedCount);
            J->SetNumberField(TEXT("consumer_received"),S.ConsumerReceivedCount-B.ConsumerReceivedCount);
            J->SetNumberField(TEXT("submitted_bytes"),double(S.TotalSubmittedBytes-B.TotalSubmittedBytes));
            J->SetNumberField(TEXT("encode_failures"),S.EncodeFailureCount-B.EncodeFailureCount);
            J->SetNumberField(TEXT("delivery_failures"),S.DeliveryFailureCount-B.DeliveryFailureCount);
            J->SetNumberField(TEXT("invalid"),S.ConsumerValidationFailureCount-B.ConsumerValidationFailureCount);
            J->SetNumberField(TEXT("gaps"),S.ConsumerFrameGapCount-B.ConsumerFrameGapCount);
            J->SetNumberField(TEXT("duplicates"),S.ConsumerDuplicateCount-B.ConsumerDuplicateCount);
            J->SetNumberField(TEXT("overloads"),S.OverloadCount-B.OverloadCount);
            J->SetNumberField(TEXT("rate_control_skipped"),S.RateControlSkippedFrameCount-B.RateControlSkippedFrameCount);
            J->SetNumberField(TEXT("budget_mib_s"),S.PcdBudgetMiBPerSecond);R->SetObjectField(S.SensorId,J);
            // External fixture validation is checked by the runner. Receipt is
            // transport evidence, never relabelled as consumer validation.
            if(Check&&(S.SubmittedFrameCount<=B.SubmittedFrameCount||S.ReceiptReceivedCount<=B.ReceiptReceivedCount||(ObserveReceive&&S.ConsumerReceivedCount<=B.ConsumerReceivedCount)||S.ConsumerValidationFailureCount!=B.ConsumerValidationFailureCount||S.EncodeFailureCount!=B.EncodeFailureCount))Failed=true;
        }
        if(Check&&Count!=SensorCount)Failed=true;return R;
    }
    void Finish(bool Complete,const FString& Reason)
    {
        if(Finished)return;Finished=true;Failed=!FinishHatchMemoryExperiment(Complete&&!Failed)||Failed;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("type"),TEXT("summary"));R->SetObjectField(TEXT("pcd"),Pcd(Complete&&!AcquisitionOff&&!OutputsOff));
        R->SetStringField(TEXT("reason"),Reason);R->SetBoolField(TEXT("complete"),Complete&&!Failed);R->SetBoolField(TEXT("diagnostic_only"),true);
        if(Complete&&!Failed){TRACE_BOOKMARK(TEXT("MEMORY26B_END"));}else{TRACE_BOOKMARK(TEXT("MEMORY26B_CANCEL"));}
        R->SetNumberField(TEXT("sample_count"),Samples);R->SetNumberField(TEXT("measurement_seconds"),Started?FPlatformTime::Seconds()-Started:0);
        R->SetNumberField(TEXT("clicks"),0);R->SetNumberField(TEXT("restores"),0);R->SetNumberField(TEXT("warm_edits"),WarmEdits);
        R->SetBoolField(TEXT("llm_enabled"),Llm);R->SetNumberField(TEXT("last_sample_total_gt_ms"),PreviousCost);
        R->SetNumberField(TEXT("sampler_total_gt_ms"),CostSum);R->SetNumberField(TEXT("sampler_max_gt_ms"),CostMax);
        const bool Saved=Write(R);if(File){Failed=!File->Close()||Failed;File.Reset();}
        if(!Saved){FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Directory,true);FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("probe-error.json")));}
        if(Complete&&!Failed&&Saved)SaveQualityOutsideMeasurement();
        // The observation/footer is complete. Quiesce only this diagnostic's
        // world/output before engine PreExit tears down scene-view delegates.
        // No stop/drain cost enters memory samples or their GT cost values.
        if(Host.IsValid()&&Host->Simulation)for(int32 I=0;I<3;++I)if(auto* A=Host->Simulation->SensorAt(I))A->StopSensor();
        if(Publisher.IsValid())Publisher->StopAllStreams(TEXT(""));
        if(World.IsValid())if(auto* View=World->GetGameViewport())View->bDisableWorldRendering=true;
        FlushRenderingCommands();
        FPlatformMisc::RequestExitWithStatus(false,Complete&&!Failed&&Saved?0:2);
    }
    bool ConfigurePcd(AShipHatchSimulation& S)
    {
        if(ObserveReceive)
            if(auto* Raw=World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>())Raw->ConfigureReceiver(FVirtualSensorReceiveSelection{});
        // Same existing project transport/publisher path and defaults as the
        // Editor memory fixture; no broker/user configuration is read or saved.
        auto* Transport=NewObject<UVirtualSensorTransportComponent>(&S);S.AddInstanceComponent(Transport);Transport->RegisterComponent();
        Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
        FVirtualSensorTransportProfile Profile;Profile.BrokerUrl=FString::Printf(TEXT("tcp://127.0.0.1:%d"),Port);Profile.UserName.Reset();
        Transport->ConfigureTransportProfile(Profile);Transport->SetSessionCredentials(TEXT(""),TEXT(""));
        auto* P=NewObject<UVirtualSensorStreamPublisherComponent>(&S);S.AddInstanceComponent(P);P->SetTransportComponent(Transport);P->RegisterComponent();Publisher=P;
        for(int32 I=0;I<SensorCount;++I)
        {
            auto* A=S.SensorAt(I);if(!A||!S.SensorEnabled(I))return false;
            A->StopSensor();A->SetSharedOutputServices(Transport,nullptr,P);
            if(DepthProjection){FString Error;if(!A->SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend::GpuDepthProjection,Error))return false;}
            FVirtualSensorStreamConfig Config;Config.SensorId=A->GetSensorId();Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.bEnabled=true;P->ConfigureStream(Config);A->StartSensor();
        }
        return true;
    }
    bool Begin(AShipHatchSimulation& S)
    {
        // The Rev7x3 fixture has no CameraGT workload. A separate30s opt-in
        // registry/writer smoke holds4KiB under that tag, explicitly labelled;
        // never enable it in trend comparisons or call it CameraGT acceptance.
        if(FParse::Param(FCommandLine::Get(),TEXT("Memory26CameraTagCanary")))
        {if(Seconds!=30)return false;MA0T10_LLM_SCOPE_BYTAG(MA0T10_CameraGT);CameraTagCanary.SetNumZeroed(4096);}
        const FString Path=Directory/TEXT("memory-trend.jsonl");if(IFileManager::Get().FileExists(*Path))return false;
        IFileManager::Get().MakeDirectory(*Directory,true);File.Reset(IFileManager::Get().CreateFileWriter(*Path,FILEWRITE_AllowRead));
#if ENABLE_LOW_LEVEL_MEM_TRACKER
        Llm=FLowLevelMemTracker::IsEnabled();
#endif
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("type"),TEXT("metadata"));R->SetNumberField(TEXT("schema_version"),1);
        R->SetStringField(TEXT("row_contract"),TEXT("memory_curve"));R->SetBoolField(TEXT("diagnostic_only"),true);
        R->SetNumberField(TEXT("requested_seconds"),Seconds);R->SetNumberField(TEXT("interval_seconds"),Interval);R->SetBoolField(TEXT("llm_enabled"),Llm);
        R->SetBoolField(TEXT("internal_receive_observation"),ObserveReceive);
        R->SetNumberField(TEXT("camera_gt_tag_canary_bytes"),CameraTagCanary.Num());
        R->SetBoolField(TEXT("project_tags_enabled"),MA0T10MemoryTags::IsEnabled());
        R->SetBoolField(TEXT("investigation"),Investigation);R->SetBoolField(TEXT("trace_canary"),TraceCanaryEnabled);R->SetStringField(TEXT("trace_bookmarks"),TEXT("MEMORY26B_BEGIN/END; shared allocation-query contract"));
        R->SetBoolField(TEXT("sampler_enabled"),true);R->SetBoolField(TEXT("pcd_loopback"),!AcquisitionOff&&!OutputsOff);R->SetBoolField(TEXT("pcd_default_compressed"),Publisher->GetPcdSendOptions(TEXT("")).bCompressed);
        R->SetStringField(TEXT("workload"),SkipWarmEdits?TEXT("diagnostic fixed geometry; no warm Dig/restore;35s warmup"):FString::Printf(TEXT("packaged idle;%d public near-surface Digs with fresh analysis, restore every17Digs,35s warmup; no Editor automation history"),WarmDigTarget));
        R->SetNumberField(TEXT("requested_warm_digs"),WarmDigTarget);
        R->SetNumberField(TEXT("diagnostic_sensor_count"),SensorCount);R->SetBoolField(TEXT("diagnostic_depth_projection"),DepthProjection);R->SetBoolField(TEXT("diagnostic_revision_changes"),!SkipWarmEdits);
        R->SetBoolField(TEXT("diagnostic_acquisition_off"),AcquisitionOff);R->SetBoolField(TEXT("diagnostic_outputs_off"),OutputsOff);
        R->SetStringField(TEXT("source_commit"),SourceCommit);R->SetStringField(TEXT("started_utc"),FDateTime::UtcNow().ToIso8601());
        R->SetStringField(TEXT("allocator"),GMalloc->GetDescriptiveName());
        if(Host.IsValid()&&Host->Monitor)R->SetNumberField(TEXT("font_percent"),Host->Monitor->GetFontScale()*100);
        if(World.IsValid())R->SetNumberField(TEXT("umg_viewport_scale"),UWidgetLayoutLibrary::GetViewportScale(World.Get()));
        if(World.IsValid())if(auto* V=World->GetGameViewport())if(auto Window=V->GetWindow())R->SetNumberField(TEXT("native_dpi_scale"),Window->GetDPIScaleFactor());
        R->SetStringField(TEXT("configuration"),
#if UE_BUILD_SHIPPING
            TEXT("Shipping")
#else
            TEXT("Development")
#endif
        );
        R->SetStringField(TEXT("uobject_method"),TEXT("GUObjectArray occupied slots; pending GC included"));
        R->SetStringField(TEXT("cost_method"),TEXT("previous_sample_total_gt_ms includes collection, JSON, write/flush and destruction; header/footer excluded"));
        bool ViewportValid=false;
        if(World.IsValid())if(auto* V=World->GetGameViewport();V&&V->Viewport){const auto Size=V->Viewport->GetSizeXY();R->SetNumberField(TEXT("viewport_x"),Size.X);R->SetNumberField(TEXT("viewport_y"),Size.Y);ViewportValid=Size==FIntPoint(1920,1080);}
        if(!Write(R)||!ViewportValid)return false;
        for(const auto& Status:Publisher->GetStreamStatuses())if(Status.StreamKind==EVirtualSensorStreamKind::PointCloud)PcdBefore.Add(Status.SensorId,Status);
        for(int32 I=0;I<SensorCount;++I)if(auto* A=S.SensorAt(I))FramesBefore.Add(A->GetSensorId(),A->ScanComponent->GetRuntimeStatus().FrameId);
        SensorUploadDiagnostics::ResetWindow();
        Started=FPlatformTime::Seconds();if(!BeginHatchMemoryExperiment(Directory))return false;TRACE_BOOKMARK(TEXT("MEMORY26B_BEGIN"));return true;
    }
    void Sample(AShipHatchSimulation& S)
    {
        const double CostStart=FPlatformTime::Seconds();
        if(!SampleHatchMemoryExperiment(CostStart-Started))Failed=true;
        {
            const auto Mem=FPlatformMemory::GetStats();auto R=MakeShared<FJsonObject>();
            R->SetStringField(TEXT("type"),TEXT("memory_curve"));R->SetNumberField(TEXT("seconds"),CostStart-Started);R->SetNumberField(TEXT("cycle"),0);
            R->SetNumberField(TEXT("rss_bytes"),double(Mem.UsedPhysical));R->SetNumberField(TEXT("commit_bytes"),double(Mem.UsedVirtual));
            R->SetNumberField(TEXT("uobject_count"),GUObjectArray.GetObjectArrayNumMinusAvailable());R->SetBoolField(TEXT("llm_enabled"),Llm);
#if ENABLE_LOW_LEVEL_MEM_TRACKER
            DefaultTags.Reset();PlatformTags.Reset();
            if(Llm){auto& L=FLowLevelMemTracker::Get();L.GetTrackedTagsNamesWithAmount(DefaultTags,ELLMTracker::Default,ELLMTagSet::None);L.GetTrackedTagsNamesWithAmount(PlatformTags,ELLMTracker::Platform,ELLMTagSet::None);}
#endif
            auto Watched=MakeShared<FJsonObject>();
            // Match the Editor row's fixed ten counters, including zero tags.
            for(const TCHAR* Name:{TEXT("Physics"),TEXT("ChaosTrimesh"),TEXT("UObject"),TEXT("UI"),TEXT("RHIMisc"),TEXT("Textures"),TEXT("FMallocUnused"),TEXT("Hatch/CargoCook"),TEXT("Hatch/RayTree"),TEXT("Hatch/CargoUpdate")})
                Watched->SetNumberField(Name,double(DefaultTags.FindRef(FName(Name))));
            for(const TCHAR* Name:{TEXT("Untracked"),TEXT("TrackedTotal"),TEXT("Total"),TEXT("FMalloc"),TEXT("ThreadStack"),TEXT("EngineMisc"),TEXT("RenderTargets")})
                if(const auto* V=DefaultTags.Find(FName(Name)))Watched->SetNumberField(Name,double(*V));
            auto Platform=MakeShared<FJsonObject>();for(const TCHAR* Name:{TEXT("ThreadStackPlatform"),TEXT("FMalloc"),TEXT("PlatformUntracked"),TEXT("GraphicsPlatform"),TEXT("PlatformTotal"),TEXT("PlatformOverhead")})if(const auto* V=PlatformTags.Find(FName(Name)))Platform->SetNumberField(Name,double(*V));R->SetObjectField(TEXT("llm_platform"),Platform);
            // Retain every observed project leaf, even below top16. An absent
            // tag stays absent; it is not evidence of a measured zero.
            if(MA0T10MemoryTags::IsEnabled())for(const TCHAR* Name:MA0T10MemoryTags::Names)
                if(const auto* V=DefaultTags.Find(FName(Name)))Watched->SetNumberField(Name,double(*V));
            R->SetObjectField(TEXT("llm_default"),Watched);R->SetObjectField(TEXT("llm_default_top"),Top(DefaultTags));R->SetObjectField(TEXT("llm_platform_top"),Top(PlatformTags));
            R->SetObjectField(TEXT("pcd"),Pcd());auto Sensors=MakeShared<FJsonObject>();
            const auto Stats=SensorOwnedRayTracing::GetSubmissionStats();auto Gpu=MakeShared<FJsonObject>();
            Gpu->SetNumberField(TEXT("batches"),Stats.Batches);Gpu->SetNumberField(TEXT("dispatches"),Stats.Dispatches);Gpu->SetNumberField(TEXT("requests"),Stats.Requests);
            Gpu->SetNumberField(TEXT("packet_creates"),Stats.PacketCreates);Gpu->SetNumberField(TEXT("packet_reuses"),Stats.PacketReuses);Gpu->SetNumberField(TEXT("ray_uploads"),Stats.RayUploads);
            Gpu->SetNumberField(TEXT("view_creates"),Stats.ViewCreates);Gpu->SetNumberField(TEXT("scene_builds"),Stats.SceneBuilds);R->SetObjectField(TEXT("rhi_submission"),Gpu);
            Gpu->SetNumberField(TEXT("binding_updates"),Stats.BindingUpdates);
            Gpu->SetNumberField(TEXT("native_dispatches"),Stats.NativeDispatches);Gpu->SetNumberField(TEXT("native_blas_builds"),Stats.NativeBlasBuilds);
            Gpu->SetNumberField(TEXT("native_scene_builds"),Stats.NativeSceneBuilds);Gpu->SetNumberField(TEXT("native_bytes"),Stats.NativeBytes);Gpu->SetNumberField(TEXT("native_peak_bytes"),Stats.NativePeakBytes);
            Gpu->SetBoolField(TEXT("native_validation_available"),Stats.NativeValidationAvailable);
            Gpu->SetNumberField(TEXT("native_fallbacks"),Stats.NativeFallbacks);
            Gpu->SetNumberField(TEXT("native_buffer_creates"),Stats.NativeBufferCreates);Gpu->SetNumberField(TEXT("native_buffer_reuses"),Stats.NativeBufferReuses);
            Gpu->SetNumberField(TEXT("inline_dispatches"),Stats.InlineDispatches);Gpu->SetNumberField(TEXT("scene_storage_creates"),Stats.SceneStorageCreates);Gpu->SetNumberField(TEXT("scene_storage_reuses"),Stats.SceneStorageReuses);
            if(Stats.NativeValidationAvailable)Gpu->SetNumberField(TEXT("native_validation_errors"),Stats.NativeValidationErrors);
            for(int32 I=0;I<SensorCount;++I)
            {
                auto* A=S.SensorAt(I);if(!A){Failed=true;continue;}const auto* Scan=A->ScanComponent.Get();const auto& Status=Scan->GetRuntimeStatus();
                FVirtualSensorEditableState State;A->ReadEditableState(State);auto J=MakeShared<FJsonObject>();
                J->SetNumberField(TEXT("frame_id"),double(Status.FrameId));J->SetNumberField(TEXT("active_acquisition_backend"),int32(Scan->GetActiveAcquisitionBackend()));
                J->SetBoolField(TEXT("running"),A->IsSensorRunning());J->SetStringField(TEXT("profile"),UEnum::GetValueAsString(State.LidarProfile));
                J->SetNumberField(TEXT("horizontal_samples"),Scan->HorizontalSamples);J->SetNumberField(TEXT("vertical_channels"),Scan->VerticalChannels);J->SetNumberField(TEXT("requested_hz"),Status.RequestedAcquisitionRateHz);
                J->SetNumberField(TEXT("preview_point_budget"),Scan->GetPreviewPolicyState().MaxPoints);J->SetNumberField(TEXT("visible_preview_points"),A->VisualizationComponent->GetVisiblePointCount());
                const auto ExpectedBackend=DepthProjection?EVirtualLidarAcquisitionBackend::GpuDepthProjection:EVirtualLidarAcquisitionBackend::HardwareRayTracing;
                const bool PreviewValid=OutputsOff?A->VisualizationComponent->GetVisiblePointCount()==0:A->VisualizationComponent->GetVisiblePointCount()>0;
                if(A->IsSensorRunning()==AcquisitionOff||State.LidarProfile!=EVirtualLidarDeviceProfile::OusterOS1_64_Rev7||Scan->HorizontalSamples!=1024||Scan->VerticalChannels!=64||(!AcquisitionOff&&!FMath::IsNearlyEqual(Status.RequestedAcquisitionRateHz,20.f))||Scan->GetPreviewPolicyState().MaxPoints!=12000||!PreviewValid||Scan->GetActiveAcquisitionBackend()!=ExpectedBackend)Failed=true;
                Sensors->SetObjectField(A->GetSensorId(),J);
            }
            R->SetObjectField(TEXT("sensors"),Sensors);R->SetNumberField(TEXT("previous_sample_total_gt_ms"),PreviousCost);R->SetNumberField(TEXT("restores"),0);
            if(SensorUploadDiagnostics::IsEnabled())
            {
                using namespace SensorUploadDiagnostics;const auto Uploads=Snapshot();auto U=MakeShared<FJsonObject>();auto Sites=MakeShared<FJsonObject>();
                U->SetBoolField(TEXT("enabled"),true);U->SetStringField(TEXT("contract"),TEXT("processing-frame peaks; RHI requested bytes, native mapped writes, engine-managed producer input and readback are separate; not driver heap bytes"));
                for(uint32 I=0;I<DomainCount;++I){U->SetNumberField(FString(DomainName(EDomain(I)))+TEXT("_peak_frame_bytes"),double(Uploads.PeakFrameBytes[I]));U->SetNumberField(FString(DomainName(EDomain(I)))+TEXT("_peak_frame_calls"),double(Uploads.PeakFrameCalls[I]));}
                for(uint32 I=0;I<SiteCount;++I)
                {
                    const auto& V=Uploads.Sites[I];auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("domain"),DomainName(Domain(ESite(I))));
                    J->SetNumberField(TEXT("bytes"),double(V.Bytes));J->SetNumberField(TEXT("calls"),double(V.CopyCalls));J->SetNumberField(TEXT("creations"),double(V.Creations));J->SetNumberField(TEXT("growths"),double(V.Growths));J->SetNumberField(TEXT("shrinks"),double(V.Shrinks));J->SetNumberField(TEXT("skipped"),double(V.Skipped));
                    J->SetNumberField(TEXT("last_capacity"),double(V.LastCapacity));J->SetNumberField(TEXT("max_capacity"),double(V.MaxCapacity));J->SetNumberField(TEXT("peak_frame_bytes"),double(V.PeakFrameBytes));J->SetNumberField(TEXT("peak_frame_calls"),double(V.PeakFrameCalls));Sites->SetObjectField(SiteName(ESite(I)),J);
                }
                U->SetObjectField(TEXT("sites"),Sites);U->SetNumberField(TEXT("record_calls"),double(Uploads.RecordCalls));U->SetNumberField(TEXT("record_cpu_ms"),FPlatformTime::ToMilliseconds64(Uploads.RecordCpuCycles));R->SetObjectField(TEXT("upload_stats"),U);
            }
            if(!Write(R))Failed=true;
        }
        LastSample=CostStart;PreviousCost=(FPlatformTime::Seconds()-CostStart)*1000;CostSum+=PreviousCost;CostMax=FMath::Max(CostMax,PreviousCost);++Samples;
    }
public:
    explicit FHatchMemoryRuntimeProbe(FString In):Directory(MoveTemp(In))
    {
        ObserveReceive=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryObserveReceive"));
        FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryRuntimeSeconds="),Seconds);FParse::Value(FCommandLine::Get(),TEXT("HatchMemorySampleSeconds="),Interval);
        FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryPcdLoopbackPort="),Port);FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryCancelFile="),CancelFile);
        FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),SourceCommit);
        FParse::Value(FCommandLine::Get(),TEXT("HatchMemorySensorCount="),SensorCount);
        const bool Stress=FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryWarmDigCount="),WarmDigTarget);

#if !UE_BUILD_SHIPPING
        DepthProjection=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryDepthProjection"));SkipWarmEdits=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryNoRevisionChanges"));
        Isolation=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryIsolation"));
        AcquisitionOff=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryAcquisitionOff"));OutputsOff=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryOutputsOff"));

#endif
        Investigation=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryInvestigation"));TraceCanaryEnabled=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryTraceCanary"));
        const bool FinalSoak=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryFinalValidation"));
        const bool ValidDuration=Investigation?(Seconds>=900&&Seconds<=1200):(Seconds==30||(Seconds>=1800&&Seconds<=3600)||(FinalSoak&&(Seconds==7200||Seconds==10800)));
        Failed=!FMath::IsFinite(Seconds)||!FMath::IsFinite(Interval)||!ValidDuration||(TraceCanaryEnabled&&Seconds!=30)||Interval<5||Interval>30||Port<1024||Port>65535||SensorCount<1||SensorCount>3||(Stress&&(Seconds!=30||SensorCount!=3||WarmDigTarget<17||WarmDigTarget>1000));
    }
    TStatId GetStatId() const override {RETURN_QUICK_DECLARE_CYCLE_STAT(FHatchMemoryRuntimeProbe,STATGROUP_Tickables);}
    ETickableTickType GetTickableTickType() const override {return ETickableTickType::Always;}
    void Tick(float) override
    {
        if(Finished)return;const double Now=FPlatformTime::Seconds();
        if(Failed){Finish(false,TEXT("diagnostic validation or write failed"));return;}
        if(Now>=NextPoll){NextPoll=Now+1;if(!CancelFile.IsEmpty()&&IFileManager::Get().FileExists(*CancelFile)){Finish(false,TEXT("environment cancelled"));return;}}
        if(Now-Created>Seconds+600){Finish(false,TEXT("diagnostic timeout"));return;}
        if(!Host.IsValid())
        {
            if(Started){Finish(false,TEXT("measurement world lost"));return;}
            if(Now<Changed+.2)return;Changed=Now;
            if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsGameWorld()){World=C.World();break;}
            if(World.IsValid())for(TActorIterator<AShipHatchMonitorHost> I(World.Get());I;++I){Host=*I;break;}
            if(!Host.IsValid()&&Now-Created>180)Finish(false,TEXT("ShipHatchTestMap host unavailable"));return;
        }
        auto* S=Host->Simulation.Get();auto* M=Host->Monitor.Get();if(!S||!M){if(Started)Finish(false,TEXT("measurement host lost"));return;}
        if(Phase<3&&(S->IsGenerating()||!M->GetAnalysis()))return;
        if(Phase==0)
        {
            if(SensorCount!=3)for(int32 I=0;I<3;++I)S->SetFieldSensor(I,I<SensorCount,S->FieldProfiles[I]);
            if(!ConfigurePcd(*S)){Finish(false,TEXT("selected enabled map sensors required"));return;}
            Changed=Now;if(Isolation)UE_LOG(LogTemp,Display,TEXT("[HatchMemoryIsolation] configured sensors=%d depth=%d revision_changes=%d;2s settling before warmup"),SensorCount,DepthProjection?1:0,SkipWarmEdits?0:1);
            ScreenMessages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);M->SetFontPercent(100);M->SelectView(1);M->SetSplitView(false);M->SetCameraPreset(0);Phase=1;return;
        }
        if(Phase==1)
        {
            if(Isolation&&Now-Changed<2)return;
            if(SkipWarmEdits){Changed=Now;Phase=2;return;}
            if(WarmEdits<WarmDigTarget)
            {
                if(WarmEdits>0&&WarmEdits%17==0&&RestoredAt!=WarmEdits)
                {if(!S->RestoreCargo()){Finish(false,TEXT("stress restore rejected"));return;}RestoredAt=WarmEdits;return;}
                const int32 I=WarmEdits%17;const double X=4+I%5;double Y=((I/5)%5-2)*3.;if(FMath::IsNearlyEqual(Y,-3.))Y=-4.5;
                // Public cargo API warmup, with a near-surface ray to avoid
                // selecting the overhead cab. This is not a mouse/UI test.
                const FVector Origin(X*100,Y*100,(S->Surface().Height(X,Y)+1)*100);
                if(!S->Dig(S->GetActorTransform().TransformPosition(Origin),S->GetActorTransform().TransformVector(FVector(0,0,-1)),2,1.5)){Finish(false,TEXT("warm Dig rejected"));return;}
                if(Isolation)UE_LOG(LogTemp,Display,TEXT("[HatchMemoryIsolation] warm_dig=%d surface_revision=%llu"),WarmEdits+1,S->Revision());
                ++WarmEdits;return;
            }
            if(!S->RestoreCargo()){Finish(false,TEXT("warm restore rejected"));return;}Changed=Now;Phase=2;return;
        }
        if(Phase==2)
        {
            if(Now-Changed<35)return;
            if((AcquisitionOff||OutputsOff)&&!CutApplied)
            {
                for(int32 I=0;I<SensorCount;++I)if(auto* A=S->SensorAt(I))
                {
                    if(AcquisitionOff)A->StopSensor();
                    if(OutputsOff)A->VisualizationComponent->SetWorldPointCloudEnabled(false);
                }
                if(Publisher.IsValid())Publisher->StopAllStreams(TEXT(""));
                CutApplied=true;CutReadyAt=Now+5;return;
            }
            if(CutApplied&&Now<CutReadyAt)return;
            if(!Begin(*S)){Finish(false,TEXT("fresh output or viewport validation failed"));return;}Phase=3;Sample(*S);return;
        }
        // The measured idle sampler does not require analysis completion: a
        // stalled measurement/analysis cannot silently suppress memory samples.
        if(Phase==3&&TraceCanaryEnabled)
        {
            if(CanaryStage==0&&Now-Started>=2){ResizeMemoryTraceCanary(TraceCanary,1024*1024);CanaryStage=1;}
            if(CanaryStage==1&&Now-Started>=10){ResizeMemoryTraceCanary(TraceCanary,4*1024*1024);CanaryStage=2;}
        }
        if(Phase==3&&Now-LastSample>=Interval)
        {
            Sample(*S);if(Now-Started>=Seconds)
            {
                for(int32 I=0;I<SensorCount;++I)if(auto* A=S->SensorAt(I)){const auto Frame=A->ScanComponent->GetRuntimeStatus().FrameId;const auto Before=FramesBefore.FindRef(A->GetSensorId());if(AcquisitionOff?Frame!=Before:Frame<=Before)Failed=true;}
                Finish(true,TEXT("finished"));
            }
        }
    }
};
TUniquePtr<FHatchMemoryRuntimeProbe> HatchMemoryProbe;
}

void StartHatchMemoryRuntimeProbe()
{
    StartLidarMapMemoryRuntimeProbe();
    FString Directory;if(FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryRuntimeReport="),Directory)&&!Directory.IsEmpty())HatchMemoryProbe=MakeUnique<FHatchMemoryRuntimeProbe>(MoveTemp(Directory));
}
void StopHatchMemoryRuntimeProbe(){HatchMemoryProbe.Reset();StopLidarMapMemoryRuntimeProbe();}
