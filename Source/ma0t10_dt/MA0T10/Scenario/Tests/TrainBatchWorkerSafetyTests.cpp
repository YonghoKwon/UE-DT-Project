#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchWorkerSafety.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchProcessPool.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchProcessProtocol.h"
#include "Device/Contracts/LocalCapturePath.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"

namespace
{
bool SaveSafetyFixture(const FString& Path,const FString& Text)
{IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);return FFileHelper::SaveStringToFile(Text,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);}
FString SafetyFixtureRoot()
{return LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("TrainBatchSafetyTests")/FGuid::NewGuid().ToString(EGuidFormats::Digits));}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchGuardCleanupTest,"MA0T10.TrainBatch.ControlledExitGuard",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchGuardCleanupTest::RunTest(const FString&)
{
    using namespace TrainBatchWorkerSafety;const FString Root=SafetyFixtureRoot(),State=GuardStatePath(Root,0),Marker=State.LeftChop(6);FExitEvidence E;E.CoordinatorTerminated=true;
    TestFalse(TEXT("slot traversal rejected"),!GuardStatePath(Root,4).IsEmpty());
    SaveSafetyFixture(State,TEXT("failures=1\nvalidated=0\n"));TestFalse(TEXT("one prior unclean exit is not banned"),GuardBanned(Root,0));
    SaveSafetyFixture(Marker,TEXT("schema=2\npid=4294967295\nstart=37\n"));
    TestFalse(TEXT("different lease stamp cannot remove marker"),CleanupControlledMarker(Root,0,MAX_uint32,38,E));
    E.NativeFault=true;TestFalse(TEXT("fault evidence preserves guard even after controlled termination"),CleanupControlledMarker(Root,0,MAX_uint32,37,E));
    E.NativeFault=false;SaveSafetyFixture(State,TEXT("failures=1\nfaulted=1\n"));TestTrue(TEXT("fault survives a missing last heartbeat/result"),GuardFaulted(Root,0));TestFalse(TEXT("persisted fault also prevents marker cleanup"),CleanupControlledMarker(Root,0,MAX_uint32,37,E));
    SaveSafetyFixture(State,TEXT("failures=1\nfaulted=0\n"));TestTrue(TEXT("dead exactly owned timeout marker cleaned"),CleanupControlledMarker(Root,0,MAX_uint32,37,E));
    TestTrue(TEXT("prior safety counter retained"),IFileManager::Get().FileExists(*State));TestFalse(TEXT("no new unclean exit recorded"),GuardBanned(Root,0));
    bool Alive=false;const uint32 Pid=FPlatformProcess::GetCurrentProcessId();const uint64 Stamp=TrainBatchProcess::ProcessStamp(Pid,Alive);
    SaveSafetyFixture(Marker,FString::Printf(TEXT("pid=%u\nstart=%llu\n"),Pid,Stamp));TestFalse(TEXT("live exact owner protected"),CleanupControlledMarker(Root,0,Pid,Stamp,E));TestTrue(TEXT("live marker retained"),IFileManager::Get().FileExists(*Marker));
    SaveSafetyFixture(State,TEXT("failures=2\nvalidated=0\n"));TestTrue(TEXT("guard's two-unclean policy authoritative"),GuardBanned(Root,0));
    IFileManager::Get().Delete(*State,false,true);TestFalse(TEXT("manual state reset clears authoritative ban"),GuardBanned(Root,0));
    if(LocalCapturePath::SafeDescendant(LocalCapturePath::Full(FPaths::ProjectSavedDir()),Root))IFileManager::Get().DeleteDirectory(*Root,false,true);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchPoolExitEvidenceTest,"MA0T10.TrainBatch.PoolExitEvidence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchPoolExitEvidenceTest::RunTest(const FString&)
{
    struct FCase{const TCHAR* Name;bool Timeout,Cancel,Fault;int32 Prior,GuardFailures;bool ResultBanned;int32 Expected;bool Ban;bool StoredFault=false;};
    const FCase Cases[]={
        {TEXT("missing result alone"),false,false,false,0,0,false,0,false},
        {TEXT("heartbeat termination"),true,false,false,0,0,false,0,false},
        {TEXT("user cancellation"),false,true,false,0,0,false,0,false},
        {TEXT("explicit native fault"),false,false,true,0,0,false,1,false},
        {TEXT("two diagnostic faults alone never ban"),false,false,true,1,0,false,2,false},
        {TEXT("controlled exit is not a new native failure"),true,false,true,1,0,false,1,false},
        {TEXT("authoritative guard ban"),false,false,false,0,2,false,0,true},
        {TEXT("worker reports guard ban"),false,false,false,0,0,true,0,true},
        {TEXT("persisted device fault without result"),false,false,false,0,1,false,1,false,true}};
    const FString Base=SafetyFixtureRoot();int32 Index=0;
    for(const auto& C:Cases)
    {
        FTrainBatchProcessPool Pool;Pool.Root=Base/FString::FromInt(Index++)/TEXT("pool");Pool.WorkerBase=FPaths::GetPath(Pool.Root)/TEXT("Workers");Pool.Identity=TEXT("test");Pool.Preset=MakeShared<FJsonObject>();Pool.Job.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);Pool.Queue.Initialize(1);const auto Lease=*Pool.Queue.LeaseNext(0,FPlatformTime::Seconds());
        Pool.Slots.SetNum(1);auto& S=Pool.Slots[0];S.Task=0;S.Lease=Lease.Lease;S.Epoch=Lease.Epoch;S.Pid=MAX_uint32;S.Stamp=37;S.Ipc=Pool.Root/TEXT("leases")/S.Lease;S.CoordinatorTerminated=C.Timeout;S.CancelRequested=C.Cancel;S.NativeFault=C.Fault;Pool.Stopping=C.Cancel;
        if(C.Prior)Pool.NativeFailureLeases.Add(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        SaveSafetyFixture(TrainBatchWorkerSafety::GuardStatePath(Pool.WorkerBase,0),FString::Printf(TEXT("failures=%d\nfaulted=%d\n"),C.GuardFailures,C.StoredFault?1:0));
        if(C.ResultBanned)
        {auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("lease"),S.Lease);R->SetNumberField(TEXT("epoch"),double(S.Epoch));R->SetStringField(TEXT("phase"),TEXT("failed"));R->SetNumberField(TEXT("native_status"),2);TrainBatchProcess::Write(S.Ipc,S.Ipc/TEXT("result.json"),R);}
        Pool.Tick(false);TestEqual(C.Name,Pool.NativeFailureLeases.Num(),C.Expected);TestEqual(*(FString(C.Name)+TEXT(" ban")),Pool.BanNative,C.Ban);
        const auto Cache=TrainBatchProcess::Read(Pool.WorkerBase/TEXT("safety-test.json"));if(TestTrue(TEXT("safety cache written"),Cache.IsValid())){TestEqual(TEXT("schema2"),int32(Cache->GetNumberField(TEXT("schema_version"))),2);TestEqual(TEXT("cache follows guard decision, not raw exit count"),Cache->GetBoolField(TEXT("banned")),C.Ban);}
    }
    if(LocalCapturePath::SafeDescendant(LocalCapturePath::Full(FPaths::ProjectSavedDir()),Base))IFileManager::Get().DeleteDirectory(*Base,false,true);return true;
}
#endif
