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
#include "HAL/LowLevelMemTracker.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/LineBatchComponent.h"
#include "Components/ScrollBox.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Json.h"
#include "RenderingThread.h"
#include "RHICommandList.h"

void ValidateHatchPresentation(FAutomationTestBase*,UShipHatchMonitorWidget*);
struct FHatchDigProbe
{
    static AShipHatchSimulation& Simulation(UShipHatchMonitorWidget& M){return *M.Simulation.Get();}
    static FPointerEvent Pointer(FVector2D P,FVector2D Last,bool Held)
    {TSet<FKey> Keys;if(Held)Keys.Add(EKeys::LeftMouseButton);return FPointerEvent(0,P,Last,Keys,EKeys::LeftMouseButton,0,FModifierKeysState());}
    static bool Project(UShipHatchMonitorWidget& M,FVector2D XY,FVector2D& P)
    {
        const auto& S=M.Simulation->Surface();const FVector World=M.Simulation->GetActorTransform().TransformPosition(FVector(XY,S.Height(XY.X,XY.Y))*100);
        auto* PC=M.GetOwningPlayer();FVector2D Pixel;int32 W,H;PC->GetViewportSize(W,H);
        if(!PC->ProjectWorldLocationToScreen(World,Pixel)||W<=0||H<=0)return false;
        const auto G=M.GetCachedGeometry();P=G.LocalToAbsolute(Pixel*G.GetLocalSize()/FVector2D(W,H));
        return M.SceneInputAt(G,P);
    }
    static bool Click(UShipHatchMonitorWidget& M,FVector2D XY)
    {
        FVector2D P;if(!Project(M,XY,P))return false;const uint64 Revision=M.Simulation->Revision();const auto G=M.GetCachedGeometry();
        M.NativeOnMouseButtonDown(G,Pointer(P,P,true));M.NativeOnMouseButtonUp(G,Pointer(P,P,false));return M.Simulation->Revision()!=Revision;
    }
    static void Controls(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        auto& Sim=*M.Simulation;double Opening,Floor,Area;Sim.Coverage(Opening,Floor,Area);
        if(!Sim.bFieldLayout)
        {
        T.TestEqual(TEXT("central layout covers complete opening FOV on 80x80 grid"),Opening,100.);
        T.TestEqual(TEXT("central layout covers structurally visible empty floor FOV on 80x80 grid"),Floor,100.);
        for(int32 X:{-1,1})for(int32 Y:{-1,1})
        {T.TestTrue(TEXT("opening corner is inside selected-device FOV"),Sim.VisibleFromSelected(FVector(X*Sim.OpeningLength*.5,Y*Sim.OpeningWidth*.5,Sim.HoldDepth+Sim.CoamingHeight)));
         T.TestTrue(TEXT("visible empty-hold floor corner remains within FOV and range"),Sim.VisibleFromSelected(FVector(X*Sim.HoldLength*.5,Y*Sim.HoldWidth*.5,0)));}
        }
        else {T.TestTrue(TEXT("field configuration computes bounded nonempty opening and visible-floor coverage"),Opening>0&&Opening<=100&&Floor>0&&Floor<=100);T.TestNotNull(TEXT("third field sensor exists"),Sim.CabRight.Get());}
        T.TestTrue(TEXT("structurally visible floor area is positive and bounded by hold"),Area>0&&Area<=Sim.HoldLength*Sim.HoldWidth);
        T.TestFalse(TEXT("laser illustration defaults off"),Sim.IsScanDisplayEnabled());
        const auto Frame=Sim.MLX->ScanComponent->GetLastFrameSnapshot();const auto* Points=Frame->Points.Get();
        M.OnScanToggle();T.TestTrue(TEXT("laser toggle uses at most 96 measured rays"),Sim.IsScanDisplayEnabled()&&Sim.DisplayRayCount()>0&&Sim.DisplayRayCount()<=AShipHatchSimulation::DisplayRayLimit);
        M.OnScanToggle();T.TestTrue(TEXT("laser off clears every displayed ray"),!Sim.IsScanDisplayEnabled()&&Sim.DisplayRayCount()==0);
        T.TestTrue(TEXT("presentation toggles never replace immutable measured point snapshot"),Sim.MLX->ScanComponent->GetLastFrameSnapshot()->Points.Get()==Points);
        auto* S=M.Simulation.Get();const double OldMin=S->FillMin,OldMax=S->FillMax,OldHeight=S->SensorAboveDeck;
        S->FillMin=47;S->FillMax=72;S->SensorAboveDeck=11;M.OnGrid(1.75);
        M.Bind(M.Host.Get(),S);M.OnSettings();M.OnSettings();M.SetFontPercent(150);
        T.TestEqual(TEXT("rebind/drawer/font retains current minimum fill"),M.MinFill->GetValue(),47.f);
        T.TestEqual(TEXT("rebind/drawer/font retains current maximum fill"),M.MaxFill->GetValue(),72.f);
        T.TestEqual(TEXT("rebind/drawer/font retains current sensor height"),M.SensorHeight->GetValue(),11.f);
        T.TestEqual(TEXT("rebind/drawer/font retains analysis cell size"),M.GridSize->GetValue(),1.75f);
        M.MinFill->SetValue(52);M.OnSettings();M.OnSettings();T.TestEqual(TEXT("drawer toggle preserves an unapplied edit"),M.MinFill->GetValue(),52.f);
        S->FillMin=OldMin;S->FillMax=OldMax;S->SensorAboveDeck=OldHeight;M.OnGrid(1);M.SyncSettings();M.SetFontPercent(100);
        T.TestFalse(TEXT("grab defaults off"),M.bDigEnabled);M.DigToggle->OnClicked.Broadcast();T.TestTrue(TEXT("production toggle enables grab"),M.bDigEnabled);
    }
    static void InputGuards(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        FVector2D P;T.TestTrue(TEXT("projected known cargo point is in the interactive pane"),Project(M,FVector2D(0,0),P));
        const auto G=M.GetCachedGeometry();auto* S=M.Simulation.Get();const auto Before=S->Surface().Hash();
        M.OnDigToggle();Click(M,FVector2D(0,0));T.TestEqual(TEXT("disabled mode ignores a short click"),S->Surface().Hash(),Before);M.OnDigToggle();
        M.NativeOnMouseButtonDown(G,Pointer(P,P,true));M.PointerDownTime-=.5;M.NativeOnMouseButtonUp(G,Pointer(P,P,false));
        T.TestEqual(TEXT("long hold never excavates"),S->Surface().Hash(),Before);
        M.NativeOnMouseButtonDown(G,Pointer(P,P,true));const auto WheelReply=M.NativeOnMouseWheel(G,Pointer(P,P,true));
        T.TestTrue(TEXT("wheel cancellation releases the owned pointer capture"),WheelReply.ShouldReleaseMouse());M.NativeOnMouseButtonUp(G,Pointer(P,P,false));
        T.TestEqual(TEXT("wheel followed by button-up never excavates"),S->Surface().Hash(),Before);
        M.NativeOnMouseButtonDown(G,Pointer(P,P,true));M.NativeOnMouseCaptureLost(FCaptureLostEvent());M.NativeOnMouseButtonUp(G,Pointer(P,P,false));
        T.TestEqual(TEXT("lost capture cancels click"),S->Surface().Hash(),Before);
        M.NativeOnMouseButtonDown(G,Pointer(P,P,true));M.SelectView(1);M.NativeOnMouseButtonUp(G,Pointer(P,P,false));
        T.TestEqual(TEXT("view change cancels an active click"),S->Surface().Hash(),Before);M.SelectView(0);
        const double Yaw=M.OrbitYaw;M.NativeOnMouseButtonDown(G,Pointer(P,P,true));M.NativeOnMouseMove(G,Pointer(P+FVector2D(30,10),P,true));M.NativeOnMouseButtonUp(G,Pointer(P+FVector2D(30,10),P,false));
        T.TestEqual(TEXT("excavation-mode left drag leaves camera locked"),M.OrbitYaw,Yaw);T.TestEqual(TEXT("drag release never excavates"),S->Surface().Hash(),Before);
        M.OnDigToggle();M.NativeOnMouseButtonDown(G,Pointer(P,P,true));M.NativeOnMouseMove(G,Pointer(P+FVector2D(30,10),P,true));M.NativeOnMouseButtonUp(G,Pointer(P+FVector2D(30,10),P,false));
        T.TestTrue(TEXT("navigation-mode drag rotates existing camera"),M.OrbitYaw!=Yaw);M.OnDigToggle();
        T.TestFalse(TEXT("outside 3D rectangle is not a grab target"),M.DigAt(G,G.LocalToAbsolute(FVector2D(5,5))));M.SetCameraPreset(0);
        FVector Hit;const auto Pose=S->GetActorTransform();
        T.TestFalse(TEXT("deck occludes cargo beneath it"),S->PickCargo(Pose.TransformPosition(FVector(0,(S->OpeningWidth*.5+2)*100,4000)),Pose.TransformVector(FVector(0,0,-1)),Hit));
        T.TestFalse(TEXT("outside hold has no target"),S->PickCargo(Pose.TransformPosition(FVector(10000,10000,4000)),Pose.TransformVector(FVector(0,0,-1)),Hit));
        T.TestTrue(TEXT("removed central gantry no longer obstructs cargo picking"),S->PickCargo(Pose.TransformPosition(FVector(0,0,10000)),Pose.TransformVector(FVector(0,0,-1)),Hit));

    }
    static void Undo(UShipHatchMonitorWidget& M){M.UndoButton->OnClicked.Broadcast();}
    static void Restore(UShipHatchMonitorWidget& M){M.RestoreButton->OnClicked.Broadcast();}
    static void SettingsForCapture(UShipHatchMonitorWidget& M)
    {if(!M.bSettings)M.OnSettings();CastChecked<UScrollBox>(M.SettingsPanel->GetContent())->ScrollWidgetIntoView(M.DigRadius,false,EDescendantScrollDestination::TopOrLeft,80);}
    static void SensorCapture(UShipHatchMonitorWidget& M,int32 View)
    {if(M.bSettings)M.OnSettings();M.SelectView(View);M.SetSplitView(true);M.SetCameraPreset(1);if(!M.Simulation->IsScanDisplayEnabled())M.OnScanToggle();}
    static void PointGate(FAutomationTestBase& T,UShipHatchMonitorWidget& M,bool Stale)
    {
        auto* S=M.Simulation.Get();auto* PC=M.GetOwningPlayer();
        for(auto* A:{S->MLX.Get(),S->Livox.Get(),S->CabRight.Get()})if(A)
        {
            TInlineComponentArray<ULineBatchComponent*> Batches(A);T.TestTrue(TEXT("owned colored point proxy exists"),Batches.Num()>0);
            for(auto* B:Batches)
            {
                T.TestEqual(TEXT("retained preview is never hidden while waiting for measurement"),PC->HiddenPrimitiveComponents.Contains(B),false);
                T.TestTrue(TEXT("edit keeps populated point buffer instead of empty/full churn"),B->BatchedPoints.Num()>0);
                double Error=0;int32 Cargo=0;
                for(const auto& P:B->BatchedPoints)
                {
                    if(P.PointSize<=0)continue;
                    const FVector L=S->GetActorTransform().InverseTransformPosition(P.Position)*.01;
                    if(FMath::Abs(L.X)<S->HoldLength*.5-.1&&FMath::Abs(L.Y)<S->HoldWidth*.5-.1&&L.Z>.05&&L.Z<S->HoldDepth-.1)
                    {++Cargo;Error=FMath::Max(Error,FMath::Abs(L.Z-S->Surface().Height(L.X,L.Y)));}
                }
                T.TestTrue(FString::Printf(TEXT("retained and fresh point preview %s: cargo=%d error=%.5fm"),*A->GetSensorId(),Cargo,Error),Cargo>100&&Error<.15);
            }
        }
    }
    static void CheckCards(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        const auto* A=M.GetAnalysis();T.TestNotNull(TEXT("analysis refreshed to current excavation revision"),A);if(!A)return;
        T.TestTrue(TEXT("fill card follows remaining heightfield"),FMath::Abs(FCString::Atod(*M.MetricValues[0]->GetText().ToString())-M.Simulation->Surface().Fill)<.051);
        T.TestTrue(TEXT("volume card follows full remaining simulated hold integral"),FMath::Abs(FCString::Atod(*M.MetricValues[1]->GetText().ToString())-M.Simulation->Surface().Volume)<.51);
        T.TestTrue(TEXT("last and total removed volume are visible"),M.DigAmounts->GetText().ToString().Contains(FString::Printf(TEXT("이번 %.2f m³ · 누적 %.2f m³"),M.Simulation->LastRemovedVolume(),M.Simulation->TotalRemovedVolume())));
    }
};
namespace
{
FString DigRoot(){FString R;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),R);return R.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-03"):R;}
void DigJson(const FString& Name,const TSharedRef<FJsonObject>& R)
{FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*DigRoot(),true);FFileHelper::SaveStringToFile(Text,*(DigRoot()/Name));}
class FHatchDig final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;bool Captures,IdleControl;double Started=FPlatformTime::Seconds(),Changed=0,LastWall=0,MeasureStart=0;int32 MeasuredView=1;
    int32 Phase=0,Clicks=0,Material=0,State=0,View=0;uint64 BeforeRevision=0,LastEngine=0,RawBefore=0,Rss=0,Commit=0,Buffers=0,MeshBytes=0;
    TArray<float> Original,AfterOne,BeforeLast;TArray<double> Engine,Game,Wall,Edits,FreshLatency;
    TArray<FColor> OpaqueReference;
    double OriginalVolume=0,BeforeVolume=0,MaxHeightError=0,WorkOpening=0,WorkFloor=0,WorkArea=0;uint64 WarmBuilds=0;
    int64 BeforeFrames[3]={-1,-1};TArray<TSharedPtr<FJsonValue>> Measurements;
    TSharedRef<FJsonObject> Result=MakeShared<FJsonObject>();TUniquePtr<TGuardValue<bool>> Messages;
    void MemoryDetails(const TCHAR* Boundary)
    {
#if ENABLE_LOW_LEVEL_MEM_TRACKER
        if(!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryAudit"))||!FLowLevelMemTracker::IsEnabled())return;
        auto& LLM=FLowLevelMemTracker::Get();LLM.UpdateStatsPerFrame();Result->SetBoolField(TEXT("diagnostic_only"),true);
        for(auto Tracker:{ELLMTracker::Default,ELLMTracker::Platform})
        {
            TMap<FName,uint64> Tags;LLM.GetTrackedTagsNamesWithAmount(Tags,Tracker,ELLMTagSet::None);auto Values=MakeShared<FJsonObject>();
            for(const auto& Pair:Tags)Values->SetNumberField(Pair.Key.ToString(),double(Pair.Value));
            Result->SetObjectField(FString(Boundary)+(Tracker==ELLMTracker::Default?TEXT("_llm_default"):TEXT("_llm_platform")),Values);
        }
#endif
    }
    TSharedRef<FJsonObject> Resources(AShipHatchSimulation& S)
    {
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("mesh"),double(S.MeshScratchBytes()));R->SetNumberField(TEXT("generation"),double(S.GenerationBufferBytes()));R->SetNumberField(TEXT("dig"),double(S.DigBufferBytes()));
        for(auto* A:{S.MLX.Get(),S.Livox.Get(),S.CabRight.Get()})if(A)
        {
            auto Sensor=MakeShared<FJsonObject>();const auto Scan=A->ScanComponent->GetResourceSnapshot(),Preview=A->VisualizationComponent->GetResourceSnapshot();
            for(const auto& P:Scan.OwnedCpuBytes)Sensor->SetNumberField(TEXT("scan/")+P.Key,double(P.Value));for(const auto& P:Scan.SharedCpuBytes)Sensor->SetNumberField(TEXT("shared/")+P.Key,double(P.Value));for(const auto& P:Preview.OwnedCpuBytes)Sensor->SetNumberField(TEXT("view/")+P.Key,double(P.Value));
            R->SetObjectField(A->GetSensorId(),Sensor);
        }
        return R;
    }
    void Stop(AShipHatchSimulation& S){for(int32 I=0;I<3;++I)if(auto* A=S.SensorAt(I))A->StopSensor();}
    void Start(AShipHatchSimulation& S){for(int32 I=0;I<3;++I)if(S.SensorEnabled(I))if(auto* A=S.SensorAt(I))A->StartSensor();}
    void DrainBoundary()
    {
        FlushRenderingCommands();if(!FParse::Param(FCommandLine::Get(),TEXT("HatchFullRhiDrain")))return;
        ENQUEUE_RENDER_COMMAND(HatchMemoryBoundary)([](FRHICommandListImmediate& Cmd){Cmd.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);Cmd.BlockUntilGPUIdle();});
        FlushRenderingCommands();
    }
    void Shot(UWorld& W,UShipHatchMonitorWidget& M,const FString& Name)
    {
        TArray<FColor> Pixels;FIntVector Size;
        Test->TestTrue(TEXT("capture actual client viewport"),FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size));
        if(!Pixels.Num())return;TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        const FString Path=DigRoot()/TEXT("captures")/Name;IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);FFileHelper::SaveArrayToFile(Png,*Path);
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetNumberField(TEXT("font_percent"),M.GetFontScale()*100);
        const auto Window=FSlateApplication::Get().FindWidgetWindow(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef());R->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);
        R->SetStringField(TEXT("input"),TEXT("automated native pointer events; not Computer Use"));R->SetNumberField(TEXT("volume_m3"),M.GetAnalysis()?M.GetAnalysis()->Volume:0);
        const auto& S=FHatchDigProbe::Simulation(M);R->SetNumberField(TEXT("surface_volume_m3"),S.Surface().Volume);R->SetNumberField(TEXT("last_removed_m3"),S.LastRemovedVolume());R->SetNumberField(TEXT("total_removed_m3"),S.TotalRemovedVolume());R->SetNumberField(TEXT("undo_count"),S.UndoCount());R->SetNumberField(TEXT("surface_hash"),S.Surface().Hash());
        double Opening,Floor,Area;S.Coverage(Opening,Floor,Area);R->SetNumberField(TEXT("opening_fov_percent"),Opening);R->SetNumberField(TEXT("visible_floor_fov_percent"),Floor);R->SetNumberField(TEXT("visible_floor_m2"),Area);R->SetNumberField(TEXT("display_rays"),S.DisplayRayCount());R->SetBoolField(TEXT("analysis_preview"),M.IsAnalysisPreview());
        DigJson(TEXT("captures/")+FPaths::ChangeExtension(Name,TEXT("json")),R);
    }
    bool Click(UShipHatchMonitorWidget& M,int32 I)
    {
        auto& S=FHatchDigProbe::Simulation(M);BeforeRevision=S.Revision();BeforeFrames[0]=S.MLX->ScanComponent->GetRuntimeStatus().FrameId;BeforeFrames[1]=S.Livox->ScanComponent->GetRuntimeStatus().FrameId;BeforeFrames[2]=S.CabRight?S.CabRight->ScanComponent->GetRuntimeStatus().FrameId:-1;
        const auto Frame=S.MLX->ScanComponent->GetLastFrameSnapshot();const uint32 Before=FCrc::MemCrc32(Frame->Points->GetData(),Frame->Points->Num()*sizeof(FVirtualLidarPoint));
        FVector2D Target(I%5==2?1.25:(I%5-2)*2.5,((I/5)%5-2)*3.),Pixel;
        // The new grab cab occupies the former Y=-3m fixture row.
        if(FMath::IsNearlyEqual(Target.Y,-3.))Target.Y=-4.5;
        if(!FHatchDigProbe::Project(M,Target,Pixel))Target=FVector2D(3,-5);
        const bool Accepted=FHatchDigProbe::Click(M,Target);
        Test->TestEqual(TEXT("immediate display correction never mutates measured payload points"),FCrc::MemCrc32(Frame->Points->GetData(),Frame->Points->Num()*sizeof(FVirtualLidarPoint)),Before);
        Test->TestTrue(TEXT("dig never stops selected acquisition scheduler"),S.MLX->IsSensorRunning()&&S.Livox->IsSensorRunning());
        if(Accepted&&MeasuredView==1){FHatchDigProbe::PointGate(*Test,M,true);Test->TestNotNull(TEXT("2D/heatmap remains populated during measurement replacement"),M.GetDisplayAnalysis());}
        return Accepted;
    }
public:
    FHatchDig(FAutomationTestBase* T,bool C,bool Idle=false):Test(T),Captures(C),IdleControl(Idle){FParse::Value(FCommandLine::Get(),TEXT("HatchDigView="),MeasuredView);MeasuredView=MeasuredView==0?0:1;}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Started>600){Test->AddError(TEXT("HATCH-03 timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||!FinalAcceptanceViewportReady(W))return false;AShipHatchMonitorHost* H=nullptr;
        for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;
        if(!H||!H->Monitor||!H->Simulation||H->Simulation->IsGenerating()||!H->Simulation->MLX||!H->Simulation->Livox)return false;
        auto& S=*H->Simulation;auto& M=*H->Monitor;if(!Messages)Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
        if(Phase==0)
        {
            if(!M.GetAnalysis())return false;FHatchDigProbe::Controls(*Test,M);int32 Font=100;FParse::Value(FCommandLine::Get(),TEXT("HatchFontPercent="),Font);M.SetFontPercent(Font);M.SelectView(0);M.SetSplitView(false);M.SetCameraPreset(0);
            S.Coverage(WorkOpening,WorkFloor,WorkArea);Result->SetNumberField(TEXT("opening_fov_percent"),WorkOpening);Result->SetNumberField(TEXT("visible_floor_fov_percent"),WorkFloor);Result->SetNumberField(TEXT("structurally_visible_floor_m2"),WorkArea);Result->SetNumberField(TEXT("mlx_above_deck_m"),S.EffectiveMLXHeight());
            Original=S.Surface().Heights;OriginalVolume=S.Surface().Volume;Phase=1;Changed=Now;return false;
        }
        if(Phase==1)
        {
            if(Now-Changed<.5)return false;FHatchDigProbe::InputGuards(*Test,M);Phase=2;Changed=Now;return false;
        }
        if(Phase==2)
        {
            if(Now-Changed<.4||!M.GetAnalysis())return false;
            Test->TestTrue(TEXT("production short pointer click excavates cargo"),FHatchDigProbe::Click(M,FVector2D(2,0)));
            AfterOne=S.Surface().Heights;BeforeRevision=S.Revision();Phase=3;return false;
        }
        if(Phase==3)
        {
            if(!M.GetAnalysis())return false;FHatchDigProbe::CheckCards(*Test,M);
            Test->TestTrue(TEXT("repeat click removes more"),FHatchDigProbe::Click(M,FVector2D(2,0)));
            FHatchDigProbe::Undo(M);Test->TestTrue(TEXT("undo restores last field bit-identically"),S.Surface().Heights==AfterOne);
            FHatchDigProbe::Undo(M);Test->TestTrue(TEXT("multi-step undo restores original bit-identically"),S.Surface().Heights==Original);
            Test->TestFalse(TEXT("empty undo is harmless"),S.UndoDig());
            FHatchDigProbe::Click(M,FVector2D(2,0));FHatchDigProbe::Restore(M);
            Test->TestTrue(TEXT("original button restores entire original field bit-identically"),S.Surface().Heights==Original&&S.Surface().Volume==OriginalVolume&&S.TotalRemovedVolume()==0&&S.UndoCount()==0);
            // Warm complete production cycles, not 17 edits in one game frame:
            // each cancellation/acquisition/preview/analysis must actually finish.
            Clicks=0;M.SelectView(MeasuredView);M.SetSplitView(false);M.SetCameraPreset(0);Phase=20;return false;
        }
        if(Phase==20)
        {
            if(!M.GetAnalysis())return false;
            if(Clicks<17){Test->TestTrue(TEXT("warm complete production click cycle"),Click(M,Clicks++));return false;}
            Test->TestEqual(TEXT("history cap 16"),S.UndoCount(),16);
            const auto Last=S.Surface().Heights;const int32 Count=S.UndoCount();
            Test->TestFalse(TEXT("invalid edit rejected without damaging a full ring"),S.Dig(S.GetActorTransform().TransformPosition(FVector(0,0,4000)),S.GetActorTransform().TransformVector(FVector(0,0,-1)),-2,1.5));
            Test->TestTrue(TEXT("failed edit preserves full history and field"),S.UndoCount()==Count&&S.Surface().Heights==Last);
            FHatchDigProbe::Restore(M);Clicks=0;Changed=Now;WarmBuilds=M.AnalysisBuilds;Phase=Captures?10:4;return false;
        }
        if(Phase==4)
        {
            int32 Warmup=35;FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryWarmup="),Warmup);
            if(Now-Changed<Warmup||M.AnalysisBuilds<WarmBuilds+5)return false;Stop(S);Changed=Now;Phase=5;return false;
        }
        if(Phase==5)
        {
            if(Now-Changed<.5)return false;DrainBoundary();CollectGarbage(RF_NoFlags);RawBefore=FPlatformMemory::GetStats().UsedPhysical;FMemory::Trim();
            Rss=FPlatformMemory::GetStats().UsedPhysical;Commit=FPlatformMemory::GetStats().UsedVirtual;Buffers=S.DigBufferBytes();MeshBytes=S.MeshScratchBytes();
            MemoryDetails(TEXT("before"));
            Result->SetObjectField(TEXT("resources_before"),Resources(S));
            MeasureStart=Now;
            BeforeRevision=S.Revision();BeforeLast=S.Surface().Heights;BeforeVolume=S.Surface().Volume;LastWall=Now;LastEngine=GFrameCounter;
            Start(S);if(!IdleControl)Test->TestTrue(TEXT("first measured production click"),Click(M,0));Edits.Add(S.LastMeshApplyMs);Changed=Now;Phase=6;return false;
        }
        if(Phase==6)
        {
            if(GFrameCounter!=LastEngine){Engine.Add(W->GetDeltaSeconds()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Wall.Add((Now-LastWall)*1000);LastWall=Now;LastEngine=GFrameCounter;}
            if((IdleControl&&Now-Changed<.6)||!M.GetAnalysis())return false;FHatchDigProbe::CheckCards(*Test,M);
            if(!IdleControl)
            {
                if(MeasuredView==1)FHatchDigProbe::PointGate(*Test,M,false);
                auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("click"),Clicks+1);Row->SetNumberField(TEXT("revision"),double(S.Revision()));Row->SetNumberField(TEXT("removed_m3"),S.LastRemovedVolume());
                FreshLatency.Add((Now-Changed)*1000);Row->SetNumberField(TEXT("fresh_analysis_ms"),FreshLatency.Last());
                Test->TestEqual(TEXT("one accepted click advances exactly one surface revision"),S.Revision(),BeforeRevision+1);
                int32 Index=0;for(auto* A:{S.MLX.Get(),S.Livox.Get(),S.CabRight.Get()}){const int64 Frame=A->ScanComponent->GetRuntimeStatus().FrameId;Test->TestTrue(TEXT("each selected sensor supplies a new post-edit frame"),Frame>BeforeFrames[Index++]);Row->SetNumberField(A->GetSensorId(),double(Frame));}
                Measurements.Add(MakeShared<FJsonValueObject>(Row));
            }
            Test->TestTrue(TEXT("click decrements remaining volume by the displayed removal within 1e-6m3"),FMath::Abs(BeforeVolume-S.Surface().Volume-S.LastRemovedVolume())<1.e-6);
            for(auto* A:{S.MLX.Get(),S.Livox.Get(),S.CabRight.Get()})
            {
                Test->TestTrue(TEXT("selected 3D mode has the expected point visibility"),MeasuredView==1?A->VisualizationComponent->GetVisiblePointCount()>0:A->VisualizationComponent->GetVisiblePointCount()==0);
                for(const auto& P:A->ScanComponent->GetLastPoints())if(P.bHit&&P.SemanticLabel==TEXT("HatchCargo"))
                {const FVector L=S.GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01;MaxHeightError=FMath::Max(MaxHeightError,FMath::Abs(L.Z-S.Surface().Height(L.X,L.Y)));}
            }
            ++Clicks;if(Clicks<50)
            {BeforeRevision=S.Revision();BeforeVolume=S.Surface().Volume;BeforeLast=S.Surface().Heights;if(!IdleControl)Test->TestTrue(TEXT("measured short pointer click"),Click(M,Clicks%25));Edits.Add(S.LastMeshApplyMs);Changed=Now;return false;}
            Stop(S);Changed=Now;Phase=7;return false;
        }
        if(Phase==7)
        {
            if(Now-Changed<.5)return false;DrainBoundary();CollectGarbage(RF_NoFlags);const uint64 RawAfter=FPlatformMemory::GetStats().UsedPhysical;FMemory::Trim();const auto Memory=FPlatformMemory::GetStats();
            MemoryDetails(TEXT("after"));
            Result->SetObjectField(TEXT("resources_after"),Resources(S));
            TFunction<void(const FJsonObject&,const FJsonObject&)> CheckResources;
            CheckResources=[&](const FJsonObject& Before,const FJsonObject& After)
            {for(const auto& P:Before.Values)if(P.Value->Type==EJson::Number)Test->TestEqual(TEXT("retained buffers/proxy count stable: ")+P.Key,After.GetNumberField(P.Key),P.Value->AsNumber());else if(P.Value->Type==EJson::Object)CheckResources(*P.Value->AsObject(),*After.GetObjectField(P.Key));};
            CheckResources(*Result->GetObjectField(TEXT("resources_before")),*Result->GetObjectField(TEXT("resources_after")));
            Result->SetNumberField(TEXT("view"),MeasuredView);Result->SetNumberField(TEXT("measurement_seconds"),Now-MeasureStart);
            Result->SetBoolField(TEXT("full_rhi_drain"),FParse::Param(FCommandLine::Get(),TEXT("HatchFullRhiDrain")));
            if(IdleControl)
            {
                Result->SetBoolField(TEXT("diagnostic_only"),true);Result->SetNumberField(TEXT("clicks"),0);Result->SetNumberField(TEXT("intervals"),Clicks);
                Result->SetNumberField(TEXT("raw_rss_before"),double(RawBefore));Result->SetNumberField(TEXT("raw_rss_after"),double(RawAfter));Result->SetNumberField(TEXT("rss_before"),double(Rss));Result->SetNumberField(TEXT("rss_after"),double(Memory.UsedPhysical));
                Result->SetNumberField(TEXT("commit_before"),double(Commit));Result->SetNumberField(TEXT("commit_after"),double(Memory.UsedVirtual));DigJson(TEXT("dig-idle-control.json"),Result);return true;
            }
            Test->TestTrue(TEXT("50 clicks raw RSS grows <=8MiB"),RawAfter<=RawBefore+8*1024*1024);
            Test->TestTrue(TEXT("50 clicks retained RSS grows <=8MiB"),Memory.UsedPhysical<=Rss+8*1024*1024);
            Test->TestTrue(TEXT("50 clicks committed memory grows <=8MiB"),Memory.UsedVirtual<=Commit+8*1024*1024);
            Test->TestTrue(TEXT("all click commits <=150ms"),Edits.Num()==50&&FMath::Max(Edits)<=150);
            Test->TestTrue(TEXT("all engine frames <=150ms"),Engine.Num()>0&&FMath::Max(Engine)<=150);
            Test->TestTrue(TEXT("all game-thread frames <=150ms"),Game.Num()>0&&FMath::Max(Game)<=150);
            Test->TestTrue(TEXT("all callback intervals <=150ms"),Wall.Num()>0&&FMath::Max(Wall)<=150);
            Test->TestEqual(TEXT("bounded undo buffers retain constant allocation"),S.DigBufferBytes(),Buffers);
            Test->TestTrue(TEXT("history/original/scratch allocation <=2MiB including maximal allowed mesh"),S.DigBufferBytes()<=2*1024*1024);
            Test->TestEqual(TEXT("in-place mesh buffers retain constant allocation"),S.MeshScratchBytes(),MeshBytes);
            Test->TestEqual(TEXT("history cap after 50"),S.UndoCount(),16);FHatchDigProbe::Undo(M);
            Test->TestTrue(TEXT("ring wrap undo restores exact 49th field"),S.Surface().Heights==BeforeLast);
            Test->TestTrue(TEXT("fresh collision/measurement height error <15cm after edits"),MaxHeightError<.15);
            Result->SetNumberField(TEXT("clicks"),Clicks);Result->SetNumberField(TEXT("raw_rss_before"),double(RawBefore));Result->SetNumberField(TEXT("rss_before"),double(Rss));Result->SetNumberField(TEXT("raw_rss_after"),double(RawAfter));Result->SetNumberField(TEXT("rss_after"),double(Memory.UsedPhysical));
            Result->SetNumberField(TEXT("commit_before"),double(Commit));Result->SetNumberField(TEXT("commit_after"),double(Memory.UsedVirtual));Result->SetNumberField(TEXT("dig_buffers_bytes"),double(Buffers));
            Result->SetNumberField(TEXT("edit_max_ms"),FMath::Max(Edits));Result->SetNumberField(TEXT("engine_max_ms"),FMath::Max(Engine));Result->SetNumberField(TEXT("game_max_ms"),FMath::Max(Game));Result->SetNumberField(TEXT("callback_max_ms"),FMath::Max(Wall));Result->SetNumberField(TEXT("height_max_error_m"),MaxHeightError);
            Result->SetStringField(TEXT("memory_method"),TEXT("17 complete click/acquisition/analysis warmup cycles + 35s live point/analysis stabilization; stop, drain, GC, allocator free-cache trim at both boundaries; raw RSS assertion retained; no OS trim/subtraction"));
            Result->SetArrayField(TEXT("fresh_measurements"),Measurements);
            Result->SetNumberField(TEXT("fresh_analysis_max_ms"),FreshLatency.Num()?FMath::Max(FreshLatency):0);
            S.Regenerate(true);Phase=8;return false;
        }
        if(Phase==8)
        {
            if(!M.GetAnalysis())return false;Test->TestTrue(TEXT("random regeneration clears all excavation history and removal counters"),S.UndoCount()==0&&S.LastRemovedVolume()==0&&S.TotalRemovedVolume()==0);
            Test->TestTrue(TEXT("random regeneration is exact fresh deterministic seed"),S.Surface().Heights==ShipHatch::Generate(S.Surface().Settings).Heights);
            Result->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());DigJson(TEXT("dig-runtime.json"),Result);return true;
        }
        if(Phase==10)
        {
            S.bCoal=Material!=0;S.Seed=19019;S.Regenerate(false);M.SelectView(0);M.SetSplitView(false);M.SetCameraPreset(0);Changed=Now;Phase=11;return false;
        }
        if(Phase==11)
        {
            if(Now-Changed<.5||!M.GetAnalysis())return false;
            if(State==0)Original=S.Surface().Heights;
            if(State==1)Test->TestTrue(TEXT("capture one click"),FHatchDigProbe::Click(M,FVector2D(2,0)));
            if(State==2){for(int32 I=0;I<7;++I)Test->TestTrue(TEXT("capture repeated click"),FHatchDigProbe::Click(M,FVector2D(2,0)));}
            if(State==3)FHatchDigProbe::Undo(M);
            if(State==4){FHatchDigProbe::Restore(M);Test->TestTrue(TEXT("capture original is bit-identical"),S.Surface().Heights==Original);}
            View=0;Changed=Now;Phase=12;return false;
        }
        if(Phase==12)
        {
            if(Now-Changed<2||!M.GetAnalysis())return false;FHatchDigProbe::CheckCards(*Test,M);ValidateHatchPresentation(Test,&M);
            const TCHAR* States[]={TEXT("before"),TEXT("one"),TEXT("many"),TEXT("undo"),TEXT("original")};const TCHAR* Views[]={TEXT("3d"),TEXT("points"),TEXT("heatmap")};
            Shot(*W,M,FString::Printf(TEXT("%s-%d-%s-%s.png"),Material?TEXT("coal"):TEXT("iron"),FMath::RoundToInt(M.GetFontScale()*100),States[State],Views[View]));
            if(++View<3){M.SelectView(View==1?1:3);Changed=Now;return false;}
            if(++State==5){State=0;if(++Material==2){M.SelectView(0);M.SetSplitView(true);FHatchDigProbe::SettingsForCapture(M);Changed=Now;Phase=13;return false;}Phase=10;return false;}
            M.SelectView(0);M.SetSplitView(false);M.SetCameraPreset(0);Changed=Now;Phase=11;return false;
        }
        if(Phase==13){if(Now-Changed<2||!M.GetAnalysis())return false;ValidateHatchPresentation(Test,&M);Shot(*W,M,FString::Printf(TEXT("settings-%d.png"),FMath::RoundToInt(M.GetFontScale()*100)));FHatchDigProbe::SensorCapture(M,0);Changed=Now;Phase=14;return false;}
        if(Phase==14||Phase==15)
        {if(Now-Changed<2||!M.GetAnalysis())return false;ValidateHatchPresentation(Test,&M);Shot(*W,M,FString::Printf(TEXT("sensor-beams-%d-%s.png"),FMath::RoundToInt(M.GetFontScale()*100),Phase==14?TEXT("3d"):TEXT("points")));
         if(Phase==14){FHatchDigProbe::SensorCapture(M,1);Changed=Now;Phase=15;return false;}S.SetScanDisplay(false);M.SelectView(0);M.SetSplitView(false);M.SetCameraPreset(1);Changed=Now;Phase=16;return false;}
        if(Phase==16)
        {if(Now-Changed<2)return false;FIntVector Size;Test->TestTrue(TEXT("opaque reference capture"),FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),OpaqueReference,Size));S.SetScanDisplay(true);Changed=Now;Phase=17;return false;}
        if(Phase==17)
        {
            if(Now-Changed<2)return false;TArray<FColor> WithRays;FIntVector Size;
            Test->TestTrue(TEXT("ray display reference capture"),FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),WithRays,Size));
            int32 Bright=0,Lost=0;
            if(WithRays.Num()==OpaqueReference.Num())for(int32 Y=FMath::RoundToInt(Size.Y*.48);Y<FMath::RoundToInt(Size.Y*.9);++Y)for(int32 X=Size.X/4;X<Size.X*3/4;++X)
            {const int32 I=Y*Size.X+X;const auto B=OpaqueReference[I],C=WithRays[I];if(int32(B.R)+B.G+B.B>=180){++Bright;Lost+=int32(C.R)+C.G+C.B<60;}}
            Test->TestTrue(TEXT("ordinary player ray rendering retains >=98 percent of bright opaque ship pixels"),Bright>500&&double(Lost)/Bright<=.02);
            auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("bright_reference_pixels"),Bright);R->SetNumberField(TEXT("lost_to_dark_pixels"),Lost);R->SetNumberField(TEXT("retained_fraction"),Bright?1.-double(Lost)/Bright:0);DigJson(TEXT("opaque-ray-regression.json"),R);S.SetScanDisplay(false);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchDigRuntime,"HATCH03.DigRuntimeRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchDigRuntime::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchDigCaptures,"HATCH03.DigCaptureRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchDigCaptures::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchDigIdle,"HATCH03.IdleMemoryControlRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchDigIdle::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
