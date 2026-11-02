#pragma once
#include "CoreMinimal.h"

// Opt-in package experiment sidecar. Never changes CVars or allocator defaults.
bool BeginHatchMemoryExperiment(const FString& Directory);
bool SampleHatchMemoryExperiment(double ElapsedSeconds);
bool FinishHatchMemoryExperiment(bool Complete);
