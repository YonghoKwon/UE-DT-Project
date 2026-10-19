#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipHatchMonitorHost.generated.h"
class AShipHatchSimulation;
class AVirtualSensorCoordinator;
class AVirtualSensorUiHostActor;
class UShipHatchMonitorWidget;
class ACameraActor;

UCLASS()
class MA0T10_DT_API AShipHatchMonitorHost : public AActor
{
    GENERATED_BODY()
public:
    AShipHatchMonitorHost();
    UPROPERTY(EditAnywhere,Category="Hatch") TObjectPtr<AShipHatchSimulation> Simulation;
    UPROPERTY(EditAnywhere,Category="Hatch") TObjectPtr<ACameraActor> ViewCamera;
    UPROPERTY(Transient) TObjectPtr<AVirtualSensorCoordinator> Coordinator;
    UPROPERTY(Transient) TObjectPtr<AVirtualSensorUiHostActor> Tools;
    UPROPERTY(Transient) TObjectPtr<UShipHatchMonitorWidget> Monitor;
    void ShowTools();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    FTimerHandle RefreshTimer;
    TWeakObjectPtr<AActor> PreviousView;
    bool bPreviousCursor=false;
    bool bRegisteredSensors=false;
    void Refresh();
};
