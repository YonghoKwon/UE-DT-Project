#pragma once
#include "CoreMinimal.h"
#include "SensorNativeSafetyStatus.h"

namespace TrainBatchWorkerSafety
{
struct FExitEvidence
{
	bool CoordinatorTerminated = false, CancelRequested = false, NativeFault = false;
	SensorNativeSafety::EStatus Status = SensorNativeSafety::EStatus::None;
};
MA0T10_DT_API bool CountsNativeFailure(const FExitEvidence& Evidence);
MA0T10_DT_API bool GuardBanned(const FString& WorkersRoot, int32 Slot);
MA0T10_DT_API bool GuardFaulted(const FString& WorkersRoot, int32 Slot);
/** Only a dead, exactly owned controlled exit without fault evidence may drop its live marker. */
MA0T10_DT_API bool CleanupControlledMarker(const FString& WorkersRoot, int32 Slot, uint32 Pid, uint64 Stamp, const FExitEvidence& Evidence);
MA0T10_DT_API FString GuardStatePath(const FString& WorkersRoot, int32 Slot);
}
