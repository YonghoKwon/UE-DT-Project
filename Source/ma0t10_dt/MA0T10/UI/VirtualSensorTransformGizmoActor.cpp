#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"

namespace
{
constexpr int32 RingSegments = 16;
constexpr float BaseAxisLength = 115.0f;
constexpr float BaseRingRadius = 82.0f;

FName MoveHandleTag(int32 AxisIndex)
{
    static const FName Tags[] = { TEXT("SensorGizmo_MoveX"), TEXT("SensorGizmo_MoveY"), TEXT("SensorGizmo_MoveZ") };
    return Tags[FMath::Clamp(AxisIndex, 0, 2)];
}

FName RotateHandleTag(int32 AxisIndex)
{
    static const FName Tags[] = { TEXT("SensorGizmo_RotateX"), TEXT("SensorGizmo_RotateY"), TEXT("SensorGizmo_RotateZ") };
    return Tags[FMath::Clamp(AxisIndex, 0, 2)];
}
}

AVirtualSensorTransformGizmoActor::AVirtualSensorTransformGizmoActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    SetActorEnableCollision(true);
    Tags.Add(TEXT("KeepInPointCloudOnly"));

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        UBoxComponent* Handle = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("MoveHandle_%d"), AxisIndex));
        Handle->SetupAttachment(SceneRoot);
        Handle->ComponentTags.Add(MoveHandleTag(AxisIndex));
        Handle->SetGenerateOverlapEvents(false);
        Handle->SetCollisionResponseToAllChannels(ECR_Ignore);
        Handle->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        MoveHandles.Add(Handle);
    }

    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        for (int32 SegmentIndex = 0; SegmentIndex < RingSegments; ++SegmentIndex)
        {
            UBoxComponent* Handle = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("RotateHandle_%d_%d"), AxisIndex, SegmentIndex));
            Handle->SetupAttachment(SceneRoot);
            Handle->ComponentTags.Add(RotateHandleTag(AxisIndex));
            Handle->SetGenerateOverlapEvents(false);
            Handle->SetCollisionResponseToAllChannels(ECR_Ignore);
            Handle->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
            RotationHandles.Add(Handle);
        }
    }
    UpdateCollisionHandles(1.0f);
    SetHandleCollisionEnabled(false);
}

void AVirtualSensorTransformGizmoActor::BindTarget(AActor* InTargetActor)
{
    if (TargetActor == InTargetActor)
    {
        return;
    }
    if (bMouseDragging)
    {
        EndMouseDrag();
    }
    TargetActor = InTargetActor;
    UpdateGizmoTransformAndScale();
}

void AVirtualSensorTransformGizmoActor::SetGizmoMode(EVirtualSensorGizmoMode InMode)
{
    GizmoMode = InMode;
    SetHandleCollisionEnabled(bManipulationEnabled && bGizmoVisible && TargetActor != nullptr);
}

void AVirtualSensorTransformGizmoActor::SetCoordinateSpace(EVirtualSensorCoordinateSpace InSpace)
{
    CoordinateSpace = InSpace;
    UpdateGizmoTransformAndScale();
}

void AVirtualSensorTransformGizmoActor::SetManipulationEnabled(bool bEnabled)
{
    bManipulationEnabled = bEnabled;
    if (!bEnabled && bMouseDragging)
    {
        EndMouseDrag();
    }
    SetHandleCollisionEnabled(bManipulationEnabled && bGizmoVisible && TargetActor != nullptr);
}

void AVirtualSensorTransformGizmoActor::SetGizmoVisible(bool bVisible)
{
    bGizmoVisible = bVisible;
    SetHandleCollisionEnabled(bManipulationEnabled && bGizmoVisible && TargetActor != nullptr);
}

void AVirtualSensorTransformGizmoActor::SetProjectionDebugEnabled(bool bEnabled)
{
    bProjectionDebugEnabled = bEnabled;
}

void AVirtualSensorTransformGizmoActor::SetStepSizes(float InTranslationStepCm, float InRotationStepDegrees)
{
    TranslationStepCm = FMath::Clamp(InTranslationStepCm, 1.0f, 1000.0f);
    RotationStepDegrees = FMath::Clamp(InRotationStepDegrees, 0.1f, 90.0f);
}

void AVirtualSensorTransformGizmoActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!TargetActor)
    {
        SetHandleCollisionEnabled(false);
        return;
    }

    UpdateGizmoTransformAndScale();
    HandleMouseInput();
    HandleKeyboardInput(DeltaSeconds);
    DrawGizmo();
    DrawProjectionDebug();
}

void AVirtualSensorTransformGizmoActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    TargetActor = nullptr;
    OnTransformChanged.Clear();
    OnTransformCommitted.Clear();
    Super::EndPlay(EndPlayReason);
}

FVector AVirtualSensorTransformGizmoActor::ResolveAxisVector(const FTransform& TargetTransform, EVirtualSensorCoordinateSpace Space, EAxis::Type Axis)
{
    if (Space == EVirtualSensorCoordinateSpace::World)
    {
        if (Axis == EAxis::X) return FVector::ForwardVector;
        if (Axis == EAxis::Y) return FVector::RightVector;
        return FVector::UpVector;
    }
    return TargetTransform.GetUnitAxis(Axis);
}

FTransform AVirtualSensorTransformGizmoActor::ApplyKeyboardDelta(const FTransform& TargetTransform, EVirtualSensorCoordinateSpace Space, const FVector& TranslationAxes, const FRotator& RotationDegrees)
{
    FTransform Result = TargetTransform;
    const FVector AxisX = ResolveAxisVector(TargetTransform, Space, EAxis::X);
    const FVector AxisY = ResolveAxisVector(TargetTransform, Space, EAxis::Y);
    const FVector AxisZ = ResolveAxisVector(TargetTransform, Space, EAxis::Z);
    Result.AddToTranslation(AxisX * TranslationAxes.X + AxisY * TranslationAxes.Y + AxisZ * TranslationAxes.Z);

    const FQuat Roll(AxisX, FMath::DegreesToRadians(RotationDegrees.Roll));
    const FQuat Pitch(AxisY, FMath::DegreesToRadians(RotationDegrees.Pitch));
    const FQuat Yaw(AxisZ, FMath::DegreesToRadians(RotationDegrees.Yaw));
    Result.SetRotation((Yaw * Pitch * Roll * TargetTransform.GetRotation()).GetNormalized());
    return Result;
}

void AVirtualSensorTransformGizmoActor::UpdateGizmoTransformAndScale()
{
    if (!TargetActor)
    {
        return;
    }
    const FTransform TargetTransform = TargetActor->GetActorTransform();
    SetActorLocation(TargetTransform.GetLocation());
    SetActorRotation(CoordinateSpace == EVirtualSensorCoordinateSpace::Local ? TargetTransform.GetRotation() : FQuat::Identity);

    float Distance = 800.0f;
    if (const UWorld* World = GetWorld())
    {
        if (const APlayerController* Controller = World->GetFirstPlayerController())
        {
            FVector ViewLocation;
            FRotator ViewRotation;
            Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
            Distance = FVector::Distance(ViewLocation, TargetTransform.GetLocation());
        }
    }
    const float NewScale = FMath::Clamp(Distance / 900.0f, 0.55f, 2.5f);
    if (!FMath::IsNearlyEqual(NewScale, CurrentVisualScale, 0.02f))
    {
        CurrentVisualScale = NewScale;
        UpdateCollisionHandles(CurrentVisualScale);
    }
}

void AVirtualSensorTransformGizmoActor::UpdateCollisionHandles(float Scale)
{
    const float AxisLength = BaseAxisLength * Scale;
    const float AxisRadius = 8.0f * Scale;
    for (int32 AxisIndex = 0; AxisIndex < MoveHandles.Num(); ++AxisIndex)
    {
        UBoxComponent* Handle = MoveHandles[AxisIndex];
        FVector Location = FVector::ZeroVector;
        FVector Extent(AxisRadius, AxisRadius, AxisRadius);
        Location[AxisIndex] = AxisLength * 0.55f;
        Extent[AxisIndex] = AxisLength * 0.55f;
        Handle->SetRelativeLocation(Location);
        Handle->SetBoxExtent(Extent, false);
    }

    const float Radius = BaseRingRadius * Scale;
    const FVector Extent(8.0f * Scale);
    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        for (int32 SegmentIndex = 0; SegmentIndex < RingSegments; ++SegmentIndex)
        {
            const float Angle = 2.0f * PI * static_cast<float>(SegmentIndex) / static_cast<float>(RingSegments);
            FVector Location = FVector::ZeroVector;
            Location[(AxisIndex + 1) % 3] = FMath::Cos(Angle) * Radius;
            Location[(AxisIndex + 2) % 3] = FMath::Sin(Angle) * Radius;
            const int32 FlatIndex = AxisIndex * RingSegments + SegmentIndex;
            RotationHandles[FlatIndex]->SetRelativeLocation(Location);
            RotationHandles[FlatIndex]->SetBoxExtent(Extent, false);
        }
    }
}

void AVirtualSensorTransformGizmoActor::SetHandleCollisionEnabled(bool bEnabled)
{
    const ECollisionEnabled::Type MoveCollision = bEnabled && GizmoMode == EVirtualSensorGizmoMode::Translate ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
    const ECollisionEnabled::Type RotateCollision = bEnabled && GizmoMode == EVirtualSensorGizmoMode::Rotate ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
    for (UBoxComponent* Handle : MoveHandles) if (Handle) Handle->SetCollisionEnabled(MoveCollision);
    for (UBoxComponent* Handle : RotationHandles) if (Handle) Handle->SetCollisionEnabled(RotateCollision);
}

void AVirtualSensorTransformGizmoActor::HandleMouseInput()
{
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Controller || !bManipulationEnabled || !bGizmoVisible)
    {
        return;
    }

    if (Controller->WasInputKeyJustPressed(EKeys::LeftMouseButton) && !bMouseDragging)
    {
        FHitResult Hit;
        if (Controller->GetHitResultUnderCursor(ECC_Visibility, true, Hit) && Hit.GetActor() == this)
        {
            if (const UBoxComponent* Box = Cast<UBoxComponent>(Hit.GetComponent()))
            {
                BeginMouseDrag(ResolveHandleKind(Box));
            }
        }
    }
    if (bMouseDragging && Controller->IsInputKeyDown(EKeys::LeftMouseButton))
    {
        UpdateMouseDrag();
    }
    if (bMouseDragging && Controller->WasInputKeyJustReleased(EKeys::LeftMouseButton))
    {
        EndMouseDrag();
    }
}

void AVirtualSensorTransformGizmoActor::HandleKeyboardInput(float DeltaSeconds)
{
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Controller || !TargetActor || !bManipulationEnabled || IsEditableTextFocused())
    {
        return;
    }
    if (Controller->WasInputKeyJustPressed(EKeys::Escape))
    {
        SetManipulationEnabled(false);
        if (bKeyboardManipulating)
        {
            bKeyboardManipulating = false;
            OnTransformCommitted.Broadcast(TargetActor->GetActorTransform());
        }
        return;
    }

    const bool bForward = Controller->IsInputKeyDown(EKeys::W);
    const bool bBack = Controller->IsInputKeyDown(EKeys::S);
    const bool bRight = Controller->IsInputKeyDown(EKeys::D);
    const bool bLeft = Controller->IsInputKeyDown(EKeys::A);
    const bool bUp = Controller->IsInputKeyDown(EKeys::E);
    const bool bDown = Controller->IsInputKeyDown(EKeys::Q);
    const bool bYawRight = Controller->IsInputKeyDown(EKeys::Right);
    const bool bYawLeft = Controller->IsInputKeyDown(EKeys::Left);
    const bool bPitchUp = Controller->IsInputKeyDown(EKeys::Up);
    const bool bPitchDown = Controller->IsInputKeyDown(EKeys::Down);
    const bool bRollLeft = Controller->IsInputKeyDown(EKeys::Z);
    const bool bRollRight = Controller->IsInputKeyDown(EKeys::C);
    const bool bAnyKey = bForward || bBack || bRight || bLeft || bUp || bDown || bYawRight || bYawLeft || bPitchUp || bPitchDown || bRollLeft || bRollRight;

    if (!bAnyKey)
    {
        if (bKeyboardManipulating)
        {
            bKeyboardManipulating = false;
            OnTransformCommitted.Broadcast(TargetActor->GetActorTransform());
        }
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    if (bKeyboardManipulating && Now < NextKeyboardStepTime)
    {
        return;
    }
    bKeyboardManipulating = true;
    NextKeyboardStepTime = Now + 0.08;

    float Multiplier = 1.0f;
    if (Controller->IsInputKeyDown(EKeys::LeftShift) || Controller->IsInputKeyDown(EKeys::RightShift)) Multiplier *= 5.0f;
    if (Controller->IsInputKeyDown(EKeys::LeftControl) || Controller->IsInputKeyDown(EKeys::RightControl)) Multiplier *= 0.2f;

    const FVector Translation(
        (static_cast<float>(bForward) - static_cast<float>(bBack)) * TranslationStepCm * Multiplier,
        (static_cast<float>(bRight) - static_cast<float>(bLeft)) * TranslationStepCm * Multiplier,
        (static_cast<float>(bUp) - static_cast<float>(bDown)) * TranslationStepCm * Multiplier);
    const FRotator Rotation(
        (static_cast<float>(bPitchUp) - static_cast<float>(bPitchDown)) * RotationStepDegrees * Multiplier,
        (static_cast<float>(bYawRight) - static_cast<float>(bYawLeft)) * RotationStepDegrees * Multiplier,
        (static_cast<float>(bRollRight) - static_cast<float>(bRollLeft)) * RotationStepDegrees * Multiplier);
    const FTransform NextTransform = ApplyKeyboardDelta(TargetActor->GetActorTransform(), CoordinateSpace, Translation, Rotation);
    TargetActor->SetActorTransform(NextTransform, false, nullptr, ETeleportType::TeleportPhysics);
    OnTransformChanged.Broadcast(NextTransform);
}

void AVirtualSensorTransformGizmoActor::BeginMouseDrag(EHandleKind HandleKind)
{
    if (!TargetActor || HandleKind == EHandleKind::None)
    {
        return;
    }
    ActiveHandle = HandleKind;
    DragStartTransform = TargetActor->GetActorTransform();
    DragOrigin = DragStartTransform.GetLocation();
    DragAxis = ResolveHandleAxis(HandleKind, DragStartTransform).GetSafeNormal();
    bMouseDragging = true;

    FVector RayOrigin;
    FVector RayDirection;
    if (HandleKind == EHandleKind::MoveX || HandleKind == EHandleKind::MoveY || HandleKind == EHandleKind::MoveZ)
    {
        if (DeprojectMouseRay(RayOrigin, RayDirection))
        {
            DragStartAxisParameter = ClosestAxisParameter(RayOrigin, RayDirection, DragOrigin, DragAxis);
        }
    }
    else
    {
        FVector PlanePoint;
        if (IntersectMousePlane(DragOrigin, DragAxis, PlanePoint))
        {
            DragStartPlaneVector = (PlanePoint - DragOrigin).GetSafeNormal();
        }
    }
}

void AVirtualSensorTransformGizmoActor::UpdateMouseDrag()
{
    if (!TargetActor)
    {
        return;
    }
    FTransform NextTransform = DragStartTransform;
    if (ActiveHandle == EHandleKind::MoveX || ActiveHandle == EHandleKind::MoveY || ActiveHandle == EHandleKind::MoveZ)
    {
        FVector RayOrigin;
        FVector RayDirection;
        if (!DeprojectMouseRay(RayOrigin, RayDirection)) return;
        const float CurrentParameter = ClosestAxisParameter(RayOrigin, RayDirection, DragOrigin, DragAxis);
        NextTransform.AddToTranslation(DragAxis * (CurrentParameter - DragStartAxisParameter));
    }
    else
    {
        FVector PlanePoint;
        if (!IntersectMousePlane(DragOrigin, DragAxis, PlanePoint)) return;
        const FVector CurrentVector = (PlanePoint - DragOrigin).GetSafeNormal();
        if (CurrentVector.IsNearlyZero() || DragStartPlaneVector.IsNearlyZero()) return;
        const float Angle = FMath::Atan2(FVector::DotProduct(DragAxis, FVector::CrossProduct(DragStartPlaneVector, CurrentVector)), FVector::DotProduct(DragStartPlaneVector, CurrentVector));
        NextTransform.SetRotation((FQuat(DragAxis, Angle) * DragStartTransform.GetRotation()).GetNormalized());
    }
    TargetActor->SetActorTransform(NextTransform, false, nullptr, ETeleportType::TeleportPhysics);
    OnTransformChanged.Broadcast(NextTransform);
}

void AVirtualSensorTransformGizmoActor::EndMouseDrag()
{
    bMouseDragging = false;
    ActiveHandle = EHandleKind::None;
    if (TargetActor)
    {
        OnTransformCommitted.Broadcast(TargetActor->GetActorTransform());
    }
}

bool AVirtualSensorTransformGizmoActor::DeprojectMouseRay(FVector& OutOrigin, FVector& OutDirection) const
{
    const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    return Controller && Controller->DeprojectMousePositionToWorld(OutOrigin, OutDirection);
}

AVirtualSensorTransformGizmoActor::EHandleKind AVirtualSensorTransformGizmoActor::ResolveHandleKind(const UBoxComponent* Component) const
{
    if (!Component) return EHandleKind::None;
    if (Component->ComponentHasTag(MoveHandleTag(0))) return EHandleKind::MoveX;
    if (Component->ComponentHasTag(MoveHandleTag(1))) return EHandleKind::MoveY;
    if (Component->ComponentHasTag(MoveHandleTag(2))) return EHandleKind::MoveZ;
    if (Component->ComponentHasTag(RotateHandleTag(0))) return EHandleKind::RotateX;
    if (Component->ComponentHasTag(RotateHandleTag(1))) return EHandleKind::RotateY;
    if (Component->ComponentHasTag(RotateHandleTag(2))) return EHandleKind::RotateZ;
    return EHandleKind::None;
}

FVector AVirtualSensorTransformGizmoActor::ResolveHandleAxis(EHandleKind HandleKind, const FTransform& Transform) const
{
    if (HandleKind == EHandleKind::MoveX || HandleKind == EHandleKind::RotateX) return ResolveAxisVector(Transform, CoordinateSpace, EAxis::X);
    if (HandleKind == EHandleKind::MoveY || HandleKind == EHandleKind::RotateY) return ResolveAxisVector(Transform, CoordinateSpace, EAxis::Y);
    return ResolveAxisVector(Transform, CoordinateSpace, EAxis::Z);
}

float AVirtualSensorTransformGizmoActor::ClosestAxisParameter(const FVector& RayOrigin, const FVector& RayDirection, const FVector& AxisOrigin, const FVector& AxisDirection) const
{
    const FVector Ray = RayDirection.GetSafeNormal();
    const FVector Axis = AxisDirection.GetSafeNormal();
    const FVector Delta = RayOrigin - AxisOrigin;
    const float CrossDot = FVector::DotProduct(Ray, Axis);
    const float Denominator = 1.0f - CrossDot * CrossDot;
    if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER)
    {
        return FVector::DotProduct(Delta, Axis);
    }
    return (FVector::DotProduct(Delta, Axis) - CrossDot * FVector::DotProduct(Delta, Ray)) / Denominator;
}

bool AVirtualSensorTransformGizmoActor::IntersectMousePlane(const FVector& PlaneOrigin, const FVector& PlaneNormal, FVector& OutPoint) const
{
    FVector RayOrigin;
    FVector RayDirection;
    if (!DeprojectMouseRay(RayOrigin, RayDirection)) return false;
    const float Denominator = FVector::DotProduct(RayDirection, PlaneNormal);
    if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER) return false;
    const float Distance = FVector::DotProduct(PlaneOrigin - RayOrigin, PlaneNormal) / Denominator;
    OutPoint = RayOrigin + RayDirection * Distance;
    return true;
}

void AVirtualSensorTransformGizmoActor::DrawGizmo() const
{
    if (!bManipulationEnabled || !bGizmoVisible || !TargetActor || !GetWorld()) return;
    const FVector Origin = GetActorLocation();
    const FTransform TargetTransform = TargetActor->GetActorTransform();
    const FVector X = ResolveAxisVector(TargetTransform, CoordinateSpace, EAxis::X);
    const FVector Y = ResolveAxisVector(TargetTransform, CoordinateSpace, EAxis::Y);
    const FVector Z = ResolveAxisVector(TargetTransform, CoordinateSpace, EAxis::Z);
    const float Thickness = 3.0f * CurrentVisualScale;
    if (GizmoMode == EVirtualSensorGizmoMode::Translate)
    {
        const float Length = BaseAxisLength * CurrentVisualScale;
        DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + X * Length, 18.0f * CurrentVisualScale, FColor::Red, false, 0.0f, 10, Thickness);
        DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + Y * Length, 18.0f * CurrentVisualScale, FColor::Green, false, 0.0f, 10, Thickness);
        DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + Z * Length, 18.0f * CurrentVisualScale, FColor::Blue, false, 0.0f, 10, Thickness);
    }
    else
    {
        const float Radius = BaseRingRadius * CurrentVisualScale;
        DrawDebugCircle(GetWorld(), Origin, Radius, 48, FColor::Red, false, 0.0f, 10, Thickness, Y, Z, false);
        DrawDebugCircle(GetWorld(), Origin, Radius, 48, FColor::Green, false, 0.0f, 10, Thickness, X, Z, false);
        DrawDebugCircle(GetWorld(), Origin, Radius, 48, FColor::Blue, false, 0.0f, 10, Thickness, X, Y, false);
    }
}

void AVirtualSensorTransformGizmoActor::DrawProjectionDebug() const
{
    if (!bProjectionDebugEnabled || !TargetActor || !GetWorld()) return;
    if (TargetActor->FindComponentByClass<UVirtualCameraComp>()) DrawCameraProjectionDebug();
    else if (TargetActor->FindComponentByClass<UVirtualLidarSensorComp>()) DrawLidarProjectionDebug();
}

void AVirtualSensorTransformGizmoActor::DrawCameraProjectionDebug() const
{
    const UVirtualCameraComp* Camera = TargetActor ? TargetActor->FindComponentByClass<UVirtualCameraComp>() : nullptr;
    if (!Camera) return;
    const FTransform Transform = Camera->GetComponentTransform();
    const FVector Origin = Transform.GetLocation();
    const FVector Forward = Transform.GetUnitAxis(EAxis::X);
    const FVector Right = Transform.GetUnitAxis(EAxis::Y);
    const FVector Up = Transform.GetUnitAxis(EAxis::Z);
    const float NearDistance = FMath::Max(10.0f, Camera->GetDeviceSpec().MinRangeCm);
    const float FarDistance = FMath::Max(NearDistance + 10.0f, Camera->GetDeviceSpec().TypicalRangeCm > 0.0f ? Camera->GetDeviceSpec().TypicalRangeCm : 600.0f);
    const float Aspect = Camera->CaptureResolution.Y > 0 ? static_cast<float>(Camera->CaptureResolution.X) / static_cast<float>(Camera->CaptureResolution.Y) : 16.0f / 9.0f;
    const float HalfHorizontalTan = FMath::Tan(FMath::DegreesToRadians(Camera->FOVAngle * 0.5f));
    const auto MakeCorners = [&](float Distance, FVector OutCorners[4])
    {
        const float HalfWidth = Distance * HalfHorizontalTan;
        const float HalfHeight = HalfWidth / FMath::Max(Aspect, 0.01f);
        const FVector Center = Origin + Forward * Distance;
        OutCorners[0] = Center + Right * HalfWidth + Up * HalfHeight;
        OutCorners[1] = Center - Right * HalfWidth + Up * HalfHeight;
        OutCorners[2] = Center - Right * HalfWidth - Up * HalfHeight;
        OutCorners[3] = Center + Right * HalfWidth - Up * HalfHeight;
    };
    FVector NearCorners[4];
    FVector FarCorners[4];
    MakeCorners(NearDistance, NearCorners);
    MakeCorners(FarDistance, FarCorners);
    const FColor Color(0, 220, 255);
    DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + Forward * FMath::Min(FarDistance, 250.0f), 20.0f, FColor::Yellow, false, 0.0f, 5, 2.0f);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const int32 Next = (Index + 1) % 4;
        DrawDebugLine(GetWorld(), NearCorners[Index], NearCorners[Next], Color, false, 0.0f, 5, 1.5f);
        DrawDebugLine(GetWorld(), FarCorners[Index], FarCorners[Next], Color, false, 0.0f, 5, 1.5f);
        DrawDebugLine(GetWorld(), NearCorners[Index], FarCorners[Index], Color, false, 0.0f, 5, 1.0f);
    }
}

void AVirtualSensorTransformGizmoActor::DrawLidarProjectionDebug() const
{
    const UVirtualLidarSensorComp* Lidar = TargetActor ? TargetActor->FindComponentByClass<UVirtualLidarSensorComp>() : nullptr;
    if (!Lidar) return;
    const FTransform Transform = Lidar->GetComponentTransform();
    const FVector Origin = Transform.GetLocation();
    const float Range = FMath::Max(10.0f, Lidar->MaxDistance);
    constexpr int32 HorizontalDebugSamples = 16;
    constexpr int32 VerticalDebugSamples = 4;
    static_assert(HorizontalDebugSamples * VerticalDebugSamples <= ProjectionDebugRayBudget, "LiDAR debug ray budget exceeded");
    for (int32 V = 0; V < VerticalDebugSamples; ++V)
    {
        const float Pitch = FMath::Lerp(Lidar->MinVerticalAngle, Lidar->MaxVerticalAngle, VerticalDebugSamples > 1 ? static_cast<float>(V) / static_cast<float>(VerticalDebugSamples - 1) : 0.5f);
        for (int32 H = 0; H < HorizontalDebugSamples; ++H)
        {
            const float Alpha = HorizontalDebugSamples > 1 ? static_cast<float>(H) / static_cast<float>(HorizontalDebugSamples - 1) : 0.5f;
            const float Yaw = Lidar->HorizontalFov >= 359.9f ? Alpha * 360.0f : FMath::Lerp(-Lidar->HorizontalFov * 0.5f, Lidar->HorizontalFov * 0.5f, Alpha);
            const FVector LocalDirection = FRotationMatrix(FRotator(Pitch, Yaw, 0.0f)).GetUnitAxis(EAxis::X);
            const FVector WorldDirection = Transform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
            const FColor Color = (V == 0 || V == VerticalDebugSamples - 1) ? FColor(255, 180, 0) : FColor(0, 220, 255);
            DrawDebugLine(GetWorld(), Origin, Origin + WorldDirection * Range, Color, false, 0.0f, 4, 0.6f);
        }
    }
    DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + Transform.GetUnitAxis(EAxis::X) * FMath::Min(Range, 250.0f), 20.0f, FColor::Yellow, false, 0.0f, 5, 2.0f);
}

bool AVirtualSensorTransformGizmoActor::IsEditableTextFocused() const
{
    if (!FSlateApplication::IsInitialized()) return false;
    const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
    return Focused.IsValid() && Focused->GetTypeAsString().Contains(TEXT("EditableText"));
}
