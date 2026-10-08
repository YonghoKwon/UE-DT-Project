#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorLayoutTypes.h"
#include "Async/Future.h"
#include "VirtualSensorLayoutSubsystem.generated.h"
class AVirtualSensorActorBase;
class AVirtualSensorCoordinator;
USTRUCT(BlueprintType)
struct FVirtualSensorLayoutItem
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AVirtualSensorActorBase> Actor;
    UPROPERTY(BlueprintReadOnly) FString LayoutGuid;
    UPROPERTY(BlueprintReadOnly) FString SensorId;
    UPROPERTY(BlueprintReadOnly) EVirtualSensorKind Kind=EVirtualSensorKind::Lidar;
    UPROPERTY(BlueprintReadOnly) bool bToolCreated=false;
    UPROPERTY(BlueprintReadOnly) bool bPreviewEnabled=false;
    UPROPERTY(BlueprintReadOnly) FString LockReason;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVirtualSensorLayoutChanged);
/** Owns only runtime sensors created through this service. Never saves a level. */
UCLASS()
class MA0T10_DT_API UVirtualSensorLayoutSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    virtual void Tick(float Delta) override;
    virtual TStatId GetStatId() const override;
    AVirtualSensorCoordinator* Coordinator() const;
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorLayout") TArray<FVirtualSensorLayoutItem> GetLayoutSensors() const;
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorLayout") bool CanCreateSensor(EVirtualSensorKind Kind,FString& Reason) const;
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorLayout") AVirtualSensorActorBase* CreateSensorAtView(EVirtualSensorKind Kind,FString& Reason);
    AVirtualSensorActorBase* CreateSensorAtPose(EVirtualSensorKind Kind,const FTransform& Pose,FString& Reason,bool bPreview=true);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorLayout") AVirtualSensorActorBase* DuplicateSensor(AVirtualSensorActorBase* Source,FString& Reason);
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorLayout") bool CanDeleteSensor(AVirtualSensorActorBase* Sensor,FString& Reason) const;
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorLayout") bool DeleteSensor(AVirtualSensorActorBase* Sensor,FString& Reason);
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorLayout") bool OwnsSensor(AVirtualSensorActorBase* Sensor) const;
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorLayout") int64 GetRevision() const {return Revision;}
    UPROPERTY(BlueprintAssignable) FVirtualSensorLayoutChanged OnLayoutChanged;
    static FString CanonicalActorPath(const AActor* Actor);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorLayout") FString RequestSaveLayout(const FString& Name,bool bOverwrite=false);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorLayout") FString PreviewLayoutRestore(const FString& Name);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorLayout") bool ApplyLayoutRestore(const FString& RequestId,FString& Reason);
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorLayout") FVirtualSensorLayoutOperationStatus GetOperationStatus(const FString& Id) const;
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorLayout") TArray<FString> GetSavedLayoutNames() const;
    bool BuildLayoutSnapshot(FVirtualSensorLayoutDocument& Document,FString& Reason) const;
#if WITH_DEV_AUTOMATION_TESTS
    FString StorageRootOverrideForTests;
#endif
private:
    TMap<TWeakObjectPtr<AVirtualSensorActorBase>,FString> Owned;
    int64 Revision=0;
    bool bEnding=false;
    AVirtualSensorActorBase* SpawnStopped(EVirtualSensorKind Kind,const FTransform& Pose,FString& Reason);
    bool ViewPose(FTransform& Pose,FString& Reason) const;
    void Changed();
    struct FWriteResult {bool Success=false;FString Error;};
    struct FPendingWrite {FString Id;TFuture<FWriteResult> Future;};
    TUniquePtr<FPendingWrite> PendingWrite;
    TMap<FString,FVirtualSensorLayoutOperationStatus> Operations;
    TArray<FString> OperationOrder;
    FVirtualSensorLayoutDocument RestoreDocument;
    FString RestoreId;
    int64 RestoreInventoryRevision=0;
    TMap<TWeakObjectPtr<AVirtualSensorActorBase>,FTransform> RestorePoses;
    TMap<TWeakObjectPtr<AVirtualSensorActorBase>,int32> RestoreRevisions;
    FString Directory() const;
    FString LayoutPath(const FString& Name) const;
    FString AddOperation(EVirtualSensorLayoutOperation State,const FString& Message,const FString& Path=FString());
    bool ValidateDocument(const FVirtualSensorLayoutDocument& Document,TArray<AVirtualSensorActorBase*>& Matches,TArray<AVirtualSensorActorBase*>& Excess,FString& Reason) const;
    static FWriteResult WriteDocument(const FString& Path,const FString& Json);
    void FinishWrite(FWriteResult Result);
};
