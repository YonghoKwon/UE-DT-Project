#pragma once
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#define MA0T10_SENSOR_HITCH_FRAME_COSTS_AVAILABLE 1

// Opt-in diagnostics shared by runtime layers. Never owns a sensor or transport.
enum class EVirtualSensorHitchSection : uint8
{
    Publisher, TransportEvents, Enqueue, Telemetry, ResumeCapacity, CancelRun,
    StopWorker, SensorApply, SensorSchedule, SensorComplete, SlabMotion, SlabTick,
    OutstandingWait, RunPendingWait, EditorProperty, SettingsUi, UiSave, GarbageCollection, Count
};
struct MA0T10_DT_API FVirtualSensorHitchSnapshot
{
    int64 Frames=0,Hitches=0,SocketIoInsideMutex=0;
    double MaxFrameMs=0,MaxOutstandingHoldMs=0,MaxRunPendingHoldMs=0;
    FString HitchCsv,SectionCsv;
};
class MA0T10_DT_API FVirtualSensorHitchDiagnostics
{
public:
    static bool IsEnabled();
    static void Start();
    static void Stop();
    static void Add(EVirtualSensorHitchSection Section,double Seconds);
    static FVirtualSensorHitchSnapshot Snapshot();
    /** GT-only numeric observation; no formatting, allocation or ownership. */
    static double CurrentFrameSeconds(EVirtualSensorHitchSection Section);
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
