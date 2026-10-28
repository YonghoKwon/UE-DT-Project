#include "SensorNativeSafety.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorNativeSafetyPolicyTest,"HATCH26.NativeSafetyPolicy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorNativeSafetyPolicyTest::RunTest(const FString&)
{
    const FString Adapter=TEXT("NVIDIA GeForce RTX 4070 Ti");
    TestTrue(TEXT("measured configuration enables default"),SensorNativeSafety::IsValidatedConfiguration(0x10de,Adapter,TEXT("621.01")));
    TestTrue(TEXT("numeric formatting preserves driver identity"),SensorNativeSafety::IsValidatedConfiguration(0x10de,Adapter,TEXT("621.1")));
    for(const FString& Version:{FString(TEXT("610.88")),FString(TEXT("")),FString(TEXT("621.01.0")),FString(TEXT("622.10")),FString(TEXT("621.01x"))})TestFalse(TEXT("unmeasured/old/unrecognized driver uses RHI"),SensorNativeSafety::IsValidatedConfiguration(0x10de,Adapter,Version));
    TestFalse(TEXT("unmeasured model uses RHI"),SensorNativeSafety::IsValidatedConfiguration(0x10de,TEXT("NVIDIA GeForce RTX 4070 Ti SUPER"),TEXT("621.01")));
    TestFalse(TEXT("other vendor uses RHI"),SensorNativeSafety::IsValidatedConfiguration(0x1002,Adapter,TEXT("621.01")));
    const FString EngineBarrier=TEXT("ID3D12CommandQueue1::ExecuteCommandLists: Using ResourceBarrier on Command List (0x1:'FD3D12CommandList (GPU 0)'): Before state (0x10: D3D12_RESOURCE_STATE_DEPTH_WRITE) of resource (0x2:'SceneDepthZ') (subresource: 1) specified by transition barrier does not match with the state (0x60: D3D12_RESOURCE_STATE_DEPTH_READ|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) specified in preceding ResourceBarrier or as InitialState");
    TestTrue(TEXT("exact recorded engine depth/list barrier has separate owner"),SensorNativeSafety::IsProvenEngineDepthBarrier(527,EngineBarrier));
    TestFalse(TEXT("native list is never excluded"),SensorNativeSafety::IsProvenEngineDepthBarrier(527,EngineBarrier.Replace(TEXT("FD3D12CommandList (GPU 0)"),TEXT("SensorOwnedLidarList"))));
    TestFalse(TEXT("native buffer is never excluded"),SensorNativeSafety::IsProvenEngineDepthBarrier(527,EngineBarrier.Replace(TEXT("SceneDepthZ"),TEXT("Job.Results"))));
    TestFalse(TEXT("similarly named buffer is not excluded"),SensorNativeSafety::IsProvenEngineDepthBarrier(527,EngineBarrier.Replace(TEXT("SceneDepthZ"),TEXT("SceneDepthZ_SensorNative"))));
    TestFalse(TEXT("other error id is not excluded"),SensorNativeSafety::IsProvenEngineDepthBarrier(528,EngineBarrier));
    TestFalse(TEXT("truncated description is not excluded"),SensorNativeSafety::IsProvenEngineDepthBarrier(527,TEXT("SceneDepthZ FD3D12CommandList (GPU 0)")));
    const FString Path=FPaths::ProjectSavedDir()/TEXT("Reports/LIDAR-NATIVE-LIFETIME-26h/safety-tests")/(FGuid::NewGuid().ToString()+TEXT(".guard"));
    FString Error;SensorNativeSafety::FSessionGuard First(Path),Second(Path);
    TestTrue(TEXT("new session starts"),First.Begin(Error));TestFalse(TEXT("other session sees active guard"),Second.CanStartDefault());
    TestFalse(TEXT("active marker is never overwritten"),Second.Begin(Error));
    First.End(true);TestTrue(TEXT("clean close permits next native session"),Second.CanStartDefault());TestTrue(TEXT("second session starts after clean close"),Second.Begin(Error));
    Second.End(false);SensorNativeSafety::FSessionGuard AfterCrash(Path);TestFalse(TEXT("unclean close keeps next-launch RHI fallback"),AfterCrash.CanStartDefault());
    IFileManager::Get().Delete(*Path,false,true);
    return true;
}
#endif
