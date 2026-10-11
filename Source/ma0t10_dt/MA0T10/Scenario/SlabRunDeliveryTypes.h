#pragma once
#include "CoreMinimal.h"
#include "VirtualSlabFrameContext.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementTypes.h"
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
    UPROPERTY(BlueprintReadOnly) bool bHasAcquisitionCounts=false;
    UPROPERTY(BlueprintReadOnly) int64 Acquired=0;
    UPROPERTY(BlueprintReadOnly) int64 AcquisitionFailures=0;
    UPROPERTY(BlueprintReadOnly) bool bHasCompatibilityJsonRate=false;
    UPROPERTY(BlueprintReadOnly) float CompatibilityJsonHz=0;
    UPROPERTY(BlueprintReadOnly) FVirtualSensorPlacementSnapshot Placement;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabRunStreamDeliverySummary
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString SensorId;
    UPROPERTY(BlueprintReadOnly) EVirtualSensorStreamKind Kind=EVirtualSensorStreamKind::PointCloud;
    UPROPERTY(BlueprintReadOnly) int64 Accepted=0;
    UPROPERTY(BlueprintReadOnly) int64 Submitted=0;
    UPROPERTY(BlueprintReadOnly) int64 Receipts=0;
    UPROPERTY(BlueprintReadOnly) int64 Failures=0;
    UPROPERTY(BlueprintReadOnly) int64 AcceptedUnconfirmed=0;
    UPROPERTY(BlueprintReadOnly) int64 ConsumerValidated=0;
    UPROPERTY(BlueprintReadOnly) int64 ConsumerInvalid=0;
    UPROPERTY(BlueprintReadOnly) bool bHasSubmittedRate=false;
    UPROPERTY(BlueprintReadOnly) bool bHasReceiptRate=false;
    UPROPERTY(BlueprintReadOnly) bool bHasConsumerRate=false;
    UPROPERTY(BlueprintReadOnly) double SubmittedHz=0;
    UPROPERTY(BlueprintReadOnly) double ReceiptHz=0;
    UPROPERTY(BlueprintReadOnly) double ConsumerValidatedHz=0;
    double FirstSubmitted=0,LastSubmitted=0,FirstReceipt=0,LastReceipt=0,FirstConsumer=0,LastConsumer=0;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabRunTerminalSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDateTime CapturedUtc;
    UPROPERTY(BlueprintReadOnly) int64 PendingAcquisitions=0;
    UPROPERTY(BlueprintReadOnly) int64 PendingOutputPreparation=0;
    UPROPERTY(BlueprintReadOnly) int64 QueuedForSocket=0;
    UPROPERTY(BlueprintReadOnly) int64 SocketWritesInFlight=0;
    UPROPERTY(BlueprintReadOnly) int64 AwaitingBrokerReceipt=0;
    UPROPERTY(BlueprintReadOnly) int64 AcceptedUnconfirmed=0;
    UPROPERTY(BlueprintReadOnly) int64 UniqueUnfinished=0;
    UPROPERTY(BlueprintReadOnly) int32 PendingTransportEvents=0;
};
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabRunDeliverySummary
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString RunUUID;
    UPROPERTY(BlueprintReadOnly) FString ScenarioUUID;
    UPROPERTY(BlueprintReadOnly) FString ScenarioContentHash;
    UPROPERTY(BlueprintReadOnly) FString MtlNo;
    UPROPERTY(BlueprintReadOnly) ESlabExecutionPolicy Policy=ESlabExecutionPolicy::ObservationAllowed;
    UPROPERTY(BlueprintReadOnly) FVirtualSlabSensorOutputSelection Outputs;
    UPROPERTY(BlueprintReadOnly) FVirtualSlabSensorOutputSelection AppliedOutputs=FVirtualSlabSensorOutputSelection::ObservationOnly();
    UPROPERTY(BlueprintReadOnly) TArray<FSlabRunSensorSnapshot> Sensors;
    UPROPERTY(BlueprintReadOnly) bool bHasStreamDetails=false;
    UPROPERTY(BlueprintReadOnly) TArray<FSlabRunStreamDeliverySummary> StreamDetails;
    UPROPERTY(BlueprintReadOnly) int64 UnattributedAcquired=0;
    UPROPERTY(BlueprintReadOnly) int64 UnattributedAcquisitionFailures=0;
    UPROPERTY(BlueprintReadOnly) int64 UnattributedDeliveryFailures=0;
    UPROPERTY(BlueprintReadOnly) FDateTime PreparedUtc;
    UPROPERTY(BlueprintReadOnly) FDateTime StartedUtc;
    UPROPERTY(BlueprintReadOnly) FDateTime StoppedUtc;
    UPROPERTY(BlueprintReadOnly) FDateTime FinishedUtc;
    UPROPERTY(BlueprintReadOnly) FSlabRunTerminalSnapshot TerminalSnapshot;
    UPROPERTY(BlueprintReadOnly) FDateTime ConsumerSnapshotUtc;
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
