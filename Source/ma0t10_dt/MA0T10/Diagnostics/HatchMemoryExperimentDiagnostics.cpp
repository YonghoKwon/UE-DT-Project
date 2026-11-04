#include "ma0t10_dt/MA0T10/Diagnostics/HatchMemoryExperimentDiagnostics.h"
#include "Memory26Diagnostics.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/MemoryBase.h"
#include "HAL/PlatformMemory.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
bool ValidTrimPeriod(double Seconds)
{
	return FMath::IsFinite(Seconds) && (Seconds == 0 || Seconds >= 60);
}
bool TrimDue(double Now, double Last, double Period)
{
	return Period > 0 && Now - Last >= Period;
}
struct FExperiment
{
	TUniquePtr<FArchive> File;
	double Period = 0, LastTrim = 0, TotalMs = 0, MaxMs = 0;
	int32 Calls = 0;
	bool Write(const TSharedRef<FJsonObject>& Row)
	{
		FString Text;
		FJsonSerializer::Serialize(Row, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		Text += TEXT("\n");
		FTCHARToUTF8 Utf8(*Text);
		File->Serialize((void*)Utf8.Get(), Utf8.Length());
		File->Flush();
		return !File->IsError();
	}
	TSharedRef<FJsonObject> Controls() const
	{
		auto Out = MakeShared<FJsonObject>();
		for (const TCHAR* Name : {TEXT("r.gpucrash.collectionenable"), TEXT("r.GPUStatsEnabled"), TEXT("r.RDG.Events"), TEXT("r.D3D12.GPUTimeFromTimestamps"),
		                          TEXT("t.MaxFPS"), TEXT("r.VSync"), TEXT("t.IdleWhenNotForeground"), TEXT("Slate.bAllowThrottling")})
		{
			auto Row = MakeShared<FJsonObject>();
			const auto* Var = IConsoleManager::Get().FindConsoleVariable(Name);
			Row->SetBoolField(TEXT("present"), Var != nullptr);
			if (Var)
			{
				Row->SetNumberField(TEXT("value"), Var->GetInt());
				Row->SetNumberField(TEXT("flags"), uint32(Var->GetFlags()));
			}
			Out->SetObjectField(Name, Row);
		}
		return Out;
	}
	TSharedRef<FJsonObject> Counters() const
	{
		auto Out = MakeShared<FJsonObject>();
		for (int32 I = 0; I < int32(Memory26::ECounter::Count); ++I)
		{
			const auto C = Memory26::ECounter(I);
			Out->SetNumberField(Memory26::CounterName(C), double(Memory26::Value(C)));
		}
		return Out;
	}
};
TUniquePtr<FExperiment> Experiment;
}
bool BeginHatchMemoryExperiment(const FString& Directory)
{
	double Period = 0;
	const bool HasPeriod = FParse::Value(FCommandLine::Get(), TEXT("HatchMemoryTrimSeconds="), Period);
	const bool Enabled = FParse::Param(FCommandLine::Get(), TEXT("HatchMemoryExperiment"));
	if (!Enabled)
		return !HasPeriod;
	if (Memory26::GetCase() == Memory26::ECase::Invalid)
		return false;
	if (!ValidTrimPeriod(Period))
		return false;
	const FString Path = Directory / TEXT("experiment.jsonl");
	if (IFileManager::Get().FileExists(*Path))
		return false;
	Experiment = MakeUnique<FExperiment>();
	Experiment->Period = Period;
	Experiment->File.Reset(IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_AllowRead));
	if (!Experiment->File)
		return false;
	auto Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("type"), TEXT("metadata"));
	Row->SetStringField(TEXT("allocator"), GMalloc->GetDescriptiveName());
	Row->SetBoolField(TEXT("diagnostic_only"), true);
	Row->SetNumberField(TEXT("trim_interval_seconds"), Period);
	Row->SetObjectField(TEXT("cvars"), Experiment->Controls());
	Row->SetStringField(TEXT("memory26_case"), Memory26::CaseName());
	Row->SetObjectField(TEXT("resource_counters"), Experiment->Counters());
	return Experiment->Write(Row);
}
bool SampleHatchMemoryExperiment(double ElapsedSeconds)
{
	if (!Experiment)
		return true;
	if (!Experiment->File)
		return false;
	if (TrimDue(ElapsedSeconds, Experiment->LastTrim, Experiment->Period))
	{
		// One call at most per sampler visit; no catch-up burst or forced RHI/GC.
		Experiment->LastTrim = ElapsedSeconds;
		const auto Before = FPlatformMemory::GetStats();
		const double Started = FPlatformTime::Seconds();
		GMalloc->Trim(true);
		const double Cost = (FPlatformTime::Seconds() - Started) * 1000;
		const auto After = FPlatformMemory::GetStats();
		++Experiment->Calls;
		Experiment->TotalMs += Cost;
		Experiment->MaxMs = FMath::Max(Experiment->MaxMs, Cost);
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("type"), TEXT("allocator_trim"));
		Row->SetNumberField(TEXT("seconds"), ElapsedSeconds);
		Row->SetNumberField(TEXT("gt_ms"), Cost);
		Row->SetNumberField(TEXT("rss_before_bytes"), double(Before.UsedPhysical));
		Row->SetNumberField(TEXT("rss_after_bytes"), double(After.UsedPhysical));
		Row->SetNumberField(TEXT("commit_before_bytes"), double(Before.UsedVirtual));
		Row->SetNumberField(TEXT("commit_after_bytes"), double(After.UsedVirtual));
		if (!Experiment->Write(Row))
			return false;
	}
	auto Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("type"), TEXT("sample"));
	Row->SetNumberField(TEXT("seconds"), ElapsedSeconds);
	Row->SetNumberField(TEXT("engine_frame_id"), double(GFrameCounter));
	Row->SetNumberField(TEXT("trim_calls"), Experiment->Calls);
	Row->SetNumberField(TEXT("trim_total_gt_ms"), Experiment->TotalMs);
	Row->SetObjectField(TEXT("cvars"), Experiment->Controls());
	Row->SetStringField(TEXT("memory26_case"), Memory26::CaseName());
	Row->SetObjectField(TEXT("resource_counters"), Experiment->Counters());
	return Experiment->Write(Row);
}
bool FinishHatchMemoryExperiment(bool Complete)
{
	if (!Experiment)
		return true;
	if (!Experiment->File)
	{
		Experiment.Reset();
		return false;
	}
	auto Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("type"), TEXT("summary"));
	Row->SetBoolField(TEXT("complete"), Complete);
	Row->SetNumberField(TEXT("trim_calls"), Experiment->Calls);
	Row->SetNumberField(TEXT("trim_total_gt_ms"), Experiment->TotalMs);
	Row->SetNumberField(TEXT("trim_max_gt_ms"), Experiment->MaxMs);
	Row->SetObjectField(TEXT("cvars"), Experiment->Controls());
	Row->SetStringField(TEXT("memory26_case"), Memory26::CaseName());
	Row->SetObjectField(TEXT("resource_counters"), Experiment->Counters());
	const bool Written = Experiment->Write(Row);
	const bool Closed = Experiment->File->Close();
	Experiment.Reset();
	return Written && Closed;
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemory26PoolTest, "HATCH26.BoundedDiagnosticPool", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMemory26PoolTest::RunTest(const FString&)
{
	struct FItem
	{
		TArray<uint8> Data;
		uint64 Bytes() const { return Data.GetAllocatedSize(); }
	};
	Memory26::TPool<FItem, 2, 1024> Pool;
	const auto C = Memory26::ECounter::PcdCreate, R = Memory26::ECounter::PcdReuse;
	auto A = Pool.Take(C, R), B = Pool.Take(C, R), D = Pool.Take(C, R);
	A->Data.SetNumUninitialized(256);
	A->Data[0] = 42;
	const void* Address = A->Data.GetData();
	TestTrue(TEXT("live leases have distinct ownership"), A.Get() != B.Get() && A.Get() != D.Get() && B.Get() != D.Get());
	Pool.Return(MoveTemp(A));
	auto E = Pool.Take(C, R);
	TestTrue(TEXT("returned capacity reused"), Address == E->Data.GetData());
	TestEqual(TEXT("contents retained until caller resets"), E->Data[0], uint8(42));
	E->Data.Reset();
	TestTrue(TEXT("reset keeps capacity"), Address == E->Data.GetData());
	Pool.Return(MoveTemp(E));
	Pool.Return(MoveTemp(B));
	Pool.Return(MoveTemp(D));
	TestEqual(TEXT("count bounded"), Pool.RetainedCount(), 2);
	TestTrue(TEXT("bytes bounded"), Pool.RetainedBytes() <= 1024);
	auto Large = Pool.Take(C, R);
	Large->Data.SetNumUninitialized(2048);
	Pool.Return(MoveTemp(Large));
	TestTrue(TEXT("oversized lease discarded"), Pool.RetainedBytes() <= 1024 && Pool.RetainedCount() == 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchTrimScheduleTest, "HATCH26.TrimDiagnosticSchedule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHatchTrimScheduleTest::RunTest(const FString&)
{
	TestTrue(TEXT("default off is valid"), ValidTrimPeriod(0));
	TestFalse(TEXT("default never trims"), TrimDue(3600, 0, 0));
	TestFalse(TEXT("negative period rejected"), ValidTrimPeriod(-1));
	TestFalse(TEXT("subminute period rejected"), ValidTrimPeriod(5));
	TestTrue(TEXT("one minute valid"), ValidTrimPeriod(60));
	TestFalse(TEXT("not due early"), TrimDue(59, 0, 60));
	TestTrue(TEXT("due at period"), TrimDue(60, 0, 60));
	double Last = 0;
	int32 Calls = 0;
	for (double Now : {0., 59., 60., 61., 300., 301.})
		if (TrimDue(Now, Last, 60))
		{
			Last = Now;
			++Calls;
		}
	TestEqual(TEXT("late sampler visit is one trim, no catchup burst"), Calls, 2);
	return true;
}
#endif
