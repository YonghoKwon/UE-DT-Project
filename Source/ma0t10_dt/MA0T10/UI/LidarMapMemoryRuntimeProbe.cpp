#include "LidarMapMemoryRuntimeProbe.h"
#include "SensorOwnedRayTracing.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Tickable.h"
#include "UObject/UObjectArray.h"
#include "UnrealClient.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

namespace
{
class FLidarMapMemoryProbe final:public FTickableGameObject
{
    FString Directory,CancelFile;
    TWeakObjectPtr<UWorld> World;
    TUniquePtr<FArchive> File;
    TMap<FName,uint64> DefaultTags,PlatformTags;
    double Created=FPlatformTime::Seconds(),Ready=0,Started=0,Next=0,Seconds=3600,PreviousCost=0,MaxCost=0,TotalCost=0;
    int32 Samples=0,ExpectedSensors=-1;
    bool Off=false,ForceHardware=false,Configured=false,Finished=false,Failed=false;
    bool Write(const TSharedRef<FJsonObject>& Row)
    {
        if(!File)return false;FString Text;FJsonSerializer::Serialize(Row,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));Text+=TEXT("\n");
        FTCHARToUTF8 Bytes(*Text);File->Serialize((void*)Bytes.Get(),Bytes.Length());File->Flush();return !File->IsError();
    }
    void Finish(bool Complete,const FString& Reason)
    {
        if(Finished)return;Finished=true;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("type"),TEXT("summary"));R->SetBoolField(TEXT("complete"),Complete&&!Failed);R->SetStringField(TEXT("reason"),Reason);
        R->SetNumberField(TEXT("measurement_seconds"),Started?FPlatformTime::Seconds()-Started:0);R->SetNumberField(TEXT("sample_count"),Samples);R->SetNumberField(TEXT("sampler_total_gt_ms"),TotalCost);R->SetNumberField(TEXT("sampler_max_gt_ms"),MaxCost);
        const bool Saved=Write(R);if(File){Failed=!File->Close()||Failed;File.Reset();}
        if(World.IsValid())
        {
            for(TActorIterator<AVirtualLidarSensorActor> I(World.Get());I;++I)I->StopSensor();
            if(auto* V=World->GetGameViewport())V->bDisableWorldRendering=true;
        }
        FlushRenderingCommands();FPlatformMisc::RequestExitWithStatus(false,Complete&&!Failed&&Saved?0:2);
    }
    void Sample(double Now)
    {
        const double Begin=FPlatformTime::Seconds();auto R=MakeShared<FJsonObject>();const auto Memory=FPlatformMemory::GetStats();
        R->SetStringField(TEXT("type"),TEXT("memory_curve"));R->SetNumberField(TEXT("seconds"),Now-Started);R->SetNumberField(TEXT("rss_bytes"),double(Memory.UsedPhysical));R->SetNumberField(TEXT("commit_bytes"),double(Memory.UsedVirtual));R->SetNumberField(TEXT("uobject_count"),GUObjectArray.GetObjectArrayNumMinusAvailable());
        DefaultTags.Reset();PlatformTags.Reset();
#if ENABLE_LOW_LEVEL_MEM_TRACKER
        if(FLowLevelMemTracker::IsEnabled()){auto& L=FLowLevelMemTracker::Get();L.GetTrackedTagsNamesWithAmount(DefaultTags,ELLMTracker::Default,ELLMTagSet::None);L.GetTrackedTagsNamesWithAmount(PlatformTags,ELLMTracker::Platform,ELLMTagSet::None);}
#endif
        auto Tags=MakeShared<FJsonObject>();for(const TCHAR* Tag:{TEXT("RHIMisc"),TEXT("Untracked"),TEXT("FMallocUnused"),TEXT("ThreadStack"),TEXT("Total")})if(const auto* Value=DefaultTags.Find(FName(Tag)))Tags->SetNumberField(Tag,double(*Value));R->SetObjectField(TEXT("llm_default"),Tags);
        auto Platform=MakeShared<FJsonObject>();for(const TCHAR* Tag:{TEXT("FMalloc"),TEXT("PlatformUntracked"),TEXT("ThreadStackPlatform"),TEXT("GraphicsPlatform")})if(const auto* Value=PlatformTags.Find(FName(Tag)))Platform->SetNumberField(Tag,double(*Value));R->SetObjectField(TEXT("llm_platform"),Platform);
        auto Sensors=MakeShared<FJsonObject>();int32 Count=0;
        for(TActorIterator<AVirtualLidarSensorActor> I(World.Get());I;++I)
        {
            ++Count;auto* Scan=I->ScanComponent.Get();const auto Status=Scan->GetRuntimeStatus();auto S=MakeShared<FJsonObject>();
            if(ForceHardware&&(!I->IsSensorRunning()||Scan->GetActiveAcquisitionBackend()!=EVirtualLidarAcquisitionBackend::HardwareRayTracing))Failed=true;
            S->SetNumberField(TEXT("frame_id"),double(Status.FrameId));S->SetBoolField(TEXT("running"),I->IsSensorRunning());S->SetNumberField(TEXT("requested_hz"),Status.RequestedAcquisitionRateHz);S->SetNumberField(TEXT("active_acquisition_backend"),int32(Scan->GetActiveAcquisitionBackend()));
            S->SetNumberField(TEXT("horizontal_samples"),Scan->HorizontalSamples);S->SetNumberField(TEXT("vertical_channels"),Scan->VerticalChannels);S->SetNumberField(TEXT("visible_preview_points"),I->VisualizationComponent->GetVisiblePointCount());Sensors->SetObjectField(I->GetSensorId(),S);
        }
        R->SetNumberField(TEXT("lidar_actor_count"),Count);
        if(ExpectedSensors<0)ExpectedSensors=Count;if(Count!=ExpectedSensors||Sensors->Values.Num()!=Count||(Off?Count!=0:Count==0))Failed=true;
        const auto Stats=SensorOwnedRayTracing::GetSubmissionStats();auto Gpu=MakeShared<FJsonObject>();Gpu->SetNumberField(TEXT("native_dispatches"),Stats.NativeDispatches);Gpu->SetNumberField(TEXT("native_fallbacks"),Stats.NativeFallbacks);Gpu->SetNumberField(TEXT("dispatches"),Stats.Dispatches);Gpu->SetNumberField(TEXT("scene_builds"),Stats.SceneBuilds);Gpu->SetBoolField(TEXT("native_selected"),SensorOwnedRayTracing::UsesNativeBackend());R->SetObjectField(TEXT("rhi_submission"),Gpu);
        R->SetObjectField(TEXT("sensors"),Sensors);R->SetNumberField(TEXT("previous_sample_total_gt_ms"),PreviousCost);if(!Write(R))Failed=true;
        PreviousCost=(FPlatformTime::Seconds()-Begin)*1000;MaxCost=FMath::Max(MaxCost,PreviousCost);TotalCost+=PreviousCost;++Samples;
    }
public:
    explicit FLidarMapMemoryProbe(FString In):Directory(MoveTemp(In))
    {
        FParse::Value(FCommandLine::Get(),TEXT("LidarMapMemorySeconds="),Seconds);FParse::Value(FCommandLine::Get(),TEXT("LidarMapMemoryCancel="),CancelFile);Off=FParse::Param(FCommandLine::Get(),TEXT("LidarMapMemoryAllOff"));
        ForceHardware=FParse::Param(FCommandLine::Get(),TEXT("LidarMapMemoryHardware"));
        Failed=(Seconds!=30&&Seconds!=3600)||(Off&&ForceHardware);IFileManager::Get().MakeDirectory(*Directory,true);const FString Path=Directory/TEXT("memory-trend.jsonl");if(IFileManager::Get().FileExists(*Path))Failed=true;else File.Reset(IFileManager::Get().CreateFileWriter(*Path,FILEWRITE_AllowRead));if(!File)Failed=true;
    }
    TStatId GetStatId()const override{RETURN_QUICK_DECLARE_CYCLE_STAT(FLidarMapMemoryProbe,STATGROUP_Tickables);}
    void Tick(float)override
    {
        if(Finished)return;const double Now=FPlatformTime::Seconds();if(Now<Next)return;Next=Now+1;
        if(Failed){Finish(false,TEXT("map probe validation/write failed"));return;}
        if(!CancelFile.IsEmpty()&&IFileManager::Get().FileExists(*CancelFile)){Finish(false,TEXT("environment cancelled"));return;}
        if(Now-Created>Seconds+600){Finish(false,TEXT("map probe timeout"));return;}
        if(!World.IsValid()){if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsGameWorld()){World=C.World();Ready=Now+45;break;}return;}
        if(!Configured)
        {
            if(Now<Ready)return;
            if(Off)
            {
                bool Found=false;for(TActorIterator<AShipHatchSimulation> I(World.Get());I;++I){if(I->IsGenerating())return;Found=true;for(int32 J=0;J<3;++J)I->SetFieldSensor(J,false,I->FieldProfiles[J]);}
                if(!Found){Finish(false,TEXT("all-off baseline requires hatch public sensor selection"));return;}
            }
            if(ForceHardware)for(TActorIterator<AVirtualLidarSensorActor> I(World.Get());I;++I)
            {FString Error;if(!I->SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend::HardwareRayTracing,Error)){Finish(false,TEXT("diagnostic hardware backend selection rejected"));return;}}
            Configured=true;Ready=Now+((Off||ForceHardware)?35:0);return;
        }
        if(Now<Ready)return;
        if(!Started)
        {
            auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("type"),TEXT("metadata"));R->SetStringField(TEXT("map"),World->GetMapName());R->SetBoolField(TEXT("all_lidar_off"),Off);R->SetStringField(TEXT("scope"),TEXT("Default map devices/settings/output retained; explicit all-off prevents hatch LiDAR creation. Diagnostic sampler only; no map/save write."));R->SetNumberField(TEXT("requested_seconds"),Seconds);R->SetNumberField(TEXT("interval_seconds"),5);
            R->SetBoolField(TEXT("diagnostic_force_hardware"),ForceHardware);
            R->SetStringField(TEXT("started_utc"),(FDateTime::UtcNow()-FTimespan::FromSeconds(FPlatformTime::Seconds()-Now)).ToIso8601());
            if(ForceHardware)R->SetStringField(TEXT("scope"),TEXT("Explicit diagnostic hardware backend through Actor API after map initialization; profile/quality/output retained; no map/save write, no product default change."));
            R->SetStringField(TEXT("configuration"),
#if UE_BUILD_SHIPPING
                TEXT("Shipping")
#else
                TEXT("Development")
#endif
            );
            bool Valid=false;if(auto* V=World->GetGameViewport();V&&V->Viewport){const auto Size=V->Viewport->GetSizeXY();R->SetNumberField(TEXT("viewport_x"),Size.X);R->SetNumberField(TEXT("viewport_y"),Size.Y);Valid=Size==FIntPoint(1920,1080);}
            if(!Valid||!Write(R)){Finish(false,TEXT("map metadata/viewport failed"));return;}Started=Now;
        }
        Next=Now+5;Sample(Now);if(Now-Started>=Seconds)Finish(true,TEXT("finished"));
    }
};
TUniquePtr<FLidarMapMemoryProbe> LidarMapMemoryProbe;
}
void StartLidarMapMemoryRuntimeProbe(){FString Directory;if(FParse::Value(FCommandLine::Get(),TEXT("LidarMapMemoryReport="),Directory)&&!Directory.IsEmpty())LidarMapMemoryProbe=MakeUnique<FLidarMapMemoryProbe>(MoveTemp(Directory));}
void StopLidarMapMemoryRuntimeProbe(){LidarMapMemoryProbe.Reset();}
