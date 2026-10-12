#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorControlTypes.h"
#include "SensorPlacementInteraction.generated.h"
class AVirtualSensorActorBase;
class AVirtualSensorCoordinator;
class AVirtualSensorTransformGizmoActor;
class UWidget;

/** UI action adapter shared by owned placement/settings. Pose storage remains in the existing placement subsystem. */
UCLASS()
class MA0T10_DT_API USensorPlacementInteraction : public UObject
{
    GENERATED_BODY()
public:
    virtual UWorld* GetWorld() const override;
    void BindCoordinator(AVirtualSensorCoordinator* In);
    AVirtualSensorActorBase* Selected() const;
    AVirtualSensorTransformGizmoActor* GetGizmo(UWidget* InputOwner=nullptr);
    AVirtualSensorTransformGizmoActor* GetExistingGizmo() const {return Gizmo;}
    bool SetManipulation(bool Enabled,UWidget* InputOwner,FString& Reason);
    void Finish(bool RestoreView=true);
    void SynchronizeSelection();
    void Shutdown();
    bool IsManipulating() const;
    AVirtualSensorActorBase* GetManipulationTarget() const {return Target.Get();}
    void SetMode(EVirtualSensorGizmoMode Value);
    void SetSpace(EVirtualSensorCoordinateSpace Value);
    void SetInputMode(EVirtualSensorPlacementInputMode Value,float MoveSpeed=125,float RotateSpeed=62.5f);
    void SetLegacyOptions(float Step,float Degrees,bool Visible,bool Debug);
    EVirtualSensorGizmoMode GetMode() const {return Mode;}
    EVirtualSensorCoordinateSpace GetSpace() const {return Space;}
    EVirtualSensorPlacementInputMode GetInputMode() const {return InputMode;}
    bool ApplyPose(const FTransform& Pose,FString& Reason);
    bool ApplyBookmark(const FString& Id,FString& Reason);
    bool Undo(FString& Reason);
    bool Redo(FString& Reason);
    FString SavePose(const FString& Name,const FString& Existing=FString());
    FSimpleMulticastDelegate OnChanged;
private:
    TWeakObjectPtr<AVirtualSensorCoordinator> Coordinator;
    TWeakObjectPtr<AVirtualSensorActorBase> LastSelected,Target;
    UPROPERTY(Transient) TObjectPtr<AVirtualSensorTransformGizmoActor> Gizmo;
    EVirtualSensorGizmoMode Mode=EVirtualSensorGizmoMode::Translate;
    EVirtualSensorCoordinateSpace Space=EVirtualSensorCoordinateSpace::Local;
    EVirtualSensorPlacementInputMode InputMode=EVirtualSensorPlacementInputMode::Continuous;
    float MoveSpeed=125,RotateSpeed=62.5f;
    bool RestoreMonitor=false,bFinishing=false;
    uint8 PreviousView=0,FollowView=0;
    void PreparePoseAction();
    void PoseChanged(const FTransform& Pose);
    void PoseCommitted(const FTransform& Pose);
    void GestureStarted();
    void ExitRequested();
    UFUNCTION() void TargetEndPlay(AActor* Actor,EEndPlayReason::Type Reason);
};
