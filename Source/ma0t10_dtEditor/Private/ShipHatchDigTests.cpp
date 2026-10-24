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
#include "ma0t10_dt/MA0T10/Sensor/LidarPlayerDisplayComponent.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/OutputDeviceRedirector.h"
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
#include "UObject/UObjectArray.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

void ValidateHatchPresentation(FAutomationTestBase*,UShipHatchMonitorWidget*);
struct FHatchDigProbe
{
    static AShipHatchSimulation& Simulation(UShipHatchMonitorWidget& M){return *M.Simulation.Get();}
    static FPointerEvent Pointer(FVector2D P,FVector2D Last,bool Held)
    {
        // FPointerEvent retains a reference to this set beyond the factory call.
        // A stack-local set leaves every returned down/up event dangling.
        static const TSet<FKey> Pressed{EKeys::LeftMouseButton},Released;
        return FPointerEvent(0,P,Last,Held?Pressed:Released,EKeys::LeftMouseButton,0,FModifierKeysState());
    }
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
        FVector2D P;const auto G=M.GetCachedGeometry();auto* PC=M.GetOwningPlayer();int W=0,H=0;PC->GetViewportSize(W,H);bool Found=false;
        // Select visible cargo before sending exactly one down/up pair. The
        // real GTSU and its labels can cover old fixture projections; both
        // production input bounds and actual hardware collision stay enabled.
        for(auto Candidate:{XY,XY+FVector2D(1.5,0),XY+FVector2D(0,1.5),FVector2D(7,-7),FVector2D(8,0),FVector2D(-8,0),FVector2D(7,5),FVector2D(-7,5)})
        {if(!Project(M,Candidate,P))continue;const auto Pixel=G.AbsoluteToLocal(P)*FVector2D(W,H)/G.GetLocalSize();FVector O,D,Hit;if(PC->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,O,D)&&M.Simulation->PickCargo(O,D,Hit)){Found=true;break;}}
        if(!Found)return false;const uint64 Revision=M.Simulation->Revision();
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
        if(S->bFieldLayout)
        {
            T.TestFalse(TEXT("real GTSU walkway blocks excavation through its roof"),S->PickCargo(Pose.TransformPosition(FVector(250,0,10000)),Pose.TransformVector(FVector(0,0,-1)),Hit));
            T.TestTrue(TEXT("clear field-layout cargo remains pickable beside the real boom"),S->PickCargo(Pose.TransformPosition(FVector(500,-500,10000)),Pose.TransformVector(FVector(0,0,-1)),Hit));
        }

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
            // Corrugations extend265mm into the hold: position alone cannot
            // identify cargo. Align the real display with its immutable source
            // and verify every sampled return by its acquired semantic label.
            A->VisualizationComponent->RefreshPointPreview();
            const auto Frame=A->ScanComponent->GetLastFrameSnapshot();
            T.TestTrue(TEXT("display validation retains an immutable source"),Frame&&Frame->Points);
            if(!Frame||!Frame->Points)return;
            const auto Policy=A->ScanComponent->GetPreviewPolicyState();
            TInlineComponentArray<ULineBatchComponent*> Batches(A);T.TestTrue(TEXT("owned colored point proxy exists"),Batches.Num()>0);
            for(auto* B:Batches)
            {
                T.TestEqual(TEXT("retained preview is never hidden while waiting for measurement"),PC->HiddenPrimitiveComponents.Contains(B),false);
                T.TestTrue(TEXT("edit keeps populated point buffer instead of empty/full churn"),B->BatchedPoints.Num()>0);
                double Error=0,StructureError=0;int32 Cargo=0,Structure=0,Rendered=0;
                for(int I=0;I<Frame->Points->Num()&&Rendered<B->BatchedPoints.Num();I+=FMath::Max(1,Policy.PointStride))
                {
                    const auto& Source=(*Frame->Points)[I];if(Policy.bHitOnly&&!Source.bHit)continue;
                    const auto& P=B->BatchedPoints[Rendered++];
                    T.TestTrue(TEXT("sampled acquired return has a visible display point"),P.PointSize>0);
                    const FVector L=S->GetActorTransform().InverseTransformPosition(P.Position)*.01;
                    if(Source.bHit&&Source.SemanticLabel==TEXT("HatchCargo"))
                    {++Cargo;Error=FMath::Max(Error,FMath::Abs(L.Z-S->Surface().Height(L.X,L.Y)));}
                    else {++Structure;StructureError=FMath::Max(StructureError,FVector::Distance(P.Position,Source.WorldLocation)*.01);}
                }
                T.TestTrue(FString::Printf(TEXT("retained and fresh point preview %s: cargo=%d error=%.5fm"),*A->GetSensorId(),Cargo,Error),Cargo>100&&Error<.15);
                T.TestTrue(TEXT("non-cargo returns remain measured geometry, never flattened onto cargo"),Structure>0&&StructureError<.0001);
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
    friend struct FHatchMemorySamplerProbe;
    FAutomationTestBase* Test;bool Captures,IdleControl;double Started=FPlatformTime::Seconds(),Changed=0,LastWall=0,MeasureStart=0;int32 MeasuredView=1;
    int32 Phase=0,Clicks=0,Material=0,State=0,View=0;uint64 BeforeRevision=0,LastEngine=0,BoundaryFrame=0,RawBefore=0,Rss=0,Commit=0,Buffers=0,MeshBytes=0;
    TArray<float> Original,AfterOne,BeforeLast;TArray<double> Engine,Game,Wall,Edits,FreshLatency;
    TArray<FColor> OpaqueReference;
    double OriginalVolume=0,BeforeVolume=0,MaxHeightError=0,WorkOpening=0,WorkFloor=0,WorkArea=0;uint64 WarmBuilds=0;
    double MatchedMemorySeconds=0;bool RhiTracking=false,MemoryPrimed=false;
    int64 BeforeFrames[3]={-1,-1};TArray<TSharedPtr<FJsonValue>> Measurements;
    TSharedRef<FJsonObject> Result=MakeShared<FJsonObject>();TUniquePtr<TGuardValue<bool>> Messages;
    // Fixed storage avoids allocating JSON/LLM maps during the measured curve.
    struct FMemoryPoint { double Seconds=0;int32 Cycle=0,Objects=0;uint64 Rss=0,Commit=0;int64 Tags[10]={}; };
    TArray<FMemoryPoint> MemoryCurve;
    // Opt-in diagnostic path only. No per-frame history, forced GC/trim, RHI
    // drain or LLM UpdateStatsPerFrame inside the long observation window.
    bool Trend=false,TrendLlm=false,TrendFailed=false,CostProbe=false,SamplerDisabled=false;double TrendSeconds=1800,TrendInterval=5,TrendLast=-1,TrendCost=0,TrendCostSum=0,TrendCostMax=0;
    bool Investigation=false,InvestigationProbe=false;FString InvestigationCut=TEXT("Reference");
    TArray<uint8> TraceCanary;
    static FORCENOINLINE void ResizeTraceCanary(TArray<uint8>& Bytes,int32 Size){Bytes.SetNumZeroed(Size);}
    int32 TrendSamples=0,TrendRestores=0;TUniquePtr<FArchive> TrendFile;
    TMap<FName,uint64> TrendDefaultTags,TrendPlatformTags;
    TWeakObjectPtr<UVirtualSensorStreamPublisherComponent> MemoryPublisher;
    TWeakObjectPtr<AShipHatchSimulation> MemorySimulation;TMap<FString,int32> CostBackendBefore;
    int32 MemoryPcdPort=0;FString MemoryCancelFile;double NextCancelPoll=0;
    TMap<FString,FVirtualSensorStreamStatus> PcdBefore;
    bool ConfigureMemoryPcd(AShipHatchSimulation& S)
    {
        MemorySimulation=&S;
        if(!MemoryPcdPort)return true;
        if(!Test->TestTrue(TEXT("memory PCD fixture requires unprivileged loopback port"),MemoryPcdPort>=1024&&MemoryPcdPort<=65535))return false;
        auto* Transport=NewObject<UVirtualSensorTransportComponent>(&S);S.AddInstanceComponent(Transport);Transport->RegisterComponent();
        Transport->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
        FVirtualSensorTransportProfile Profile;Profile.BrokerUrl=FString::Printf(TEXT("tcp://127.0.0.1:%d"),MemoryPcdPort);Profile.UserName.Reset();
        Transport->ConfigureTransportProfile(Profile);Transport->SetSessionCredentials(TEXT(""),TEXT(""));
        auto* Publisher=NewObject<UVirtualSensorStreamPublisherComponent>(&S);S.AddInstanceComponent(Publisher);
        Publisher->SetTransportComponent(Transport);Publisher->RegisterComponent();MemoryPublisher=Publisher;
        for(int32 I=0;I<3;++I)if(S.SensorEnabled(I))if(auto* A=S.SensorAt(I))
        {
            A->StopSensor();A->SetSharedOutputServices(Transport,nullptr,Publisher);
            FVirtualSensorStreamConfig Config;Config.SensorId=A->GetSensorId();Config.StreamKind=EVirtualSensorStreamKind::PointCloud;Config.bEnabled=true;Publisher->ConfigureStream(Config);
            A->StartSensor();
        }
        return true;
    }
    void PcdBaseline()
    {if(MemoryPublisher.IsValid())for(const auto& S:MemoryPublisher->GetStreamStatuses())if(S.StreamKind==EVirtualSensorStreamKind::PointCloud)PcdBefore.Add(S.SensorId,S);}
    TSharedRef<FJsonObject> PcdJson(bool Check=false)
    {
        auto R=MakeShared<FJsonObject>();
        if(MemoryPublisher.IsValid())for(const auto& S:MemoryPublisher->GetStreamStatuses())if(S.StreamKind==EVirtualSensorStreamKind::PointCloud)
        {
            auto Row=MakeShared<FJsonObject>();const auto B=PcdBefore.FindRef(S.SensorId);
            Row->SetNumberField(TEXT("input_frames"),S.InputFrameCount-B.InputFrameCount);Row->SetNumberField(TEXT("serialized_frames"),S.SerializedFrameCount-B.SerializedFrameCount);
            Row->SetNumberField(TEXT("submitted_frames"),S.SubmittedFrameCount-B.SubmittedFrameCount);Row->SetNumberField(TEXT("receipt_frames"),S.ReceiptReceivedCount-B.ReceiptReceivedCount);
            Row->SetNumberField(TEXT("consumer_received"),S.ConsumerReceivedCount-B.ConsumerReceivedCount);Row->SetNumberField(TEXT("submitted_bytes"),double(S.TotalSubmittedBytes-B.TotalSubmittedBytes));
            Row->SetNumberField(TEXT("encode_failures"),S.EncodeFailureCount-B.EncodeFailureCount);Row->SetNumberField(TEXT("delivery_failures"),S.DeliveryFailureCount-B.DeliveryFailureCount);
            Row->SetNumberField(TEXT("invalid"),S.ConsumerValidationFailureCount-B.ConsumerValidationFailureCount);Row->SetNumberField(TEXT("gaps"),S.ConsumerFrameGapCount-B.ConsumerFrameGapCount);
            Row->SetNumberField(TEXT("duplicates"),S.ConsumerDuplicateCount-B.ConsumerDuplicateCount);Row->SetNumberField(TEXT("overloads"),S.OverloadCount-B.OverloadCount);
            Row->SetNumberField(TEXT("rate_control_skipped"),S.RateControlSkippedFrameCount-B.RateControlSkippedFrameCount);Row->SetBoolField(TEXT("processing"),S.bProcessing);Row->SetNumberField(TEXT("budget_mib_s"),S.PcdBudgetMiBPerSecond);
            if(Check&&!(Investigation&&InvestigationCut==TEXT("LidarOff")))
            {
                Test->TestTrue(TEXT("memory workload actually submits and receives PCD"),S.SubmittedFrameCount>B.SubmittedFrameCount&&S.ConsumerReceivedCount>B.ConsumerReceivedCount);
                Test->TestEqual(TEXT("memory PCD invalid payload count"),S.ConsumerValidationFailureCount-B.ConsumerValidationFailureCount,int64(0));
                Test->TestEqual(TEXT("memory PCD encode failures"),S.EncodeFailureCount-B.EncodeFailureCount,int64(0));
            }
            R->SetObjectField(S.SensorId,Row);
        }
        return R;
    }
    static TSharedRef<FJsonObject> CurveJson(const FMemoryPoint& P)
    {
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("cycle"),P.Cycle);R->SetNumberField(TEXT("seconds"),P.Seconds);
        R->SetNumberField(TEXT("rss_bytes"),double(P.Rss));R->SetNumberField(TEXT("commit_bytes"),double(P.Commit));R->SetNumberField(TEXT("uobject_count"),P.Objects);
        const TCHAR* Names[]={TEXT("Physics"),TEXT("ChaosTrimesh"),TEXT("UObject"),TEXT("UI"),TEXT("RHIMisc"),TEXT("Textures"),TEXT("FMallocUnused"),TEXT("Hatch/CargoCook"),TEXT("Hatch/RayTree"),TEXT("Hatch/CargoUpdate")};
        auto Tags=MakeShared<FJsonObject>();for(int I=0;I<10;++I)Tags->SetNumberField(Names[I],double(P.Tags[I]));R->SetObjectField(TEXT("llm_default"),Tags);return R;
    }
    bool TrendWrite(const TSharedRef<FJsonObject>& Row)
    {
        if(!TrendFile)return false;FString Text;
        FJsonSerializer::Serialize(Row,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));Text+=TEXT("\n");
        FTCHARToUTF8 Utf8(*Text);TrendFile->Serialize((void*)Utf8.Get(),Utf8.Length());TrendFile->Flush();return !TrendFile->IsError();
    }
    TSharedRef<FJsonObject> TrendTop(TMap<FName,uint64>& Values)
    {
        // Keep at most16 tagged amounts without sorting/copying the full map.
        TArray<TPair<FName,uint64>,TInlineAllocator<16>> Top;
        for(const auto& Pair:Values)
        {
            int32 Index=0;while(Index<Top.Num()&&(Top[Index].Value>Pair.Value||(Top[Index].Value==Pair.Value&&Top[Index].Key.LexicalLess(Pair.Key))))++Index;
            if(Index>=16)continue;if(Top.Num()==16)Top.Pop(false);Top.Insert(TPair<FName,uint64>(Pair.Key,Pair.Value),Index);
        }
        auto R=MakeShared<FJsonObject>();for(const auto& Pair:Top)R->SetNumberField(Pair.Key.ToString(),double(Pair.Value));return R;
    }
    bool TrendBegin()
    {
#if ENABLE_LOW_LEVEL_MEM_TRACKER
        TrendLlm=FLowLevelMemTracker::IsEnabled();
#endif
        if(CostProbe){Engine.Reserve(8192);Game.Reserve(8192);Wall.Reserve(8192);if(MemorySimulation.IsValid())for(int32 I=0;I<3;++I)if(auto* A=MemorySimulation->SensorAt(I))CostBackendBefore.Add(A->GetSensorId(),int32(A->ScanComponent->GetActiveAcquisitionBackend()));}
        IFileManager::Get().MakeDirectory(*DigRoot(),true);
        const FString Path=DigRoot()/TEXT("memory-trend.jsonl");
        if(IFileManager::Get().FileExists(*Path)){Test->AddError(TEXT("Memory trend requires a fresh report directory"));return false;}
        TrendFile.Reset(IFileManager::Get().CreateFileWriter(*Path,FILEWRITE_AllowRead));
        auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("type"),TEXT("metadata"));Meta->SetNumberField(TEXT("schema_version"),1);
        Meta->SetStringField(TEXT("row_contract"),TEXT("memory_curve"));Meta->SetBoolField(TEXT("diagnostic_only"),true);Meta->SetNumberField(TEXT("requested_seconds"),TrendSeconds);Meta->SetNumberField(TEXT("interval_seconds"),TrendInterval);
        Meta->SetBoolField(TEXT("cost_probe"),CostProbe);Meta->SetBoolField(TEXT("sampler_enabled"),!SamplerDisabled);Meta->SetBoolField(TEXT("llm_enabled"),TrendLlm);
        Meta->SetBoolField(TEXT("investigation"),Investigation);Meta->SetBoolField(TEXT("investigation_probe"),InvestigationProbe);Meta->SetStringField(TEXT("cut"),InvestigationCut);
        Meta->SetBoolField(TEXT("pcd_loopback"),MemoryPcdPort!=0);Meta->SetBoolField(TEXT("pcd_default_compressed"),MemoryPublisher.IsValid()?MemoryPublisher->GetPcdSendOptions(TEXT("")).bCompressed:false);
        Meta->SetStringField(TEXT("workload"),IdleControl?TEXT("idle-after-shared-warmup"):TEXT("dig-every-2s-restore-every-50"));
        Meta->SetStringField(TEXT("started_utc"),FDateTime::UtcNow().ToIso8601());FString Source;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Source);Meta->SetStringField(TEXT("source_commit"),Source);
        Meta->SetStringField(TEXT("uobject_method"),TEXT("GUObjectArray occupied slots, includes pending GC; not a live-object traversal"));
        Meta->SetStringField(TEXT("cost_method"),TEXT("previous_sample_total_gt_ms includes collection, JSON, write and flush; final sample cost in summary; header/footer excluded"));
        return TrendWrite(Meta);
    }
    bool TrendFinish(bool Complete)
    {
        auto R=MakeShared<FJsonObject>();R->SetObjectField(TEXT("pcd"),PcdJson(Complete));R->SetStringField(TEXT("type"),TEXT("summary"));R->SetBoolField(TEXT("complete"),Complete&&!Test->HasAnyErrors());R->SetBoolField(TEXT("diagnostic_only"),true);
        R->SetNumberField(TEXT("sample_count"),TrendSamples);R->SetNumberField(TEXT("measurement_seconds"),FPlatformTime::Seconds()-MeasureStart);
        R->SetNumberField(TEXT("clicks"),Clicks);R->SetNumberField(TEXT("restores"),TrendRestores);R->SetBoolField(TEXT("llm_enabled"),TrendLlm);
        R->SetNumberField(TEXT("last_sample_total_gt_ms"),TrendCost);R->SetNumberField(TEXT("sampler_total_gt_ms"),TrendCostSum);R->SetNumberField(TEXT("sampler_max_gt_ms"),TrendCostMax);
        R->SetBoolField(TEXT("cost_probe"),CostProbe);R->SetBoolField(TEXT("sampler_enabled"),!SamplerDisabled);
        if(CostProbe)
        {
            auto Before=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>();
            for(const auto& Pair:CostBackendBefore)Before->SetNumberField(Pair.Key,Pair.Value);
            if(MemorySimulation.IsValid())for(int32 I=0;I<3;++I)if(auto* A=MemorySimulation->SensorAt(I)){const int32 Backend=int32(A->ScanComponent->GetActiveAcquisitionBackend());After->SetNumberField(A->GetSensorId(),Backend);Test->TestTrue(TEXT("cost probe uses actual HWRT before and after"),Backend==int32(EVirtualLidarAcquisitionBackend::HardwareRayTracing)&&CostBackendBefore.FindRef(A->GetSensorId())==Backend);}
            Test->TestTrue(TEXT("cost probe keeps all three sensors"),Before->Values.Num()==3&&After->Values.Num()==3);R->SetObjectField(TEXT("backend_before"),Before);R->SetObjectField(TEXT("backend_after"),After);
            const double Seconds=R->GetNumberField(TEXT("measurement_seconds"));double GameSum=0;for(double V:Game)GameSum+=V;
            TArray<double> Sorted=Engine;Sorted.Sort();const double P95=Sorted.Num()?Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num()*.95)-1,0,Sorted.Num()-1)]:0;
            R->SetNumberField(TEXT("engine_frames"),Engine.Num());R->SetNumberField(TEXT("average_fps"),Seconds>0?Engine.Num()/Seconds:0);
            R->SetNumberField(TEXT("engine_frame_p95_ms"),P95);R->SetNumberField(TEXT("game_thread_mean_ms"),Game.Num()?GameSum/Game.Num():0);
            R->SetNumberField(TEXT("engine_max_ms"),Engine.Num()?FMath::Max(Engine):0);R->SetNumberField(TEXT("game_max_ms"),Game.Num()?FMath::Max(Game):0);R->SetNumberField(TEXT("callback_max_ms"),Wall.Num()?FMath::Max(Wall):0);
            Test->TestTrue(TEXT("cost probe records actual engine frames"),Engine.Num()>0&&Game.Num()==Engine.Num());
            Test->TestTrue(TEXT("cost probe engine/game/callback frames stay <=150ms"),Engine.Num()&&FMath::Max(Engine)<=150&&FMath::Max(Game)<=150&&FMath::Max(Wall)<=150);
            R->SetBoolField(TEXT("complete"),Complete&&!Test->HasAnyErrors());DigJson(TEXT("memory-cost.json"),R);
        }
        bool Saved=TrendWrite(R);if(TrendFile){Saved=TrendFile->Close()&&Saved;TrendFile.Reset();}return Saved;
    }
    void CurvePoint(int32 Cycle,const AShipHatchSimulation* Simulation=nullptr)
    {
        if(CostProbe&&SamplerDisabled)return;
        if(!Trend&&!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryAudit")))return;
        const double CostStart=FPlatformTime::Seconds();
        if(Trend&&TrendLast>=0&&CostStart-TrendLast<TrendInterval)return;
        if(Trend){TrendLlm=false;TrendDefaultTags.Reset();TrendPlatformTags.Reset();}
        FMemoryPoint P;P.Seconds=(Trend?CostStart:FPlatformTime::Seconds())-MeasureStart;P.Cycle=Cycle;
        const auto M=FPlatformMemory::GetStats();P.Rss=M.UsedPhysical;P.Commit=M.UsedVirtual;P.Objects=GUObjectArray.GetObjectArrayNumMinusAvailable();
#if ENABLE_LOW_LEVEL_MEM_TRACKER
        if(FLowLevelMemTracker::IsEnabled())
        {
            static const FName Names[]={TEXT("Physics"),TEXT("ChaosTrimesh"),TEXT("UObject"),TEXT("UI"),TEXT("RHIMisc"),TEXT("Textures"),TEXT("FMallocUnused"),TEXT("Hatch/CargoCook"),TEXT("Hatch/RayTree"),TEXT("Hatch/CargoUpdate")};
            auto& L=FLowLevelMemTracker::Get();if(!Trend)L.UpdateStatsPerFrame();
            for(int I=0;I<10;++I)P.Tags[I]=L.GetTagAmountForTracker(ELLMTracker::Default,Names[I],ELLMTagSet::None);
            if(Trend)
            {
                TrendLlm=true;
                L.GetTrackedTagsNamesWithAmount(TrendDefaultTags,ELLMTracker::Default,ELLMTagSet::None);
                L.GetTrackedTagsNamesWithAmount(TrendPlatformTags,ELLMTracker::Platform,ELLMTagSet::None);
            }
        }
#endif
        if(Trend)
        {
            auto R=CurveJson(P);R->SetStringField(TEXT("type"),TEXT("memory_curve"));R->SetBoolField(TEXT("llm_enabled"),TrendLlm);
            auto Watched=R->GetObjectField(TEXT("llm_default"));
            // Always retain these attribution counters even when outside top16.
            for(const TCHAR* Name:{TEXT("Untracked"),TEXT("TrackedTotal"),TEXT("Total"),TEXT("FMalloc")})
                if(const uint64* Amount=TrendDefaultTags.Find(FName(Name)))Watched->SetNumberField(Name,double(*Amount));
            R->SetObjectField(TEXT("llm_default_top"),TrendTop(TrendDefaultTags));R->SetObjectField(TEXT("llm_platform_top"),TrendTop(TrendPlatformTags));
            R->SetObjectField(TEXT("pcd"),PcdJson());
            if(Simulation)
            {
                auto Sensors=MakeShared<FJsonObject>();
                for(int32 I=0;I<3;++I)if(auto* A=Simulation->SensorAt(I))
                {
                    auto Sensor=MakeShared<FJsonObject>();Sensor->SetNumberField(TEXT("frame_id"),double(A->ScanComponent->GetRuntimeStatus().FrameId));
                    Sensor->SetNumberField(TEXT("active_acquisition_backend"),int32(A->ScanComponent->GetActiveAcquisitionBackend()));
                    Sensor->SetBoolField(TEXT("running"),A->IsSensorRunning());Sensor->SetNumberField(TEXT("preview_point_budget"),A->ScanComponent->GetPreviewPolicyState().MaxPoints);
                    Sensor->SetNumberField(TEXT("visible_preview_points"),A->VisualizationComponent->GetVisiblePointCount());
                    Sensors->SetObjectField(A->GetSensorId(),Sensor);
                }
                R->SetObjectField(TEXT("sensors"),Sensors);
            }
            R->SetNumberField(TEXT("previous_sample_total_gt_ms"),TrendCost);R->SetNumberField(TEXT("restores"),TrendRestores);
            if(!TrendWrite(R)){TrendFailed=true;Test->AddError(TEXT("Memory trend sample write failed"));}
            // Include destruction of transient JSON/maps in the measured cost.
        }
        else MemoryCurve.Add(P);
        if(Trend){TrendLast=CostStart;TrendCost=(FPlatformTime::Seconds()-CostStart)*1000;TrendCostSum+=TrendCost;TrendCostMax=FMath::Max(TrendCostMax,TrendCost);++TrendSamples;}
    }
    void SaveCurve()
    {
        TArray<TSharedPtr<FJsonValue>> Rows;Rows.Reserve(MemoryCurve.Num());
        for(const auto& P:MemoryCurve)Rows.Add(MakeShared<FJsonValueObject>(CurveJson(P)));
        Result->SetArrayField(TEXT("memory_curve"),Rows);
    }
    void MemoryDetails(const TCHAR* Boundary,const AShipHatchSimulation& Simulation)
    {
        auto Pools=MakeShared<FJsonObject>();
        for(const auto* Actor:{Simulation.MLX.Get(),Simulation.Livox.Get(),Simulation.CabRight.Get()})
            if(Actor&&Actor->ScanComponent)Pools->SetNumberField(Actor->GetSensorId(),double(Actor->ScanComponent->GetScenePointPoolFallbacksForTesting()));
        Result->SetObjectField(FString(Boundary)+TEXT("_point_pool_fallbacks"),Pools);
        if(FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryDeepReport")))
        {
            UE_LOG(LogTemp,Display,TEXT("[FOLLOWUP26-ALLOC] %s"),Boundary);
            FPlatformMemory::DumpPlatformAndAllocatorStats(*GLog);
        }
#if ENABLE_LOW_LEVEL_MEM_TRACKER
        if(!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryAudit"))||!FLowLevelMemTracker::IsEnabled())return;
        auto& LLM=FLowLevelMemTracker::Get();LLM.UpdateStatsPerFrame();Result->SetBoolField(TEXT("diagnostic_only"),true);
        for(auto Tracker:{ELLMTracker::Default,ELLMTracker::Platform})
        {
            TMap<FName,uint64> Tags;LLM.GetTrackedTagsNamesWithAmount(Tags,Tracker,ELLMTagSet::None);auto Values=MakeShared<FJsonObject>();
            for(const auto& Pair:Tags)Values->SetNumberField(Pair.Key.ToString(),double(Pair.Value));
            Result->SetObjectField(FString(Boundary)+(Tracker==ELLMTracker::Default?TEXT("_llm_default"):TEXT("_llm_platform")),Values);
        }
#if RHI_ENABLE_RESOURCE_INFO
        if(RhiTracking)
        {
            TArray<TSharedPtr<FRHIResourceStats>> Stats;
            ENQUEUE_RENDER_COMMAND(HatchMemoryResourceCounts)([&Stats](FRHICommandListImmediate&){RHIGetTrackedResourceStats(Stats);});FlushRenderingCommands();
            auto Counts=MakeShared<FJsonObject>(),Bytes=MakeShared<FJsonObject>();int32 HatchCount=0;uint64 HatchBytes=0;
            for(const auto& P:Stats)if(P){const FString K=P->Type;double Count=0,Size=0;Counts->TryGetNumberField(K,Count);Bytes->TryGetNumberField(K,Size);Counts->SetNumberField(K,Count+1);Bytes->SetNumberField(K,Size+P->SizeInBytes);if(P->Name.ToString().Contains(TEXT("Hatch"))){++HatchCount;HatchBytes+=P->SizeInBytes;}}
            auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("global_tracking_available"),Stats.Num()>0);R->SetNumberField(TEXT("total_count"),Stats.Num());R->SetNumberField(TEXT("hatch_named_count"),HatchCount);R->SetNumberField(TEXT("hatch_named_bytes"),double(HatchBytes));R->SetObjectField(TEXT("counts_by_type"),Counts);R->SetObjectField(TEXT("bytes_by_type"),Bytes);Result->SetObjectField(FString(Boundary)+TEXT("_rhi_resources"),R);
        }
#endif
#endif
    }
    TSharedRef<FJsonObject> Resources(AShipHatchSimulation& S)
    {
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("mesh"),double(S.MeshScratchBytes()));R->SetNumberField(TEXT("generation"),double(S.GenerationBufferBytes()));R->SetNumberField(TEXT("dig"),double(S.DigBufferBytes()));
        for(auto* A:{S.MLX.Get(),S.Livox.Get(),S.CabRight.Get()})if(A)
        {
            auto Sensor=MakeShared<FJsonObject>();const auto Scan=A->ScanComponent->GetResourceSnapshot(),Preview=A->VisualizationComponent->GetResourceSnapshot();
            for(const auto& P:Scan.OwnedCpuBytes)Sensor->SetNumberField(TEXT("scan/")+P.Key,double(P.Value));for(const auto& P:Scan.SharedCpuBytes)Sensor->SetNumberField(TEXT("shared/")+P.Key,double(P.Value));for(const auto& P:Preview.OwnedCpuBytes)Sensor->SetNumberField(TEXT("view/")+P.Key,double(P.Value));
            TInlineComponentArray<ULidarPlayerDisplayComponent*> Batches(A);for(auto* B:Batches)if(B->bFixedPointSubmission){Sensor->SetNumberField(TEXT("view/fixed_proxy_creations"),double(B->GetFixedPointProxyCreations()));Sensor->SetNumberField(TEXT("view/fixed_point_uploads"),double(B->GetFixedPointUploads()));if(FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryAudit"))){const auto RhiValues=B->GetFixedPointRhiResources();Sensor->SetNumberField(TEXT("view/rhi_vertex_index_count"),RhiValues.X);Sensor->SetNumberField(TEXT("view/rhi_srv_count"),RhiValues.Y);Sensor->SetNumberField(TEXT("view/rhi_buffer_bytes"),RhiValues.Z);}}
            R->SetObjectField(A->GetSensorId(),Sensor);
        }
        return R;
    }
    void Stop(AShipHatchSimulation& S){for(int32 I=0;I<3;++I)if(auto* A=S.SensorAt(I))A->StopSensor();}
    void Start(AShipHatchSimulation& S){for(int32 I=0;I<3;++I)if(S.SensorEnabled(I))if(auto* A=S.SensorAt(I))A->StartSensor();}
    void DrainBoundary()
    {
        FlushRenderingCommands();if(!FParse::Param(FCommandLine::Get(),TEXT("HatchFullRhiDrain")))return;
        ENQUEUE_RENDER_COMMAND(HatchMemoryBoundary)([](FRHICommandListImmediate& Cmd){Cmd.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);Cmd.BlockUntilGPUIdle();Cmd.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);});
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
        // Real GTSU box girders/walkways now occupy the old +/-2.5m columns.
        // Keep all50 accepted production cycles on physically clear cargo;
        // the blocked walkway is asserted independently above.
        if(S.bFieldLayout)Target=FVector2D(4+(I%5),((I/5)%5-2)*3.);
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
    FHatchDig(FAutomationTestBase* T,bool C,bool Idle=false,bool LongTrend=false,bool Cost=false,bool Investigate=false,bool Probe=false):Test(T),Captures(C),IdleControl(Idle),Trend(LongTrend),CostProbe(Cost),Investigation(Investigate),InvestigationProbe(Probe){FParse::Value(FCommandLine::Get(),TEXT("HatchDigView="),MeasuredView);MeasuredView=MeasuredView==0?0:1;FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryWindowSeconds="),MatchedMemorySeconds);
        FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryPcdLoopbackPort="),MemoryPcdPort);FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryCancelFile="),MemoryCancelFile);
        if(Trend){FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryTrendSeconds="),TrendSeconds);FParse::Value(FCommandLine::Get(),TEXT("HatchMemorySampleSeconds="),TrendInterval);}
        if(CostProbe){TrendSeconds=60;SamplerDisabled=FParse::Param(FCommandLine::Get(),TEXT("HatchMemorySamplerDisabled"));}
        if(Investigation){FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryInvestigationSeconds="),TrendSeconds);FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryCut="),InvestigationCut);if(InvestigationProbe)TrendSeconds=30;}
#if RHI_ENABLE_RESOURCE_INFO
        RhiTracking=FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryAudit"));
#endif
    }
    bool Update() override
    {
        if(Trend&&(!FMath::IsFinite(TrendSeconds)||!FMath::IsFinite(TrendInterval)||(CostProbe?TrendSeconds!=60:Investigation?(InvestigationProbe?TrendSeconds!=30:(TrendSeconds<600||TrendSeconds>1200)):(TrendSeconds<1800||TrendSeconds>3600))||TrendInterval<5||TrendInterval>30))
        {Test->AddError(TEXT("Memory trend requires 1800..3600 seconds and 5..30 second samples"));return true;}
        const double Now=FPlatformTime::Seconds();if(Now-Started>(Trend?TrendSeconds+600:600)){if(TrendFile)TrendFinish(false);Test->AddError(TEXT("HATCH-03 timeout"));return true;}
        if(!MemoryCancelFile.IsEmpty()&&Now>=NextCancelPoll){NextCancelPoll=Now+1;if(IFileManager::Get().FileExists(*MemoryCancelFile)){TRACE_BOOKMARK(TEXT("MEMORY26B_CANCEL"));Test->AddError(TEXT("memory run cancelled by environment monitor"));if(TrendFile)TrendFinish(false);return true;}}
        if(TrendFailed){TrendFinish(false);return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||!FinalAcceptanceViewportReady(W))return false;AShipHatchMonitorHost* H=nullptr;
        for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;
        if(!H||!H->Monitor||!H->Simulation||H->Simulation->IsGenerating()||!H->Simulation->MLX||!H->Simulation->Livox)return false;
        auto& S=*H->Simulation;auto& M=*H->Monitor;if(!Messages)Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
        if(Phase==0)
        {
            if(!M.GetAnalysis())return false;if(!ConfigureMemoryPcd(S))return true;FHatchDigProbe::Controls(*Test,M);int32 Font=100;FParse::Value(FCommandLine::Get(),TEXT("HatchFontPercent="),Font);M.SetFontPercent(Font);M.SelectView(0);M.SetSplitView(false);M.SetCameraPreset(0);
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
            if(Trend)
            {
                int32 Warmup=35;FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryWarmup="),Warmup);
                if(Now-Changed<Warmup||M.AnalysisBuilds<WarmBuilds+5)return false;
                if(Investigation)
                {
                    if(InvestigationCut==TEXT("LidarOff"))Stop(S);
                    else if(InvestigationCut==TEXT("PreviewOff"))for(int32 I=0;I<3;++I)if(auto* A=S.SensorAt(I))A->VisualizationComponent->SetWorldPointCloudEnabled(false);
                    else if(InvestigationCut!=TEXT("Reference")&&InvestigationCut!=TEXT("PcdOff")&&InvestigationCut!=TEXT("Cpu")){Test->AddError(TEXT("unknown memory isolation cut"));return true;}
                    Changed=Now;Phase=41;return false;
                }
                if(!TrendBegin()){Test->AddError(TEXT("Memory trend output could not be created"));return true;}
                PcdBaseline();MeasureStart=FPlatformTime::Seconds();LastWall=MeasureStart;LastEngine=GFrameCounter;CurvePoint(0,&S);Changed=MeasureStart;Phase=60;return false;
            }
            if(!MemoryPrimed){MemoryCurve.Reserve(64);MeasureStart=Now;CurvePoint(-1);MemoryCurve.Reset();MemoryDetails(TEXT("priming"),S);if(FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryDeepReport")))GEngine->Exec(W,TEXT("memreport -full"));MemoryPrimed=true;}
            int32 Warmup=35;FParse::Value(FCommandLine::Get(),TEXT("HatchMemoryWarmup="),Warmup);
            if(Now-Changed<Warmup||M.AnalysisBuilds<WarmBuilds+5)return false;Stop(S);Changed=Now;Phase=5;return false;
        }
        if(Phase==41)
        {
            if(Now-Changed<2)return false;
            if(InvestigationCut==TEXT("PreviewOff"))for(int32 I=0;I<3;++I)if(auto* A=S.SensorAt(I))Test->TestEqual(TEXT("isolation disables point preview"),A->VisualizationComponent->GetVisiblePointCount(),0);
            if(!TrendBegin()){Test->AddError(TEXT("Memory investigation output failed"));return true;}
            PcdBaseline();MeasureStart=FPlatformTime::Seconds();LastWall=MeasureStart;LastEngine=GFrameCounter;TRACE_BOOKMARK(TEXT("MEMORY26B_BEGIN"));CurvePoint(0,&S);Changed=MeasureStart;Phase=60;return false;
        }
        if(Phase==5)
        {
            if(Now-Changed<.5)return false;
            // GC schedules render releases and scene removals. Sampling before
            // their next scene update included startup resources in the baseline.
            CollectGarbage(RF_NoFlags);DrainBoundary();BoundaryFrame=GFrameCounter;Changed=Now;Phase=50;return false;
        }
        if(Phase==50)
        {
            if(Now-Changed<.2||GFrameCounter<BoundaryFrame+3)return false;
            DrainBoundary();RawBefore=FPlatformMemory::GetStats().UsedPhysical;FMemory::Trim();
            Rss=FPlatformMemory::GetStats().UsedPhysical;Commit=FPlatformMemory::GetStats().UsedVirtual;Buffers=S.DigBufferBytes();MeshBytes=S.MeshScratchBytes();
            MemoryDetails(TEXT("before"),S);
            PcdBaseline();
            Result->SetObjectField(TEXT("resources_before"),Resources(S));
            MeasureStart=Now;
            CurvePoint(0);
            BeforeRevision=S.Revision();BeforeLast=S.Surface().Heights;BeforeVolume=S.Surface().Volume;LastWall=Now;LastEngine=GFrameCounter;
            Start(S);if(!IdleControl)Test->TestTrue(TEXT("first measured production click"),Click(M,0));Edits.Add(S.LastMeshApplyMs);Changed=Now;Phase=6;return false;
        }
        if(Phase==60)
        {
            if(InvestigationProbe&&Now-MeasureStart>=2){const int32 Wanted=Now-MeasureStart>=10?4*1024*1024:1024*1024;if(TraceCanary.Num()!=Wanted)ResizeTraceCanary(TraceCanary,Wanted);}
            if(CostProbe&&GFrameCounter!=LastEngine){Engine.Add(W->GetDeltaSeconds()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Wall.Add((Now-LastWall)*1000);LastWall=Now;LastEngine=GFrameCounter;}
            CurvePoint(Clicks,&S);
            if(CostProbe?Now-MeasureStart>=60:TrendLast-MeasureStart>=TrendSeconds)
            {
                if(Investigation){TRACE_BOOKMARK(TEXT("MEMORY26B_END"));}
                for(auto* A:{S.MLX.Get(),S.Livox.Get(),S.CabRight.Get()})if(A)Test->TestEqual(TEXT("memory run expected sensor running state"),A->IsSensorRunning(),!(Investigation&&InvestigationCut==TEXT("LidarOff")));
                if(Investigation&&InvestigationCut==TEXT("PreviewOff"))for(int32 I=0;I<3;++I)if(auto* A=S.SensorAt(I))Test->TestEqual(TEXT("preview isolation stays disabled"),A->VisualizationComponent->GetVisiblePointCount(),0);
                if(!IdleControl)Test->TestTrue(TEXT("long memory trend completed repeated edit/restore workload"),Clicks>=50&&TrendRestores>0);
                Test->TestTrue(TEXT("long memory trend summary saved"),TrendFinish(!TrendFailed&&!Test->HasAnyErrors()));return true;
            }
            if(!IdleControl&&Now-Changed>=2&&M.GetAnalysis())
            {
                // Restore the original cargo every50 accepted edits so the same
                // bounded history/workload can run for an hour without depletion.
                if(Clicks>0&&Clicks%50==0&&TrendRestores<Clicks/50){FHatchDigProbe::Restore(M);++TrendRestores;}
                else if(Click(M,Clicks%25))++Clicks;
                else {Test->AddError(TEXT("long memory trend production click rejected"));TrendFailed=true;}
                Changed=Now;
            }
            return false;
        }
        if(Phase==6)
        {
            if(GFrameCounter!=LastEngine){Engine.Add(W->GetDeltaSeconds()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Wall.Add((Now-LastWall)*1000);LastWall=Now;LastEngine=GFrameCounter;}
            const bool WaitMatched=MatchedMemorySeconds>0&&Now-MeasureStart<MatchedMemorySeconds*(Clicks+1)/50.;
            if(WaitMatched||(MatchedMemorySeconds<=0&&IdleControl&&Now-Changed<.6)||!M.GetAnalysis())return false;FHatchDigProbe::CheckCards(*Test,M);
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
            ++Clicks;CurvePoint(Clicks);if(Clicks<50)
            {BeforeRevision=S.Revision();BeforeVolume=S.Surface().Volume;BeforeLast=S.Surface().Heights;if(!IdleControl)Test->TestTrue(TEXT("measured short pointer click"),Click(M,Clicks%25));Edits.Add(S.LastMeshApplyMs);Changed=Now;return false;}
            Stop(S);Changed=Now;Phase=7;return false;
        }
        if(Phase==7)
        {
            if(Now-Changed<.5)return false;CollectGarbage(RF_NoFlags);DrainBoundary();BoundaryFrame=GFrameCounter;Changed=Now;Phase=70;return false;
        }
        if(Phase==70)
        {
            if(Now-Changed<.2||GFrameCounter<BoundaryFrame+3)return false;
            DrainBoundary();const uint64 RawAfter=FPlatformMemory::GetStats().UsedPhysical;FMemory::Trim();const auto Memory=FPlatformMemory::GetStats();
            MemoryDetails(TEXT("after"),S);
            SaveCurve();
            Result->SetObjectField(TEXT("pcd"),PcdJson(true));
            if(FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryDeepReport")))GEngine->Exec(W,TEXT("memreport -full"));
            Result->SetObjectField(TEXT("resources_after"),Resources(S));
            TFunction<void(const FJsonObject&,const FJsonObject&)> CheckResources;
            CheckResources=[&](const FJsonObject& Before,const FJsonObject& After)
            {for(const auto& P:Before.Values)if(P.Value->Type==EJson::Number){if(P.Key!=TEXT("view/fixed_point_uploads"))Test->TestEqual(TEXT("retained buffers/proxy count stable: ")+P.Key,After.GetNumberField(P.Key),P.Value->AsNumber());}else if(P.Value->Type==EJson::Object)CheckResources(*P.Value->AsObject(),*After.GetObjectField(P.Key));};
            CheckResources(*Result->GetObjectField(TEXT("resources_before")),*Result->GetObjectField(TEXT("resources_after")));
            Result->SetNumberField(TEXT("view"),MeasuredView);Result->SetNumberField(TEXT("measurement_seconds"),Now-MeasureStart);
            Result->SetNumberField(TEXT("matched_window_seconds"),MatchedMemorySeconds);
            Result->SetNumberField(TEXT("viewport_point_budget"),S.SensorAt(0)->ScanComponent->GetPreviewPolicyState().MaxPoints);
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
            if(MeasuredView==1)for(int I=0;I<3;++I)if(auto* A=S.SensorAt(I))
            {
                const auto Before=Result->GetObjectField(TEXT("resources_before"))->GetObjectField(A->GetSensorId()),After=Result->GetObjectField(TEXT("resources_after"))->GetObjectField(A->GetSensorId());
                Test->TestTrue(TEXT("all50 edits update fixed GPU buffers without recreating point proxies"),Before->GetNumberField(TEXT("view/fixed_proxy_creations"))>0&&Before->GetNumberField(TEXT("view/fixed_proxy_creations"))==After->GetNumberField(TEXT("view/fixed_proxy_creations"))&&After->GetNumberField(TEXT("view/fixed_point_uploads"))>Before->GetNumberField(TEXT("view/fixed_point_uploads")));
            }
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
            Result->SetStringField(TEXT("memory_method"),TEXT("17 complete click/acquisition/analysis warmup cycles + 35s live point/analysis stabilization; both stopped boundaries: GC, render/RHI drain, >=3 scene frames and200ms, final drain, raw RSS, allocator free-cache trim; raw8MiB and separate difference8MiB unchanged; no OS trim/subtraction"));
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
namespace
{
struct FHatchMemorySamplerProbe
{
    static bool Check(FAutomationTestBase& T)
    {
        FHatchDig Run(&T,false,false,true);
        TMap<FName,uint64> Tags;for(int32 I=0;I<31;++I)Tags.Add(FName(*FString::Printf(TEXT("SamplerTag%02d"),I)),I);
        const auto Top=Run.TrendTop(Tags);T.TestEqual(TEXT("LLM top list stays bounded at16"),Top->Values.Num(),16);
        T.TestEqual(TEXT("largest tag retained"),Top->GetNumberField(TEXT("SamplerTag30")),30.);
        T.TestFalse(TEXT("small tag omitted rather than recorded as zero"),Top->HasField(TEXT("SamplerTag00")));
        if(!T.TestTrue(TEXT("open incremental sampler report"),Run.TrendBegin()))return false;
        Run.MeasureStart=FPlatformTime::Seconds();Run.CurvePoint(0);Run.CurvePoint(1);
        T.TestEqual(TEXT("immediate repeated callback cannot oversample"),Run.TrendSamples,1);
        T.TestEqual(TEXT("long sampler never accumulates curve rows in memory"),Run.MemoryCurve.Num(),0);
        T.TestTrue(TEXT("short contract test saves incomplete footer"),Run.TrendFinish(false));
        FString Text;T.TestTrue(TEXT("incremental evidence readable after close"),FFileHelper::LoadFileToString(Text,*(DigRoot()/TEXT("memory-trend.jsonl"))));
        TArray<FString> Lines;Text.ParseIntoArrayLines(Lines);if(!T.TestEqual(TEXT("header plus one row plus footer"),Lines.Num(),3))return false;
        TSharedPtr<FJsonObject> Row,Footer;
        T.TestTrue(TEXT("sample is independently valid JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Lines[1]),Row));
        T.TestTrue(TEXT("footer is independently valid JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Lines[2]),Footer));
        if(Row&&Footer)
        {
            T.TestTrue(TEXT("sample includes process RSS and occupied UObject slots"),Row->GetNumberField(TEXT("rss_bytes"))>0&&Row->GetNumberField(TEXT("uobject_count"))>0);
            T.TestEqual(TEXT("footer count matches emitted sample count"),Footer->GetNumberField(TEXT("sample_count")),1.);
            T.TestFalse(TEXT("short test cannot claim completed long observation"),Footer->GetBoolField(TEXT("complete")));
        }
        return !T.HasAnyErrors();
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchMemorySamplerContract,"HATCH26.MemorySamplerContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchMemorySamplerContract::RunTest(const FString&){return FHatchMemorySamplerProbe::Check(*this);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchDigRuntime,"HATCH03.DigRuntimeRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchDigRuntime::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchDigCaptures,"HATCH03.DigCaptureRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchDigCaptures::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchDigIdle,"HATCH03.IdleMemoryControlRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchDigIdle::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchMemoryTrendDig,"HATCH26.MemoryTrendDigRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchMemoryTrendDig::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryTrend"))){AddInfo(TEXT("[OPT-IN SKIP] requires -HatchMemoryTrend; 30..60 minute diagnostic"));return true;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchMemoryTrendIdle,"HATCH26.MemoryTrendIdleRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchMemoryTrendIdle::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryTrend"))){AddInfo(TEXT("[OPT-IN SKIP] requires -HatchMemoryTrend; 30..60 minute diagnostic"));return true;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchMemorySamplerOverhead,"HATCH26.MemorySamplerOverheadRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchMemorySamplerOverhead::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryCostProbe"))){AddInfo(TEXT("[OPT-IN SKIP] requires -HatchMemoryCostProbe; separate60s overhead diagnostic"));return true;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false,true,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchMemoryInvestigation,"HATCH26.MemoryInvestigationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchMemoryInvestigation::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryInvestigate"))){AddInfo(TEXT("[OPT-IN SKIP] requires -HatchMemoryInvestigate;10..20minute attribution"));return true;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false,true,true,false,true,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchMemoryTraceProbe,"HATCH26.MemoryTraceProbeRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchMemoryTraceProbe::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("HatchMemoryTraceProbe"))){AddInfo(TEXT("[OPT-IN SKIP] requires -HatchMemoryTraceProbe;30s instrumentation check only"));return true;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDig(this,false,true,true,false,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
