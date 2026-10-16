#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioTypes.h"
#include "AgentTestSetSubsystem.generated.h"
class ASlabActor;
struct FAgentTestSetState;
class FJsonObject;

enum class EAgentTestSetItemState : uint8 { Waiting, Generating, Completed, Failed };
struct FAgentTestSetItem
{
    FString ScenarioId,RunId,Directory,Message;
    EAgentTestSetItemState State=EAgentTestSetItemState::Waiting;
    int32 Frames=0,CompletedFrames=0,Attempts=0;
    double Seconds=0;
};
struct FAgentTestSetSettings
{
    bool Enabled=true,Pipeline=true,CaptureCamera=false,CameraTruth=false,IncludeImu=false,LidarFidelity=false,ExportUprightEnvelope=false;
    FString Trigger=TEXT("9999"),LidarId,CameraId,ExportFormat=TEXT("none");
    int32 Target=100,SampleStride=1,SettleFrames=1;
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
    bool IsComplete() const;
    bool HasItems() const;
    int32 Completed() const;
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
    void Pump();
    void EnsurePump();
};
