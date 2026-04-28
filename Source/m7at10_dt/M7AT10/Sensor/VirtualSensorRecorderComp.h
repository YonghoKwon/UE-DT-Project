#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualSensorRecorderComp.generated.h"

USTRUCT(BlueprintType)
struct M7AT10_DT_API FVirtualSensorRecordedFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorRecorder")
    FString SensorId;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorRecorder")
    FString SensorType;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorRecorder")
    int64 FrameId = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorRecorder")
    FDateTime TimestampUtc;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorRecorder")
    FString PayloadJson;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVirtualSensorReplayFrame, const FVirtualSensorRecordedFrame&, Frame);

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualSensorRecorderComp : public UActorComponent
{
    GENERATED_BODY()

public:
    UVirtualSensorRecorderComp();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorRecorder")
    void StartRecording();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorRecorder")
    void StopRecording();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorRecorder")
    void ClearRecording();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorRecorder")
    void RecordJsonFrame(const FString& SensorId, const FString& SensorType, int64 FrameId, const FString& PayloadJson);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorRecorder")
    bool SaveSession(const FString& FileNamePrefix = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorRecorder")
    bool LoadSessionFromFile(const FString& AbsoluteFilePath);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorRecorder")
    void StartReplay(float FramesPerSecond = 10.0f);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorRecorder")
    void StopReplay();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorRecorder")
    bool IsRecording() const { return bRecording; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorRecorder")
    int32 GetRecordedFrameCount() const { return RecordedFrames.Num(); }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|SensorRecorder")
    FOnVirtualSensorReplayFrame OnReplayFrame;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorRecorder")
    FString SaveSubDirectory = TEXT("SensorRecordings");

private:
    void ReplayNextFrame();
    FString BuildSessionPath(const FString& FileNamePrefix) const;

private:
    UPROPERTY(Transient)
    TArray<FVirtualSensorRecordedFrame> RecordedFrames;

    bool bRecording = false;
    int32 ReplayFrameIndex = 0;
    FTimerHandle ReplayTimerHandle;
};
