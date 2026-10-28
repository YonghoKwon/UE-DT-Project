#include "NativeLidarLifetimeCommandlet.h"
#include "SensorOwnedRayTracing.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

extern MA0T10_DT_API int32 RunNativeLifetimeHarnessCommandlet();
UNativeLidarLifetimeCommandlet::UNativeLidarLifetimeCommandlet()
{IsClient=true;IsServer=false;IsEditor=true;LogToConsole=true;ShowErrorCount=true;}
int32 UNativeLidarLifetimeCommandlet::Main(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("AllowCommandletRendering"))||!SensorOwnedRayTracing::IsAvailable())
    {UE_LOG(LogTemp,Error,TEXT("Native lifetime commandlet requires actual D3D12 HWRT and -AllowCommandletRendering"));return 2;}
    return RunNativeLifetimeHarnessCommandlet();
}
