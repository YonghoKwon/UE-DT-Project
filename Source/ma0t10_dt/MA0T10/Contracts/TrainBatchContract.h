#pragma once
#include "CoreMinimal.h"

/** Application job values only. No sensor, UI, actor or DTCore implementation dependency. */
namespace TrainBatchContract
{
    inline constexpr const TCHAR* Schema=TEXT("ma0t10.train.job.v1");
    inline constexpr const TCHAR* MessageId=TEXT("ITrain-agent-batch");
    inline constexpr const TCHAR* Topic=TEXT("topic.train.scenario");
    inline constexpr int32 SampleHz=20,MaxScenarios=1000,MaxFrames=10000,MaxJobFrames=600000,MaxSessionJobs=16;
    inline constexpr int64 ManifestBytes=256*1024,ScenarioBytes=8ll*1024*1024,EnvelopeBytes=16ll*1024*1024;
    inline constexpr int64 JobInputBytes=256ll*1024*1024,QueueBytes=64ll*1024*1024;
    inline constexpr double MaxDuration=300,MaxTotalDuration=30000;
}

enum class ETrainBatchItemState : uint8 { Receiving,Waiting,Running,Completed,Failed };
struct FTrainBatchScenarioSpec
{
    FString Id,InputSha256,InputFile;
    int64 InputBytes=0;
    double Duration=0;
};
struct FTrainBatchJobSpec
{
    FString Id,PresetId,DimensionUnit,PositionUnit,ManifestSha256,OriginalJson;
    FString ArchiveFormat=TEXT("zip");
    int32 WorkerLimit=1;
    bool CaptureCamera=false,CameraTruth=false;
    TArray<FTrainBatchScenarioSpec> Scenarios;
};
struct FTrainBatchItemView
{
    FTrainBatchScenarioSpec Spec;
    ETrainBatchItemState State=ETrainBatchItemState::Receiving;
    FString RunId,RunDirectory,Message;
    int32 CompletedFrames=0;
};
struct FTrainBatchJobView
{
    FTrainBatchJobSpec Spec;
    TArray<FTrainBatchItemView> Items;
    FString Message;
    bool StorageHealthy=true;
    int32 CompleteCount() const
    {int32 Count=0;for(const auto& I:Items)Count+=I.State==ETrainBatchItemState::Completed;return Count;}
    bool IsReady() const
    {return StorageHealthy&&!Items.IsEmpty()&&!Items.ContainsByPredicate([](const auto& I){return I.State==ETrainBatchItemState::Receiving;});}
};
