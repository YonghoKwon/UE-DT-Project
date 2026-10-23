#include "VirtualSensorHitchDiagnostics.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"
#include <atomic>

namespace
{
    std::atomic<bool> GSensorHitchEnabled{false};
    std::atomic<int64> GSensorHitchIoInsideMutex{0},GSensorHitchOutstandingUs{0},GSensorHitchRunUs{0};
    thread_local int32 GSensorHitchMutexDepth=0;
    constexpr int32 GSensorHitchSectionCount=static_cast<int32>(EVirtualSensorHitchSection::Count);
    const TCHAR* GSensorHitchSectionNames[]={TEXT("publisher"),TEXT("transportEvents"),TEXT("enqueue"),TEXT("telemetry"),TEXT("resumeCapacity"),TEXT("cancelRun"),
        TEXT("stopWorker"),TEXT("sensorApply"),TEXT("sensorSchedule"),TEXT("sensorComplete"),TEXT("slabMotion"),TEXT("slabTick"),TEXT("outstandingWait"),TEXT("runPendingWait"),TEXT("editorProperty"),TEXT("settingsUi"),TEXT("uiSave"),TEXT("gc")};
    double GSensorHitchSections[GSensorHitchSectionCount]={},GSensorHitchPreviousStart=0;
    double GSensorHitchSectionMax[GSensorHitchSectionCount]={};
    int64 GSensorHitchSectionCalls[GSensorHitchSectionCount]={};
    int64 GSensorHitchFrames=0,GSensorHitchCount=0;
    double GSensorHitchMaxFrameMs=0;
    FString GSensorHitchCsv;
    FDelegateHandle GSensorHitchFrameHandle;
    FDelegateHandle GSensorHitchGcBeginHandle,GSensorHitchGcEndHandle;
    double GSensorHitchGcStarted=0;
    void UpdateSensorHitchMaximum(std::atomic<int64>& Value,int64 Candidate)
    {auto Old=Value.load(std::memory_order_relaxed);while(Old<Candidate&&!Value.compare_exchange_weak(Old,Candidate,std::memory_order_relaxed)){} }
    void BeginSensorHitchFrame()
    {
        const double Now=FPlatformTime::Seconds();
        if(GSensorHitchPreviousStart>0)
        {
            const double Ms=(Now-GSensorHitchPreviousStart)*1000; ++GSensorHitchFrames;GSensorHitchMaxFrameMs=FMath::Max(GSensorHitchMaxFrameMs,Ms);
            if(Ms>150)
            {
                ++GSensorHitchCount;
                // Bounded evidence in memory. Probe writes it asynchronously at
                // completion; diagnosing a hitch never adds synchronous file IO.
                if(GSensorHitchCount<=1024)
                {
                    GSensorHitchCsv+=FString::Printf(TEXT("%lld,%.6f,%.3f"),GSensorHitchFrames,GSensorHitchPreviousStart,Ms);
                    for(double Seconds:GSensorHitchSections)GSensorHitchCsv+=FString::Printf(TEXT(",%.3f"),Seconds*1000);
                    GSensorHitchCsv+=TEXT("\n");
                }
                UE_LOG(LogTemp,Warning,TEXT("[SensorHitch] frame=%lld wallMs=%.3f publisherMs=%.3f eventsMs=%.3f stopMs=%.3f applyMs=%.3f schedulerMs=%.3f outstandingWaitMs=%.3f runWaitMs=%.3f"),
                    GSensorHitchFrames,Ms,GSensorHitchSections[0]*1000,GSensorHitchSections[1]*1000,GSensorHitchSections[6]*1000,GSensorHitchSections[7]*1000,GSensorHitchSections[8]*1000,GSensorHitchSections[12]*1000,GSensorHitchSections[13]*1000);
            }
        }
        FMemory::Memzero(GSensorHitchSections);GSensorHitchPreviousStart=Now;
    }
}
bool FVirtualSensorHitchDiagnostics::IsEnabled(){return GSensorHitchEnabled.load(std::memory_order_relaxed);}
double FVirtualSensorHitchDiagnostics::CurrentFrameSeconds(EVirtualSensorHitchSection Section)
{
    const int32 Index=static_cast<int32>(Section);
    return IsEnabled()&&IsInGameThread()&&Index>=0&&Index<GSensorHitchSectionCount?GSensorHitchSections[Index]:0.0;
}
void FVirtualSensorHitchDiagnostics::Start()
{
    check(IsInGameThread());Stop();GSensorHitchFrames=GSensorHitchCount=0;GSensorHitchMaxFrameMs=0;GSensorHitchPreviousStart=0;FMemory::Memzero(GSensorHitchSections);
    GSensorHitchIoInsideMutex=0;GSensorHitchOutstandingUs=0;GSensorHitchRunUs=0;
    FMemory::Memzero(GSensorHitchSectionMax);FMemory::Memzero(GSensorHitchSectionCalls);
    GSensorHitchCsv=TEXT("frame,start_monotonic,wall_ms");for(const TCHAR* Name:GSensorHitchSectionNames)GSensorHitchCsv+=FString(TEXT(","))+Name+TEXT("_ms");GSensorHitchCsv+=TEXT("\n");
    GSensorHitchEnabled.store(true,std::memory_order_relaxed);GSensorHitchFrameHandle=FCoreDelegates::OnBeginFrame.AddStatic(&BeginSensorHitchFrame);
    GSensorHitchGcBeginHandle=FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddLambda([](){GSensorHitchGcStarted=FPlatformTime::Seconds();});
    GSensorHitchGcEndHandle=FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([](){if(GSensorHitchGcStarted>0)FVirtualSensorHitchDiagnostics::Add(EVirtualSensorHitchSection::GarbageCollection,FPlatformTime::Seconds()-GSensorHitchGcStarted);});
}
void FVirtualSensorHitchDiagnostics::Stop()
{
    GSensorHitchEnabled.store(false,std::memory_order_relaxed);
    if(GSensorHitchFrameHandle.IsValid()){FCoreDelegates::OnBeginFrame.Remove(GSensorHitchFrameHandle);GSensorHitchFrameHandle.Reset();}
    if(GSensorHitchGcBeginHandle.IsValid()){FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(GSensorHitchGcBeginHandle);GSensorHitchGcBeginHandle.Reset();}
    if(GSensorHitchGcEndHandle.IsValid()){FCoreUObjectDelegates::GetPostGarbageCollect().Remove(GSensorHitchGcEndHandle);GSensorHitchGcEndHandle.Reset();}
}
void FVirtualSensorHitchDiagnostics::Add(EVirtualSensorHitchSection Section,double Seconds)
{
    if(IsEnabled()&&IsInGameThread())
    {
        const int32 Index=static_cast<int32>(Section);GSensorHitchSections[Index]+=Seconds;
        GSensorHitchSectionMax[Index]=FMath::Max(GSensorHitchSectionMax[Index],Seconds);++GSensorHitchSectionCalls[Index];
    }
}
FVirtualSensorHitchSnapshot FVirtualSensorHitchDiagnostics::Snapshot()
{
    check(IsInGameThread());FVirtualSensorHitchSnapshot S;S.Frames=GSensorHitchFrames;S.Hitches=GSensorHitchCount;S.MaxFrameMs=GSensorHitchMaxFrameMs;S.HitchCsv=GSensorHitchCsv;
    S.SectionCsv=TEXT("section,calls,max_ms\n");
    for(int32 I=0;I<GSensorHitchSectionCount;++I)S.SectionCsv+=FString::Printf(TEXT("%s,%lld,%.6f\n"),GSensorHitchSectionNames[I],GSensorHitchSectionCalls[I],GSensorHitchSectionMax[I]*1000);
    S.SocketIoInsideMutex=GSensorHitchIoInsideMutex.load();S.MaxOutstandingHoldMs=GSensorHitchOutstandingUs.load()/1000.0;S.MaxRunPendingHoldMs=GSensorHitchRunUs.load()/1000.0;return S;
}
void FVirtualSensorHitchDiagnostics::SocketWrite()
{if(IsEnabled()&&GSensorHitchMutexDepth>0)++GSensorHitchIoInsideMutex;}
FVirtualSensorHitchScope::FVirtualSensorHitchScope(EVirtualSensorHitchSection In):Section(In)
{if(FVirtualSensorHitchDiagnostics::IsEnabled()&&IsInGameThread())Started=FPlatformTime::Seconds();}
FVirtualSensorHitchScope::~FVirtualSensorHitchScope()
{if(Started>0)FVirtualSensorHitchDiagnostics::Add(Section,FPlatformTime::Seconds()-Started);}
FVirtualSensorTransportLock::FVirtualSensorTransportLock(FCriticalSection* In,bool IsOutstanding):Mutex(In),Outstanding(IsOutstanding)
{
    Measured=FVirtualSensorHitchDiagnostics::IsEnabled();const double Before=Measured?FPlatformTime::Seconds():0;Mutex->Lock();
    if(Measured){HeldSince=FPlatformTime::Seconds();++GSensorHitchMutexDepth;FVirtualSensorHitchDiagnostics::Add(Outstanding?EVirtualSensorHitchSection::OutstandingWait:EVirtualSensorHitchSection::RunPendingWait,HeldSince-Before);}
}
FVirtualSensorTransportLock::~FVirtualSensorTransportLock()
{
    if(Measured){UpdateSensorHitchMaximum(Outstanding?GSensorHitchOutstandingUs:GSensorHitchRunUs,static_cast<int64>((FPlatformTime::Seconds()-HeldSince)*1e6));--GSensorHitchMutexDepth;}
    Mutex->Unlock();
}
