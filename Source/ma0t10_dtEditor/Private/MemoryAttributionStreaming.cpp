#include "MemoryAttributionStreaming.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Trace/Analyzer.h"
#include "Trace/Analysis.h"
#include "Trace/DataStream.h"
#include <map>
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <dbghelp.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace MemoryStream
{
struct FHeap {uint16 Parent=65535,Flags=0;FString Name;};
struct FImage {uint64 Base=0;uint32 Size=0;FString File;bool Symbols=false;};
struct FInventory final:UE::Trace::IAnalyzer
{
    TMap<uint16,FHeap> Heaps;TMap<uint32,TArray<uint64>> Stacks;TArray<FImage> Images;TMap<uint64,FString> Specs;TArray<TPair<uint64,double>> Marks;
    uint8 BaseShift=16;double Begin=-1,End=-1;
    virtual void OnAnalysisBegin(const FOnAnalysisContext& C) override
    {
        C.InterfaceBuilder.RouteEvent(1,"Memory","HeapSpec");C.InterfaceBuilder.RouteEvent(2,"Memory","CallstackSpec");
        C.InterfaceBuilder.RouteEvent(3,"Misc","BookmarkSpec");C.InterfaceBuilder.RouteEvent(4,"Misc","Bookmark");
        C.InterfaceBuilder.RouteEvent(5,"Diagnostics","ModuleInit");C.InterfaceBuilder.RouteEvent(6,"Diagnostics","ModuleLoad");
    }
    virtual bool OnEvent(uint16 Route,EStyle,const FOnEventContext& C) override
    {
        const auto& E=C.EventData;
        if(Route==1){FHeap H;H.Parent=E.GetValue<uint16>("ParentId");H.Flags=E.GetValue<uint16>("Flags");E.GetString("Name",H.Name);Heaps.Add(E.GetValue<uint16>("Id"),MoveTemp(H));}
        if(Route==2){const uint32 Id=E.GetValue<uint32>("CallstackId");const auto& F=E.GetArray<uint64>("Frames");auto& A=Stacks.FindOrAdd(Id);A.Reset();A.Append(F.GetData(),F.Num());}
        if(Route==3){FString S;if(!E.GetString("FormatString",S)){const ANSICHAR* File=(const ANSICHAR*)E.GetAttachment();S=(const TCHAR*)(E.GetAttachment()+FCStringAnsi::Strlen(File)+1);}Specs.Add(E.GetValue<uint64>("BookmarkPoint"),MoveTemp(S));}
        if(Route==4)Marks.Emplace(E.GetValue<uint64>("BookmarkPoint"),C.EventTime.AsSeconds(E.GetValue<uint64>("Cycle")));
        if(Route==5)BaseShift=E.GetValue<uint8>("ModuleBaseShift",16);
        if(Route==6){FImage I;I.Base=BaseShift?uint64(E.GetValue<uint32>("Base"))<<BaseShift:E.GetValue<uint64>("Base");I.Size=E.GetValue<uint32>("Size");E.GetString("Name",I.File);Images.Add(MoveTemp(I));}
        return true;
    }
    void Finish(){for(const auto& M:Marks)if(const FString* S=Specs.Find(M.Key)){if(*S==TEXT("MEMORY26B_BEGIN"))Begin=M.Value;if(*S==TEXT("MEMORY26B_END"))End=M.Value;}}
    uint16 Root(uint16 Id) const {for(int N=0;N<128;++N){const auto* H=Heaps.Find(Id);if(!H||H->Parent==65535||(H->Flags&1))return Id;Id=H->Parent;}return 65535;}
};
struct FActive {uint64 Size=0,Owner=0;uint32 Stack=0;uint16 Group=0;bool IsHeap=false;};
struct FStat {int64 Bytes=0,Count=0;};
template<typename V>struct TKeyHash:TDefaultMapHashableKeyFuncs<uint64,V,false>
{
    // Packed stack/address keys need low-bit avalanche for power-of-two UE buckets.
    static uint32 GetKeyHash(uint64 K){K^=K>>30;K*=0xbf58476d1ce4e5b9ull;K^=K>>27;K*=0x94d049bb133111ebull;K^=K>>31;return uint32(K);}
};
template<typename V>using FMap64=TMap<uint64,V,FDefaultSetAllocator,TKeyHash<V>>;
using FTotals=FMap64<FStat>;
struct FLinear {uint64 Size=0;TSet<uint64> Children;};
struct FLive final:UE::Trace::IAnalyzer
{
    const FInventory& Inventory;FMap64<FActive> Normal,Backing;std::map<uint64,FLinear> Linear;
    FTotals Totals,A,B;double TargetA,TargetB,Time=0,ActualA=-1,ActualB=-1;uint8 Shift=0;bool Init=false;
    uint64 Events=0,MissingBefore=0,MissingWindow=0,Duplicates=0,ImplicitFrees=0;
    uint64 LastKey=0;FActive LastValue;bool LastUsed=false;
    FLive(const FInventory& I,double T1,double T2):Inventory(I),TargetA(T1),TargetB(T2){}
    static uint64 GroupKey(const FActive& X){return (uint64(X.Stack)<<24)|(uint64(X.Group)<<1)|uint64(X.IsHeap);}
    static uint64 PtrKey(uint8 Root,uint64 Address){return (uint64(Root)<<56)|Address;}
    void Add(const FActive& X,int Sign){auto& S=Totals.FindOrAdd(GroupKey(X));S.Bytes+=int64(X.Size)*Sign;S.Count+=Sign;}
    void Missing(){if(Time<TargetA)++MissingBefore;else if(Time<=TargetB)++MissingWindow;}
    void FreeNormal(uint64 Key,bool TouchOwner=true)
    {
        FActive X;if(!Normal.RemoveAndCopyValue(Key,X)){Missing();return;}Add(X,-1);
        if(TouchOwner&&X.Owner){auto I=Linear.find(X.Owner);if(I!=Linear.end())I->second.Children.Remove(Key);}
    }
    void FlushLast()
    {
        if(!LastUsed)return;FActive X=LastValue;const uint64 Address=LastKey&0x00ffffffffffffffull;const uint8 Root=uint8(LastKey>>56);
        auto I=Linear.upper_bound(LastKey);if(I!=Linear.begin()){--I;if((I->first>>56)==Root&&Address>=(I->first&0x00ffffffffffffffull)&&Address-(I->first&0x00ffffffffffffffull)<I->second.Size){X.Owner=I->first;I->second.Children.Add(LastKey);}}
        Add(X,1);Normal.Add(LastKey,X);LastUsed=false;
    }
    virtual void OnAnalysisBegin(const FOnAnalysisContext& C) override
    {
        const ANSICHAR* Names[]={"Init","Alloc","AllocSystem","AllocVideo","Free","FreeSystem","FreeVideo","ReallocAlloc","ReallocAllocSystem","ReallocFree","ReallocFreeSystem","Marker","HeapMarkAlloc","HeapUnmarkAlloc"};
        for(uint16 I=0;I<14;++I)C.InterfaceBuilder.RouteEvent(I,"Memory",Names[I]);
    }
    virtual bool OnEvent(uint16 Route,EStyle,const FOnEventContext& C) override
    {
        ++Events;const auto& E=C.EventData;
        if(Route==0){Init=E.GetValue<uint32>("Version")==2;Shift=E.GetValue<uint8>("SizeShift");return Init;}
        if(!Init)return true;
        if(LastUsed&&(Route==4||Route==5||Route==6||Route==9||Route==10)){const uint64 P=E.GetValue<uint64>("Address");const uint8 H=E.GetValue<uint8>("RootHeap",Route==6?1:0);if(P&&P<(1ull<<56)&&PtrKey(H,P)==LastKey){LastUsed=false;return true;}}
        FlushLast();
        if(Route==11)
        {
            const double Next=C.EventTime.AsSeconds(E.GetValue<uint64>("Cycle"));
            if(ActualA<0&&Next>TargetA){A=Totals;ActualA=Time;}
            if(ActualB<0&&Next>TargetB){B=Totals;ActualB=Time;return false;}
            Time=Next;return true;
        }
        const uint64 Address=E.GetValue<uint64>("Address");if(!Address||Address>=(1ull<<56))return true;
        const uint8 Root=E.GetValue<uint8>("RootHeap",Route==3||Route==6?1:0);uint64 Key=PtrKey(Root,Address);
        if(Route==1||Route==2||Route==3||Route==7||Route==8)
        {
            if(Normal.Contains(Key)){++Duplicates;FreeNormal(Key);}
            FActive X;X.Size=(uint64(E.GetValue<uint32>("Size"))<<Shift)|(E.GetValue<uint8>("AlignmentPow2_SizeLower")&((1u<<Shift)-1));X.Stack=E.GetValue<uint32>("CallstackId",0);X.Group=Root;
            LastKey=Key;LastValue=X;LastUsed=true;return true;
        }
        if(Route==4||Route==5||Route==6||Route==9||Route==10){FreeNormal(Key);return true;}
        const uint16 Heap=E.GetValue<uint16>("Heap");const uint16 ParentRoot=Inventory.Root(Heap);if(ParentRoot>255){Missing();return true;}Key=PtrKey(uint8(ParentRoot),Address);
        const uint32 Stack=E.GetValue<uint32>("CallstackId",0);
        if(Route==12)
        {
            FActive X;if(!Normal.RemoveAndCopyValue(Key,X)){Missing();return true;}Add(X,-1);X.Owner=0;X.IsHeap=true;X.Group=Heap;if(Stack)X.Stack=Stack;Backing.Add(Key,X);Add(X,1);
            const auto* H=Inventory.Heaps.Find(Heap);if(H&&(H->Flags&2))Linear[Key].Size=X.Size;
        }
        if(Route==13)
        {
            FActive X;if(!Backing.RemoveAndCopyValue(Key,X)){Missing();return true;}Add(X,-1);
            auto I=Linear.find(Key);if(I!=Linear.end()){for(uint64 Child:I->second.Children)if(Normal.Contains(Child)){FreeNormal(Child,false);++ImplicitFrees;}Linear.erase(I);}
            // UE5.3 reallocation after heap-unmark preserves the original allocation stack.
            X.IsHeap=false;X.Group=ParentRoot;Normal.Add(Key,X);Add(X,1);
        }
        return true;
    }
};
bool Process(const FString& Input,UE::Trace::IAnalyzer& Analyzer,const TCHAR* Label)
{
    UE::Trace::FFileDataStream File;if(!File.Open(*Input))return false;UE::Trace::FAnalysisContext Context;Context.AddAnalyzer(Analyzer);auto Worker=Context.Process(File);double Start=FPlatformTime::Seconds(),Next=0;
    while(Worker.IsActive())
    {
        double Now=FPlatformTime::Seconds();if(Now-Start>3600||FPlatformMemory::GetStats().UsedPhysical>24ull*1024*1024*1024){Worker.Stop();Worker.Wait();return false;}
        if(Now>=Next){Next=Now+15;UE_LOG(LogTemp,Display,TEXT("[MemoryStream] %s elapsed %.0fs RSS %.2fGiB"),Label,Now-Start,FPlatformMemory::GetStats().UsedPhysical/double(1ull<<30));if(FCString::Strcmp(Label,TEXT("live-only"))==0){const auto& L=static_cast<FLive&>(Analyzer);UE_LOG(LogTemp,Display,TEXT("[MemoryStream] marker %.3f / %.3f events %llu"),L.Time,L.TargetB,L.Events);}}FPlatformProcess::Sleep(.1f);
    }
    Worker.Wait();return true;
}
}

int32 ExportStreamingMemory(const FString& Input,const FString& Output,double Offset,int32 Top,const FString& SymbolCache)
{
    using namespace MemoryStream;FInventory Inventory;
    if(!Process(Input,Inventory,TEXT("inventory")))return 2;Inventory.Finish();
    if(Inventory.Begin<0||Inventory.End<=Inventory.Begin+Offset)return 2;
    FLive Live(Inventory,Inventory.Begin+Offset,Inventory.End-.001);
    if(!Process(Input,Live,TEXT("live-only"))||Live.ActualA<0||Live.ActualB<0)return 2;
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("Valid"),true);R->SetBoolField(TEXT("streaming"),true);R->SetStringField(TEXT("input_trace"),Input);
    R->SetNumberField(TEXT("snapshot_a"),Live.TargetA);R->SetNumberField(TEXT("snapshot_b"),Live.TargetB);R->SetNumberField(TEXT("snapshot_a_last_marker"),Live.ActualA);R->SetNumberField(TEXT("snapshot_b_last_marker"),Live.ActualB);
    R->SetNumberField(TEXT("events"),double(Live.Events));R->SetNumberField(TEXT("unmatched_before_a"),double(Live.MissingBefore));R->SetNumberField(TEXT("unmatched_window"),double(Live.MissingWindow));R->SetNumberField(TEXT("duplicate_allocs"),double(Live.Duplicates));R->SetNumberField(TEXT("implicit_linear_frees"),double(Live.ImplicitFrees));
    R->SetStringField(TEXT("method"),TEXT("UE5.3 TraceAnalysis two passes: metadata, then current live allocation maps only. Memory marker timestamps follow UE5.3 allocation analyzer; sample at last marker not later than target. CPU user/backing/GPU/nested heaps separate; no cross-heap sums."));
    struct FRow{uint64 Key;FStat A,B;int64 Delta;};TSet<uint64> Keys;for(const auto& P:Live.A)Keys.Add(P.Key);for(const auto& P:Live.B)Keys.Add(P.Key);
    TArray<FRow> Rows;TMap<uint32,TArray<int64>> Totals;
    for(uint64 Key:Keys){FRow X{Key,Live.A.FindRef(Key),Live.B.FindRef(Key),0};X.Delta=X.B.Bytes-X.A.Bytes;Rows.Add(X);const uint32 Heap=uint32(Key&0x1ffff);auto& T=Totals.FindOrAdd(Heap);if(!T.Num())T.Init(0,4);T[0]+=X.A.Bytes;T[1]+=X.B.Bytes;T[2]+=X.A.Count;T[3]+=X.B.Count;}
    Rows.Sort([](const FRow& A,const FRow& B){return A.Delta>B.Delta;});
    auto H=MakeShared<FJsonObject>();for(const auto& P:Totals){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("before_bytes"),double(P.Value[0]));J->SetNumberField(TEXT("after_bytes"),double(P.Value[1]));J->SetNumberField(TEXT("delta_bytes"),double(P.Value[1]-P.Value[0]));J->SetNumberField(TEXT("before_count"),double(P.Value[2]));J->SetNumberField(TEXT("after_count"),double(P.Value[3]));H->SetObjectField(FString::Printf(TEXT("root%u_isheap%u"),P.Key>>1,P.Key&1),J);}R->SetObjectField(TEXT("heap_totals"),H);
#if PLATFORM_WINDOWS
    HANDLE Token=(HANDLE)UPTRINT(0x26b26b);const FString Search=SymbolCache+TEXT(";")+FPaths::ProjectDir()/TEXT("Binaries/Win64")+TEXT(";")+FPaths::EngineDir()/TEXT("Binaries/Win64");
    SymSetOptions(SymGetOptions()|SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_LOAD_LINES|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_NO_PROMPTS);
    const bool SymbolsReady=SymInitializeW(Token,*Search,false)!=0;
    if(SymbolsReady)for(auto& I:Inventory.Images)
    {
        const FString Name=FPaths::GetCleanFilename(I.File);FString File=SymbolCache/Name;
        if(!FPaths::FileExists(File))File=I.File;
        if(!FPaths::FileExists(File))File=FPaths::EngineDir()/TEXT("Binaries/Win64")/Name;
        if(FPaths::FileExists(File))I.Symbols=SymLoadModuleExW(Token,nullptr,*File,*Name,I.Base,I.Size,nullptr,0)!=0;
    }
#endif
    TMap<uint64,TSharedPtr<FJsonObject>> FrameCache;
    auto Frame=[&](uint64 Address)
    {
        if(auto* Cached=FrameCache.Find(Address))return *Cached;auto J=MakeShared<FJsonObject>();FString Name=TEXT("Unknown");
        J->SetStringField(TEXT("address"),FString::Printf(TEXT("0x%llx"),Address));
        for(const auto& I:Inventory.Images)if(Address>=I.Base&&Address-I.Base<I.Size){Name=FPaths::GetCleanFilename(I.File);J->SetStringField(TEXT("module_offset"),FString::Printf(TEXT("0x%llx"),Address-I.Base));break;}
        J->SetStringField(TEXT("module"),Name);J->SetStringField(TEXT("symbol"),TEXT("Unknown"));J->SetStringField(TEXT("resolution"),TEXT("Module+offset only"));
#if PLATFORM_WINDOWS
        if(SymbolsReady)
        {
            alignas(16) uint8 Buffer[sizeof(SYMBOL_INFOW)+2048*sizeof(WCHAR)]={};auto* S=(SYMBOL_INFOW*)Buffer;S->SizeOfStruct=sizeof(SYMBOL_INFOW);S->MaxNameLen=2048;DWORD64 Displacement=0;
            if(SymFromAddrW(Token,Address,&Displacement,S)){J->SetStringField(TEXT("symbol"),FString(S->NameLen,S->Name));J->SetStringField(TEXT("resolution"),TEXT("Ok"));}
            IMAGEHLP_LINEW64 L{};L.SizeOfStruct=sizeof(L);DWORD D=0;if(SymGetLineFromAddrW64(Token,Address,&D,&L)){J->SetStringField(TEXT("file"),L.FileName?L.FileName:TEXT(""));J->SetNumberField(TEXT("line"),L.LineNumber);}
        }
#endif
        FrameCache.Add(Address,J);return TSharedPtr<FJsonObject>(J);
    };
    auto Convert=[&](const FRow& X)
    {
        auto J=MakeShared<FJsonObject>();const uint32 Stack=uint32(X.Key>>24);J->SetNumberField(TEXT("stack_id"),Stack);J->SetNumberField(TEXT("root_heap"),uint16((X.Key>>1)&65535));J->SetBoolField(TEXT("is_heap"),(X.Key&1)!=0);J->SetNumberField(TEXT("before_bytes"),double(X.A.Bytes));J->SetNumberField(TEXT("after_bytes"),double(X.B.Bytes));J->SetNumberField(TEXT("delta_bytes"),double(X.Delta));J->SetNumberField(TEXT("count_delta"),double(X.B.Count-X.A.Count));
        TArray<TSharedPtr<FJsonValue>> Frames;bool Project=false;if(const auto* Addresses=Inventory.Stacks.Find(Stack))for(uint64 Address:*Addresses){auto F=Frame(Address);Project|=F->GetStringField(TEXT("module")).Contains(TEXT("ma0t10_dt"));Frames.Add(MakeShared<FJsonValueObject>(F));}J->SetArrayField(TEXT("frames"),Frames);J->SetBoolField(TEXT("has_project_frame"),Project);return MakeShared<FJsonValueObject>(J);
    };
    TArray<TSharedPtr<FJsonValue>> Growth,Decline,Cpu,CpuHeap,Gpu,Modules,HeapSpecs;
    for(const auto& X:Rows)if(X.Delta>0)
    {
        if(Growth.Num()<Top)Growth.Add(Convert(X));const uint16 Heap=uint16((X.Key>>1)&65535);const bool IsHeap=(X.Key&1)!=0;
        if(Heap==0&&!IsHeap&&Cpu.Num()<Top)Cpu.Add(Convert(X));if(Heap==0&&IsHeap&&CpuHeap.Num()<Top)CpuHeap.Add(Convert(X));if(Heap==1&&!IsHeap&&Gpu.Num()<Top)Gpu.Add(Convert(X));
    }
    for(int32 I=Rows.Num()-1;I>=0&&Decline.Num()<Top;--I)if(Rows[I].Delta<0)Decline.Add(Convert(Rows[I]));
    for(const auto& I:Inventory.Images){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),FPaths::GetCleanFilename(I.File));J->SetStringField(TEXT("base"),FString::Printf(TEXT("0x%llx"),I.Base));J->SetNumberField(TEXT("size"),I.Size);J->SetStringField(TEXT("status"),I.Symbols?TEXT("Loaded/deferred"):TEXT("Not loaded"));Modules.Add(MakeShared<FJsonValueObject>(J));}
    for(const auto& P:Inventory.Heaps){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("id"),P.Key);J->SetNumberField(TEXT("parent"),P.Value.Parent);J->SetStringField(TEXT("name"),P.Value.Name);HeapSpecs.Add(MakeShared<FJsonValueObject>(J));}
    R->SetArrayField(TEXT("top_growth"),Growth);R->SetArrayField(TEXT("top_decline"),Decline);R->SetArrayField(TEXT("cpu_user_growth"),Cpu);R->SetArrayField(TEXT("cpu_backing_growth"),CpuHeap);R->SetArrayField(TEXT("gpu_user_growth"),Gpu);R->SetArrayField(TEXT("modules"),Modules);R->SetArrayField(TEXT("heap_specs"),HeapSpecs);
    R->SetStringField(TEXT("limits"),TEXT("Live-only parser requires companion calibration cross-validation; unmatched events and unsupported symbols preserved. No inference of leak from one in-flight snapshot. Native code outside emitted trace coverage is not accounted."));
#if PLATFORM_WINDOWS
    if(SymbolsReady)SymCleanup(Token);
#endif
    FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*Output)?0:2;
}
