#pragma once
#include "CoreMinimal.h"
#include "AgentTestSetSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Async/Future.h"
#include <atomic>
class UVirtualLidarScanComponent;
class UVirtualLidarVisualizationComponent;
struct FTestSetPcdFrame
{
    TArray<FVirtualLidarPoint> Points;
    TSharedPtr<const FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe> Snapshot;
    FTransform Pose;
    FString Error;
    int32 Index=0,Width=1,Height=1;
    double Elapsed=0,Range=100;
};
/** Saved-file viewer. Private components have no Actor output router, transport or recorder. */
class FAgentTestSetPcdPreview
{
public:
    FAgentTestSetPcdPreview();
    ~FAgentTestSetPcdPreview();
    void Start(UWorld* World,const FAgentTestSetItem& Item,int32 Index);
    void Pump(UWorld* World);
    void Stop();
    void SetPaused(bool Paused);
    bool NeedsPump() const;
    FAgentTestSetPreviewStatus Status() const { return View; }
    UTexture2D* Texture() const;
    static FTestSetPcdFrame ReadFrame(const FAgentTestSetItem& Item,int32 Index,const std::atomic<bool>& Cancel);
private:
    void Queue(int32 Index);
    FAgentTestSetItem Source;
    FAgentTestSetPreviewStatus View;
    TFuture<FTestSetPcdFrame> Read;
    TOptional<FTestSetPcdFrame> Ready;
    TSharedRef<std::atomic<bool>,ESPMode::ThreadSafe> Cancelled=MakeShared<std::atomic<bool>,ESPMode::ThreadSafe>(false);
    TStrongObjectPtr<UVirtualLidarScanComponent> Scan;
    TStrongObjectPtr<UVirtualLidarVisualizationComponent> Visualization;
    double AppliedAt=0;
};
