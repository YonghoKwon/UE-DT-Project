#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VirtualDatasetTypes.h"
#include "VirtualDatasetSubsystem.generated.h"
class ASlabActor;
class ADirectionalLight;
class AVirtualLidarSensorActor;
class AVirtualCameraSensorActor;
class UVirtualCameraGroundTruth;
struct FVirtualDatasetSession;

/** Lazy session orchestrator. No ticker, directories, scans or workers before explicit Start. */
UCLASS()
class MA0T10_DT_API UVirtualDatasetSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    bool Start(const FVirtualDatasetSettings& Settings,ASlabActor* Slab,AVirtualLidarSensorActor* Lidar,AVirtualCameraSensorActor* Camera,ADirectionalLight* Light,FString& Error,TSharedPtr<const FVirtualDatasetFramePlan,ESPMode::ThreadSafe> Plan=nullptr);
    /** Yield before a live scenario touches the shared World. In-flight immutable write may drain. */
    void YieldPlannedCapture();
    bool OwnsPlannedRun(const FString& RunId) const;
    void Pause();
    bool Resume(FString& Error);
    void Stop();
    bool IsActive() const;
    bool IsPaused() const;
    int32 CompletedScenes() const;
    int32 RequestedScenes() const;
    FString Directory() const;
    FString ExportDirectory() const;
    bool HasSucceeded() const;
    FString Message;
    int64 Revision=0;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    TFunction<void(const FJsonObject&)> OnSceneSnapshotForTests;
    bool HasTruthResourcesForTests() const;
    bool IsCapturingForTests() const;
    bool IsWritingForTests() const;
    int32 GetReadbackAllocationsForTests() const;
    float WorkerDelayForTests=0;
#endif
    virtual void Deinitialize() override;
private:
    TSharedPtr<FVirtualDatasetSession,ESPMode::ThreadSafe> Session;
    UPROPERTY(Transient) TObjectPtr<UVirtualCameraGroundTruth> Truth;
    FTimerHandle Timer;
    void Pump();
    void PumpPlanned();
    void Restore();
    bool ApplyScene(FString& Error);
    void WriteScene();
    void StartExport();
    bool TargetsCurrent() const;
};
