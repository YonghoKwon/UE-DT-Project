#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualLidarSensorTypes.h"
#include "LidarJsonLinesReplaySourceComp.generated.h"

class UVirtualLidarSensorComp;

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API ULidarJsonLinesReplaySourceComp : public USceneComponent
{
    GENERATED_BODY()

public:
    ULidarJsonLinesReplaySourceComp();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorReplay")
    bool LoadJsonLinesFrame(TArray<FVirtualLidarPoint>& OutPoints) const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorReplay")
    bool PushFrameOnce(bool bSendTransport = true);

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorReplay")
    void PushFrameOnceInEditor();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorReplay")
    void PushFrameOnceNoTransportInEditor();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorReplay")
    void StartReplay();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorReplay")
    void StartReplayInEditor();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorReplay")
    void StopReplay();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorReplay")
    void StopReplayInEditor();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorReplay")
    bool IsReplayActive() const { return bReplayActive; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    TObjectPtr<UVirtualLidarSensorComp> TargetLidar;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (FilePathFilter = "jsonl", RelativeToGameDir))
    FString JsonLinesFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    bool bAutoStartReplay = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (ClampMin = "0.033"))
    float ReplayInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    bool bSendTransportOnReplay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    bool bSkipMissPoints = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (ClampMin = "1", ClampMax = "1000"))
    int32 PointStride = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (ClampMin = "0", ClampMax = "10000000"))
    int32 MaxPointsToLoad = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    FName DefaultSemanticLabel = TEXT("Unknown");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorReplay|Status")
    int32 LastLoadedPointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorReplay|Status")
    FString LastResolvedJsonLinesPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorReplay|Status")
    FString LastReplayMessage;

private:
    UVirtualLidarSensorComp* ResolveTargetLidar() const;
    FString ResolveJsonLinesFilePath() const;
    bool ParseJsonPointLine(const FString& Line, FVirtualLidarPoint& OutPoint) const;
    void PushFrameFromTimer();

private:
    bool bReplayActive = false;
    FTimerHandle ReplayTimerHandle;
};
