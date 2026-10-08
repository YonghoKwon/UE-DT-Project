#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VirtualSlabFrameContext.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorOutputPreflight.generated.h"
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorOutputPreflightStream
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString SensorId;
    UPROPERTY(BlueprintReadOnly) EVirtualSensorStreamKind Kind=EVirtualSensorStreamKind::PointCloud;
    UPROPERTY(BlueprintReadOnly) double RequestedHz=0;
    UPROPERTY(BlueprintReadOnly) int64 EstimatedBytesPerFrame=0;
    UPROPERTY(BlueprintReadOnly) int64 ObservedBytesPerFrame=0;
    UPROPERTY(BlueprintReadOnly) int32 ConfigurationRevision=0;
    UPROPERTY(BlueprintReadOnly) FString Specification;
    UPROPERTY(BlueprintReadOnly) bool bEstimateUncertain=false;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorOutputPreflightResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FVirtualSensorOutputPreflightStream> Streams;
    UPROPERTY(BlueprintReadOnly) TArray<FString> BlockingReasons;
    UPROPERTY(BlueprintReadOnly) TArray<FString> Warnings;
    UPROPERTY(BlueprintReadOnly) double MessagesPerSecond=0;
    UPROPERTY(BlueprintReadOnly) double PcdMegabytesPerSecond=0;
    UPROPERTY(BlueprintReadOnly) double TheoreticalSubmissionHz=0;
    UPROPERTY(BlueprintReadOnly) double FpsUsed=60;
    UPROPERTY(BlueprintReadOnly) bool bAssumedFps=true;
    UPROPERTY(BlueprintReadOnly) bool bTransportConnected=false;
    UPROPERTY(BlueprintReadOnly) bool bSessionBusy=false;
    UPROPERTY(BlueprintReadOnly) int32 SubmissionsPerFrame=2;
    UPROPERTY(BlueprintReadOnly) double PcdBudgetMegabytesPerSecond=64;
    UPROPERTY(BlueprintReadOnly) FString Summary;
};
/** Read-only capture followed by a pure capacity evaluator. No connection or measurement is started. */
UCLASS()
class MA0T10_DT_API UVirtualSensorOutputPreflight : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorOutput",meta=(WorldContext="Context"))
    static FVirtualSensorOutputPreflightResult Inspect(const UObject* Context,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& TargetSensorIds,double ObservedFps=0);
    static FVirtualSensorOutputPreflightResult Evaluate(FVirtualSensorOutputPreflightResult Snapshot,int64 MaxMessageBytes=8388608);
};
