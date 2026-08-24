#include "ma0t10_dt/MA0T10/Core/VirtualSensorCadence.h"

namespace
{
constexpr int32 MaxCadenceSamples = 512;

float Percentile95(TArray<float> Values)
{
	if (Values.IsEmpty()) return 0.0f;
	Values.Sort();
	return Values[FMath::Clamp(FMath::CeilToInt(Values.Num() * 0.95f) - 1, 0, Values.Num() - 1)];
}
}

void FVirtualSensorCadenceState::Start(
	double NowMonotonicSeconds,
	int64 NowUnixNanoseconds,
	double InIntervalSeconds,
	double PhaseSeconds)
{
	IntervalSeconds = FMath::Max(0.001, InIntervalSeconds);
	AnchorMonotonicSeconds = NowMonotonicSeconds;
	AnchorUnixNanoseconds = NowUnixNanoseconds;
	NextDeadlineMonotonicSeconds = NowMonotonicSeconds + FMath::Fmod(FMath::Max(0.0, PhaseSeconds), IntervalSeconds);
	LastActualStartUnixNanoseconds = 0;
	StartJitterSamplesMs.Reset();
	IntervalErrorSamplesMs.Reset();
	Telemetry = FVirtualSensorCadenceTelemetry();
	Telemetry.TargetHz = static_cast<float>(1.0 / IntervalSeconds);
}

void FVirtualSensorCadenceState::Stop()
{
	NextDeadlineMonotonicSeconds = -1.0;
}

void FVirtualSensorCadenceState::Resume(double NowMonotonicSeconds, int64 NowUnixNanoseconds)
{
	if (!IsRunning()) return;
	AnchorMonotonicSeconds = NowMonotonicSeconds;
	AnchorUnixNanoseconds = NowUnixNanoseconds;
	NextDeadlineMonotonicSeconds = NowMonotonicSeconds + IntervalSeconds;
	LastActualStartUnixNanoseconds = 0;
}

bool FVirtualSensorCadenceState::IsDue(double NowMonotonicSeconds) const
{
	return IsRunning() && NowMonotonicSeconds + UE_DOUBLE_SMALL_NUMBER >= NextDeadlineMonotonicSeconds;
}

bool FVirtualSensorCadenceState::ConsumeDeadline(
	double NowMonotonicSeconds,
	int64 ActualStartUnixNanoseconds,
	FVirtualSensorCadenceDeadline& OutDeadline)
{
	if (!IsDue(NowMonotonicSeconds)) return false;
	const double OverdueSeconds = FMath::Max(0.0, NowMonotonicSeconds - NextDeadlineMonotonicSeconds);
	const int32 Missed = FMath::Max(0, FMath::FloorToInt(OverdueSeconds / IntervalSeconds));
	const double EffectiveDeadline = NextDeadlineMonotonicSeconds + Missed * IntervalSeconds;
	NextDeadlineMonotonicSeconds = EffectiveDeadline + IntervalSeconds;

	OutDeadline.ScheduledMonotonicSeconds = EffectiveDeadline;
	OutDeadline.ScheduledUnixNanoseconds = MonotonicToUnixNanoseconds(EffectiveDeadline);
	OutDeadline.ActualStartUnixNanoseconds = ActualStartUnixNanoseconds;
	OutDeadline.MissedDeadlines = Missed;

	++Telemetry.ScheduledAcquisitionCount;
	Telemetry.DeadlineMissCount += Missed;
	Telemetry.LastScheduledUnixNanoseconds = OutDeadline.ScheduledUnixNanoseconds;
	Telemetry.LastAcquisitionStartUnixNanoseconds = ActualStartUnixNanoseconds;
	Telemetry.LastStartJitterMs = static_cast<float>((NowMonotonicSeconds - EffectiveDeadline) * 1000.0);
	StartJitterSamplesMs.Add(FMath::Abs(Telemetry.LastStartJitterMs));
	if (LastActualStartUnixNanoseconds > 0)
	{
		const double ActualIntervalSeconds = (ActualStartUnixNanoseconds - LastActualStartUnixNanoseconds) / 1.0e9;
		IntervalErrorSamplesMs.Add(static_cast<float>(FMath::Abs(ActualIntervalSeconds - IntervalSeconds) * 1000.0));
	}
	LastActualStartUnixNanoseconds = ActualStartUnixNanoseconds;
	if (StartJitterSamplesMs.Num() > MaxCadenceSamples) StartJitterSamplesMs.RemoveAt(0, StartJitterSamplesMs.Num() - MaxCadenceSamples, false);
	if (IntervalErrorSamplesMs.Num() > MaxCadenceSamples) IntervalErrorSamplesMs.RemoveAt(0, IntervalErrorSamplesMs.Num() - MaxCadenceSamples, false);
	RefreshPercentiles();
	return true;
}

void FVirtualSensorCadenceState::MarkAcquisitionEnd(int64 AcquisitionEndUnixNanoseconds)
{
	Telemetry.LastAcquisitionEndUnixNanoseconds = AcquisitionEndUnixNanoseconds;
}

void FVirtualSensorCadenceState::RefreshPercentiles()
{
	Telemetry.StartJitterP95Ms = Percentile95(StartJitterSamplesMs);
	Telemetry.IntervalErrorP95Ms = Percentile95(IntervalErrorSamplesMs);
}

int64 FVirtualSensorCadenceState::MonotonicToUnixNanoseconds(double MonotonicSeconds) const
{
	return AnchorUnixNanoseconds + static_cast<int64>((MonotonicSeconds - AnchorMonotonicSeconds) * 1.0e9);
}
