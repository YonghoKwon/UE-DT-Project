#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DxWebSocketSubsystem.h"
#include "ma0t10_dt/MA0T10/Contracts/TrainBatchContract.h"
#include "TrainBatchSubsystem.generated.h"

class ASlabActor;
class AVirtualLidarSensorActor;
class AVirtualCameraSensorActor;
class UTrainBatchScenarioTC;
struct FTrainBatchTransaction;
struct FTrainBatchState;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
struct FTrainBatchTestCounters
{
    uint64 PumpCalls=0,AsyncStarts=0,SubscriptionRequests=0;
    bool TimerActive=false,FutureActive=false,SubscriptionActive=false;
};
#endif

/** 1A serial baseline in the current World. No resources before explicit enable/work. */
UCLASS()
class MA0T10_DT_API UTrainBatchSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable,Category="Training") bool SetEnabled(bool Enabled,FString& Error);
    UFUNCTION(BlueprintPure,Category="Training") bool IsEnabled() const;
    UFUNCTION(BlueprintCallable,Category="Training") bool ConfigureTargets(const FString& PresetId,ASlabActor* Slab,AVirtualLidarSensorActor* Lidar,AVirtualCameraSensorActor* Camera,FString& Error);
    UFUNCTION(BlueprintCallable,Category="Training") bool ImportManifest(const FString& AbsolutePath,FString& Error);
    UFUNCTION(BlueprintCallable,Category="Training") bool StartJob(const FString& JobId,FString& Error);
    UFUNCTION(BlueprintCallable,Category="Training") bool SetProcessWorkers(int32 Count,FString& Error);
    UFUNCTION(BlueprintCallable,Category="Training") bool RestoreJob(const FString& JobId,FString& Error);
    UFUNCTION(BlueprintCallable,Category="Training") void CancelJob();
    UFUNCTION(BlueprintPure,Category="Training") FString GetStatus() const;
    bool SubmitEnvelope(const FString& Json,FString& Error);
    void AcceptTransaction(const FTrainBatchTransaction& Transaction);
    const TArray<FTrainBatchJobView>& Jobs() const;
    int64 Revision() const;
    bool SelectPreview(const FString& JobId,int32 Index,FString& Error);
    void StopPreview();
    class UTexture2D* PreviewTexture() const;
    FString PreviewStatus() const;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    bool HasBackgroundWorkForTests() const;
    FTrainBatchTestCounters CountersForTests() const;
    void UseInProcessForTests();
#endif
private:
    friend class UTrainBatchScenarioTC;
    bool IsSubscriptionCurrent(uint64 Epoch) const;
    TSharedPtr<FTrainBatchState> State;
    UPROPERTY(Transient) TObjectPtr<UTrainBatchScenarioTC> Handler;
    FTimerHandle Timer;
    void EnsurePump();
    void Pump();
    void Subscribe();
    void DisconnectOwnedSubscription();
    UFUNCTION() void OnMessage(const FWebSocketMessage& Message);
    UFUNCTION() void OnConnectionState(EDxWebSocketConnectionState Connection,FString Reason);
    UFUNCTION() void OnSubscribed(bool Success,FString Error);
};
