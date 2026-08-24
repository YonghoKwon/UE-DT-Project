#pragma once

#include "CoreMinimal.h"
#include "VirtualSensorCadence.generated.h"

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorCadenceTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") float TargetHz = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") int64 ScheduledAcquisitionCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") int64 DeadlineMissCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") int64 LastScheduledUnixNanoseconds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") int64 LastAcquisitionStartUnixNanoseconds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") int64 LastAcquisitionEndUnixNanoseconds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") float LastStartJitterMs = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") float StartJitterP95Ms = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|VirtualSensor|Cadence") float IntervalErrorP95Ms = 0.0f;
};

struct MA0T10_DT_API FVirtualSensorCadenceDeadline
{
	double ScheduledMonotonicSeconds = 0.0;
	int64 ScheduledUnixNanoseconds = 0;
	int64 ActualStartUnixNanoseconds = 0;
	int32 MissedDeadlines = 0;
};

/** Fixed-period monotonic deadline state shared by virtual Camera and LiDAR acquisition. */
struct MA0T10_DT_API FVirtualSensorCadenceState
{
	void Start(double NowMonotonicSeconds, int64 NowUnixNanoseconds, double IntervalSeconds, double PhaseSeconds = 0.0);
	void Stop();
	void Resume(double NowMonotonicSeconds, int64 NowUnixNanoseconds);
	void ForceDue(double NowMonotonicSeconds, int64 NowUnixNanoseconds);
	bool IsRunning() const { return NextDeadlineMonotonicSeconds >= 0.0; }
	bool IsDue(double NowMonotonicSeconds) const;
	bool ConsumeDeadline(double NowMonotonicSeconds, int64 ActualStartUnixNanoseconds, FVirtualSensorCadenceDeadline& OutDeadline);
	void MarkAcquisitionEnd(int64 AcquisitionEndUnixNanoseconds);
	double GetNextDeadlineMonotonicSeconds() const { return NextDeadlineMonotonicSeconds; }
	double GetIntervalSeconds() const { return IntervalSeconds; }
	const FVirtualSensorCadenceTelemetry& GetTelemetry() const { return Telemetry; }

private:
	void RefreshPercentiles();
	int64 MonotonicToUnixNanoseconds(double MonotonicSeconds) const;

	double IntervalSeconds = 0.05;
	double NextDeadlineMonotonicSeconds = -1.0;
	double AnchorMonotonicSeconds = 0.0;
	int64 AnchorUnixNanoseconds = 0;
	int64 LastActualStartUnixNanoseconds = 0;
	TArray<float> StartJitterSamplesMs;
	TArray<float> IntervalErrorSamplesMs;
	FVirtualSensorCadenceTelemetry Telemetry;
};
