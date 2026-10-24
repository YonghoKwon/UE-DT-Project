#include "MemoryAttributionCommandlet.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMemory.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Common/ProviderLock.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ModuleService.h"
#include "TraceServices/Model/AllocationsProvider.h"
#include "TraceServices/Model/Bookmarks.h"
#include "TraceServices/Model/Callstack.h"
#include "TraceServices/Model/Modules.h"

namespace
{
struct FLiveStack { int64 Bytes=0,Count=0;uint32 Stack=0;uint8 Heap=0;bool HeapAllocation=false; };
using FSnapshot=TMap<uint64,FLiveStack>;
bool Snapshot(const TraceServices::IAllocationsProvider& Provider,double At,FSnapshot& Out)
{
    TraceServices::IAllocationsProvider::FQueryParams P{};P.Rule=TraceServices::IAllocationsProvider::EQueryRule::aAf;P.TimeA=At;
    TraceServices::IAllocationsProvider::FQueryHandle Query;
    {TraceServices::FProviderReadScopeLock Lock(Provider);Query=Provider.StartQuery(P);}
    if(!Query)return false;const double Deadline=FPlatformTime::Seconds()+600;
    for(;;)
    {
        auto Status=Provider.PollQuery(Query);
        if(Status.Status==TraceServices::IAllocationsProvider::EQueryStatus::Done)return true;
        if(Status.Status==TraceServices::IAllocationsProvider::EQueryStatus::Unknown)return false;
        if(FPlatformTime::Seconds()>Deadline){Provider.CancelQuery(Query);return false;}
        if(Status.Status==TraceServices::IAllocationsProvider::EQueryStatus::Working){FPlatformProcess::Sleep(.01f);continue;}
        TraceServices::FProviderReadScopeLock Lock(Provider);
        auto Page=Status.NextResult();while(Page)
        {
            for(uint32 I=0;I<Page->Num();++I)
            {
                const auto* A=Page->Get(I);const uint32 Stack=A->GetCallstackId();const uint8 Heap=uint8(A->GetRootHeap());const bool IsHeap=A->IsHeap();
                const uint64 Key=(uint64(Stack)<<16)|(uint64(Heap)<<1)|uint64(IsHeap);auto& S=Out.FindOrAdd(Key);
                S.Stack=Stack;S.Heap=Heap;S.HeapAllocation=IsHeap;S.Bytes+=int64(A->GetSize());++S.Count;
            }
            Page=Status.NextResult();
        }
    }
}
}

UMemoryAttributionCommandlet::UMemoryAttributionCommandlet()
{IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}

int32 UMemoryAttributionCommandlet::Main(const FString& Params)
{
    FString Input,Output;double Offset=300;int32 Top=50;
    FParse::Value(*Params,TEXT("InputTrace="),Input);FParse::Value(*Params,TEXT("OutputReport="),Output);FParse::Value(*Params,TEXT("CompareOffset="),Offset);FParse::Value(*Params,TEXT("Top="),Top);
    if(Input.IsEmpty()||Output.IsEmpty()||Offset<=0||Top<1||Top>200){UE_LOG(LogTemp,Error,TEXT("InputTrace/OutputReport and positive CompareOffset required"));return 2;}
    auto& Module=FModuleManager::LoadModuleChecked<ITraceServicesModule>(TEXT("TraceServices"));
    Module.GetModuleService()->SetModuleEnabled(FName(TEXT("TraceModule_Memory")),true);
    UE_LOG(LogTemp,Display,TEXT("[MemoryAttribution] analyzing trace"));
    const auto Session=Module.GetAnalysisService()->StartAnalysis(*Input);if(!Session){UE_LOG(LogTemp,Error,TEXT("Trace analysis could not start"));return 2;}
    const double AnalysisStart=FPlatformTime::Seconds();double NextProgress=0;
    while(!Session->IsAnalysisComplete())
    {
        const double Now=FPlatformTime::Seconds();if(Now-AnalysisStart>1800||FPlatformMemory::GetStats().UsedPhysical>48ull*1024*1024*1024){Session->Stop(true);UE_LOG(LogTemp,Error,TEXT("Offline analysis time/RAM safety cap reached"));return 2;}
        if(Now>=NextProgress){NextProgress=Now+10;UE_LOG(LogTemp,Display,TEXT("[MemoryAttribution] analysis elapsed %.0fs"),Now-AnalysisStart);}
        FPlatformProcess::Sleep(.1f);
    }
    double Begin=-1,End=-1;const TraceServices::IAllocationsProvider* Alloc=nullptr;const TraceServices::ICallstacksProvider* Stacks=nullptr;const TraceServices::IModuleProvider* Modules=nullptr;
    {
        TraceServices::FAnalysisSessionReadScope Lock(*Session);
        TraceServices::ReadBookmarkProvider(*Session).EnumerateBookmarks(0,Session->GetDurationSeconds(),[&](const TraceServices::FBookmark& B)
        {if(FCString::Strcmp(B.Text,TEXT("MEMORY26B_BEGIN"))==0)Begin=B.Time;else if(FCString::Strcmp(B.Text,TEXT("MEMORY26B_END"))==0)End=B.Time;});
        Alloc=TraceServices::ReadAllocationsProvider(*Session);Stacks=TraceServices::ReadCallstacksProvider(*Session);Modules=TraceServices::ReadModuleProvider(*Session);
    }
    if(!Alloc||!Stacks||Begin<0||End<=Begin+Offset){UE_LOG(LogTemp,Error,TEXT("Memory providers or valid measurement bookmarks absent: begin=%.3f end=%.3f"),Begin,End);return 2;}
    {TraceServices::FProviderReadScopeLock Lock(*Alloc);if(!Alloc->IsInitialized()||Alloc->GetTimelineNumPoints()==0){UE_LOG(LogTemp,Error,TEXT("Allocation channel is empty"));return 2;}}
    FSnapshot Before,After;
    if(!Snapshot(*Alloc,Begin+Offset,Before)||!Snapshot(*Alloc,End-.001,After)){UE_LOG(LogTemp,Error,TEXT("Live allocation query failed"));return 2;}
    struct FDiff {uint64 Key=0;FLiveStack A,B;int64 Delta=0;};TArray<FDiff> Rows;TSet<uint64> Keys;
    for(const auto& P:Before)Keys.Add(P.Key);for(const auto& P:After)Keys.Add(P.Key);
    TMap<uint32,TArray<int64>> Totals;
    for(uint64 Key:Keys)
    {
        FDiff D;D.Key=Key;D.A=Before.FindRef(Key);D.B=After.FindRef(Key);D.Delta=D.B.Bytes-D.A.Bytes;Rows.Add(D);
        const auto& S=D.B.Count?D.B:D.A;auto& T=Totals.FindOrAdd((uint32(S.Heap)<<1)|uint32(S.HeapAllocation));if(T.Num()==0)T.Init(0,4);T[0]+=D.A.Bytes;T[1]+=D.B.Bytes;T[2]+=D.A.Count;T[3]+=D.B.Count;
    }
    Rows.Sort([](const FDiff& A,const FDiff& B){return A.Delta>B.Delta;});
    auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("Valid"),true);Report->SetStringField(TEXT("input_trace"),Input);Report->SetNumberField(TEXT("measurement_begin"),Begin);Report->SetNumberField(TEXT("snapshot_a"),Begin+Offset);Report->SetNumberField(TEXT("snapshot_b"),End-.001);
    Report->SetStringField(TEXT("method"),TEXT("UE5.3 TraceServices active allocations at Begin+offset versus End bookmark before teardown; root heaps/heap allocations separate, never summed across nested heaps"));
    auto HeapJson=MakeShared<FJsonObject>();for(const auto& P:Totals){auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("before_bytes"),double(P.Value[0]));R->SetNumberField(TEXT("after_bytes"),double(P.Value[1]));R->SetNumberField(TEXT("delta_bytes"),double(P.Value[1]-P.Value[0]));R->SetNumberField(TEXT("before_count"),double(P.Value[2]));R->SetNumberField(TEXT("after_count"),double(P.Value[3]));HeapJson->SetObjectField(FString::Printf(TEXT("root%u_isheap%u"),P.Key>>1,P.Key&1),R);}Report->SetObjectField(TEXT("heap_totals"),HeapJson);
    TArray<TSharedPtr<FJsonValue>> Growth,Decline,ModuleRows,CpuUser,CpuBacking,GpuUser,Heaps;
    auto Convert=[&](const FDiff& D)
    {
        const auto& S=D.B.Count?D.B:D.A;auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("stack_id"),S.Stack);R->SetNumberField(TEXT("root_heap"),S.Heap);R->SetBoolField(TEXT("is_heap"),S.HeapAllocation);R->SetNumberField(TEXT("before_bytes"),double(D.A.Bytes));R->SetNumberField(TEXT("after_bytes"),double(D.B.Bytes));R->SetNumberField(TEXT("delta_bytes"),double(D.Delta));R->SetNumberField(TEXT("count_delta"),double(D.B.Count-D.A.Count));
        TArray<TSharedPtr<FJsonValue>> Frames;bool Project=false;const auto* Stack=Stacks->GetCallstack(S.Stack);
        if(Stack)for(uint32 I=0;I<Stack->Num();++I)
        {
            const auto* F=Stack->Frame(uint8(I));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("address"),FString::Printf(TEXT("0x%llx"),F->Addr));
            if(F->Symbol){const auto* Y=F->Symbol;const FString M=Y->Module?Y->Module:TEXT("");J->SetStringField(TEXT("module"),M);J->SetStringField(TEXT("symbol"),Y->Name?Y->Name:TEXT(""));J->SetStringField(TEXT("file"),Y->File?Y->File:TEXT(""));J->SetNumberField(TEXT("line"),Y->Line);J->SetStringField(TEXT("resolution"),TraceServices::QueryResultToString(Y->GetResult()));Project|=M.Contains(TEXT("ma0t10_dt"));}
            Frames.Add(MakeShared<FJsonValueObject>(J));
        }
        R->SetBoolField(TEXT("has_project_frame"),Project);R->SetArrayField(TEXT("frames"),Frames);return MakeShared<FJsonValueObject>(R);
    };
    {TraceServices::FAnalysisSessionReadScope Lock(*Session);
        for(const auto& D:Rows)if(D.Delta>0)
        {
            if(Growth.Num()<Top)Growth.Add(Convert(D));const auto& S=D.B.Count?D.B:D.A;
            if(S.Heap==0&&!S.HeapAllocation&&CpuUser.Num()<Top)CpuUser.Add(Convert(D));
            if(S.Heap==0&&S.HeapAllocation&&CpuBacking.Num()<Top)CpuBacking.Add(Convert(D));
            if(S.Heap==1&&!S.HeapAllocation&&GpuUser.Num()<Top)GpuUser.Add(Convert(D));
        }
        for(int32 I=Rows.Num()-1;I>=0&&Decline.Num()<Top;--I)if(Rows[I].Delta<0)Decline.Add(Convert(Rows[I]));}
    Report->SetArrayField(TEXT("top_growth"),Growth);Report->SetArrayField(TEXT("top_decline"),Decline);
    Report->SetArrayField(TEXT("cpu_user_growth"),CpuUser);Report->SetArrayField(TEXT("cpu_backing_growth"),CpuBacking);Report->SetArrayField(TEXT("gpu_user_growth"),GpuUser);
    {TraceServices::FProviderReadScopeLock Lock(*Alloc);Alloc->EnumerateHeaps([&](HeapId Id,const TraceServices::IAllocationsProvider::FHeapSpec& H){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("id"),Id);J->SetStringField(TEXT("name"),H.Name?H.Name:TEXT(""));J->SetNumberField(TEXT("parent"),H.Parent?int32(H.Parent->Id):-1);Heaps.Add(MakeShared<FJsonValueObject>(J));});}Report->SetArrayField(TEXT("heap_specs"),Heaps);
    if(Modules){TraceServices::FAnalysisSessionReadScope Lock(*Session);Modules->EnumerateModules(0,[&](const TraceServices::FModule& M){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),M.Name?M.Name:TEXT(""));J->SetStringField(TEXT("base"),FString::Printf(TEXT("0x%llx"),M.Base));J->SetNumberField(TEXT("size"),M.Size);J->SetStringField(TEXT("status"),TraceServices::ModuleStatusToString(M.Status.load()));J->SetStringField(TEXT("message"),M.StatusMessage?M.StatusMessage:TEXT(""));ModuleRows.Add(MakeShared<FJsonValueObject>(J));});}
    Report->SetArrayField(TEXT("modules"),ModuleRows);Report->SetStringField(TEXT("limits"),TEXT("Only allocations emitted to trace are covered. Missing symbols and driver/unhooked allocations remain unattributed. Positive live bytes do not by themselves prove a leak."));
    FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true);
    if(!FFileHelper::SaveStringToFile(Text,*Output))return 2;
    UE_LOG(LogTemp,Display,TEXT("[MemoryAttribution] wrote top%d live-allocation deltas: %.3f -> %.3f"),Top,Begin+Offset,End-.001);return 0;
}
