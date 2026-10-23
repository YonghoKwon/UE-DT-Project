#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSlabContextCompatibility.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorOutputPreflight.generated.h"
UENUM(BlueprintType)
enum class EVirtualSensorPreflightPurpose:uint8 { LegacyUnion UMETA(Hidden), ManualStreamStart, ScenarioTransition };
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
    UPROPERTY(BlueprintReadOnly) EVirtualSensorStreamTransportBackend TransportBackend=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;
    UPROPERTY(BlueprintReadOnly) FString Topic;
    UPROPERTY(BlueprintReadOnly) bool bPlannedOutput=false;
    UPROPERTY(BlueprintReadOnly) bool bTransportConnected=false;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorOutputPreflightResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FVirtualSensorOutputPreflightStream> Streams;
    UPROPERTY(BlueprintReadOnly) TArray<FString> BlockingReasons;
    UPROPERTY(BlueprintReadOnly) TArray<FString> Warnings;
    UPROPERTY(BlueprintReadOnly) double MessagesPerSecond=0;
    UPROPERTY(BlueprintReadOnly) double PcdMessagesPerSecond=0;
    UPROPERTY(BlueprintReadOnly) double PcdMegabytesPerSecond=0;
    UPROPERTY(BlueprintReadOnly) double TheoreticalSubmissionHz=0;
    UPROPERTY(BlueprintReadOnly) double FpsUsed=60;
    UPROPERTY(BlueprintReadOnly) bool bAssumedFps=true;
    UPROPERTY(BlueprintReadOnly) bool bTransportConnected=false;
    UPROPERTY(BlueprintReadOnly) bool bRequiresRaw=false;
    UPROPERTY(BlueprintReadOnly) bool bRequiresEngine=false;
    UPROPERTY(BlueprintReadOnly) bool bRawConnected=false;
    UPROPERTY(BlueprintReadOnly) bool bEngineConnected=false;
    UPROPERTY(BlueprintReadOnly) bool bSessionBusy=false;
    UPROPERTY(BlueprintReadOnly) int32 SubmissionsPerFrame=2;
    UPROPERTY(BlueprintReadOnly) double PcdBudgetMegabytesPerSecond=VirtualSensorDefaults::PointCloudBandwidthMiBPerSecond;
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
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorOutput",meta=(WorldContext="Context"))
    static FVirtualSensorOutputPreflightResult InspectForPurpose(const UObject* Context,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& TargetSensorIds,EVirtualSensorPreflightPurpose Purpose,double ObservedFps=0);
    static FVirtualSensorOutputPreflightResult Evaluate(FVirtualSensorOutputPreflightResult Snapshot,int64 MaxMessageBytes=8388608);
    /** Same inspection/decision, without formatting hidden detail text. Not a new output policy. */
    static FVirtualSensorOutputPreflightResult InspectForPresentation(const UObject* Context,const FVirtualSlabSensorOutputSelection& Outputs,const TArray<FString>& TargetSensorIds,EVirtualSensorPreflightPurpose Purpose,bool bBuildDetails,double ObservedFps=0,bool bRequireAllPcdFrames=false);
    static FString FormatDetails(const FVirtualSensorOutputPreflightResult& Snapshot);
    /** Shared pre-start contract: required-data PCD covers every acquisition. */
    static FString RequiredPcdDivisorError(const FString& SensorId,int32 Divisor);
};
