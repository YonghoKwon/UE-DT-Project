#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlabScenarioValidationRig.generated.h"
class ASlabActor;
class AVirtualLidarSensorActor;
class AVirtualCameraSensorActor;
class AVirtualSensorCoordinator;
class SWidget;

/** Explicit test-map controls. Never placed by the production Slab actor. */
UCLASS()
class MA0T10_DT_API ASlabScenarioValidationRig : public AActor
{
    GENERATED_BODY()
public:
    ASlabScenarioValidationRig();
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditInstanceOnly) TObjectPtr<ASlabActor> SlabActor;
    UPROPERTY(EditInstanceOnly) TObjectPtr<AVirtualLidarSensorActor> Lidar;
    UPROPERTY(EditInstanceOnly) TObjectPtr<AVirtualCameraSensorActor> Camera;
    UPROPERTY(EditInstanceOnly) TObjectPtr<AVirtualSensorCoordinator> Coordinator;
    UFUNCTION(BlueprintCallable) void SubmitSynthetic(bool bPcd);
    UFUNCTION(BlueprintCallable) void SaveEvidence(bool bScreenshot=true);
private:
    bool bInitialized=false;
    bool bReportWritten=false;
    FString ActiveRun;
    float UiTime=0;
    TArray<double> FrameTimes;
    TSharedPtr<SWidget> Controls;
    FString DisplayStatus;
    void ConfigureLocalBroker();
    void WriteReport();
};
