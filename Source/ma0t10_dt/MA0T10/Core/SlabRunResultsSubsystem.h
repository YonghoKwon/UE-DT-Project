#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Async/Future.h"
#include "SlabRunDeliveryTypes.h"
#include "SlabRunResultsSubsystem.generated.h"
class AVirtualSensorActorBase;
class UVirtualSensorTransportComponent;
struct FVirtualSensorTransportObservation;
struct FVirtualSensorTransportResult;
struct FVirtualSensorTopicReceivedDataBase;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlabRunResultsChanged);
UCLASS()
class MA0T10_DT_API USlabRunResultsSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float Delta) override;
    virtual TStatId GetStatId() const override;
    void BeginRun(const FString& Run,const FString& Scenario,ESlabExecutionPolicy Policy,const FVirtualSlabSensorOutputSelection& Outputs);
    void SetSensors(const FString& Run,const TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>>& Sensors,UVirtualSensorTransportComponent* Transport);
    void RecordAcquisition(const FString& Run,bool Success);
    void MarkStopped(const FString& Run);
    void SetAppliedOutputs(const FString& Run,const FVirtualSlabSensorOutputSelection& Outputs);
    void SetMeasuredRates(const FString& Run,const TMap<FString,TWeakObjectPtr<AVirtualSensorActorBase>>& Sensors);
    int64 GetUnconfirmedCount(const FString& Run) const;
    void UpdateProgress(const FString& Run,const FString& Mtl,int64 Frame,double Elapsed);
    void RecordOutputFailure(const FString& Run,const FString& Sensor,int32 Kind,int64 Frame,const FString& Reason);
    void ObserveTransport(const FVirtualSensorTransportObservation& Event);
    void Finalize(const FVirtualSlabSessionStatus& Session,bool PreparationFailed=false);
    bool HasLedgerFailure(const FString& Run) const;
    UFUNCTION(BlueprintPure,Category="Slab|Results") TArray<FSlabRunDeliverySummary> GetRecentRunResults() const { return Results; }
    UFUNCTION(BlueprintPure,Category="Slab|Results") bool GetRunResult(const FString& Run,FSlabRunDeliverySummary& Result) const;
    UFUNCTION(BlueprintCallable,Category="Slab|Results") bool OpenRunReport(const FString& Run) const;
    static FString Describe(const FSlabRunDeliverySummary& Result);
    UPROPERTY(BlueprintAssignable) FSlabRunResultsChanged OnResultsChanged;
private:
    struct FWriteResult { FString Run,Path,Error;bool Success=false; };
    struct FRequestState { uint8 Flags=0;FString RequestId; };
    TMap<FString,FRequestState> Requests;
    TArray<FSlabRunDeliverySummary> Results;
    TArray<TFuture<FWriteResult>> Writes;
    FString ActiveRun;
    FDelegateHandle ObservationHandle,ReceiveHandle;
    TWeakObjectPtr<UVirtualSensorTransportComponent> BoundTransport;
    bool bEnding=false;
    UFUNCTION() void HandleEngineResult(const FVirtualSensorTransportResult& Result);
    void HandleReceive(const TSharedPtr<FVirtualSensorTopicReceivedDataBase>& Result);
    FSlabRunDeliverySummary* Find(const FString& Run);
    void QueueReport(const FSlabRunDeliverySummary& Summary);
    void ApplyWriteResult(const FWriteResult& Result);
    static FWriteResult WriteReport(FString Run,FString Json,FString Directory,int32 MaxFiles);
    static FString SerializeSummary(const FSlabRunDeliverySummary& Summary);
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSlabRunLedgerTest;
#endif
};
