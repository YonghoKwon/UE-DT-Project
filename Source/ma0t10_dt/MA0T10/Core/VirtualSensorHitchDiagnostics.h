#pragma once
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

// Opt-in diagnostics shared by runtime layers. Never owns a sensor or transport.
enum class EVirtualSensorHitchSection : uint8
{
    Publisher, TransportEvents, Enqueue, Telemetry, ResumeCapacity, CancelRun,
    StopWorker, SensorApply, SensorSchedule, SensorComplete, SlabMotion, SlabTick,
    OutstandingWait, RunPendingWait, Count
};
struct MA0T10_DT_API FVirtualSensorHitchSnapshot
{
    int64 Frames=0,Hitches=0,SocketIoInsideMutex=0;
    double MaxFrameMs=0,MaxOutstandingHoldMs=0,MaxRunPendingHoldMs=0;
    FString HitchCsv;
};
class MA0T10_DT_API FVirtualSensorHitchDiagnostics
{
public:
    static bool IsEnabled();
    static void Start();
    static void Stop();
    static void Add(EVirtualSensorHitchSection Section,double Seconds);
    static FVirtualSensorHitchSnapshot Snapshot();
    static void SocketWrite();
};
class MA0T10_DT_API FVirtualSensorHitchScope
{
    EVirtualSensorHitchSection Section;
    double Started=0;
public:
    explicit FVirtualSensorHitchScope(EVirtualSensorHitchSection In);
    ~FVirtualSensorHitchScope();
};
// Same locking semantics as FScopeLock; additionally measures GT wait and worker
// hold, and detects a socket write from inside either transport mutex.
class MA0T10_DT_API FVirtualSensorTransportLock
{
    FCriticalSection* Mutex;
    bool Outstanding,Measured=false;
    double HeldSince=0;
public:
    FVirtualSensorTransportLock(FCriticalSection* In,bool IsOutstanding);
    ~FVirtualSensorTransportLock();
};
