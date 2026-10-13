#pragma once
#include "CoreMinimal.h"

/** Value-only acquisition linkage. Names retain the existing wire vocabulary.
 * Captured on the game thread; copied into immutable frames before worker use.
 * No sensor, scenario, UObject, or provider reference is carried in this value.
 */
struct MA0T10_DT_API FVirtualSensorAcquisitionContext
{
    FString RunId;
    FString ScenarioUUID;
    FString MtlNo;
    int64 SlabFrameNo = -1;
    double ElapsedSec = 0;
    int32 Generation = 0;
    int32 Segment = 0;
    bool bEligible = false;
    int64 AcquisitionUtcTicks = 0;
    TMap<FString,FString> ToHeaders() const;
};

/** Host output identifiers, independent of the L1 stream enum's numeric order. */
enum class EVirtualSensorAcquisitionOutput : uint8 { LidarTelemetry, CameraImage, PointCloud };

/** Game-thread host adapter only. Workers must never resolve or invoke it. */
class MA0T10_DT_API IVirtualSensorAcquisitionContextProvider
{
public:
    virtual ~IVirtualSensorAcquisitionContextProvider() = default;
    virtual FVirtualSensorAcquisitionContext CaptureAcquisitionContext(const FString& SensorId, int64 FrameId) = 0;
    virtual void CompleteAcquisition(const FString& SensorId, int64 FrameId, bool bSuccess = true) = 0;
    virtual bool BeginContextDerivation(const FString& SensorId, int64 FrameId, const FVirtualSensorAcquisitionContext& Context) = 0;
    virtual void EndCompatibilityDerivation(const FString& RunId, const FString& SensorId, int64 FrameId, bool bFailed = false) = 0;
    virtual bool AllowsAcquisitionFrame(const FString& SensorId, const FVirtualSensorAcquisitionContext& Context) const = 0;
    virtual bool AllowsOutputFrame(const FString& SensorId, EVirtualSensorAcquisitionOutput Output, const FVirtualSensorAcquisitionContext& Context) const = 0;
    virtual bool AllowsOutputDemand(const FString& SensorId, EVirtualSensorAcquisitionOutput Output) const = 0;
    virtual bool IsSensorConfigurationLocked(const FString& SensorId) const = 0;
    virtual bool IsSensorPlacementLocked(const FString& SensorId) const = 0;
    virtual bool IsAcquisitionSessionBusy() const = 0;
    virtual void RecordAcquisitionOutputFailure(const FString& RunId, const FString& SensorId, EVirtualSensorAcquisitionOutput Output, int64 FrameId, const FString& Reason) = 0;
};

class UWorld;
/** Weak, world-scoped registration; no ownership of the host, no subscription loop. */
struct MA0T10_DT_API FVirtualSensorAcquisitionContextProviders
{
    static bool Register(UWorld* World, UObject* Owner, IVirtualSensorAcquisitionContextProvider* Provider);
    static void Unregister(UWorld* World, const IVirtualSensorAcquisitionContextProvider* Provider);
    static IVirtualSensorAcquisitionContextProvider* Get(const UWorld* World);
};
