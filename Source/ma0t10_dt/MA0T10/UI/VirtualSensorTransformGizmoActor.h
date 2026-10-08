#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Input/Events.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorTransformGizmoActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UWidget;
class IInputProcessor;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorGizmoTransformChanged, const FTransform&);
DECLARE_MULTICAST_DELEGATE(FOnVirtualSensorGizmoManipulationExitRequested);

UCLASS(BlueprintType)
class MA0T10_DT_API AVirtualSensorTransformGizmoActor : public AActor
{
    GENERATED_BODY()

public:
    AVirtualSensorTransformGizmoActor();

    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void BindTarget(AActor* InTargetActor);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetGizmoMode(EVirtualSensorGizmoMode InMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetCoordinateSpace(EVirtualSensorCoordinateSpace InSpace);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetManipulationEnabled(bool bEnabled);

    /** Escape/end request, distinct from committing a single drag. */
    void RequestManipulationExit();
    void SetInputOwner(UWidget* InOwner);
    bool IsInputFocusOwned(bool bAllowOwnedTextInput = false) const;
    bool HandleOwnedPointerDown(const FVector2D& ScreenPosition);
    bool HandleOwnedPointerMove(const FVector2D& ScreenPosition);
    bool HandleOwnedPointerUp(const FVector2D& ScreenPosition);
    void CancelOwnedPointerDrag();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetGizmoVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetProjectionDebugEnabled(bool bEnabled);

    void SetStepSizes(float InTranslationStepCm, float InRotationStepDegrees);
    void ConfigureResponsivePlacement(bool bEnabled,EVirtualSensorPlacementInputMode Mode,float MoveSpeedCmPerSecond=125,float RotateSpeedDegreesPerSecond=62.5f);
    bool IsResponsivePlacement() const {return bResponsivePlacement;}
    bool IsObserverNavigationActive() const {return bObserverNavigation;}
    bool HandlePlacementKeyDown(const FKeyEvent& Event);
    bool HandlePlacementKeyUp(const FKeyEvent& Event);
    bool BeginObserverNavigation(const FVector2D& ScreenPosition);
    void EndObserverNavigation();
    bool IsFallbackObserverNavigation() const {return bFallbackNavigation;}
    bool HandleObserverPointerDelta(const FVector2D& Delta);
    void FinishPlacementGesture();
    void BlockHeldPlacementKeys();
    FSimpleMulticastDelegate OnTransformGestureStarted;
    FSimpleMulticastDelegate OnUndoRequested,OnRedoRequested;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    bool IsManipulationEnabled() const { return bManipulationEnabled; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    bool IsGizmoVisible() const { return bGizmoVisible; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    bool IsProjectionDebugEnabled() const { return bProjectionDebugEnabled; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    EVirtualSensorGizmoMode GetGizmoMode() const { return GizmoMode; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    EVirtualSensorCoordinateSpace GetCoordinateSpace() const { return CoordinateSpace; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    AActor* GetBoundTargetActor() const { return TargetActor; }

    static FVector ResolveAxisVector(const FTransform& TargetTransform, EVirtualSensorCoordinateSpace Space, EAxis::Type Axis);
    static FTransform ApplyKeyboardDelta(const FTransform& TargetTransform, EVirtualSensorCoordinateSpace Space, const FVector& TranslationAxes, const FRotator& RotationDegrees);
    static constexpr int32 ProjectionDebugRayBudget = 64;

    FOnVirtualSensorGizmoTransformChanged OnTransformChanged;
    FOnVirtualSensorGizmoTransformChanged OnTransformCommitted;
    FOnVirtualSensorGizmoManipulationExitRequested OnManipulationExitRequested;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    enum class EHandleKind : uint8
    {
        None,
        MoveX,
        MoveY,
        MoveZ,
        RotateX,
        RotateY,
        RotateZ
    };

    void UpdateGizmoTransformAndScale();
    void UpdateCollisionHandles(float Scale);
    void SetHandleCollisionEnabled(bool bEnabled);
    void HandleMouseInput();
    void HandleKeyboardInput(float DeltaSeconds);
    bool ApplyTargetTransform(const FTransform& Transform);
    void ApplyResponsiveKeyboardDelta(float DeltaSeconds,bool bOneStep);
    static bool IsPlacementKey(const FKey& Key);
    void BeginMouseDrag(EHandleKind HandleKind);
    void UpdateMouseDrag();
    void EndMouseDrag();
    bool DeprojectMouseRay(FVector& OutOrigin, FVector& OutDirection) const;
    EHandleKind ResolveHandleKind(const UBoxComponent* Component) const;
    FVector ResolveHandleAxis(EHandleKind HandleKind, const FTransform& Transform) const;
    float ClosestAxisParameter(const FVector& RayOrigin, const FVector& RayDirection, const FVector& AxisOrigin, const FVector& AxisDirection) const;
    bool IntersectMousePlane(const FVector& PlaneOrigin, const FVector& PlaneNormal, FVector& OutPoint) const;
    void DrawGizmo() const;
    void DrawProjectionDebug() const;
    void DrawCameraProjectionDebug() const;
    void DrawLidarProjectionDebug() const;
    bool IsEditableTextFocused() const;
    void RefreshInputRouter();
    void ReleasePointerOwnership();
    bool UpdateOwnedPointerPosition(const FVector2D& ScreenPosition);
    bool bRouterPointerDragging=false;
    bool bHasPointerPosition=false;
    FVector2D PointerViewportPosition=FVector2D::ZeroVector;
    TWeakObjectPtr<UWidget> InputOwner;
    TSharedPtr<IInputProcessor> InputRouter;
    TWeakObjectPtr<APlayerController> PointerController;
    bool PreviousPossibleClick=true;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(Transient)
    TObjectPtr<AActor> TargetActor;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBoxComponent>> MoveHandles;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBoxComponent>> RotationHandles;

    EVirtualSensorGizmoMode GizmoMode = EVirtualSensorGizmoMode::Translate;
    EVirtualSensorCoordinateSpace CoordinateSpace = EVirtualSensorCoordinateSpace::Local;
    bool bManipulationEnabled = false;
    bool bGizmoVisible = true;
    bool bProjectionDebugEnabled = false;
    bool bMouseDragging = false;
    bool bKeyboardManipulating = false;
    EHandleKind ActiveHandle = EHandleKind::None;
    FTransform DragStartTransform = FTransform::Identity;
    FVector DragOrigin = FVector::ZeroVector;
    FVector DragAxis = FVector::ForwardVector;
    FVector DragStartPlaneVector = FVector::ForwardVector;
    float DragStartAxisParameter = 0.0f;
    float TranslationStepCm = 10.0f;
    float RotationStepDegrees = 5.0f;
    float CurrentVisualScale = 1.0f;
    double NextKeyboardStepTime = 0.0;
    bool bResponsivePlacement=false,bObserverNavigation=false;
    bool bFallbackNavigation=false,FallbackCursor=true,FallbackClicks=true,FallbackHover=true,FallbackLag=false,FallbackRotationLag=false;
    TWeakObjectPtr<APlayerController> FallbackController;
    TWeakObjectPtr<APawn> FallbackPawn;
    TSet<FKey> ObserverKeys;
    EVirtualSensorPlacementInputMode PlacementInputMode=EVirtualSensorPlacementInputMode::Continuous;
    float TranslationSpeed=125,RotationSpeed=62.5f;
    TSet<FKey> PlacementKeys,BlockedPlacementKeys;
    double PendingInputSeconds=-1;
    uint64 PendingInputFrame=0;
public:
    double LastInputToTransformMs=0;
    uint64 LastInputToTransformFrames=0;
    int32 MeasuredInputEvents=0;
};
