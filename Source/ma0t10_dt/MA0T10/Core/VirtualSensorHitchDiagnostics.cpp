#include "VirtualSensorHitchDiagnostics.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include <atomic>

namespace
{
    std::atomic<bool> Enabled{false};
    std::atomic<int64> IoInsideMutex{0},MaxOutstandingUs{0},MaxRunUs{0};
    thread_local int32 TransportMutexDepth=0;
    constexpr int32 Count=static_cast<int32>(EVirtualSensorHitchSection::Count);
    const TCHAR* Names[]={TEXT("publisher"),TEXT("transportEvents"),TEXT("enqueue"),TEXT("telemetry"),TEXT("resumeCapacity"),TEXT("cancelRun"),
        TEXT("stopWorker"),TEXT("sensorApply"),TEXT("sensorSchedule"),TEXT("sensorComplete"),TEXT("slabMotion"),TEXT("slabTick"),TEXT("outstandingWait"),TEXT("runPendingWait")};
    double FrameSections[Count]={},PreviousStart=0;
    int64 Frames=0,Hitches=0;
    double MaxFrameMs=0;
    FString Csv;
    FDelegateHandle FrameHandle;
    void Maximum(std::atomic<int64>& Value,int64 Candidate)
    {auto Old=Value.load(std::memory_order_relaxed);while(Old<Candidate&&!Value.compare_exchange_weak(Old,Candidate,std::memory_order_relaxed)){} }
    void BeginFrame()
    {
        const double Now=FPlatformTime::Seconds();
        if(PreviousStart>0)
        {
            const double Ms=(Now-PreviousStart)*1000; ++Frames;MaxFrameMs=FMath::Max(MaxFrameMs,Ms);
            if(Ms>150)
            {
                ++Hitches;
                // Bounded evidence in memory. Probe writes it asynchronously at
                // completion; diagnosing a hitch never adds synchronous file IO.
                if(Hitches<=1024)
                {
                    Csv+=FString::Printf(TEXT("%lld,%.6f,%.3f"),Frames,PreviousStart,Ms);
                    for(double Seconds:FrameSections)Csv+=FString::Printf(TEXT(",%.3f"),Seconds*1000);
                    Csv+=TEXT("\n");
                }
                UE_LOG(LogTemp,Warning,TEXT("[SensorHitch] frame=%lld wallMs=%.3f publisherMs=%.3f eventsMs=%.3f stopMs=%.3f applyMs=%.3f schedulerMs=%.3f outstandingWaitMs=%.3f runWaitMs=%.3f"),
                    Frames,Ms,FrameSections[0]*1000,FrameSections[1]*1000,FrameSections[6]*1000,FrameSections[7]*1000,FrameSections[8]*1000,FrameSections[12]*1000,FrameSections[13]*1000);
            }
        }
        FMemory::Memzero(FrameSections);PreviousStart=Now;
    }
}
bool FVirtualSensorHitchDiagnostics::IsEnabled(){return Enabled.load(std::memory_order_relaxed);}
void FVirtualSensorHitchDiagnostics::Start()
{
    check(IsInGameThread());Stop();Frames=Hitches=0;MaxFrameMs=0;PreviousStart=0;FMemory::Memzero(FrameSections);
    IoInsideMutex=0;MaxOutstandingUs=0;MaxRunUs=0;
    Csv=TEXT("frame,start_monotonic,wall_ms");for(const TCHAR* Name:Names)Csv+=FString(TEXT(","))+Name+TEXT("_ms");Csv+=TEXT("\n");
    Enabled.store(true,std::memory_order_relaxed);FrameHandle=FCoreDelegates::OnBeginFrame.AddStatic(&BeginFrame);
}
void FVirtualSensorHitchDiagnostics::Stop()
{
    Enabled.store(false,std::memory_order_relaxed);
    if(FrameHandle.IsValid()){FCoreDelegates::OnBeginFrame.Remove(FrameHandle);FrameHandle.Reset();}
}
void FVirtualSensorHitchDiagnostics::Add(EVirtualSensorHitchSection Section,double Seconds)
{if(IsEnabled()&&IsInGameThread())FrameSections[static_cast<int32>(Section)]+=Seconds;}
FVirtualSensorHitchSnapshot FVirtualSensorHitchDiagnostics::Snapshot()
{
    check(IsInGameThread());FVirtualSensorHitchSnapshot S;S.Frames=Frames;S.Hitches=Hitches;S.MaxFrameMs=MaxFrameMs;S.HitchCsv=Csv;
    S.SocketIoInsideMutex=IoInsideMutex.load();S.MaxOutstandingHoldMs=MaxOutstandingUs.load()/1000.0;S.MaxRunPendingHoldMs=MaxRunUs.load()/1000.0;return S;
}
void FVirtualSensorHitchDiagnostics::SocketWrite()
{if(IsEnabled()&&TransportMutexDepth>0)++IoInsideMutex;}
FVirtualSensorHitchScope::FVirtualSensorHitchScope(EVirtualSensorHitchSection In):Section(In)
{if(FVirtualSensorHitchDiagnostics::IsEnabled()&&IsInGameThread())Started=FPlatformTime::Seconds();}
FVirtualSensorHitchScope::~FVirtualSensorHitchScope()
{if(Started>0)FVirtualSensorHitchDiagnostics::Add(Section,FPlatformTime::Seconds()-Started);}
FVirtualSensorTransportLock::FVirtualSensorTransportLock(FCriticalSection* In,bool IsOutstanding):Mutex(In),Outstanding(IsOutstanding)
{
    Measured=FVirtualSensorHitchDiagnostics::IsEnabled();const double Before=Measured?FPlatformTime::Seconds():0;Mutex->Lock();
    if(Measured){HeldSince=FPlatformTime::Seconds();++TransportMutexDepth;FVirtualSensorHitchDiagnostics::Add(Outstanding?EVirtualSensorHitchSection::OutstandingWait:EVirtualSensorHitchSection::RunPendingWait,HeldSince-Before);}
}
FVirtualSensorTransportLock::~FVirtualSensorTransportLock()
{
    if(Measured){Maximum(Outstanding?MaxOutstandingUs:MaxRunUs,static_cast<int64>((FPlatformTime::Seconds()-HeldSince)*1e6));--TransportMutexDepth;}
    Mutex->Unlock();
}
