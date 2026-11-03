#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioTypes.h"

/** Demo routing only. Does not relax FSlabScenarioCodec or construct a received scenario. */
namespace AgentTestSetTrigger
{
    MA0T10_DT_API FString Material(const FString& Json);
    /** Template declares its units explicitly; convert to the receiving Slab's existing units. */
    MA0T10_DT_API bool LoadTemplate(const FString& Path,const FString& Trigger,ESlabInputUnit Dimension,ESlabInputUnit Position,FSlabScenarioDataPtr& Out,FString& Error);
}
