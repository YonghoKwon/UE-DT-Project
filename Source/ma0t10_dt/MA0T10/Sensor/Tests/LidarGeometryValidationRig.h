#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LidarGeometryValidationRig.generated.h"
class AStaticMeshActor;
class ACameraActor;
class AVirtualLidarSensorActor;
class AVirtualSensorCoordinator;
class UVirtualSensorMonitorPanelWidget;
class SWidget;

/** Dedicated validation map only. No production scenario or topic subscription. */
UCLASS()
class MA0T10_DT_API ALidarGeometryValidationRig : public AActor
{
    GENERATED_BODY()
public:
    ALidarGeometryValidationRig();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditInstanceOnly) TObjectPtr<AStaticMeshActor> Plate;
    UPROPERTY(EditInstanceOnly) TObjectPtr<AStaticMeshActor> MovingObject;
    UPROPERTY(EditInstanceOnly) TObjectPtr<AVirtualLidarSensorActor> Sensor;
    UPROPERTY(EditInstanceOnly) TObjectPtr<AVirtualSensorCoordinator> Coordinator;
    UPROPERTY(EditInstanceOnly) TObjectPtr<ACameraActor> ObservationCamera;
    UPROPERTY(Transient) TObjectPtr<UVirtualSensorMonitorPanelWidget> Monitor;
    bool bMoving = true;
    void ToggleBackend();
    void ToggleDisplay();
    void SaveEvidence();
    FString Status;
private:
    bool bInitialized=false;
    float Elapsed=0;
    float UiElapsed=0;
    TSharedPtr<SWidget> Controls;
};
