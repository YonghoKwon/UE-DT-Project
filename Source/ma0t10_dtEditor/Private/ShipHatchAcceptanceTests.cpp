#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorSemanticLabels.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMemory.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "ImageUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "RenderingThread.h"
#include "Components/ComboBoxString.h"
#include "Components/LineBatchComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"

namespace
{
FString ReportRoot(){FString P;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-01"):P;}
void WriteReport(const TCHAR* Name,const TSharedRef<FJsonObject>& Json)
{const FString P=ReportRoot();IFileManager::Get().MakeDirectory(*P,true);FString T;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&T));FFileHelper::SaveStringToFile(T,*(P/Name));}
double Mean(const TArray<double>& A){double S=0;for(double X:A)S+=X;return A.Num()?S/A.Num():0;}
double P95(TArray<double> A){A.Sort();return A.Num()?A[FMath::Clamp(FMath::CeilToInt(A.Num()*.95)-1,0,A.Num()-1)]:0;}
class FHatchRuntime final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    UWorld* World=nullptr;
    AShipHatchSimulation* Sim=nullptr;
    AShipHatchMonitorHost* Host=nullptr;
    double Started=FPlatformTime::Seconds(),Ready=0,LastWall=0,MeasureStart=0,Observed=0;
    int32 Phase=0,Selection=0,Regenerations=0,Capture=0;
    uint64 BeforeRevision=0,LastFrame=0;
    uint64 MemoryBefore=0,MemoryAfter=0;
    uint64 RawMemoryBefore=0,RawMemoryAfter=0;
    uint64 CommittedBefore=0,CommittedAfter=0;
    double RandomHeightMaxError=0;
    uint64 AnalysisAfterTen=0,AnalysisAtBaseline=0;
    bool Measuring=false;
    bool BoundaryStopped=false;
    TSharedPtr<FJsonObject> ResourceBefore;
    TSharedPtr<FJsonObject> ResultReport;
    uint64 HiddenBuilds=0;
    int64 HiddenFrames[2]={0,0};
    int64 PerformanceStartFrames[2]={0,0};
    FString AnalysisRequest;
    void StopBoundary(){Sim->MLX->StopSensor();Sim->Livox->StopSensor();}
    TSharedRef<FJsonObject> Resources()
    {
        auto Root=MakeShared<FJsonObject>();
        for(auto* A:{Sim->MLX.Get(),Sim->Livox.Get()})
        {
            auto R=MakeShared<FJsonObject>();const auto Scan=A->ScanComponent->GetResourceSnapshot();
            const auto View=A->VisualizationComponent->GetResourceSnapshot();
            for(const auto& P:Scan.OwnedCpuBytes)R->SetNumberField(TEXT("scan/")+P.Key,double(P.Value));
            for(const auto& P:Scan.SharedCpuBytes)R->SetNumberField(TEXT("shared/")+P.Key,double(P.Value));
            for(const auto& P:View.OwnedCpuBytes)R->SetNumberField(TEXT("view/")+P.Key,double(P.Value));
            Root->SetObjectField(A->GetSensorId(),R);
        }
        return Root;
    }
    TArray<double> Engine,Game,Wall,Mesh;
    TArray<TSharedPtr<FJsonValue>> SensorEvidence;
    TUniquePtr<TGuardValue<bool>> ScreenMessages;
    bool Captures=false,Manual=false,Performance=false,Requested=false;
    FString CapturePath;
    FDateTime PreviousCapture;
    int32 SensorCount=2;
    TSet<int32> RandomSeeds;
    void ClickRandom()
    {
        TArray<UWidget*> Widgets;Host->Monitor->WidgetTree->GetAllWidgets(Widgets);
        for(auto* W:Widgets)if(auto* Button=Cast<UButton>(W))if(auto* Text=Cast<UTextBlock>(Button->GetContent()))
            if(Text->GetText().ToString()==TEXT("적재 상태 바꾸기(랜덤)")){Button->OnClicked.Broadcast();return;}
        Test->AddError(TEXT("production random button missing"));
    }
public:
    FHatchRuntime(FAutomationTestBase* T,bool C,bool P):Test(T),Captures(C),Manual(FParse::Param(FCommandLine::Get(),TEXT("HatchManual"))),Performance(P)
    {FParse::Value(FCommandLine::Get(),TEXT("HatchSensorCount="),SensorCount);}
    void CheckSensors()
    {
        for(auto* A:{Sim->bMLX?Sim->MLX.Get():nullptr,Sim->bLivox?Sim->Livox.Get():nullptr})if(A)
        {
            auto* S=A->ScanComponent.Get();const auto F=S->GetLastFrameSnapshot();
            Test->TestTrue(TEXT("selected sensor produces immutable frame"),F&&F->Points&&F->Points->Num()>0);if(!F||!F->Points)return;
            const int32 Expected=S->DeviceProfile==EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE?576*56:S->DeviceProfile==EVirtualLidarDeviceProfile::LivoxMid360S?360*60:200*56;
            Test->TestEqual(TEXT("existing FullSpec profile ray count is preserved"),F->Points->Num(),Expected);
            int32 Hit=0,Cargo=0,BadLabel=0;double MaxError=0,SumError=0,ReflectivityError=0,Intensity=0;
            for(const auto& P:*F->Points)if(P.bHit)
            {
                ++Hit;
                if(P.SemanticLabel==TEXT("HatchCargo"))
                {++Cargo;ReflectivityError=FMath::Max(ReflectivityError,FMath::Abs(P.SurfaceReflectivity-ShipHatch::Reflectivity(Sim->Surface().Settings.Cargo)));Intensity+=P.NormalizedIntensity;
                 const FVector Local=Sim->GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01;
                 double E=FMath::Abs(Local.Z-Sim->Surface().Height(Local.X,Local.Y));MaxError=FMath::Max(MaxError,E);SumError+=E;}
                else if(P.SemanticLabel!=TEXT("HatchHold")&&P.SemanticLabel!=TEXT("HatchCoaming")&&P.SemanticLabel!=TEXT("HatchOutside"))++BadLabel;
                if(!P.HitActorTags.Contains(P.SemanticLabel))++BadLabel;
            }
            Test->TestTrue(TEXT("physical hits and cargo are present"),Hit>100&&Cargo>100);
            Test->TestEqual(TEXT("each hit has exactly a supported hatch label"),BadLabel,0);
            Test->TestTrue(TEXT("cargo hit height vs generated collision surface error < 15 cm"),MaxError<.15);
            Test->TestTrue(TEXT("actual cargo returns use the material response override"),ReflectivityError<1.e-6);
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),A->GetSensorId());Row->SetNumberField(TEXT("selection"),Selection);
            Row->SetNumberField(TEXT("points"),F->Points->Num());Row->SetNumberField(TEXT("hit"),Hit);Row->SetNumberField(TEXT("hit_ratio"),double(Hit)/F->Points->Num());
            Row->SetNumberField(TEXT("cargo"),Cargo);Row->SetNumberField(TEXT("bad_label"),BadLabel);Row->SetNumberField(TEXT("height_max_error_m"),MaxError);Row->SetNumberField(TEXT("height_mean_error_m"),Cargo?SumError/Cargo:0);
            Row->SetNumberField(TEXT("reflectivity_max_error"),ReflectivityError);Row->SetNumberField(TEXT("mean_normalized_intensity"),Cargo?Intensity/Cargo:0);
            TArray<ShipHatch::FObservation> Observations,Reference;
            for(const auto& P:*F->Points)if(P.bHit&&P.SemanticLabel==TEXT("HatchCargo"))
            {const FVector Local=Sim->GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01;Observations.Add({Local,P.SemanticLabel});Reference.Add({FVector(Local.X,Local.Y,Sim->Surface().Height(Local.X,Local.Y)),P.SemanticLabel});}
            const auto Measured=ShipHatch::Analyze(Sim->Surface().Settings,1,Observations),Truth=ShipHatch::Analyze(Sim->Surface().Settings,1,Reference);
            const double VisibleError=Truth.Volume>0?FMath::Abs(Measured.Volume-Truth.Volume)/Truth.Volume:1;
            Row->SetNumberField(TEXT("coverage"),Measured.Coverage);Row->SetNumberField(TEXT("observed_volume_m3"),Measured.Volume);
            Row->SetNumberField(TEXT("generated_total_volume_m3"),Sim->Surface().Volume);Row->SetNumberField(TEXT("observed_volume_error_percent"),VisibleError*100);
            Row->SetNumberField(TEXT("whole_hold_underestimate_percent"),(Sim->Surface().Volume-Measured.Volume)/Sim->Surface().Volume*100);
            Test->TestTrue(TEXT("actual point-based heatmap integral vs generated surface at measured samples <1 percent"),VisibleError<.01);
            SensorEvidence.Add(MakeShared<FJsonValueObject>(Row));
            // Hold exactly one immutable acquisition while changing only preview controls.
            FVirtualSensorFrameEnvelope E;E.SensorId=A->GetSensorId();E.SensorKind=EVirtualSensorKind::Lidar;E.FrameId=F->FrameId;
            E.TimestampUtc=F->GetAcquisitionStartUtc();E.PointSnapshot=F->Points;E.LidarFrameSnapshot=F;
            FVirtualSensorStreamConfig Config;Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;
            FString Extension,Error;TArray<uint8> Before,After;int32 Count=0;
            Test->TestTrue(TEXT("original binary PCD encoder succeeds"),UVirtualSensorStreamPublisherComponent::SerializePointCloudFrame(E,Config,Extension,Before,Count,Error));
            for(int32 Color=0;Color<5;++Color)
            {
                Host->Monitor->SelectView(1);Host->Monitor->SelectColor(Color);
                auto* PC=World->GetFirstPlayerController();
                for(auto* Selected:{Sim->bMLX?Sim->MLX.Get():nullptr,Sim->bLivox?Sim->Livox.Get():nullptr})if(Selected)
                {
                    Test->TestTrue(TEXT("each selected sensor actually uploads its point preview"),Selected->VisualizationComponent->GetVisiblePointCount()>0);
                    TInlineComponentArray<ULineBatchComponent*> Batches(Selected);
                    Test->TestTrue(TEXT("selected sensor owns a colour point batch"),Batches.Num()>0);
                    for(auto* Batch:Batches)Test->TestFalse(TEXT("both selected point batches remain visible in the actual player viewport"),PC->HiddenPrimitiveComponents.Contains(Batch));
                }
                Host->Monitor->SelectView(0);
            }
            Test->TestTrue(TEXT("PCD encoder succeeds after view changes"),UVirtualSensorStreamPublisherComponent::SerializePointCloudFrame(E,Config,Extension,After,Count,Error));
            Test->TestTrue(TEXT("all point palettes and PointCloudOnly leave payload bytes invariant"),Before==After);
            Test->TestTrue(TEXT("point snapshot ownership remains immutable"),E.PointSnapshot==F->Points);
        }
    }
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        UWorld* Candidate=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)Candidate=C.World();
        if(World!=Candidate){World=Candidate;Sim=nullptr;Host=nullptr;Observed=Now;return false;}
        if(Phase==1&&Measuring&&World&&GFrameCounter!=LastFrame)
        {LastFrame=GFrameCounter;Engine.Add(World->GetDeltaSeconds()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Wall.Add((Now-LastWall)*1000);LastWall=Now;}
        if(Now-Started>(Manual?3600:900)){Test->AddError(TEXT("HATCH-01 runtime timeout"));return true;}
        if(!World||Now-Observed<2||!FinalAcceptanceViewportReady(World))return false;
        if(!Sim){for(TActorIterator<AShipHatchSimulation> I(World);I;++I)Sim=*I;for(TActorIterator<AShipHatchMonitorHost>I(World);I;++I)Host=*I;}
        if(!Sim||!Host||!Host->Monitor||Sim->IsGenerating()||!Sim->MLX||!Sim->Livox)return false;
        if(!ScreenMessages)ScreenMessages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
        if(Manual)
        {if(!Ready){Ready=Now;IFileManager::Get().MakeDirectory(*(ReportRoot()/TEXT("manual")),true);FFileHelper::SaveStringToFile(TEXT("Ready: direct user input only; captures/runtime tests are separate."),*(ReportRoot()/TEXT("manual/ready.txt")));}
         return FPaths::FileExists(ReportRoot()/TEXT("manual/finish.flag"));}
        if(Performance)
        {
            if(!Ready){Ready=Now;Sim->SetSensorSelection(true,SensorCount==2,false);Host->Monitor->SelectView(1);LastWall=Now;return false;}
            if(Now-Ready<10)return false;
            if(!MeasureStart){MeasureStart=Now;LastWall=Now;PerformanceStartFrames[0]=Sim->MLX->ScanComponent->GetRuntimeStatus().FrameId;PerformanceStartFrames[1]=Sim->Livox->ScanComponent->GetRuntimeStatus().FrameId;return false;}
            if(GFrameCounter!=LastFrame){LastFrame=GFrameCounter;Engine.Add(World->GetDeltaSeconds()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Wall.Add((Now-LastWall)*1000);LastWall=Now;}
            if(Now-MeasureStart<30)return false;
            auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("sensors"),SensorCount);R->SetNumberField(TEXT("average_fps"),Mean(Engine)>0?1000/Mean(Engine):0);
            R->SetNumberField(TEXT("engine_p95_ms"),P95(Engine));R->SetNumberField(TEXT("gt_mean_ms"),Mean(Game));R->SetNumberField(TEXT("callback_p95_ms"),P95(Wall));
            R->SetStringField(TEXT("backend"),TEXT("AccurateCpuTrace"));R->SetStringField(TEXT("output"),TEXT("acquisition+3D preview; no broker send"));
            R->SetNumberField(TEXT("mlx_frame"),Sim->MLX->ScanComponent->GetRuntimeStatus().FrameId);R->SetNumberField(TEXT("livox_frame"),Sim->Livox->ScanComponent->GetRuntimeStatus().FrameId);
            const int64 M=Sim->MLX->ScanComponent->GetRuntimeStatus().FrameId-PerformanceStartFrames[0],L=Sim->Livox->ScanComponent->GetRuntimeStatus().FrameId-PerformanceStartFrames[1];
            R->SetNumberField(TEXT("mlx_frames_in_measurement"),double(M));R->SetNumberField(TEXT("livox_frames_in_measurement"),double(L));
            R->SetNumberField(TEXT("mlx_hz"),M/(Now-MeasureStart));R->SetNumberField(TEXT("livox_hz"),L/(Now-MeasureStart));
            Test->TestTrue(TEXT("each selected hatch device keeps acquiring"),M>0&&(SensorCount==1?L==0:L>0));
            R->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
            R->SetNumberField(TEXT("warmup_seconds"),10);R->SetNumberField(TEXT("measurement_seconds"),Now-MeasureStart);
            R->SetStringField(TEXT("profiles"),TEXT("IYOBOT ML-X80 Integration200 FullSpec; optional Livox Mid360S FullSpec"));
            R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
            WriteReport(SensorCount==2?TEXT("performance-2.json"):TEXT("performance-1.json"),R);return true;
        }
        if(Captures)
        {
            const int32 View=Capture%6,Font=(Capture/6)%2,Material=Capture/12;
            if(!Ready)
            {
                Sim->bCoal=Material!=0;Sim->Seed=19019;Sim->Regenerate(false);
                Host->Monitor->SetFontPercent(Font?150:100);Host->Monitor->SelectView(View==0?0:View<4?1:View==4?2:3);
                Host->Monitor->SelectColor(View==2?1:View==3?2:0);Ready=Now;return false;
            }
            if(Now-Ready<2)return false;
            if(!Requested)
            {
                const TCHAR* Names[]={TEXT("render"),TEXT("points-height"),TEXT("points-class"),TEXT("points-intensity"),TEXT("top"),TEXT("heatmap")};
                CapturePath=ReportRoot()/TEXT("captures")/FString::Printf(TEXT("%s-%d-%s.png"),Material?TEXT("coal"):TEXT("iron"),Font?150:100,Names[View]);
                IFileManager::Get().MakeDirectory(*(ReportRoot()/TEXT("captures")),true);PreviousCapture=IFileManager::Get().GetTimeStamp(*CapturePath);
                TArray<FColor> Pixels;FIntVector Size;
                Test->TestTrue(TEXT("capture actual client viewport widget"),FSlateApplication::Get().TakeScreenshot(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size));
                Test->TestEqual(TEXT("capture width is 1920"),Size.X,1920);Test->TestEqual(TEXT("capture height is 1080"),Size.Y,1080);
                int32 VisiblePixels=0;for(int32 Y=50;Y<1000;Y+=4)for(int32 X=30;X<1300;X+=4){const auto P=Pixels[Y*Size.X+X];if(P.R>20||P.G>20||P.B>20)++VisiblePixels;}
                Test->TestTrue(TEXT("3D scene or analysis content is visible in the client area"),VisiblePixels>500);
                TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*CapturePath);
                auto Meta=MakeShared<FJsonObject>();auto* PC=World->GetFirstPlayerController();Meta->SetStringField(TEXT("view_target"),PC->GetViewTarget()->GetName());
                Meta->SetStringField(TEXT("camera_location"),PC->PlayerCameraManager->GetCameraLocation().ToString());Meta->SetStringField(TEXT("camera_rotation"),PC->PlayerCameraManager->GetCameraRotation().ToString());
                Meta->SetStringField(TEXT("ui_geometry"),Host->Monitor->WidgetTree->RootWidget->GetCachedGeometry().GetLocalSize().ToString());
                Meta->SetNumberField(TEXT("font_percent"),Font?150:100);Meta->SetNumberField(TEXT("client_width"),Size.X);Meta->SetNumberField(TEXT("client_height"),Size.Y);
                const auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Meta->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);
                FString Json;FJsonSerializer::Serialize(Meta,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*FPaths::ChangeExtension(CapturePath,TEXT("json")));
                Requested=true;return false;
            }
            if(IFileManager::Get().GetTimeStamp(*CapturePath)<=PreviousCapture)return false;
            Requested=false;Ready=0;return ++Capture==24;
        }
        if(Phase==0)
        {
            if(!Ready){if(Selection>=6)Sim->SetSensorSelection(true,Selection==7,true);else Sim->SetSensorSelection(Selection%3!=1,Selection%3!=0,false);Ready=Now;return false;}
            if(Now-Ready<1||!Sim->HasFreshMeasurement())return false;CheckSensors();Ready=0;
            if(++Selection==3){Sim->bCoal=true;Sim->Regenerate(false);return false;}
            if(Selection<8)return false;
            Sim->SetSensorSelection(true,true,false);
            Phase=1;Host->Monitor->SelectView(3);BeforeRevision=Sim->Revision();ClickRandom();return false;
        }
        if(Phase==1)
        {
            if(Sim->Revision()==BeforeRevision||!Sim->HasFreshMeasurement())return false;
            for(auto* Sensor:{Sim->MLX.Get(),Sim->Livox.Get()})
                for(const auto& P:Sensor->ScanComponent->GetLastPoints())if(P.bHit&&P.SemanticLabel==TEXT("HatchCargo"))
                {const FVector L=Sim->GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01;RandomHeightMaxError=FMath::Max(RandomHeightMaxError,FMath::Abs(L.Z-Sim->Surface().Height(L.X,L.Y)));}
            Mesh.Add(Sim->LastMeshApplyMs);++Regenerations;if(Regenerations>10)RandomSeeds.Add(Sim->Surface().Settings.Seed);
            if(Regenerations==10)
            {
                // Shape churn can prevent the 5Hz analysis from ever displaying a frame.
                // Warm the same visible heatmap/texture path before measuring retained memory.
                AnalysisAfterTen=Host->Monitor->AnalysisBuilds;Phase=2;Ready=Now;return false;
            }
            if(Regenerations<60){BeforeRevision=Sim->Revision();ClickRandom();return false;}
            Measuring=false;StopBoundary();Phase=3;Ready=Now;return false;
        }
        if(Phase==3)
        {
            if(Now-Ready<.5)return false;
            FlushRenderingCommands();CollectGarbage(RF_NoFlags);RawMemoryAfter=FPlatformMemory::GetStats().UsedPhysical;
            FMemory::Trim();MemoryAfter=FPlatformMemory::GetStats().UsedPhysical;CommittedAfter=FPlatformMemory::GetStats().UsedVirtual;
            auto R=MakeShared<FJsonObject>();R->SetArrayField(TEXT("sensor_evidence"),SensorEvidence);R->SetNumberField(TEXT("regenerations"),50);
            R->SetNumberField(TEXT("memory_before"),double(MemoryBefore));R->SetNumberField(TEXT("memory_after"),double(MemoryAfter));
            R->SetNumberField(TEXT("raw_working_set_before_free_cache_trim"),double(RawMemoryBefore));R->SetNumberField(TEXT("raw_working_set_after_fifty_before_free_cache_trim"),double(RawMemoryAfter));
            R->SetNumberField(TEXT("committed_before"),double(CommittedBefore));R->SetNumberField(TEXT("committed_after"),double(CommittedAfter));
            R->SetStringField(TEXT("memory_method"),TEXT("same visible-path warmup; render drain + GC + engine free allocator cache Trim at both boundaries; raw working set retained separately"));
            R->SetObjectField(TEXT("resources_before"),ResourceBefore);R->SetObjectField(TEXT("resources_after"),Resources());
            R->SetNumberField(TEXT("analysis_after_initial_ten"),double(AnalysisAfterTen));R->SetNumberField(TEXT("analysis_at_memory_baseline"),double(AnalysisAtBaseline));
            R->SetNumberField(TEXT("analysis_after_fifty"),double(Host->Monitor->AnalysisBuilds));
            R->SetNumberField(TEXT("mesh_max_ms"),Mesh.Num()?FMath::Max(Mesh):0);R->SetNumberField(TEXT("engine_max_ms"),Engine.Num()?FMath::Max(Engine):0);
            R->SetNumberField(TEXT("game_max_ms"),Game.Num()?FMath::Max(Game):0);R->SetNumberField(TEXT("generation_callback_max_ms"),Wall.Num()?FMath::Max(Wall):0);
            Test->TestTrue(TEXT("50 mesh commits have no >150ms hitch"),Mesh.Num()==50&&FMath::Max(Mesh)<=150);
            Test->TestTrue(TEXT("regeneration engine frames remain <=150ms"),Engine.Num()&&FMath::Max(Engine)<=150);
            Test->TestTrue(TEXT("regeneration game thread remains <=150ms"),Game.Num()&&FMath::Max(Game)<=150);
            // WindowsPlatformMemory.cpp: UsedPhysical=WorkingSetSize, UsedVirtual=PagefileUsage (commit).
            // Resident pages can warm without allocating memory. Keep both raw measurements in the report.
            Test->TestTrue(TEXT("retained committed process memory after 50 changes grows <=8MiB"),CommittedAfter<=CommittedBefore+8*1024*1024);
            Test->TestTrue(TEXT("resident process memory after 50 changes grows <=8MiB"),MemoryAfter<=MemoryBefore+8*1024*1024);
            auto After=Resources();
            for(auto* Sensor:{Sim->MLX.Get(),Sim->Livox.Get()})
            {const auto B=ResourceBefore->GetObjectField(Sensor->GetSensorId()),A=After->GetObjectField(Sensor->GetSensorId());
             for(const auto& Pair:B->Values)Test->TestEqual(TEXT("each retained acquisition/preview buffer has constant allocation"),A->GetNumberField(Pair.Key),Pair.Value->AsNumber());}
            int32 Parts=0;for(TActorIterator<AShipHatchPart>I(World);I;++I)if(!I->IsActorBeingDestroyed())++Parts;
            Test->TestEqual(TEXT("no helper Actor accumulation after 50 changes"),Parts,4);
            Test->TestEqual(TEXT("50 production button clicks use 50 distinct seeds"),RandomSeeds.Num(),50);
            Test->TestTrue(TEXT("all regenerated collision surfaces match fresh measured cargo within 15 cm"),RandomHeightMaxError<.15);
            R->SetNumberField(TEXT("random_height_max_error_m"),RandomHeightMaxError);
            R->SetNumberField(TEXT("live_parts"),Parts);ResultReport=R;
            HiddenBuilds=Host->Monitor->AnalysisBuilds;HiddenFrames[0]=Sim->MLX->ScanComponent->GetRuntimeStatus().FrameId;HiddenFrames[1]=Sim->Livox->ScanComponent->GetRuntimeStatus().FrameId;
            Host->Monitor->SetVisibility(ESlateVisibility::Collapsed);Sim->MLX->StartSensor();Sim->Livox->StartSensor();Phase=4;Ready=Now;return false;
        }
        if(Phase==2)
        {
            if(!BoundaryStopped)
            {if(Now-Ready<10||Host->Monitor->AnalysisBuilds<AnalysisAfterTen+5)return false;StopBoundary();BoundaryStopped=true;Ready=Now;return false;}
            if(Now-Ready<.5)return false;
            FlushRenderingCommands();CollectGarbage(RF_NoFlags);RawMemoryBefore=FPlatformMemory::GetStats().UsedPhysical;
            FMemory::Trim();MemoryBefore=FPlatformMemory::GetStats().UsedPhysical;CommittedBefore=FPlatformMemory::GetStats().UsedVirtual;
            ResourceBefore=Resources();
            AnalysisAtBaseline=Host->Monitor->AnalysisBuilds;Mesh.Reset();Engine.Reset();Game.Reset();Wall.Reset();LastWall=FPlatformTime::Seconds();
            Measuring=true;Phase=1;BeforeRevision=Sim->Revision();ClickRandom();return false;
        }
        if(Phase==4)
        {
            if(Now-Ready<1.5)return false;
            Test->TestEqual(TEXT("hidden analysis performs zero builds"),Host->Monitor->AnalysisBuilds,HiddenBuilds);
            Test->TestTrue(TEXT("hiding monitor never stops measurement"),Sim->MLX->IsSensorRunning()&&Sim->Livox->IsSensorRunning()&&
                Sim->MLX->ScanComponent->GetRuntimeStatus().FrameId>HiddenFrames[0]&&Sim->Livox->ScanComponent->GetRuntimeStatus().FrameId>HiddenFrames[1]);
            Host->Monitor->SetVisibility(ESlateVisibility::Visible);
            FString PolicyText;Test->TestTrue(TEXT("existing vocabulary policy loaded"),FFileHelper::LoadFileToString(PolicyText,*(FPaths::ProjectDir()/TEXT("Scripts/fixtures/sensor_ui_vocabulary.json"))));
            TSharedPtr<FJsonObject> Policy;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PolicyText),Policy);
            TArray<FString> Texts;TArray<UWidget*> Widgets;Host->Monitor->WidgetTree->GetAllWidgets(Widgets);
            for(auto* W:Widgets){if(auto* T=Cast<UTextBlock>(W))Texts.Add(T->GetText().ToString());if(auto* C=Cast<UComboBoxString>(W))for(int32 I=0;I<C->GetOptionCount();++I)Texts.Add(C->GetOptionAtIndex(I));}
            if(Policy)for(const auto& Word:Policy->GetArrayField(TEXT("forbidden")))for(const auto& T:Texts)Test->TestFalse(TEXT("HATCH default copy follows owned UI vocabulary: ")+Word->AsString(),T.Contains(Word->AsString(),ESearchCase::IgnoreCase));
            ResultReport->SetNumberField(TEXT("hidden_analysis_build_delta"),double(Host->Monitor->AnalysisBuilds-HiddenBuilds));ResultReport->SetNumberField(TEXT("vocabulary_texts_checked"),Texts.Num());
            auto* Files=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>();
            Test->TestTrue(TEXT("map startup and 50 random changes never auto-save analysis"),Files->GetRecentRequests().IsEmpty());
            for(auto* W:Widgets)if(auto* B=Cast<UButton>(W))if(auto* T=Cast<UTextBlock>(B->GetContent()))if(T->GetText().ToString()==TEXT("분석 JSON 저장 · 현재 측정값")){B->OnClicked.Broadcast();break;}
            const auto Requests=Files->GetRecentRequests();Test->TestEqual(TEXT("explicit analysis button admits exactly one request"),Requests.Num(),1);
            if(Requests.Num())AnalysisRequest=Requests[0].RequestId;
            Phase=5;Ready=Now;return false;
        }
        if(Phase==5)
        {
            auto* Files=World->GetSubsystem<UVirtualSensorFileSaveSubsystem>();const auto Status=Files->GetRequestStatus(AnalysisRequest);
            if(!Status.IsTerminal()&&Now-Ready<10)return false;
            Test->TestEqual(TEXT("analysis uses the existing asynchronous writer successfully"),Status.State,EVirtualSensorFileSaveState::Succeeded);
            Test->TestEqual(TEXT("analysis exports exactly one JSON file"),Status.FileResults.Num(),1);
            if(Status.FileResults.Num())
            {
                FString Json;Test->TestTrue(TEXT("analysis file exists"),FFileHelper::LoadFileToString(Json,*Status.FileResults[0].AbsolutePath));
                TSharedPtr<FJsonObject> Export;Test->TestTrue(TEXT("analysis file is JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Export));
                if(Export)
                {
                    Test->TestEqual(TEXT("independent analysis schema"),Export->GetStringField(TEXT("schema")),FString(TEXT("ma0t10.hatch-analysis.v1")));
                    Test->TestEqual(TEXT("analysis keeps the measured geometry seed"),Export->GetNumberField(TEXT("seed")),double(Sim->Surface().Settings.Seed));
                    Test->TestEqual(TEXT("grid dimensions explain the row-major array"),Export->GetArrayField(TEXT("height_m_row_major")).Num(),int32(Export->GetNumberField(TEXT("nx"))*Export->GetNumberField(TEXT("ny"))));
                    int32 Missing=0;for(const auto& Cell:Export->GetArrayField(TEXT("height_m_row_major")))Missing+=Cell->Type==EJson::Null;
                    Test->TestTrue(TEXT("unobserved cells are null, never fabricated zero heights"),Missing>0);
                }
                ResultReport->SetStringField(TEXT("explicit_analysis_file"),Status.FileResults[0].AbsolutePath);
            }
            ResultReport->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());WriteReport(TEXT("runtime.json"),ResultReport.ToSharedRef());return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchRuntimeTest,"HATCH01.RuntimeRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchRuntimeTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchRuntime(this,false,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchCaptureTest,"HATCH01.CaptureRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchCaptureTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchRuntime(this,true,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchPerformanceTest,"HATCH01.PerformanceRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchPerformanceTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchRuntime(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
