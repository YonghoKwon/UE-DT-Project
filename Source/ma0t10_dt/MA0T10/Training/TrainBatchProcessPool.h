#pragma once
#include "ma0t10_dt/MA0T10/Training/TrainBatchProcessProtocol.h"
#include "HAL/PlatformProcess.h"

struct FTrainBatchPoolEvent
{
	int32 Index = INDEX_NONE, Slot = INDEX_NONE;
	FString Lease, RunId, WorkerRoot, Phase, Error;
	uint64 Epoch = 0;
};
/** App-side process supervisor. One isolated game per leased scenario; slots keep stable UserDirs. */
class MA0T10_DT_API FTrainBatchProcessPool
{
public:
	bool Start(const FTrainBatchJobView& Job, const TSharedRef<FJsonObject>& Preset, int32 Requested, FString& Error);
	void Tick(bool Pause);
	void Cancel();
	bool Pop(FTrainBatchPoolEvent& Event);
	void Acknowledge(const FTrainBatchPoolEvent& Event, bool ValidResult);
	bool IsDone() const;
	FString Status() const;
	int32 ActualWorkers() const { return MaxObserved; }
	const FString& Error() const { return Failure; }
	~FTrainBatchProcessPool();

private:
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
	friend class FTrainBatchPoolExitEvidenceTest;
#endif
	struct FSlot
	{
		FProcHandle Process;
		uint32 Pid = 0;
		uint64 Stamp = 0, Sequence = 0;
		int32 Task = INDEX_NONE;
		FString Ipc, Lease;
		uint64 Epoch = 0;
		bool Reported = false, CoordinatorTerminated = false, CancelRequested = false, NativeFault = false;
		double Launched = 0;
	};
	FTrainBatchJobSpec Job;
	TSharedPtr<FJsonObject> Preset;
	TrainBatchProcess::FQueue Queue;
	TArray<FSlot> Slots;
	TArray<FTrainBatchPoolEvent> Events;
	TUniquePtr<FArchive> OwnerLease;
	FString Root, WorkerBase, Failure, Identity;
	uint64 OwnerStamp = 0, Peak = 0, LastBudget = 0, LastUsage = 0;
	int32 Requested = 1, Allowed = 1, MaxObserved = 0, NativeCrashes = 0, AdapterIndex = INDEX_NONE;
	bool Stopping = false, BanNative = false, LastPause = false, Calibrated = false;
	double NextPoll = 0, CancelAt = 0;
	TArray<FString> NativeFailureLeases;
	bool Acquire(FString& Error);
	bool Save();
	bool SaveSafety();
	bool Launch(int32 Slot);
	void Control(FSlot& Slot, bool Pause, bool Cancel);
};
