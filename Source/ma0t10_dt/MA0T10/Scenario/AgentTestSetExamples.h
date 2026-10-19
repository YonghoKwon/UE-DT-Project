#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioTypes.h"

/** Temporary local examples only. JSON is derived on a worker; received bytes stay untouched. */
namespace AgentTestSetExamples
{
    MA0T10_DT_API int32 SeedFor(const FString& ContentSha1,int32 Seed);
    MA0T10_DT_API FString Variation(int32 Seed,int32 Example,ESlabInputUnit Dimension,ESlabInputUnit Position);
    MA0T10_DT_API bool Generate(const FSlabScenarioData& Source,const FString& Id,const FString& Variation,FSlabScenarioDataPtr& Out,FString& Error);
    MA0T10_DT_API TArray<int32> SampleRows(int32 Rows,int32 Stride,int32 Cap);
}
