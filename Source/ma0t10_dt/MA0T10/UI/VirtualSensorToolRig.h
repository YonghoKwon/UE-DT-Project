#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualSensorToolRig.generated.h"
class AVirtualSensorCoordinator;
class AVirtualSensorUiHostActor;
class ASlabSimulationUiHostActor;
class ASlabScenarioReplayUiHostActor;
class ASlabActor;
class UVirtualSensorCaptureExportPanelWidget;

/** Map-local composition only. No environment, player, shared settings or asset ownership. */
UCLASS(BlueprintType)
class MA0T10_DT_API AVirtualSensorToolRig : public AActor
{
    GENERATED_BODY()
public:
    AVirtualSensorToolRig();
    UPROPERTY(EditAnywhere, Category="Sensor Tools|Existing") TObjectPtr<AVirtualSensorCoordinator> Coordinator;
    UPROPERTY(EditAnywhere, Category="Sensor Tools|Existing") TObjectPtr<AVirtualSensorUiHostActor> SensorUiHost;
    UPROPERTY(EditAnywhere, Category="Sensor Tools|Existing") TObjectPtr<ASlabSimulationUiHostActor> SlabUiHost;
    UPROPERTY(EditAnywhere, Category="Sensor Tools|Existing") TObjectPtr<ASlabScenarioReplayUiHostActor> ReplayUiHost;
    UPROPERTY(EditAnywhere, Category="Sensor Tools|Existing") TObjectPtr<ASlabActor> TargetSlab;
    UPROPERTY(VisibleAnywhere, Category="Sensor Tools|Status") FString SetupMessage;
    bool EnsureSetup();
    bool IsSetupReady() const { return bReady; }
    int32 GetOwnedActorCount() const;
    FString GetSlabSetupMessage() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TArray<TWeakObjectPtr<AActor>> OwnedActors;
    TWeakObjectPtr<AVirtualSensorCoordinator> PreviousManager;
    TWeakObjectPtr<ASlabActor> PreviousSlab;
    TWeakObjectPtr<ASlabActor> LastBoundSlab,PreviousDataSlab;
    TWeakObjectPtr<UVirtualSensorCaptureExportPanelWidget> BoundDataWidget;
    bool bReady=false, bEnding=false, bReusedSensor=false, bReusedSlab=false;
    FTimerHandle SetupTimer;
    void RetrySetup();
};
