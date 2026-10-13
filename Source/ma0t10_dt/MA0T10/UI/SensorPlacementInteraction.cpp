#include "SensorPlacementInteraction.h"
#include "VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"

UWorld* USensorPlacementInteraction::GetWorld() const {return GetOuter()?GetOuter()->GetWorld():nullptr;}
void USensorPlacementInteraction::BindCoordinator(AVirtualSensorCoordinator* In)
{if(Coordinator.Get()!=In){Finish(false);Coordinator=In;LastSelected.Reset();}SynchronizeSelection();}
AVirtualSensorActorBase* USensorPlacementInteraction::Selected() const {return Coordinator.IsValid()?Coordinator->GetSelectedSensorActor():nullptr;}
bool USensorPlacementInteraction::IsManipulating() const {return Target.IsValid()&&Target->IsInteractiveManipulationActive()&&IsValid(Gizmo)&&Gizmo->IsManipulationEnabled();}
AVirtualSensorTransformGizmoActor* USensorPlacementInteraction::GetGizmo(UWidget* Owner)
{
    if(!IsValid(Gizmo)&&GetWorld())
    {
        Gizmo=GetWorld()->SpawnActor<AVirtualSensorTransformGizmoActor>();
        Gizmo->OnTransformChanged.AddUObject(this,&ThisClass::PoseChanged);
        Gizmo->OnTransformCommitted.AddUObject(this,&ThisClass::PoseCommitted);
        Gizmo->OnTransformGestureStarted.AddUObject(this,&ThisClass::GestureStarted);
        Gizmo->OnManipulationExitRequested.AddUObject(this,&ThisClass::ExitRequested);
        Gizmo->OnUndoRequested.AddWeakLambda(this,[this](){FString R;Undo(R);});
        Gizmo->OnRedoRequested.AddWeakLambda(this,[this](){FString R;Redo(R);});
        Gizmo->SetGizmoMode(Mode);Gizmo->SetCoordinateSpace(Space);Gizmo->BindTarget(Selected());
        Gizmo->ConfigureResponsivePlacement(true,InputMode,MoveSpeed,RotateSpeed);
    }
    if(Gizmo&&Owner&&!IsManipulating())Gizmo->SetInputOwner(Owner);
    return Gizmo;
}
bool USensorPlacementInteraction::SetManipulation(bool Enabled,UWidget* Owner,FString& Reason)
{
    if(!Enabled){Finish();Reason=TEXT("조작 종료 · 원래 측정 설정으로 복원했습니다.");return true;}
    SynchronizeSelection();auto* A=Selected();
    if(IsManipulating()&&Target.Get()==A)return true;
    Finish(false);FVirtualSensorInteractionRequest Request;Request.CameraPreviewInterval=1.f/15;Request.LidarPreviewInterval=.1f;
    if(!A||!A->CanEditSensorPlacement(Reason)||!A->BeginInteractiveManipulation(Request))
    {if(Reason.IsEmpty())Reason=TEXT("조작할 장비를 선택하세요.");return false;}
    Target=A;A->OnDataCollectionStarting.AddUObject(this,&ThisClass::ExitRequested);
    A->OnEndPlay.AddUniqueDynamic(this,&ThisClass::TargetEndPlay);
    auto* G=GetGizmo(Owner);if(!G){Finish(false);Reason=TEXT("조작 표시를 만들지 못했습니다. 다시 시도하세요.");return false;}
    G->SetInputOwner(Owner);G->BindTarget(A);G->SetManipulationEnabled(true);
    // UXR-13 (user request 2026-10-14): keep the view the user is looking at. The
    // preview card no longer jumps to the manipulated device's own view, and keyboard
    // focus goes to the 3D view so W/A/S/D move the device without an extra click.
    RestoreMonitor=false;
    if(FSlateApplication::IsInitialized())if(auto* World=GetWorld())if(auto* View=World->GetGameViewport())if(const auto Widget=View->GetGameViewportWidget())
        FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::SetDirectly);
    Reason=TEXT("조작 중 · Esc로 현재 위치를 확정하세요.");OnChanged.Broadcast();return true;
}
void USensorPlacementInteraction::Finish(bool RestoreView)
{
    if(bFinishing)return;TGuardValue<bool> Guard(bFinishing,true);
    auto* A=Target.Get();if(A&&A!=Selected())RestoreView=false;
    if(IsValid(Gizmo))Gizmo->SetManipulationEnabled(false);
    if(A){A->OnDataCollectionStarting.RemoveAll(this);A->OnEndPlay.RemoveDynamic(this,&ThisClass::TargetEndPlay);A->EndInteractiveManipulation();if(GetWorld())GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->BeginPlacementPreviewRefresh(A);}
    Target.Reset();
    if(RestoreView&&RestoreMonitor&&Coordinator.IsValid()&&uint8(Coordinator->GetViewMode())==FollowView)Coordinator->SetViewMode(EVirtualSensorViewMode(PreviousView));
    RestoreMonitor=false;OnChanged.Broadcast();
}
void USensorPlacementInteraction::SynchronizeSelection()
{
    auto* A=Selected();
    if(LastSelected.Get()!=A){Finish(false);LastSelected=A;if(Gizmo)Gizmo->BindTarget(A);OnChanged.Broadcast();}
    else if(Target.IsValid()&&(!IsValid(Gizmo)||!Gizmo->IsManipulationEnabled()))Finish();
    if(RestoreMonitor&&Coordinator.IsValid()&&uint8(Coordinator->GetViewMode())!=FollowView)RestoreMonitor=false;
}
void USensorPlacementInteraction::Shutdown(){Finish(false);if(Gizmo){Gizmo->Destroy();Gizmo=nullptr;}Coordinator.Reset();OnChanged.Clear();}
void USensorPlacementInteraction::SetMode(EVirtualSensorGizmoMode V){Mode=V;if(Gizmo)Gizmo->SetGizmoMode(V);OnChanged.Broadcast();}
void USensorPlacementInteraction::SetSpace(EVirtualSensorCoordinateSpace V){Space=V;if(Gizmo)Gizmo->SetCoordinateSpace(V);OnChanged.Broadcast();}
void USensorPlacementInteraction::SetInputMode(EVirtualSensorPlacementInputMode V,float M,float R){InputMode=V;MoveSpeed=M;RotateSpeed=R;if(Gizmo)Gizmo->ConfigureResponsivePlacement(true,V,M,R);OnChanged.Broadcast();}
void USensorPlacementInteraction::SetLegacyOptions(float Step,float Degrees,bool Visible,bool Debug){if(Gizmo){Gizmo->SetStepSizes(Step,Degrees);Gizmo->SetGizmoVisible(Visible);Gizmo->SetProjectionDebugEnabled(Debug);}}
void USensorPlacementInteraction::ExitRequested(){Finish();}
void USensorPlacementInteraction::TargetEndPlay(AActor*,EEndPlayReason::Type)
{if(auto* A=Target.Get()){A->OnDataCollectionStarting.RemoveAll(this);A->OnEndPlay.RemoveDynamic(this,&ThisClass::TargetEndPlay);}Target.Reset();RestoreMonitor=false;if(Gizmo)Gizmo->SetManipulationEnabled(false);OnChanged.Broadcast();}
void USensorPlacementInteraction::GestureStarted(){if(Target.IsValid())GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->BeginGesture(Target.Get());}
void USensorPlacementInteraction::PoseChanged(const FTransform&){OnChanged.Broadcast();}
void USensorPlacementInteraction::PoseCommitted(const FTransform&){if(Target.IsValid()){auto* P=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>();P->CommitGesture(Target.Get());P->BeginPlacementPreviewRefresh(Target.Get());}OnChanged.Broadcast();}
void USensorPlacementInteraction::PreparePoseAction(){if(Gizmo){Gizmo->FinishPlacementGesture();Gizmo->BlockHeldPlacementKeys();}}
bool USensorPlacementInteraction::ApplyPose(const FTransform& Pose,FString& Reason)
{
    PreparePoseAction();auto* A=Selected();if(!A){Reason=TEXT("장비를 선택하세요.");return false;}const auto Before=A->GetActorTransform();
    if(!A->ApplyEditableTransform(Pose,Reason))return false;auto* P=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>();P->RecordPoseChange(A,Before,A->GetActorTransform());P->BeginPlacementPreviewRefresh(A);OnChanged.Broadcast();return true;
}
bool USensorPlacementInteraction::ApplyBookmark(const FString& Id,FString& R){PreparePoseAction();const bool Ok=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->ApplySensorPoseBookmark(Selected(),Id,R);if(Ok)OnChanged.Broadcast();return Ok;}
bool USensorPlacementInteraction::Undo(FString& R){PreparePoseAction();const bool Ok=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->UndoSensorPlacement(Selected(),R);if(Ok){R=TEXT("되돌리기 완료 · 장비 위치를 확인하세요.");OnChanged.Broadcast();}return Ok;}
bool USensorPlacementInteraction::Redo(FString& R){PreparePoseAction();const bool Ok=GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->RedoSensorPlacement(Selected(),R);if(Ok){R=TEXT("다시 적용 완료 · 장비 위치를 확인하세요.");OnChanged.Broadcast();}return Ok;}
FString USensorPlacementInteraction::SavePose(const FString& Name,const FString& Existing){return GetWorld()->GetSubsystem<UVirtualSensorPlacementSubsystem>()->RequestSaveSensorPose(Selected(),Name,Existing);}
