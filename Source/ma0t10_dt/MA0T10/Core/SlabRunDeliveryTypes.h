#pragma once
#include "CoreMinimal.h"
#include "VirtualSlabFrameContext.h"
#include "SlabRunDeliveryTypes.generated.h"

UENUM(BlueprintType)
enum class ESlabRunDeliveryOutcome : uint8 { Pending, PreparationFailed, UserStopped, Partial, BrokerAccepted, ObservationCompleted };
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabRunSensorSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString SensorId;
    UPROPERTY(BlueprintReadOnly) FString Configuration;
    UPROPERTY(BlueprintReadOnly) int32 SettingsRevision=0;
    UPROPERTY(BlueprintReadOnly) float RequestedHz=0;
    UPROPERTY(BlueprintReadOnly) float MeasuredHz=0;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabRunDeliverySummary
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString RunUUID;
    UPROPERTY(BlueprintReadOnly) FString ScenarioUUID;
    UPROPERTY(BlueprintReadOnly) FString MtlNo;
    UPROPERTY(BlueprintReadOnly) ESlabExecutionPolicy Policy=ESlabExecutionPolicy::ObservationAllowed;
    UPROPERTY(BlueprintReadOnly) FVirtualSlabSensorOutputSelection Outputs;
    UPROPERTY(BlueprintReadOnly) FVirtualSlabSensorOutputSelection AppliedOutputs=FVirtualSlabSensorOutputSelection::ObservationOnly();
    UPROPERTY(BlueprintReadOnly) TArray<FSlabRunSensorSnapshot> Sensors;
    UPROPERTY(BlueprintReadOnly) FDateTime PreparedUtc;
    UPROPERTY(BlueprintReadOnly) FDateTime StartedUtc;
    UPROPERTY(BlueprintReadOnly) FDateTime StoppedUtc;
    UPROPERTY(BlueprintReadOnly) FDateTime FinishedUtc;
    UPROPERTY(BlueprintReadOnly) int64 LastSlabFrame=-1;
    UPROPERTY(BlueprintReadOnly) double ElapsedSec=0;
    UPROPERTY(BlueprintReadOnly) int64 Acquired=0;
    UPROPERTY(BlueprintReadOnly) int64 AcquisitionFailures=0;
    UPROPERTY(BlueprintReadOnly) int64 Accepted=0;
    UPROPERTY(BlueprintReadOnly) int64 Submitted=0;
    UPROPERTY(BlueprintReadOnly) int64 Receipts=0;
    UPROPERTY(BlueprintReadOnly) int64 DeliveryFailures=0;
    UPROPERTY(BlueprintReadOnly) int64 Unfinished=0;
    UPROPERTY(BlueprintReadOnly) int64 ConsumerValidated=0;
    UPROPERTY(BlueprintReadOnly) int64 ConsumerInvalid=0;
    UPROPERTY(BlueprintReadOnly) ESlabRunDeliveryOutcome Outcome=ESlabRunDeliveryOutcome::Pending;
    UPROPERTY(BlueprintReadOnly) FString Reason;
    UPROPERTY(BlueprintReadOnly) FString Warning;
    UPROPERTY(BlueprintReadOnly) FString ReportPath;
    UPROPERTY(BlueprintReadOnly) FString SaveState;
    UPROPERTY(BlueprintReadOnly) bool bFinalized=false;
};
