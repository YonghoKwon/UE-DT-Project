#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SpringArmComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SViewport.h"
#include "RHI.h"
#include "RenderingThread.h"
#include <atomic>
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Player/DxPlayerControllerTest.h"
#include "ma0t10_dt/MA0T10/Player/DxPlayerTest.h"

namespace
{
class FPlacementInputCheck final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),StepAt=0,LastDiagnostic=0;int32 Step=0;
    TWeakObjectPtr<AVirtualSensorActorBase> Sensor;TWeakObjectPtr<APawn> Pawn;TWeakObjectPtr<AVirtualSensorTransformGizmoActor> Gizmo;
    FTransform SensorBefore,PawnBefore;TSharedPtr<SEditableTextBox> Foreign;bool PreviousLag=false;int32 UndoCount=0;
public:
    explicit FPlacementInputCheck(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>45){Test->AddError(TEXT("Placement input fixture timed out"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!FinalAcceptanceViewportReady(W)||FPlatformTime::Seconds()-Started<3)return false;
        auto* Workspace=W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Manager=Workspace->GetCoordinator();
        auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Settings));
        auto* PC=W->GetFirstPlayerController();auto* View=W->GetGameInstance()->GetGameViewportClient();auto V=View->GetGameViewportWidget();
        if(!Manager||!Settings||!PC||!V.IsValid())
        {if(FPlatformTime::Seconds()-LastDiagnostic>2){LastDiagnostic=FPlatformTime::Seconds();Test->AddInfo(FString::Printf(TEXT("placement prerequisites manager=%d settings=%d pc=%s pawn=%s viewport=%d"),Manager!=nullptr,Settings!=nullptr,*GetNameSafe(W->GetFirstPlayerController()?W->GetFirstPlayerController()->GetClass():nullptr),*GetNameSafe(W->GetFirstPlayerController()&&W->GetFirstPlayerController()->GetPawn()?W->GetFirstPlayerController()->GetPawn()->GetClass():nullptr),V.IsValid()));}return false;}
        auto& App=FSlateApplication::Get();
        if(Step==0)
        {
            Manager->SetViewMode(EVirtualSensorViewMode::Camera);Workspace->SynchronizeOwnedSelection();Workspace->SetPanelOpen(ESensorToolPanelRole::Settings,true);Workspace->SetPanelOpen(ESensorToolPanelRole::Monitor,false);
            Settings->SetSensorManipulationEnabled(true);Settings->SetSensorCoordinateSpace(EVirtualSensorCoordinateSpace::World);
            Sensor=Manager->GetSelectedSensorActor();Pawn=PC->GetPawn();Gizmo=Settings->GetTransformGizmoActor();
            if(!Sensor.IsValid()||!Pawn.IsValid()||!Gizmo.IsValid()){Test->AddError(TEXT("Expected project actor/pawn/gizmo"));return true;}
            SensorBefore=Sensor->GetActorTransform();PawnBefore=Pawn->GetActorTransform();
            Test->TestTrue(TEXT("owned panel opts in to responsive placement"),Gizmo->IsResponsivePlacement());
            App.SetKeyboardFocus(V,EFocusCause::SetDirectly);
            Test->TestTrue(TEXT("movement key consumed by sensor owner"),App.ProcessKeyDownEvent(FKeyEvent(EKeys::W,FModifierKeysState(),0,false,0,0)));
            StepAt=FPlatformTime::Seconds();Step=1;return false;
        }
        if(Step==1)
        {
            if(FPlatformTime::Seconds()-StepAt<.3)return false;
            App.ProcessKeyUpEvent(FKeyEvent(EKeys::W,FModifierKeysState(),0,false,0,0));
            Test->TestTrue(TEXT("continuous held key changes actual sensor pose"),FVector::Dist(SensorBefore.GetLocation(),Sensor->GetActorLocation())>10);
            Test->TestTrue(TEXT("sensor keyboard movement leaves observer still"),Pawn->GetActorTransform().Equals(PawnBefore));
            Test->TestTrue(TEXT("input to actual transform within two engine frames"),Gizmo->MeasuredInputEvents>0&&Gizmo->LastInputToTransformFrames<=2);
            Test->AddInfo(FString::Printf(TEXT("input_to_transform_ms=%.3f frames=%llu"),Gizmo->LastInputToTransformMs,Gizmo->LastInputToTransformFrames));
            Settings->SetPlacementInputMode(EVirtualSensorPlacementInputMode::Precision);SensorBefore=Sensor->GetActorTransform();
            App.ProcessKeyDownEvent(FKeyEvent(EKeys::W,FModifierKeysState(),0,false,0,0));
            for(int32 I=0;I<8;++I)App.ProcessKeyDownEvent(FKeyEvent(EKeys::W,FModifierKeysState(),0,true,0,0));
            App.ProcessKeyUpEvent(FKeyEvent(EKeys::W,FModifierKeysState(),0,false,0,0));
            Test->TestTrue(TEXT("precision ignores auto-repeat, one ten-cm move"),FMath::Abs(FVector::Dist(SensorBefore.GetLocation(),Sensor->GetActorLocation())-10)<.01);
            SensorBefore=Sensor->GetActorTransform();Foreign=SNew(SEditableTextBox);View->AddViewportWidgetContent(Foreign.ToSharedRef(),9000);App.SetKeyboardFocus(Foreign,EFocusCause::SetDirectly);
            Test->TestFalse(TEXT("foreign input is outside placement ownership"),Gizmo->IsInputFocusOwned());
            App.ProcessKeyDownEvent(FKeyEvent(EKeys::W,FModifierKeysState(),0,false,0,0));StepAt=FPlatformTime::Seconds();Step=2;return false;
        }
        if(Step==2)
        {
            if(FPlatformTime::Seconds()-StepAt<.2)return false;
            Test->TestTrue(TEXT("foreign typing never moves sensor"),Sensor->GetActorTransform().Equals(SensorBefore));
            App.ProcessKeyUpEvent(FKeyEvent(EKeys::W,FModifierKeysState(),0,false,0,0));View->RemoveViewportWidgetContent(Foreign.ToSharedRef());Foreign.Reset();App.SetKeyboardFocus(V,EFocusCause::SetDirectly);
            const auto Point=V->GetCachedGeometry().LocalToAbsolute(V->GetCachedGeometry().GetLocalSize()*FVector2D(.4,.8));
            const auto* Arm=Pawn->FindComponentByClass<USpringArmComponent>();PreviousLag=Arm?Arm->bEnableCameraLag:false;
            Test->TestTrue(TEXT("RMB gesture can start only in actual owned viewport"),Gizmo->BeginObserverNavigation(Point));
            // The production Enhanced Input adapter invokes these virtual Pawn methods.
            PawnBefore=Pawn->GetActorTransform();SensorBefore=Sensor->GetActorTransform();
            if(auto* Project=Cast<ADxPlayerTest>(Pawn.Get())){Project->Move(FVector2D(1,1));Project->MoveUpDown(.5f);Project->Look(FVector2D(1,1));}
            else {App.ProcessKeyDownEvent(FKeyEvent(EKeys::W,FModifierKeysState(),0,false,0,0));Gizmo->HandleObserverPointerDelta(FVector2D(10,10));}
            StepAt=FPlatformTime::Seconds();Step=3;return false;
        }
        if(Step==3)
        {
            if(FPlatformTime::Seconds()-StepAt<.2)return false;
            Test->TestTrue(TEXT("owned observer mode moves pawn"),FVector::Dist(PawnBefore.GetLocation(),Pawn->GetActorLocation())>10);
            Test->TestTrue(TEXT("observer navigation leaves sensor pose unchanged"),Sensor->GetActorTransform().Equals(SensorBefore));
            Gizmo->EndObserverNavigation();Test->TestFalse(TEXT("RMB release exits navigation"),Gizmo->IsObserverNavigationActive());
            const auto* Arm=Pawn->FindComponentByClass<USpringArmComponent>();Test->TestEqual(TEXT("navigation restores previous camera lag"),Arm?Arm->bEnableCameraLag:false,PreviousLag);
            App.SetKeyboardFocus(V,EFocusCause::SetDirectly);Settings->SetSensorManipulationEnabled(false);
            if(auto* ProjectPC=Cast<ADxPlayerControllerTest>(PC))Test->TestFalse(TEXT("exit releases controller input owner"),ProjectPC->GetPlacementInputOwner()!=nullptr);
            return true;
        }
        return false;
    }
};

struct FPlacementFrameProof {FGPUFenceRHIRef Fence;std::atomic<bool> Submitted{false};};
/** The fence is queued at OnEndFrame after viewport render work, not at pose application. */
class FPlacementRenderLatencyCheck final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),InputAt=0;int32 Kind=0,Completed=0;bool Waiting=false,PoseApplied=false,Queued=false;
    TWeakObjectPtr<AVirtualSensorTransformGizmoActor> Gizmo;FDelegateHandle PoseHandle,EndHandle;
    TSharedPtr<FPlacementFrameProof,ESPMode::ThreadSafe> Proof;TArray<double> Samples;
public:
    explicit FPlacementRenderLatencyCheck(FAutomationTestBase* T):Test(T){}
    ~FPlacementRenderLatencyCheck(){if(Gizmo.IsValid())Gizmo->OnTransformChanged.Remove(PoseHandle);FCoreDelegates::OnEndFrame.Remove(EndHandle);}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>20){Test->AddError(TEXT("Placement render latency fixture timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W)return false;
        auto* Workspace=W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Manager=Workspace->GetCoordinator();auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(Workspace->GetOwnedPanel(ESensorToolPanelRole::Settings));
        if(!Manager||!Settings)return false;auto V=W->GetGameInstance()->GetGameViewportClient()->GetGameViewportWidget();if(!V.IsValid())return false;auto& App=FSlateApplication::Get();
        if(!Gizmo.IsValid())
        {
            Manager->SetViewMode(Kind==0?EVirtualSensorViewMode::Camera:EVirtualSensorViewMode::Lidar);Workspace->SynchronizeOwnedSelection();Settings->SetSensorManipulationEnabled(true);Settings->SetPlacementInputMode(EVirtualSensorPlacementInputMode::Continuous);
            Gizmo=Settings->GetTransformGizmoActor();if(!Gizmo.IsValid())return false;
            PoseHandle=Gizmo->OnTransformChanged.AddLambda([this](const FTransform&){if(Waiting)PoseApplied=true;});
            EndHandle=FCoreDelegates::OnEndFrame.AddLambda([this](){if(!Waiting||!PoseApplied||Queued)return;Queued=true;auto P=Proof;
                ENQUEUE_RENDER_COMMAND(PlacementEndFrameProof)([P](FRHICommandListImmediate& Cmd){P->Fence=RHICreateGPUFence(TEXT("PlacementViewportEndFrame"));Cmd.WriteGPUFence(P->Fence);P->Submitted.store(true,std::memory_order_release);});});
        }
        if(Waiting)
        {
            if(!Proof->Submitted.load(std::memory_order_acquire)||!Proof->Fence.IsValid()||!Proof->Fence->Poll())return false;
            Samples.Add((FPlatformTime::Seconds()-InputAt)*1000);App.ProcessKeyUpEvent(FKeyEvent(Completed%2?EKeys::A:EKeys::D,FModifierKeysState(),0,false,0,0));Waiting=false;++Completed;
            if(Completed>=32)
            {
                auto Sorted=Samples;Sorted.Sort();const double P95=Sorted[FMath::CeilToInt(Sorted.Num()*.95)-1];
                Test->AddInfo(FString::Printf(TEXT("kind=%s input_to_viewport_end_frame_gpu_p95_ms=%.3f samples=%d; physical input/display latency excluded"),Kind==0?TEXT("Camera"):TEXT("LiDAR"),P95,Samples.Num()));
                Test->TestTrue(TEXT("input to changed viewport frame GPU completion p95 <=50ms"),P95<=50);
                Gizmo->OnTransformChanged.Remove(PoseHandle);FCoreDelegates::OnEndFrame.Remove(EndHandle);Settings->SetSensorManipulationEnabled(false);Gizmo.Reset();Samples.Reset();Completed=0;
                if(++Kind>=2)return true;
            }
            return false;
        }
        App.SetKeyboardFocus(V,EFocusCause::SetDirectly);Proof=MakeShared<FPlacementFrameProof,ESPMode::ThreadSafe>();PoseApplied=false;Queued=false;Waiting=true;InputAt=FPlatformTime::Seconds();
        App.ProcessKeyDownEvent(FKeyEvent(Completed%2?EKeys::A:EKeys::D,FModifierKeysState(),0,false,0,0));return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPlacementInputRhi,"MA0T10.Placement.InputOwnershipRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPlacementInputRhi::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_PLACEMENT_RHI"))!=TEXT("1")){AddInfo(TEXT("SKIP: MA0T10_PLACEMENT_RHI=1 for actual Slate/PIE input fixture"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPlacementInputCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FPlacementRenderLatencyCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
