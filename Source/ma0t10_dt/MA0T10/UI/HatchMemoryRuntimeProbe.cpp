#include "HatchMemoryRuntimeProbe.h"

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
#include "UObject/UObjectArray.h"
#include "UnrealClient.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

namespace
{
class FHatchMemoryRuntimeProbe final : public FTickableGameObject
{
    FString Directory, CancelFile, SourceCommit;
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
    int32 Port=0, Phase=0, WarmEdits=0, Samples=0;
    bool Finished=false, Failed=false, Llm=false;

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
            if(Check&&(S.SubmittedFrameCount<=B.SubmittedFrameCount||S.ConsumerReceivedCount<=B.ConsumerReceivedCount||S.ConsumerValidationFailureCount!=B.ConsumerValidationFailureCount||S.EncodeFailureCount!=B.EncodeFailureCount))Failed=true;
        }
        if(Check&&Count!=3)Failed=true;return R;
    }
    void Finish(bool Complete,const FString& Reason)
    {
        if(Finished)return;Finished=true;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("type"),TEXT("summary"));R->SetObjectField(TEXT("pcd"),Pcd(Complete));
        R->SetStringField(TEXT("reason"),Reason);R->SetBoolField(TEXT("complete"),Complete&&!Failed);R->SetBoolField(TEXT("diagnostic_only"),true);
        R->SetNumberField(TEXT("sample_count"),Samples);R->SetNumberField(TEXT("measurement_seconds"),Started?FPlatformTime::Seconds()-Started:0);
        R->SetNumberField(TEXT("clicks"),0);R->SetNumberField(TEXT("restores"),0);R->SetNumberField(TEXT("warm_edits"),WarmEdits);
        R->SetBoolField(TEXT("llm_enabled"),Llm);R->SetNumberField(TEXT("last_sample_total_gt_ms"),PreviousCost);
        R->SetNumberField(TEXT("sampler_total_gt_ms"),CostSum);R->SetNumberField(TEXT("sampler_max_gt_ms"),CostMax);
        const bool Saved=Write(R);if(File){Failed=!File->Close()||Failed;File.Reset();}
        if(!Saved){FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Directory,true);FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("probe-error.json")));}
        // The observation/footer is complete. Quiesce only this diagnostic's
        // world/output before engine PreExit tears down scene-view delegates.
        // No stop/drain cost enters memory samples or their GT cost values.
        if(Host.IsValid()&&Host->Simulation)for(int32 I=0;I<3;++I)if(auto* A=Host->Simulation->SensorAt(I))A->StopSensor();
        if(Publisher.IsValid())Publisher->StopAllStreams(TEXT("*"));
        if(World.IsValid())if(auto* View=World->GetGameViewport())View->bDisableWorldRendering=true;
        FlushRenderingCommands();
        FPlatformMisc::RequestExitWithStatus(false,Complete&&!Failed&&Saved?0:2);
    }
    bool ConfigurePcd(AShipHatchSimulation& S)
    {
        // Same existing project transport/publisher path and defaults as the
        // Editor memory fixture; no broker/user configuration is read or saved.
        auto* Transport=NewObject<UVirtualSensorTransportComponent>(&S);S.AddInstanceComponent(Transport);Transport->RegisterComponent();
        Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
        FVirtualSensorTransportProfile Profile;Profile.BrokerUrl=FString::Printf(TEXT("tcp://127.0.0.1:%d"),Port);Profile.UserName.Reset();
        Transport->ConfigureTransportProfile(Profile);Transport->SetSessionCredentials(TEXT(""),TEXT(""));
        auto* P=NewObject<UVirtualSensorStreamPublisherComponent>(&S);S.AddInstanceComponent(P);P->SetTransportComponent(Transport);P->RegisterComponent();Publisher=P;
        for(int32 I=0;I<3;++I)
        {
            auto* A=S.SensorAt(I);if(!A||!S.SensorEnabled(I))return false;
            A->StopSensor();A->SetSharedOutputServices(Transport,nullptr,P);
            FVirtualSensorStreamConfig Config;Config.SensorId=A->GetSensorId();Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.bEnabled=true;P->ConfigureStream(Config);A->StartSensor();
        }
        return true;
    }
    bool Begin(AShipHatchSimulation& S)
    {
        const FString Path=Directory/TEXT("memory-trend.jsonl");if(IFileManager::Get().FileExists(*Path))return false;
        IFileManager::Get().MakeDirectory(*Directory,true);File.Reset(IFileManager::Get().CreateFileWriter(*Path,FILEWRITE_AllowRead));
#if ENABLE_LOW_LEVEL_MEM_TRACKER
        Llm=FLowLevelMemTracker::IsEnabled();
#endif
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("type"),TEXT("metadata"));R->SetNumberField(TEXT("schema_version"),1);
        R->SetStringField(TEXT("row_contract"),TEXT("memory_curve"));R->SetBoolField(TEXT("diagnostic_only"),true);
        R->SetNumberField(TEXT("requested_seconds"),Seconds);R->SetNumberField(TEXT("interval_seconds"),Interval);R->SetBoolField(TEXT("llm_enabled"),Llm);
        R->SetBoolField(TEXT("sampler_enabled"),true);R->SetBoolField(TEXT("pcd_loopback"),true);R->SetBoolField(TEXT("pcd_default_compressed"),Publisher->GetPcdSendOptions(TEXT("")).bCompressed);
        R->SetStringField(TEXT("workload"),TEXT("packaged idle;17 public Dig near-surface ray cycles with fresh analysis, restore,35s warmup; no Editor automation history"));
        R->SetStringField(TEXT("source_commit"),SourceCommit);R->SetStringField(TEXT("started_utc"),FDateTime::UtcNow().ToIso8601());
        R->SetStringField(TEXT("allocator"),GMalloc->GetDescriptiveName());
        if(Host.IsValid()&&Host->Monitor)R->SetNumberField(TEXT("font_percent"),Host->Monitor->GetFontScale()*100);
        if(World.IsValid())R->SetNumberField(TEXT("umg_viewport_scale"),UWidgetLayoutLibrary::GetViewportScale(World.Get()));
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
        for(int32 I=0;I<3;++I)if(auto* A=S.SensorAt(I))FramesBefore.Add(A->GetSensorId(),A->ScanComponent->GetRuntimeStatus().FrameId);
        Started=FPlatformTime::Seconds();return true;
    }
    void Sample(AShipHatchSimulation& S)
    {
        const double CostStart=FPlatformTime::Seconds();
        {
            const auto Mem=FPlatformMemory::GetStats();auto R=MakeShared<FJsonObject>();
            R->SetStringField(TEXT("type"),TEXT("memory_curve"));R->SetNumberField(TEXT("seconds"),CostStart-Started);R->SetNumberField(TEXT("cycle"),0);
            R->SetNumberField(TEXT("rss_bytes"),double(Mem.UsedPhysical));R->SetNumberField(TEXT("commit_bytes"),double(Mem.UsedVirtual));
            R->SetNumberField(TEXT("uobject_count"),GUObjectArray.GetObjectArrayNumMinusAvailable());R->SetBoolField(TEXT("llm_enabled"),Llm);
#if ENABLE_LOW_LEVEL_MEM_TRACKER
            if(Llm){auto& L=FLowLevelMemTracker::Get();L.GetTrackedTagsNamesWithAmount(DefaultTags,ELLMTracker::Default,ELLMTagSet::None);L.GetTrackedTagsNamesWithAmount(PlatformTags,ELLMTracker::Platform,ELLMTagSet::None);}
#endif
            auto Watched=MakeShared<FJsonObject>();
            // Match the Editor row's fixed ten counters, including zero tags.
            for(const TCHAR* Name:{TEXT("Physics"),TEXT("ChaosTrimesh"),TEXT("UObject"),TEXT("UI"),TEXT("RHIMisc"),TEXT("Textures"),TEXT("FMallocUnused"),TEXT("Hatch/CargoCook"),TEXT("Hatch/RayTree"),TEXT("Hatch/CargoUpdate")})
                Watched->SetNumberField(Name,double(DefaultTags.FindRef(FName(Name))));
            for(const TCHAR* Name:{TEXT("Untracked"),TEXT("TrackedTotal"),TEXT("Total"),TEXT("FMalloc")})
                if(const auto* V=DefaultTags.Find(FName(Name)))Watched->SetNumberField(Name,double(*V));
            R->SetObjectField(TEXT("llm_default"),Watched);R->SetObjectField(TEXT("llm_default_top"),Top(DefaultTags));R->SetObjectField(TEXT("llm_platform_top"),Top(PlatformTags));
            R->SetObjectField(TEXT("pcd"),Pcd());auto Sensors=MakeShared<FJsonObject>();
            for(int32 I=0;I<3;++I)
            {
                auto* A=S.SensorAt(I);if(!A){Failed=true;continue;}const auto* Scan=A->ScanComponent.Get();const auto& Status=Scan->GetRuntimeStatus();
                FVirtualSensorEditableState State;A->ReadEditableState(State);auto J=MakeShared<FJsonObject>();
                J->SetNumberField(TEXT("frame_id"),double(Status.FrameId));J->SetNumberField(TEXT("active_acquisition_backend"),int32(Scan->GetActiveAcquisitionBackend()));
                J->SetBoolField(TEXT("running"),A->IsSensorRunning());J->SetStringField(TEXT("profile"),UEnum::GetValueAsString(State.LidarProfile));
                J->SetNumberField(TEXT("horizontal_samples"),Scan->HorizontalSamples);J->SetNumberField(TEXT("vertical_channels"),Scan->VerticalChannels);J->SetNumberField(TEXT("requested_hz"),Status.RequestedAcquisitionRateHz);
                J->SetNumberField(TEXT("preview_point_budget"),Scan->GetPreviewPolicyState().MaxPoints);J->SetNumberField(TEXT("visible_preview_points"),A->VisualizationComponent->GetVisiblePointCount());
                if(!A->IsSensorRunning()||State.LidarProfile!=EVirtualLidarDeviceProfile::OusterOS1_64_Rev7||Scan->HorizontalSamples!=1024||Scan->VerticalChannels!=64||!FMath::IsNearlyEqual(Status.RequestedAcquisitionRateHz,20.f)||Scan->GetPreviewPolicyState().MaxPoints!=12000||A->VisualizationComponent->GetVisiblePointCount()<=0||Scan->GetActiveAcquisitionBackend()!=EVirtualLidarAcquisitionBackend::HardwareRayTracing)Failed=true;
                Sensors->SetObjectField(A->GetSensorId(),J);
            }
            R->SetObjectField(TEXT("sensors"),Sensors);R->SetNumberField(TEXT("previous_sample_total_gt_ms"),PreviousCost);R->SetNumberField(TEXT("restores"),0);
            if(!Write(R))Failed=true;
        }
        LastSample=CostStart;PreviousCost=(FPlatformTime::Seconds()-CostStart)*1000;CostSum+=PreviousCost;CostMax=FMath::Max(CostMax,PreviousCost);++Samples;
    }
public:
    explicit FHatchMemoryRuntimeProbe(FString In):Directory(MoveTemp(In))
    {
        FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryRuntimeSeconds="),Seconds);FParse::Value(FCommandLine::Get(),TEXT("HatchMemorySampleSeconds="),Interval);
        FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryPcdLoopbackPort="),Port);FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryCancelFile="),CancelFile);
        FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),SourceCommit);
        Failed=!FMath::IsFinite(Seconds)||!FMath::IsFinite(Interval)||(Seconds!=30&&(Seconds<1800||Seconds>3600))||Interval<5||Interval>30||Port<1024||Port>65535;
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
            if(!ConfigurePcd(*S)){Finish(false,TEXT("three enabled map sensors required"));return;}
            ScreenMessages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);M->SetFontPercent(100);M->SelectView(1);M->SetSplitView(false);M->SetCameraPreset(0);Phase=1;return;
        }
        if(Phase==1)
        {
            if(WarmEdits<17)
            {
                const int32 I=WarmEdits;const double X=4+I%5;double Y=((I/5)%5-2)*3.;if(FMath::IsNearlyEqual(Y,-3.))Y=-4.5;
                // Public cargo API warmup, with a near-surface ray to avoid
                // selecting the overhead cab. This is not a mouse/UI test.
                const FVector Origin(X*100,Y*100,(S->Surface().Height(X,Y)+1)*100);
                if(!S->Dig(S->GetActorTransform().TransformPosition(Origin),S->GetActorTransform().TransformVector(FVector(0,0,-1)),2,1.5)){Finish(false,TEXT("warm Dig rejected"));return;}
                ++WarmEdits;return;
            }
            if(!S->RestoreCargo()){Finish(false,TEXT("warm restore rejected"));return;}Changed=Now;Phase=2;return;
        }
        if(Phase==2)
        {
            if(Now-Changed<35)return;if(!Begin(*S)){Finish(false,TEXT("fresh output or viewport validation failed"));return;}Phase=3;Sample(*S);return;
        }
        // The measured idle sampler does not require analysis completion: a
        // stalled measurement/analysis cannot silently suppress memory samples.
        if(Phase==3&&Now-LastSample>=Interval)
        {
            Sample(*S);if(Now-Started>=Seconds)
            {
                for(int32 I=0;I<3;++I)if(auto* A=S->SensorAt(I))if(A->ScanComponent->GetRuntimeStatus().FrameId<=FramesBefore.FindRef(A->GetSensorId()))Failed=true;
                Finish(true,TEXT("finished"));
            }
        }
    }
};
TUniquePtr<FHatchMemoryRuntimeProbe> HatchMemoryProbe;
}

void StartHatchMemoryRuntimeProbe()
{
    FString Directory;if(FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryRuntimeReport="),Directory)&&!Directory.IsEmpty())HatchMemoryProbe=MakeUnique<FHatchMemoryRuntimeProbe>(MoveTemp(Directory));
}
void StopHatchMemoryRuntimeProbe(){HatchMemoryProbe.Reset();}
