#include "SensorNativeSafety.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include <atomic>

namespace SensorNativeSafety
{
namespace
{
std::atomic<bool> Disabled{false},Warned{false};
const FGPUDriverInfo& DriverInfo()
{
    // RHI captured these at device initialization. GetGPUDriverInfo itself reads
    // game-thread-only CVars and must not run lazily from render-thread dispatch.
    static const FGPUDriverInfo Info=[](){FGPUDriverInfo I;I.VendorId=GRHIVendorId;I.DeviceDescription=GRHIAdapterName;I.UserDriverVersion=GRHIAdapterUserDriverVersion;return I;}();
    return Info;
}
const FString& MarkerPath()
{
    static const FString Path=FPaths::ProjectSavedDir()/TEXT("NativeLidarSafety")/FString::Printf(TEXT("active-%08x.guard"),FCrc::StrCrc32(*(DriverInfo().DeviceDescription+TEXT("|")+DriverInfo().UserDriverVersion)));
    return Path;
}
FSessionGuard& Session(){static FSessionGuard Guard(MarkerPath());return Guard;}
}
bool FSessionGuard::CanStartDefault() const{return Owned.load()||!IFileManager::Get().FileExists(*Path);}
bool FSessionGuard::Begin(FString& Error)
{
    if(Owned.load())return true;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    Lease.Reset(IFileManager::Get().CreateFileWriter(*Path,FILEWRITE_NoReplaceExisting|FILEWRITE_AllowRead));
    if(!Lease){Error=TEXT("native safety marker exists or cannot be created; RHI selected");return false;}
    const FString Text=FString::Printf(TEXT("schema=1\npid=%u\n"),FPlatformProcess::GetCurrentProcessId());FTCHARToUTF8 Bytes(*Text);Lease->Serialize((void*)Bytes.Get(),Bytes.Length());Lease->Flush();
    if(Lease->IsError()){Error=TEXT("native safety marker write failed; RHI selected");Lease.Reset();return false;}
    Owned.store(true);return true;
}
void FSessionGuard::End(bool Healthy)
{
    if(!Owned.load())return;
    const bool Closed=!Lease||Lease->Close();Lease.Reset();
    if(Healthy&&Closed)IFileManager::Get().Delete(*Path,false,true);
    Owned.store(false);
}
bool IsValidatedConfiguration(uint32 VendorId,const FString& Adapter,const FString& Driver)
{
    // Defaults follow measured hardware only. Unknown/new drivers retain RHI;
    // explicit -SensorRtNative remains the diagnostic entry on supported DXR.
    if(VendorId!=0x10de||!Adapter.Equals(TEXT("NVIDIA GeForce RTX 4070 Ti"),ESearchCase::IgnoreCase))return false;
    TArray<FString> Parts;Driver.ParseIntoArray(Parts,TEXT("."),false);
    return Parts.Num()==2&&Parts[0].IsNumeric()&&Parts[1].IsNumeric()&&FCString::Atoi(*Parts[0])==621&&FCString::Atoi(*Parts[1])==1;
}
bool CanAttempt(bool Explicit)
{
    if(Disabled.load())return false;
    if(Explicit)return true;
    static const bool Validated=IsValidatedConfiguration(DriverInfo().VendorId,DriverInfo().DeviceDescription,DriverInfo().UserDriverVersion);
    if(!Validated)return false;
    if(Session().IsOwned())return true;
    static const bool CleanPrevious=Session().CanStartDefault();
    if(!CleanPrevious&&!Warned.exchange(true))UE_LOG(LogTemp,Warning,TEXT("[SensorNativeSafety] previous unclean native session; supported RHI selected"));
    return CleanPrevious;
}
bool BeginSession(FString& Error)
{
    if(Session().IsOwned())return true;
    if(!Session().Begin(Error))return false;
    UE_LOG(LogTemp,Display,TEXT("[SensorNativeSafety] native session guarded; driver=%s adapter=%s"),*DriverInfo().UserDriverVersion,*DriverInfo().DeviceDescription);return true;
}
void DisableForSession(const FString& Reason)
{
    if(!Disabled.exchange(true))UE_LOG(LogTemp,Warning,TEXT("[SensorNativeSafety] native disabled for this session: %s; future admissions use supported RHI"),*Reason);
}
void EndSession(bool Healthy)
{
    Session().End(Healthy&&!Disabled.load());
    // Device removal/unclean exit leaves the marker for next-launch fallback.
}
}
