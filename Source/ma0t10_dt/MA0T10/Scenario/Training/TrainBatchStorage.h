#pragma once
#include "TrainBatchCodec.h"
#include <atomic>

/** Pure serial disk owner. Called only from the subsystem's one background operation. */
class FTrainBatchStorage
{
public:
    TArray<FTrainBatchJobView> Jobs;
    bool Receive(const FTrainBatchMessage& Message,FString& Error);
    bool Import(const FString& ManifestFile,const std::atomic<bool>& Cancel,FString& Error);
    TSharedPtr<FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Prepare(const FString& JobId,int32 Index,const FTransform& Track,ESlabInputUnit Dimension,ESlabInputUnit Position,uint32 Configuration,int64 EstimatedFrameBytes,FString& Error);
    bool Finish(const FString& JobId,int32 Index,bool Success,const FString& Reason,const std::atomic<bool>& Cancel,FString& Error);
    static bool ReadUtf8(const FString& Path,int64 Limit,FString& Out);
private:
    TMap<FString,int64> Sequences;
    TMap<FString,FString> PreviousHashes;
    FTrainBatchJobView* Find(const FString& Id);
    bool StoreInput(FTrainBatchJobView& Job,int32 Index,const FString& Raw,FString& Error);
    bool Snapshot(FTrainBatchJobView& Job,const FString& Event,FString& Error);
    bool Verify(FTrainBatchJobView& Job,int32 Index,const std::atomic<bool>& Cancel,FString& Error);
    static bool AtomicText(const FString& Path,const FString& Text,bool Replace);
    static bool HasDiskSpace(uint64 Required,FString& Error);
};
