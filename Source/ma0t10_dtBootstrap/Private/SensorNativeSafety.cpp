#include "SensorNativeSafety.h"
#include "SensorNativeRayTracing.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RHI.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace SensorNativeSafety
{
namespace
{
std::atomic<bool> Disabled{false},ValidationRequested{false};
FCriticalSection PolicyMutex;
bool Prepared=false,Allowed=false;
uint64 ProcessStamp(uint32 Pid,bool& Alive)
{
    Alive=false;
#if PLATFORM_WINDOWS
    HANDLE Process=::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,0,Pid);
    if(!Process){Alive=::GetLastError()==ERROR_ACCESS_DENIED;return 0;}
    DWORD Exit=0;FILETIME Created={},Exited={},Kernel={},User={};
    Alive=::GetExitCodeProcess(Process,&Exit)&&Exit==STILL_ACTIVE;
    const bool HasTime=::GetProcessTimes(Process,&Created,&Exited,&Kernel,&User)!=0;
    ::CloseHandle(Process);return HasTime?(uint64(Created.dwHighDateTime)<<32)|Created.dwLowDateTime:0;
#else
    auto Process=FPlatformProcess::OpenProcess(Pid);Alive=Process.IsValid()&&FPlatformProcess::IsProcRunning(Process);FPlatformProcess::CloseProc(Process);return 0;
#endif
}
const FGPUDriverInfo& DriverInfo()
{
    static const FGPUDriverInfo Info=[](){FGPUDriverInfo I;I.VendorId=GRHIVendorId;I.DeviceDescription=GRHIAdapterName;I.UserDriverVersion=GRHIAdapterUserDriverVersion;return I;}();
    return Info;
}
const FString& MarkerPath()
{
    // Driver/model/vendor changes select an independent safety record.
    static const FString Path=[](){const FString Identity=FString::Printf(TEXT("%x|%s|%s"),DriverInfo().VendorId,*DriverInfo().DeviceDescription,*DriverInfo().UserDriverVersion);FSHAHash Hash;FSHA1::HashBuffer(*Identity,Identity.Len()*sizeof(TCHAR),Hash.Hash);return FPaths::ProjectSavedDir()/TEXT("NativeLidarSafety")/(TEXT("active-")+Hash.ToString()+TEXT(".guard"));}();
    return Path;
}
FSessionGuard& Session()
{
    static const FString Legacy=FPaths::ProjectSavedDir()/TEXT("NativeLidarSafety")/FString::Printf(TEXT("active-%08x.guard"),FCrc::StrCrc32(*(DriverInfo().DeviceDescription+TEXT("|")+DriverInfo().UserDriverVersion)));
    static FSessionGuard Guard(MarkerPath(),Legacy);return Guard;
}
}
void FSessionGuard::SaveState()
{
    // The live marker's exclusive writer serializes state changes across instances.
    const FString State=FString::Printf(TEXT("failures=%d\nvalidated=%d\nrevalidate=%d\nfaulted=%d\nreason=%s\n"),Failures,Validated?1:0,RevalidationRequired?1:0,Unhealthy.load()?1:0,*LastDecision);
    const FString Destination=Path+TEXT(".state"),Temporary=Destination+TEXT(".tmp");
    if(!FFileHelper::SaveStringToFile(State,*Temporary,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        ||!IFileManager::Get().Move(*Destination,*Temporary,true,true,false,true))
        UE_LOG(LogTemp,Warning,TEXT("[SensorNativeSafety] validation cache write failed; next launch will revalidate"));
}
bool FSessionGuard::CanStartDefault() const{return Owned.load()?(!Recovery&&!IsBanned()):!IFileManager::Get().FileExists(*Path);}
bool FSessionGuard::Begin(FString& Error)
{
    if(Owned.load())return true;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    if(!LegacyPath.IsEmpty()&&IFileManager::Get().FileExists(*LegacyPath))
    {
        FString Legacy;if(!FFileHelper::LoadFileToString(Legacy,*LegacyPath)){Error=TEXT("legacy guard cannot be read; RHI selected");return false;}
        uint32 Pid=0;uint64 Stamp=0;FParse::Value(*Legacy,TEXT("pid="),Pid);FParse::Value(*Legacy,TEXT("start="),Stamp);
        bool Alive=false;const uint64 Actual=ProcessStamp(Pid,Alive);
        if(Alive&&(!Stamp||!Actual||Stamp==Actual)){Error=TEXT("live owner from earlier native version; RHI selected");return false;}
        // Preserve an old unclean marker across the naming upgrade. A current
        // marker already represents the combination and must not be counted twice.
        const bool Migrated=IFileManager::Get().FileExists(*Path)?IFileManager::Get().Delete(*LegacyPath,false,true):IFileManager::Get().Move(*Path,*LegacyPath,false,false,false,true);
        if(!Migrated){Error=TEXT("legacy guard is still locked; RHI selected");return false;}
    }
    FString State;FFileHelper::LoadFileToString(State,*(Path+TEXT(".state")));
    FParse::Value(*State,TEXT("failures="),Failures);int32 Cache=0;FParse::Value(*State,TEXT("validated="),Cache);Validated=Cache==1;
    int32 Revalidate=0;FParse::Value(*State,TEXT("revalidate="),Revalidate);RevalidationRequired=Revalidate==1;
    if(IFileManager::Get().FileExists(*Path))
    {
        FString Prior;if(!FFileHelper::LoadFileToString(Prior,*Path)){Error=TEXT("unreadable/live safety marker; RHI selected");return false;}
        uint32 Pid=0;uint64 Stamp=0;FParse::Value(*Prior,TEXT("pid="),Pid);FParse::Value(*Prior,TEXT("start="),Stamp);
        bool Alive=false;const uint64 Actual=ProcessStamp(Pid,Alive);
        if(Alive&&(!Stamp||!Actual||Stamp==Actual)){Error=TEXT("concurrent native safety owner is alive; RHI selected");return false;}
        if(!IFileManager::Get().Delete(*Path,false,true)){Error=TEXT("stale marker is still locked; RHI selected");return false;}
        Recovery=true;Failures=FMath::Min(2,Failures+1);Validated=false;
    }
    Lease.Reset(IFileManager::Get().CreateFileWriter(*Path,FILEWRITE_NoReplaceExisting|FILEWRITE_AllowRead));
    if(!Lease){Error=TEXT("concurrent safety owner won lease; RHI selected");return false;}
    bool Alive=false;const uint64 Stamp=ProcessStamp(FPlatformProcess::GetCurrentProcessId(),Alive);
    const FString Text=FString::Printf(TEXT("schema=2\npid=%u\nstart=%llu\nrecovery=%d\n"),FPlatformProcess::GetCurrentProcessId(),Stamp,Recovery?1:0);
    FTCHARToUTF8 Bytes(*Text);Lease->Serialize((void*)Bytes.Get(),Bytes.Length());Lease->Flush();
    if(Lease->IsError()){Error=TEXT("native safety marker write failed; RHI selected");Lease.Reset();return false;}
    Unhealthy.store(false);Owned.store(true);SaveState();
    if(Recovery)Error=FString::Printf(TEXT("previous unclean session: safe RHI recovery launch; failures=%d"),Failures);
    return true;
}
void FSessionGuard::Validate(){if(Owned.load()&&!Recovery&&!IsBanned()){Validated=true;RevalidationRequired=false;SaveState();}}
void FSessionGuard::Invalidate(){if(Owned.load()){Validated=false;RevalidationRequired=true;SaveState();}}
void FSessionGuard::DisableForSession(bool DeviceHealthy,const FString& Reason)
{
    // A healthy-device submission failure is recoverable through RHI in this
    // session. Only a device fault keeps an unclean marker after orderly exit.
    if(!DeviceHealthy)Unhealthy.store(true);
    Invalidate();RecordDecision(Reason);
}
void FSessionGuard::RecordDecision(const FString& Reason){if(Owned.load()){LastDecision=Reason;SaveState();}}
void FSessionGuard::End(bool Healthy)
{
    if(!Owned.load())return;
    Healthy=Healthy&&!Unhealthy.load();
    if(Healthy&&!Recovery&&Validated&&!IsBanned()){Failures=0;SaveState();}
    const bool Closed=!Lease||Lease->Close();Lease.Reset();
    if(Healthy&&Closed)IFileManager::Get().Delete(*Path,false,true);
    Owned.store(false);
}
bool IsValidatedConfiguration(uint32 VendorId,const FString& Adapter,const FString& Driver)
{
    if(VendorId!=0x10de||!Adapter.Equals(TEXT("NVIDIA GeForce RTX 4070 Ti"),ESearchCase::IgnoreCase))return false;
    TArray<FString> Parts;Driver.ParseIntoArray(Parts,TEXT("."),false);
    return Parts.Num()==2&&Parts[0].IsNumeric()&&Parts[1].IsNumeric()&&FCString::Atoi(*Parts[0])==621&&FCString::Atoi(*Parts[1])==1;
}
bool IsProvenEngineDepthBarrier(uint32 MessageId,const FString& Description)
{
    return MessageId==527&&Description.Contains(TEXT("Using ResourceBarrier on Command List ("),ESearchCase::CaseSensitive)
        &&Description.Contains(TEXT(":'FD3D12CommandList (GPU 0)')"),ESearchCase::CaseSensitive)
        &&Description.Contains(TEXT(":'SceneDepthZ')"),ESearchCase::CaseSensitive)
        &&Description.Contains(TEXT("Before state ("),ESearchCase::CaseSensitive)
        &&Description.Contains(TEXT("specified by transition barrier does not match with the state"),ESearchCase::CaseSensitive);
}
bool CanAttempt(bool Explicit)
{
    if(Disabled.load())return false;
    FScopeLock Lock(&PolicyMutex);
    if(!Prepared)
    {
        Prepared=true;FString Reason;
        const bool KnownVendor=DriverInfo().VendorId==0x10de||DriverInfo().VendorId==0x1002||DriverInfo().VendorId==0x8086;
        if(!KnownVendor||DriverInfo().UserDriverVersion.IsEmpty())Reason=TEXT("adapter/driver identity unavailable");
        else if(Session().Begin(Reason)&&!Session().IsRecovery()&&!Session().IsBanned())
        {
            Allowed=Explicit||Session().HasValidation()||(!Session().RequiresValidation()&&IsValidatedConfiguration(DriverInfo().VendorId,DriverInfo().DeviceDescription,DriverInfo().UserDriverVersion));
#if !UE_BUILD_SHIPPING
            if(FParse::Param(FCommandLine::Get(),TEXT("SensorNativeForceValidation")))Allowed=false;
#endif
            ValidationRequested.store(!Allowed);if(Allowed)Session().Validate();
            Reason=Allowed?TEXT("validated native configuration"):TEXT("bounded native/RHI startup oracle pending; RHI until validated");
        }
        if(Reason.IsEmpty())Reason=Session().IsBanned()?TEXT("two unclean exits: this driver/adapter disabled until driver changes"):TEXT("safe RHI recovery launch");
        // Shipping compiles UE_LOG out; retain the same bounded decision in the
        // per-combination state file without enabling global Shipping logging.
        Session().RecordDecision(Reason);
        UE_LOG(LogTemp,Display,TEXT("[SensorNativeSafety] adapter=%s driver=%s native=%d reason=%s"),*DriverInfo().DeviceDescription,*DriverInfo().UserDriverVersion,Allowed?1:0,*Reason);
    }
    return Allowed&&!Disabled.load();
}
bool NeedsValidation(){return ValidationRequested.load()&&!Disabled.load();}
EStatus GetStatus(){FScopeLock Lock(&PolicyMutex);return Prepared?Session().GetStatus():EStatus::None;}
FSnapshot GetSnapshot()
{
    FScopeLock Lock(&PolicyMutex);FSnapshot S;
    if(Prepared){S.Status=Session().GetStatus();S.Failures=Session().FailureCount();S.Faulted=Session().HasDeviceFault()||!SensorNativeRayTracing::IsHealthy();}return S;
}
FString StateFileName(){return FPaths::GetCleanFilename(MarkerPath())+TEXT(".state");}
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
void RequestValidationForTests(){CanAttempt(false);FScopeLock Lock(&PolicyMutex);Allowed=false;Disabled.store(false);ValidationRequested.store(true);}
#endif
void CompleteValidation(bool Passed,const FString& Reason)
{
    FScopeLock Lock(&PolicyMutex);ValidationRequested.store(false);Allowed=Passed&&!Disabled.load();
    if(Allowed)Session().Validate();else {Session().Invalidate();Disabled.store(true);}
    Session().RecordDecision(Reason);
    UE_LOG(LogTemp,Display,TEXT("[SensorNativeSafety] startup oracle %s; native=%d reason=%s"),Passed?TEXT("PASS"):TEXT("FAIL"),Allowed?1:0,*Reason);
}
bool BeginSession(FString& Error){return Session().IsOwned()||Session().Begin(Error);}
void DisableForSession(const FString& Reason)
{
    const bool FirstDisable=!Disabled.exchange(true);
    FScopeLock Lock(&PolicyMutex);Allowed=false;ValidationRequested.store(false);
    const bool DeviceHealthy=SensorNativeRayTracing::IsHealthy();
    if(FirstDisable||!DeviceHealthy)Session().DisableForSession(DeviceHealthy,Reason);
    if(FirstDisable)UE_LOG(LogTemp,Warning,TEXT("[SensorNativeSafety] native disabled: %s; supported RHI on healthy device, safe recovery next launch after device loss"),*Reason);
}
void EndSession(bool Healthy){Session().End(Healthy);}
}
