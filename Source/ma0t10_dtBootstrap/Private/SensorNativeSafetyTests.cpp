#include "SensorNativeSafety.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "SensorNativeRayTracing.h"
#include "SensorOwnedRayTracing.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
    // A dead PID simulates an unclean process, not a concurrent live instance.
    FFileHelper::SaveStringToFile(TEXT("schema=2\npid=0\nstart=0\n"),*Path);
    SensorNativeSafety::FSessionGuard Recovery(Path);TestTrue(TEXT("stale marker can be exclusively claimed"),Recovery.Begin(Error));
    TestTrue(TEXT("first unclean native exit forces one recovery launch"),Recovery.IsRecovery());TestEqual(TEXT("first failure counted once"),Recovery.FailureCount(),1);
    TestTrue(TEXT("one-failure recovery is visible to callers"),Recovery.GetStatus()==SensorNativeSafety::EStatus::Recovery);
    Recovery.End(true);SensorNativeSafety::FSessionGuard Retry(Path);TestTrue(TEXT("healthy recovery clears marker and permits retry"),Retry.Begin(Error));TestFalse(TEXT("retry is not permanently pinned to RHI"),Retry.IsRecovery());
    Retry.End(false);FFileHelper::SaveStringToFile(TEXT("schema=2\npid=0\nstart=0\n"),*Path);
    SensorNativeSafety::FSessionGuard Banned(Path);TestTrue(TEXT("second failure claimed"),Banned.Begin(Error));TestTrue(TEXT("two unclean exits disable this combination"),Banned.IsBanned());Banned.End(true);
    TestTrue(TEXT("two-failure ban is visible and takes priority over recovery"),Banned.GetStatus()==SensorNativeSafety::EStatus::Banned);
    SensorNativeSafety::FSessionGuard StillBanned(Path);TestTrue(TEXT("banned RHI run owns its own guard"),StillBanned.Begin(Error));TestTrue(TEXT("safe RHI run does not clear two-failure ban"),StillBanned.IsBanned());StillBanned.End(true);
    SensorNativeSafety::FSessionGuard NewDriver(Path+TEXT("-driver2"));TestTrue(TEXT("different driver has independent state"),NewDriver.Begin(Error));TestFalse(TEXT("driver change clears combination-specific ban"),NewDriver.IsBanned());NewDriver.Validate();TestTrue(TEXT("successful oracle caches validation"),NewDriver.HasValidation());NewDriver.End(true);
    SensorNativeSafety::FSessionGuard Cached(Path+TEXT("-driver2"));TestTrue(TEXT("validated combination reopens"),Cached.Begin(Error));TestTrue(TEXT("successful validation survives clean restart"),Cached.HasValidation());Cached.RecordDecision(TEXT("native oracle PASS; exact records"));Cached.End(true);
    FString Decision;FFileHelper::LoadFileToString(Decision,*(Path+TEXT("-driver2.state")));TestTrue(TEXT("decision remains auditable when Shipping UE_LOG is compiled out"),Decision.Contains(TEXT("reason=native oracle PASS; exact records")));
    IFileManager::Get().Delete(*Path,false,true);IFileManager::Get().Delete(*(Path+TEXT(".state")),false,true);IFileManager::Get().Delete(*(Path+TEXT("-driver2.state")),false,true);
    FFileHelper::SaveStringToFile(TEXT("schema=1\npid=0\n"),*(Path+TEXT("-old")));
    SensorNativeSafety::FSessionGuard Migrated(Path+TEXT("-new"),Path+TEXT("-old"));TestTrue(TEXT("legacy marker migrates without ignoring an old crash"),Migrated.Begin(Error));TestTrue(TEXT("legacy crash receives one safe RHI recovery"),Migrated.IsRecovery());Migrated.End(true);
    SensorNativeSafety::FSessionGuard AfterMigration(Path+TEXT("-new"),Path+TEXT("-old"));TestTrue(TEXT("legacy recovery permits next retry"),AfterMigration.Begin(Error));TestFalse(TEXT("legacy crash is not counted twice"),AfterMigration.IsRecovery());AfterMigration.End(true);IFileManager::Get().Delete(*(Path+TEXT("-new.state")),false,true);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorNativeSoftDisableTest,"HATCH26.NativeSoftDisable",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorNativeSoftDisableTest::RunTest(const FString&)
{
    const FString Path=FPaths::ProjectSavedDir()/TEXT("Reports/LIDAR-MEMORY-26-FINAL/safety-tests")/(FGuid::NewGuid().ToString()+TEXT(".guard"));
    FString Error;
    SensorNativeSafety::FSessionGuard Soft(Path);
    TestTrue(TEXT("soft-disable fixture begins"),Soft.Begin(Error));Soft.Validate();
    Soft.DisableForSession(true,TEXT("healthy-device native submission failed"));
    TestFalse(TEXT("soft disable invalidates cached oracle"),Soft.HasValidation());
    TestTrue(TEXT("soft disable requires next-launch oracle even for recorded adapters"),Soft.RequiresValidation());
    Soft.End(true);
    TestFalse(TEXT("orderly healthy-device exit removes marker"),IFileManager::Get().FileExists(*Path));
    SensorNativeSafety::FSessionGuard Next(Path);
    TestTrue(TEXT("next session begins"),Next.Begin(Error));
    TestFalse(TEXT("soft disable is not an unclean-exit recovery"),Next.IsRecovery());
    TestEqual(TEXT("soft disable does not increase failure count"),Next.FailureCount(),0);
    TestTrue(TEXT("oracle invalidation persists through clean restart"),Next.RequiresValidation());
    TestFalse(TEXT("invalidated cache stays invalid on restart"),Next.HasValidation());
    Next.Validate();TestFalse(TEXT("successful oracle clears revalidation request"),Next.RequiresValidation());Next.End(true);
    SensorNativeSafety::FSessionGuard Lost(Path);
    TestTrue(TEXT("device-loss fixture begins"),Lost.Begin(Error));Lost.Validate();
    Lost.DisableForSession(false,TEXT("device removed"));
    Lost.DisableForSession(true,TEXT("later soft failure cannot clear device fault"));Lost.End(true);
    TestTrue(TEXT("device fault retains marker despite orderly outer shutdown"),IFileManager::Get().FileExists(*Path));
    // The same live test process owns the synthetic marker; replace only its
    // PID with a dead-owner fixture to model the next process, not concurrency.
    FFileHelper::SaveStringToFile(TEXT("schema=2\npid=0\nstart=0\n"),*Path);
    SensorNativeSafety::FSessionGuard Recovery(Path);
    TestTrue(TEXT("device-loss next process begins"),Recovery.Begin(Error));
    TestTrue(TEXT("device loss requires safe RHI recovery"),Recovery.IsRecovery());
    TestEqual(TEXT("device loss increments one failure"),Recovery.FailureCount(),1);Recovery.End(true);
    IFileManager::Get().Delete(*Path,false,true);IFileManager::Get().Delete(*(Path+TEXT(".state")),false,true);
    return true;
}
class FWaitNativeStartupOracle final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();
    TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe> ProductionScan;
    uint64 NativeBefore=0;
public:
    explicit FWaitNativeStartupOracle(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(ProductionScan)
        {
            TArray<FSensorRtHit> Hits;FString Error;
            if(!ProductionScan->Poll(Hits,Error)&&FPlatformTime::Seconds()-Started<10.)return false;
            Test->TestTrue(TEXT("production acquisition completes after startup decision"),Error.IsEmpty()&&Hits.Num()==1);
            if(Hits.Num()==1)Test->TestTrue(TEXT("fallback preserves exact production hit"),Hits[0].NormalAndDistance.W==10.f&&Hits[0].Mesh==0&&Hits[0].Surface==0);
            const bool Mismatch=FParse::Param(FCommandLine::Get(),TEXT("SensorNativeOracleMismatch"));
            const uint64 NativeAfter=SensorNativeRayTracing::GetStats().Dispatches;
            Test->TestTrue(TEXT("mismatch actually dispatches through supported RHI, not native"),Mismatch?NativeAfter==NativeBefore:NativeAfter>NativeBefore);
            if(Hits.Num())SensorOwnedRayTracing::RecycleHitArray(MoveTemp(Hits));ProductionScan.Reset();return true;
        }
        if(SensorNativeSafety::NeedsValidation()&&FPlatformTime::Seconds()-Started<7.)return false;
        Test->TestFalse(TEXT("startup oracle has a bounded terminal state"),SensorNativeSafety::NeedsValidation());
        const bool Mismatch=FParse::Param(FCommandLine::Get(),TEXT("SensorNativeOracleMismatch"));
        Test->TestEqual(TEXT("oracle success enables native; injected mismatch retains supported RHI"),SensorOwnedRayTracing::UsesNativeBackend(),!Mismatch);
        Test->TestTrue(TEXT("startup validation leaves device healthy"),SensorNativeRayTracing::IsHealthy());
        FSensorRtRequest Request;auto Vertices=MakeShared<TArray<FVector3f>,ESPMode::ThreadSafe>();*Vertices={FVector3f(-4,-4,10),FVector3f(4,-4,10),FVector3f(0,4,10)};
        auto Surfaces=MakeShared<TArray<FVector4f>,ESPMode::ThreadSafe>();Surfaces->Add(FVector4f(0,0,-1,0));
        auto& Mesh=Request.Meshes.AddDefaulted_GetRef();Mesh.Revision=0xffff260000000010ull;Mesh.Vertices=Vertices;Mesh.NormalsAndSurfaces=Surfaces;
        auto& Ray=Request.Rays.AddDefaulted_GetRef();Ray.Origin=FVector3f::ZeroVector;Ray.Direction=FVector3f(0,0,1);Ray.MaxDistance=100;
        NativeBefore=SensorNativeRayTracing::GetStats().Dispatches;ProductionScan=SensorOwnedRayTracing::Submit(MoveTemp(Request));
        Test->TestTrue(TEXT("unchanged production submission API accepts post-validation work"),ProductionScan.IsValid());return !ProductionScan;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorNativeStartupOracleTest,"HATCH26.StartupOracleRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorNativeStartupOracleTest::RunTest(const FString&)
{
    if(!SensorOwnedRayTracing::IsAvailable()||!SensorNativeRayTracing::IsSupported()){AddError(TEXT("actual D3D12 DXR Tier1.1 required"));return false;}
    SensorNativeSafety::RequestValidationForTests();ADD_LATENT_AUTOMATION_COMMAND(FWaitNativeStartupOracle(this));return true;
}
#endif
