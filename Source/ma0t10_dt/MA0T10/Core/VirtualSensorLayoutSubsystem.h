#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
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
private:
    TMap<TWeakObjectPtr<AVirtualSensorActorBase>,FString> Owned;
    int64 Revision=0;
    bool bEnding=false;
    AVirtualSensorActorBase* SpawnStopped(EVirtualSensorKind Kind,const FTransform& Pose,FString& Reason);
    bool ViewPose(FTransform& Pose,FString& Reason) const;
    void Changed();
};
