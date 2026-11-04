#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "ma0t10_dt/MA0T10/Contracts/TrainBatchContract.h"
class UWorld;
class AActor;

namespace TrainBatchProcess
{
enum class ETask : uint8
{
	Pending,
	Leased,
	Completed,
	Failed
};
struct FTask
{
	ETask State = ETask::Pending;
	int32 Index = 0, Attempts = 0, Slot = INDEX_NONE;
	FString Lease;
	uint64 Epoch = 0;
	double LastSeen = 0;
	FString Error;
};
/** Deterministic, scenario-granular leases. Time comes from the coordinator monotonic clock. */
class MA0T10_DT_API FQueue
{
public:
	TArray<FTask> Tasks;
	void Initialize(int32 Count);
	FTask* LeaseNext(int32 Slot, double Now);
	bool Matches(int32 Index, const FString& Lease, uint64 Epoch) const;
	bool Heartbeat(int32 Index, const FString& Lease, uint64 Epoch, double Now);
	bool Complete(int32 Index, const FString& Lease, uint64 Epoch);
	bool Retry(int32 Index, const FString& Lease, uint64 Epoch, const FString& Why, bool CountFailure = true);
	bool Terminal() const;
	TSharedRef<FJsonObject> Json() const;
	bool Restore(const TSharedPtr<FJsonObject>& Json, int32 Count, FString& Error);
};
struct FGpuBudget
{
	uint64 Budget = 0, Usage = 0, Dedicated = 0;
	int32 AdapterIndex = INDEX_NONE;
	FString Adapter, Driver, Identity;
};
MA0T10_DT_API bool QueryGpuBudget(FGpuBudget& Out);
MA0T10_DT_API int32 Capacity(int32 Requested, int32 Items, uint64 Budget, uint64 Usage, uint64 WorkerPeak);
MA0T10_DT_API uint64 ProcessStamp(uint32 Pid, bool& Alive);
MA0T10_DT_API bool SameProcess(uint32 Pid, uint64 Stamp);
MA0T10_DT_API FString Encode(const TSharedRef<FJsonObject>& Json);
MA0T10_DT_API TSharedPtr<FJsonObject> Read(const FString& Path, int64 Limit = 1024 * 1024);
MA0T10_DT_API bool Write(const FString& Root, const FString& Path, const TSharedRef<FJsonObject>& Json);
MA0T10_DT_API FString Quote(const FString& Value);
/** Cooked game executables take a map, not an Editor .uproject argument. */
MA0T10_DT_API FString WorkerMapArguments(const FString& Project, const FString& Map, bool EditorExecutable);
enum class EMapEntry : uint8
{
	Ready,
	Travelling,
	Rejected
};
MA0T10_DT_API bool IsProjectMap(const FString& Map);
/** Shipping ignores positional startup maps. Only explicit workers/probes call this. */
MA0T10_DT_API EMapEntry EnterAssignedMap(UWorld* World, const FString& Map, FString& Error);
MA0T10_DT_API FString SceneSignature(UWorld* World, const AActor* Slab, const AActor* Lidar, const AActor* Camera);
}
