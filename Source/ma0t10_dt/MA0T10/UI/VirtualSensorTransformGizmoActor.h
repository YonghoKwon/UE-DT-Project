#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorTransformGizmoActor.generated.h"

class UBoxComponent;
class USceneComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorGizmoTransformChanged, const FTransform&);

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

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetGizmoVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetProjectionDebugEnabled(bool bEnabled);

    void SetStepSizes(float InTranslationStepCm, float InRotationStepDegrees);

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

    static FVector ResolveAxisVector(const FTransform& TargetTransform, EVirtualSensorCoordinateSpace Space, EAxis::Type Axis);
    static FTransform ApplyKeyboardDelta(const FTransform& TargetTransform, EVirtualSensorCoordinateSpace Space, const FVector& TranslationAxes, const FRotator& RotationDegrees);
    static constexpr int32 ProjectionDebugRayBudget = 64;

    FOnVirtualSensorGizmoTransformChanged OnTransformChanged;
    FOnVirtualSensorGizmoTransformChanged OnTransformCommitted;

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
};
