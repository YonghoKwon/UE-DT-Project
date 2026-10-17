#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualImuTypes.h"
#include "VirtualImuComponent.generated.h"
class UVirtualSensorRecorderComponent;
class UVirtualSensorStreamPublisherComponent;

/** Explicitly attached only. No capture, scan, global subscription or default sensor creation. */
UCLASS(ClassGroup=(DigitalTwin),meta=(BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualImuComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UVirtualImuComponent();
    static UVirtualImuComponent* Attach(AActor* Actor);
    /** Native lifecycle path; keeps the source alive until its CSV is finalized. */
    bool RequestDetach(FString& Message);
    bool IsDetaching() const { return Detaching; }
    const FString& GetDetachStatus() const { return DetachStatus; }
    const FString& GetDetachCsvRequest() const { return DetachCsvRequest; }
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|IMU") bool Configure(const FVirtualImuConfig& Value,FString& Error);
    UFUNCTION(BlueprintPure,Category="DigitalTwin|IMU") FVirtualImuConfig GetConfig() const {return Config;}
    UFUNCTION(BlueprintPure,Category="DigitalTwin|IMU") FVirtualImuSample GetLatestSample() const {return Latest;}
    UFUNCTION(BlueprintPure,Category="DigitalTwin|IMU") bool HasSample() const {return HasLatest;}
    UFUNCTION(BlueprintPure,Category="DigitalTwin|IMU") FString GetImuId() const;
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|IMU") void SetRecording(bool Enabled);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|IMU") void SetPublishing(bool Enabled);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|IMU") FString SaveCsv();
    UFUNCTION(BlueprintPure,Category="DigitalTwin|IMU") bool IsRecording() const {return Recording;}
    UFUNCTION(BlueprintPure,Category="DigitalTwin|IMU") bool IsPublishing() const;
    TSharedRef<class FJsonObject> StatusJson() const;
    static TSharedRef<class FJsonObject> BatchJson(const FString& Id,const FVirtualImuConfig& Config,const TArray<FVirtualImuSample>& Samples);
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DigitalTwin|IMU") TObjectPtr<UVirtualSensorRecorderComponent> Recorder;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DigitalTwin|IMU") TObjectPtr<UVirtualSensorStreamPublisherComponent> Publisher;
    int32 Generation=0;
    int64 SampleCount=0;
    int64 BatchCount=0;
private:
    FVirtualImuConfig Config;
    FVirtualImuModel Model;
    TArray<FVirtualImuSample> TickSamples;
    FVirtualImuSample Latest;
    bool Initialized=false,HasLatest=false,Recording=false;
    bool Detaching=false;
    FString DetachCsvRequest,DetachStatus;
    void FinishDetach();
    void ResolveServices();
};
