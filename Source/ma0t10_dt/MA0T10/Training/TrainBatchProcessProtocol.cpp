#include "ma0t10_dt/MA0T10/Training/TrainBatchProcessProtocol.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchCodec.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchStorage.h"
#include "Device/Contracts/LocalCapturePath.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "RHIGlobals.h"
#include "Json.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include <dxgi1_4.h>
#endif

void TrainBatchProcess::FQueue::Initialize(int32 Count)
{Tasks.Reset();for(int32 I=0;I<Count;++I){FTask T;T.Index=I;Tasks.Add(MoveTemp(T));}}
TrainBatchProcess::FTask* TrainBatchProcess::FQueue::LeaseNext(int32 Slot,double Now)
{
    auto* T=Tasks.FindByPredicate([](const FTask& V){return V.State==ETask::Pending;});if(!T)return nullptr;
    T->State=ETask::Leased;T->Slot=Slot;T->Lease=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);++T->Epoch;T->LastSeen=Now;return T;
}
bool TrainBatchProcess::FQueue::Matches(int32 I,const FString& Lease,uint64 Epoch) const
{return Tasks.IsValidIndex(I)&&Tasks[I].State==ETask::Leased&&Tasks[I].Lease==Lease&&Tasks[I].Epoch==Epoch;}
bool TrainBatchProcess::FQueue::Heartbeat(int32 I,const FString& Lease,uint64 Epoch,double Now)
{if(!Matches(I,Lease,Epoch))return false;Tasks[I].LastSeen=Now;return true;}
bool TrainBatchProcess::FQueue::Complete(int32 I,const FString& Lease,uint64 Epoch)
{if(!Matches(I,Lease,Epoch))return false;Tasks[I].State=ETask::Completed;return true;}
bool TrainBatchProcess::FQueue::Retry(int32 I,const FString& Lease,uint64 Epoch,const FString& Why,bool CountFailure)
{
    if(!Matches(I,Lease,Epoch))return false;auto& T=Tasks[I];if(CountFailure)++T.Attempts;
    T.State=T.Attempts>2?ETask::Failed:ETask::Pending;T.Error=Why;T.Slot=INDEX_NONE;return true;
}
bool TrainBatchProcess::FQueue::Terminal() const
{return !Tasks.ContainsByPredicate([](const auto& T){return T.State==ETask::Pending||T.State==ETask::Leased;});}
TSharedRef<FJsonObject> TrainBatchProcess::FQueue::Json() const
{
    auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> A;
    for(const auto& T:Tasks){auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("index"),T.Index);V->SetNumberField(TEXT("state"),int32(T.State));V->SetNumberField(TEXT("attempts"),T.Attempts);V->SetNumberField(TEXT("slot"),T.Slot);V->SetStringField(TEXT("lease"),T.Lease);V->SetNumberField(TEXT("epoch"),double(T.Epoch));V->SetStringField(TEXT("error"),T.Error);A.Add(MakeShared<FJsonValueObject>(V));}
    O->SetArrayField(TEXT("tasks"),A);return O;
}
bool TrainBatchProcess::FQueue::Restore(const TSharedPtr<FJsonObject>& O,int32 Count,FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(!O||!O->TryGetArrayField(TEXT("tasks"),A)||A->Num()!=Count){Error=TEXT("Pool task count mismatch");return false;}
    Initialize(Count);
    for(int32 I=0;I<Count;++I)
    {
        const TSharedPtr<FJsonObject>* V=nullptr;double Index=0,State=0,Attempts=0,Slot=0,Epoch=0;auto& T=Tasks[I];
        if(!(*A)[I]->TryGetObject(V)||!(*V)->TryGetNumberField(TEXT("index"),Index)||Index!=I||!(*V)->TryGetNumberField(TEXT("state"),State)||State<0||State>3||State!=FMath::FloorToDouble(State)||!(*V)->TryGetNumberField(TEXT("attempts"),Attempts)||Attempts<0||Attempts>3||Attempts!=FMath::FloorToDouble(Attempts)||!(*V)->TryGetNumberField(TEXT("slot"),Slot)||Slot< -1||Slot>3||Slot!=FMath::FloorToDouble(Slot)||!(*V)->TryGetNumberField(TEXT("epoch"),Epoch)||Epoch<0||Epoch>100000||Epoch!=FMath::FloorToDouble(Epoch)||!(*V)->TryGetStringField(TEXT("lease"),T.Lease))
        {Error=TEXT("Corrupt pool task");Tasks.Reset();return false;}
        T.State=ETask(int32(State));T.Attempts=int32(Attempts);T.Slot=int32(Slot);T.Epoch=uint64(Epoch);(*V)->TryGetStringField(TEXT("error"),T.Error);
        if(T.State==ETask::Leased&&!TrainBatchCodec::CanonicalGuid(T.Lease)){Error=TEXT("Invalid restored lease");return false;}
        T.LastSeen=FPlatformTime::Seconds();
    }
    return true;
}
int32 TrainBatchProcess::Capacity(int32 Requested,int32 Items,uint64 Budget,uint64 Usage,uint64 Peak)
{
    if(Requested<1||Requested>4||Items<1||!Budget||!Peak)return 0;
    const uint64 Reserve=FMath::Max<uint64>(2ull*1024*1024*1024,Budget/5);
    if(Usage>=Budget||Reserve>=Budget-Usage||Peak>(MAX_uint64-3)/5)return 0;
    const uint64 PerWorker=(Peak*5+3)/4;
    return FMath::Min(FMath::Min(Requested,Items),int32(FMath::Min<uint64>(4,(Budget-Usage-Reserve)/PerWorker)));
}
uint64 TrainBatchProcess::ProcessStamp(uint32 Pid,bool& Alive)
{
    Alive=false;
#if PLATFORM_WINDOWS
    HANDLE P=::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,0,Pid);if(!P){Alive=GetLastError()==ERROR_ACCESS_DENIED;return 0;}
    DWORD Code=0;FILETIME Created={},Exit={},Kernel={},User={};Alive=GetExitCodeProcess(P,&Code)&&Code==STILL_ACTIVE;
    const bool Ok=GetProcessTimes(P,&Created,&Exit,&Kernel,&User)!=0;CloseHandle(P);return Ok?(uint64(Created.dwHighDateTime)<<32)|Created.dwLowDateTime:0;
#else
    return 0;
#endif
}
bool TrainBatchProcess::SameProcess(uint32 Pid,uint64 Stamp)
{bool Alive=false;const uint64 Actual=ProcessStamp(Pid,Alive);return Alive&&Stamp&&Actual==Stamp;}
bool TrainBatchProcess::QueryGpuBudget(FGpuBudget& Out)
{
    Out=FGpuBudget();Out.Adapter=GRHIAdapterName;Out.Driver=GRHIAdapterUserDriverVersion;
#if PLATFORM_WINDOWS
    TCHAR SystemDirectory[MAX_PATH]={};const UINT Length=::GetSystemDirectoryW(SystemDirectory,UE_ARRAY_COUNT(SystemDirectory));if(!Length||Length>=UE_ARRAY_COUNT(SystemDirectory))return false;
    const FString Dll=FString(SystemDirectory)/TEXT("dxgi.dll");void* Library=FPlatformProcess::GetDllHandle(*Dll);if(!Library)return false;
    using FCreate=HRESULT(WINAPI*)(REFIID,void**);const auto Create=reinterpret_cast<FCreate>(FPlatformProcess::GetDllExport(Library,TEXT("CreateDXGIFactory1")));
    IDXGIFactory1* Factory=nullptr;bool Ok=false;
    if(Create&&SUCCEEDED(Create(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&Factory))))
    {
        for(UINT I=0;;++I)
        {
            IDXGIAdapter1* Adapter=nullptr;if(Factory->EnumAdapters1(I,&Adapter)==DXGI_ERROR_NOT_FOUND)break;if(!Adapter)break;
            DXGI_ADAPTER_DESC1 D={};Adapter->GetDesc1(&D);
            if(D.VendorId==GRHIVendorId&&FString(D.Description)==Out.Adapter)
            {
                IDXGIAdapter3* A3=nullptr;
                if(SUCCEEDED(Adapter->QueryInterface(__uuidof(IDXGIAdapter3),reinterpret_cast<void**>(&A3))))
                {DXGI_QUERY_VIDEO_MEMORY_INFO M={};if(SUCCEEDED(A3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&M))){Out.Budget=M.Budget;Out.Usage=M.CurrentUsage;Out.Dedicated=D.DedicatedVideoMemory;Out.AdapterIndex=int32(I);Ok=true;}A3->Release();}
            }
            Adapter->Release();if(Ok)break;
        }
        Factory->Release();
    }
    FPlatformProcess::FreeDllHandle(Library);
    const FString Identity=FString::Printf(TEXT("%x|%s|%s"),GRHIVendorId,*Out.Adapter,*Out.Driver);FTCHARToUTF8 U(*Identity);TrainBatchCodec::Sha256(U.Get(),U.Length(),Out.Identity);return Ok;
#else
    return false;
#endif
}
FString TrainBatchProcess::Encode(const TSharedRef<FJsonObject>& O)
{FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
TSharedPtr<FJsonObject> TrainBatchProcess::Read(const FString& Path,int64 Limit)
{
    const int64 Size=IFileManager::Get().FileSize(*Path);if(Size<=0||Size>Limit)return nullptr;
    TArray<uint8> B;TSharedPtr<FJsonObject> O;
    // A live coordinator keeps its lease writer open. Readers must share WRITE too,
    // otherwise Windows rejects the read even though the owner allows readers.
    if(FFileHelper::LoadFileToArray(B,*Path,FILEREAD_AllowWrite)&&B.Num()==Size)
    {FUTF8ToTCHAR U(reinterpret_cast<const ANSICHAR*>(B.GetData()),B.Num());const FString S(U.Length(),U.Get());FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O);}return O;
}
bool TrainBatchProcess::Write(const FString& Root,const FString& Path,const TSharedRef<FJsonObject>& O)
{
    if(!LocalCapturePath::SafeDescendant(Root,Path)||!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true))return false;
    const FString Pending=Path+TEXT(".pending");if(!FFileHelper::SaveStringToFile(Encode(O),*Pending,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return false;
#if PLATFORM_WINDOWS
    return ::MoveFileExW(*Pending,*Path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return false;
#endif
}
FString TrainBatchProcess::Quote(const FString& Value)
{
    for(TCHAR C:Value)if(C<32||C==TEXT('"'))return FString();
    FString V=Value;while(V.EndsWith(TEXT("\\")))V.LeftChopInline(1,false);return TEXT("\"")+V+TEXT("\"");
}
FString TrainBatchProcess::WorkerMapArguments(const FString& Project,const FString& Map,bool EditorExecutable)
{
    const FString MapArg=Quote(Map);if(Map.IsEmpty()||MapArg.IsEmpty())return FString();
    if(!EditorExecutable)return MapArg;
    const FString ProjectArg=Quote(Project);if(Project.IsEmpty()||ProjectArg.IsEmpty())return FString();
    return ProjectArg+TEXT(" ")+MapArg+TEXT(" -game");
}
bool TrainBatchProcess::IsProjectMap(const FString& Map)
{return Map.StartsWith(TEXT("/Game/"))&&FPackageName::IsValidLongPackageName(Map);}
TrainBatchProcess::EMapEntry TrainBatchProcess::EnterAssignedMap(UWorld* World,const FString& Map,FString& Error)
{
    if(!World||!IsProjectMap(Map)||!FPackageName::DoesPackageExist(Map)){Error=TEXT("Assigned project map is invalid or not cooked");return EMapEntry::Rejected;}
    if(World->GetOutermost()->GetName()==Map)return EMapEntry::Ready;
    const TWeakObjectPtr<UWorld> Owner(World);
    World->GetTimerManager().SetTimerForNextTick([Owner,Map](){if(Owner.IsValid())UGameplayStatics::OpenLevel(Owner.Get(),FName(*Map),true);});
    return EMapEntry::Travelling;
}
FString TrainBatchProcess::SceneSignature(UWorld* World,const AActor* Slab,const AActor* Lidar,const AActor* Camera)
{
    TArray<FString> Rows;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        const AActor* A=*It;if(A==Slab||A==Lidar||A==Camera)continue;
        TInlineComponentArray<UStaticMeshComponent*> Components;A->GetComponents(Components);
        for(const auto* C:Components)if(C->GetStaticMesh()&&C->IsVisible()&&!C->bHiddenInGame)
        {Rows.Add(A->GetName()+TEXT("|")+C->GetName()+TEXT("|")+C->GetStaticMesh()->GetPathName()+TEXT("|")+C->GetComponentTransform().ToString());}
    }
    Rows.Sort();const FString Text=FString::Join(Rows,TEXT("\n"));FTCHARToUTF8 U(*Text);FString Hash;TrainBatchCodec::Sha256(U.Get(),U.Length(),Hash);return Hash;
}
