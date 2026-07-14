#pragma once

#include "CoreMinimal.h"
#include "RealSensorSourceComp.h"
#include "VirtualLidarSensorTypes.h"
#include "LidarCsvReplaySourceComp.generated.h"

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API ULidarCsvReplaySourceComp : public URealSensorSourceComp
{
    GENERATED_BODY()

public:
    ULidarCsvReplaySourceComp();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorReplay")
    bool LoadCsvFrame(TArray<FVirtualLidarPoint>& OutPoints, int32& OutRows, int32& OutCols) const;

    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorReplay")
    void PushFrameOnceInEditor();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorReplay")
    void PushFrameOnceNoTransportInEditor();

    virtual bool StartSource() override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorReplay")
    void StartReplay();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorReplay")
    void StartReplayInEditor();

    virtual void StopSource() override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorReplay")
    void StopReplay();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorReplay")
    void StopReplayInEditor();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorReplay")
    bool IsReplayActive() const { return bReplayActive; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (FilePathFilter = "csv", RelativeToGameDir))
    FString CsvFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (ClampMin = "0.033"))
    float ReplayInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    bool bUpdateLidarDimensionsFromCsv = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    bool bTreatCsvCoordinatesAsWorldSpace = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    bool bSkipZeroVectorPoints = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (ClampMin = "0.001"))
    float CoordinateScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (ClampMin = "1", ClampMax = "1000"))
    int32 PointStride = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay", meta = (ClampMin = "0", ClampMax = "10000000"))
    int32 MaxPointsToLoad = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorReplay")
    FName ReplaySemanticLabel = TEXT("Slab");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorReplay|Status")
    int32 LastLoadedPointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorReplay|Status")
    FString LastResolvedCsvPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorReplay|Status")
    FString LastReplayMessage;

private:
    FString ResolveCsvFilePath() const;
    bool ParseCsvPointLine(const FString& Line, int32 LineIndex, int32& OutRow, int32& OutCol, bool& bOutHasGridCoord, FVector& OutPoint) const;
    void PushFrameFromTimer();

private:
    bool bReplayActive = false;
    FTimerHandle ReplayTimerHandle;
};
