#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Contracts/TrainBatchContract.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioTypes.h"
#include "ma0t10_dt/MA0T10/Dataset/VirtualDatasetTypes.h"

struct FTrainBatchMessage
{
    enum class EKind : uint8 { Job,Scenario,Seal } Kind=EKind::Job;
    FTrainBatchJobSpec Job;
    FString JobId,ScenarioId,InputSha256;
    int64 InputBytes=0;
    FSlabScenarioDataPtr Scenario;
};
namespace TrainBatchCodec
{
    MA0T10_DT_API bool SafeId(const FString& Id);
    MA0T10_DT_API bool CanonicalGuid(const FString& Id);
    MA0T10_DT_API bool Sha256(const void* Bytes,uint32 Size,FString& Out);
    /** [0,Duration) at20Hz. Only floating-point boundary noise (1e-9 sample) is tolerated. */
    MA0T10_DT_API bool SampleCount(double Duration,int32& Out,FString& Error);
    MA0T10_DT_API bool Parse(const FString& Json,FTrainBatchMessage& Out,FString& Error);
    MA0T10_DT_API bool ValidateScenario(const FTrainBatchScenarioSpec& Spec,const FString& Json,FSlabScenarioDataPtr& Out,FString& Error);
    MA0T10_DT_API bool Plan(const FSlabScenarioData& Data,double Duration,const FTransform& Track,ESlabInputUnit Dimension,ESlabInputUnit Position,FVirtualDatasetFramePlan& Out,FString& Error);
    MA0T10_DT_API FString JobsRoot();
    MA0T10_DT_API FString JobDirectory(const FString& JobId);
    MA0T10_DT_API FString RunDirectory(const FString& JobId,const FString& ScenarioId,const FString& RunId);
}
