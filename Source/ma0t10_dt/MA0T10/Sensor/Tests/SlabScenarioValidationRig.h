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
#if WITH_DEV_AUTOMATION_TESTS
    void SaveResourceEvidence(const FString& Phase, int32 Repetition, int64 AuditEntries=0, int64 AuditBytes=0) const;
    void BeginAcceptanceObservation();
    void SetValidationControlsVisibleForTests(bool bVisible);
#endif
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
    TMap<FString,TMap<FString,int64>> AcceptanceAcquisitions;
    TMap<FString,int64> AcceptanceCameraFrames;
    void ObserveAcquisition(const FString& SensorId);
};
