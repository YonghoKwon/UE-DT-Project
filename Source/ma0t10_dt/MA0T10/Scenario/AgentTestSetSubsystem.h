#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioTypes.h"
#include "AgentTestSetSubsystem.generated.h"
class ASlabActor;
struct FAgentTestSetState;
class FJsonObject;
class FAgentTestSetPcdPreview;
struct FAgentTestSetPreviewStatus
{
    int32 Scenario=INDEX_NONE,Frame=INDEX_NONE,Frames=0,Points=0;
    double Elapsed=0;
    bool Playing=false,Loading=false;
    FString Message;
};

enum class EAgentTestSetItemState : uint8 { Waiting, Generating, Completed, Failed };
struct FAgentTestSetItem
{
    FString ScenarioId,RunId,Directory,Message;
    EAgentTestSetItemState State=EAgentTestSetItemState::Waiting;
    int32 Frames=0,CompletedFrames=0,Attempts=0;
    double Seconds=0;
    bool Example=false;
    FString SourceUUID,VariationJson;
    int32 Seed=0;
};
struct FAgentTestSetSettings
{
    bool Enabled=true,Pipeline=true,CaptureCamera=false,CameraTruth=false,IncludeImu=false,LidarFidelity=false,ExportUprightEnvelope=false;
    FString Trigger=TEXT("9999"),LidarId,CameraId,ExportFormat=TEXT("none");
    int32 Target=100,SampleStride=1,SettleFrames=1;
    // TESTSET-02 temporary autofill mode uses the same frame cap for received and example slots.
    bool AutoFillExamples=true;
    int32 ExampleSeed=1337,ExampleMaxFrames=1;
    static FAgentTestSetSettings Load();
};

/** Temporary TESTSET-01 adapter over DATASET-01. No resources/timer/output before a trigger. */
UCLASS()
class MA0T10_DT_API UAgentTestSetSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    static bool Matches(const FSlabScenarioData& Data,const FString& Trigger);
    bool IsTrigger(const FSlabScenarioData& Data) const;
    /** A matching input is consumed even when admission fails; never falls through to PCD. */
    bool Enqueue(FSlabScenarioDataPtr Data,ASlabActor* Slab,FString& Error);
    void YieldToOrdinaryScenario();
    void SetPaused(bool Paused);
    bool IsPaused() const;
    bool Retry(int32 Index,FString& Error);
    bool ResetConfirmed(FString& Error);
    bool SelectPreview(int32 Index,FString& Error);
    void StopPreview();
    void SetPreviewPaused(bool Paused);
    FAgentTestSetPreviewStatus PreviewStatus() const;
    class UTexture2D* PreviewTexture() const;
    bool StopBusyWorkAndStart(FString& Error);
    bool HasBusyWork() const;
    bool IsComplete() const;
    bool HasItems() const;
    int32 Completed() const;
    double ScenariosPerSecond() const;
    double RemainingSeconds() const;
    const TArray<FAgentTestSetItem>& Items() const;
    const FAgentTestSetSettings& Settings() const {return Config;}
    FString Directory() const;
    FString Message() const;
    int64 Revision() const;
    TSharedRef<FJsonObject> StatusJson() const;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    bool ConfigureForTests(const FAgentTestSetSettings& In);
#endif
private:
    FAgentTestSetSettings Config;
    TSharedPtr<FAgentTestSetState> State;
    FTimerHandle Timer;
    TSharedPtr<FAgentTestSetPcdPreview> PcdPreview;
    bool BusyReason(FString& Reason) const;
    void SuspendBusyWork();
    void RestoreStoppedWork(bool Resume=true);
    void Pump();
    void EnsurePump();
};
