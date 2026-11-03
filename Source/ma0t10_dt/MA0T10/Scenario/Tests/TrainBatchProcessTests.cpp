#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchProcessProtocol.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchCodec.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Device/Contracts/LocalCapturePath.h"
#include "Json.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainLeaseTest,"MA0T10.TrainBatch.ProcessLease",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainLeaseTest::RunTest(const FString&)
{
    using namespace TrainBatchProcess;FQueue Q;Q.Initialize(3);
    const auto First=*Q.LeaseNext(0,10),Second=*Q.LeaseNext(1,10);
    TestNotEqual(TEXT("different workers receive distinct scenarios"),First.Index,Second.Index);
    TestTrue(TEXT("current heartbeat"),Q.Heartbeat(First.Index,First.Lease,First.Epoch,12));TestEqual(TEXT("monotonic lease renewal"),Q.Tasks[First.Index].LastSeen,12.);
    TestTrue(TEXT("failed worker requeued"),Q.Retry(First.Index,First.Lease,First.Epoch,TEXT("owned process exited")));
    const auto Retry=*Q.LeaseNext(0,13);TestEqual(TEXT("same pending scenario"),Retry.Index,First.Index);TestTrue(TEXT("new lease epoch"),Retry.Epoch>First.Epoch);
    TestFalse(TEXT("stale completion rejected"),Q.Complete(First.Index,First.Lease,First.Epoch));
    TestFalse(TEXT("stale heartbeat rejected"),Q.Heartbeat(First.Index,First.Lease,First.Epoch,14));
    TestTrue(TEXT("other worker continues independently"),Q.Complete(Second.Index,Second.Lease,Second.Epoch));
    TestTrue(TEXT("safe recovery does not exhaust retry count"),Q.Retry(Retry.Index,Retry.Lease,Retry.Epoch,TEXT("one RHI recovery"),false));
    TestEqual(TEXT("only actual failure counted"),Q.Tasks[Retry.Index].Attempts,1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainLeaseResumeTest,"MA0T10.TrainBatch.ProcessLeaseResume",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainLeaseResumeTest::RunTest(const FString&)
{
    using namespace TrainBatchProcess;FQueue Q;Q.Initialize(2);const auto T=*Q.LeaseNext(0,1);Q.Tasks[1].State=ETask::Completed;
    FString Text=Encode(Q.Json()),Error;TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O);
    FQueue R;TestTrue(TEXT("disk model round-trip"),R.Restore(O,2,Error));TestTrue(TEXT("in-flight identity retained for live-process adoption"),R.Matches(T.Index,T.Lease,T.Epoch));TestTrue(TEXT("completed task retained"),R.Tasks[1].State==ETask::Completed);
    TestFalse(TEXT("cannot resume different task set"),R.Restore(O,3,Error));
    for(int32 I=0;I<3;++I){auto* V=Q.LeaseNext(0,2+I);if(!V){if(Q.Tasks[0].State==ETask::Leased)V=&Q.Tasks[0];else break;}const auto C=*V;Q.Retry(C.Index,C.Lease,C.Epoch,TEXT("failed"));}
    TestTrue(TEXT("bounded automatic retries"),Q.Tasks[0].State==ETask::Failed);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBudgetTest,"MA0T10.TrainBatch.GpuBudgetAdmission",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBudgetTest::RunTest(const FString&)
{
    using namespace TrainBatchProcess;constexpr uint64 GiB=1024ull*1024*1024;
    TestEqual(TEXT("12GiB is not unconditional M4"),Capacity(4,100,12*GiB,GiB,2*GiB),3);
    TestEqual(TEXT("requested upper bound"),Capacity(1,100,12*GiB,GiB,2*GiB),1);
    TestEqual(TEXT("not more workers than items"),Capacity(4,1,12*GiB,GiB,2*GiB),1);
    TestEqual(TEXT("no budget no launch"),Capacity(4,10,0,0,GiB),0);
    TestEqual(TEXT("reserve protected"),Capacity(4,10,12*GiB,11*GiB,GiB),0);
    TestEqual(TEXT("overflow rejected"),Capacity(4,10,MAX_uint64,0,MAX_uint64),0);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainOwnershipTest,"MA0T10.TrainBatch.ProcessIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainOwnershipTest::RunTest(const FString&)
{
    bool Alive=false;const uint32 Pid=FPlatformProcess::GetCurrentProcessId();const uint64 Stamp=TrainBatchProcess::ProcessStamp(Pid,Alive);
    TestTrue(TEXT("current process identity available"),Alive&&Stamp!=0);TestTrue(TEXT("exact PID plus creation stamp"),TrainBatchProcess::SameProcess(Pid,Stamp));
    TestFalse(TEXT("reused PID identity rejected"),TrainBatchProcess::SameProcess(Pid,Stamp+1));TestFalse(TEXT("unknown identity not adopted"),TrainBatchProcess::SameProcess(Pid,0));
    TestTrue(TEXT("quoted internal path with spaces"),TrainBatchProcess::Quote(TEXT("C:/Unreal Projects/worker"))==TEXT("\"C:/Unreal Projects/worker\""));TestTrue(TEXT("argument injection rejected"),TrainBatchProcess::Quote(TEXT("x\" -evil")).IsEmpty());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainWorkerArgumentsTest,"MA0T10.TrainBatch.WorkerExecutableArguments",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainWorkerArgumentsTest::RunTest(const FString&)
{
    using namespace TrainBatchProcess;const FString Map=TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap");
    TestEqual(TEXT("packaged executable receives only its cooked map"),WorkerMapArguments(FString(),Map,false),Quote(Map));
    TestEqual(TEXT("Editor executable receives absolute project and game mode"),WorkerMapArguments(TEXT("C:/Project Space/game.uproject"),Map,true),TEXT("\"C:/Project Space/game.uproject\" ")+Quote(Map)+TEXT(" -game"));
    TestTrue(TEXT("missing Editor project rejected"),WorkerMapArguments(FString(),Map,true).IsEmpty());
    TestTrue(TEXT("missing map rejected"),WorkerMapArguments(TEXT("C:/game.uproject"),FString(),false).IsEmpty());
    TestTrue(TEXT("quote injection rejected"),WorkerMapArguments(FString(),TEXT("/Game/X\" -ExecCmds=quit"),false).IsEmpty());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainWorkerMapBoundaryTest,"MA0T10.TrainBatch.WorkerMapBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainWorkerMapBoundaryTest::RunTest(const FString&)
{
    using namespace TrainBatchProcess;
    TestTrue(TEXT("saved project map allowed"),IsProjectMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap")));
    for(const TCHAR* Invalid:{TEXT(""),TEXT("/Engine/Maps/Entry"),TEXT("/Game/../Other"),TEXT("/Game/X?listen"),TEXT("C:/Other.umap")})TestFalse(TEXT("map URL/traversal/non-project map rejected"),IsProjectMap(Invalid));
    FString Error;TestTrue(TEXT("no world cannot request travel"),EnterAssignedMap(nullptr,TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),Error)==EMapEntry::Rejected);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainLiveLeaseReadTest,"MA0T10.TrainBatch.LiveLeaseRead",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainLiveLeaseReadTest::RunTest(const FString&)
{
    const FString Root=LocalCapturePath::Full(FPaths::ProjectSavedDir()/TEXT("Reports/TrainLeaseRead"));IFileManager::Get().MakeDirectory(*Root,true);
    const FString Path=Root/(FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".lock"));
    TUniquePtr<FArchive> Owner(IFileManager::Get().CreateFileWriter(*Path,FILEWRITE_NoReplaceExisting|FILEWRITE_AllowRead));if(!Owner)return false;
    const FString Text=TEXT("{\"pid\":123,\"stamp\":\"134380815887182186\"}");FTCHARToUTF8 U(*Text);Owner->Serialize((void*)U.Get(),U.Length());Owner->Flush();
    const auto O=TrainBatchProcess::Read(Path);TestTrue(TEXT("live writer permits protocol reader"),O.IsValid());FString Stamp;if(O)O->TryGetStringField(TEXT("stamp"),Stamp);TestEqual(TEXT("64-bit creation stamp never rounded through JSON number"),Stamp,FString(TEXT("134380815887182186")));
    Owner.Reset();if(LocalCapturePath::SafeDescendant(Root,Path))IFileManager::Get().Delete(*Path,false,true);return true;
}
#endif
