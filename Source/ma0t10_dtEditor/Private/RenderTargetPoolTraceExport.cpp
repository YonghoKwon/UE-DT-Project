#include "RenderTargetPoolTraceExport.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "Json.h"
#include "Misc/Paths.h"
#include "Trace/Analyzer.h"
#include "Trace/Analysis.h"
#include "Trace/DataStream.h"

namespace
{
struct FPoolTrace final : UE::Trace::IAnalyzer
{
	FArchive& Writer;
	uint64 Count = 0, Missing = 0;
	explicit FPoolTrace(FArchive& In)
	    : Writer(In) {}
	void Write(const TSharedRef<FJsonObject>& Row)
	{
		FString Text;
		FJsonSerializer::Serialize(Row, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		Text += TEXT("\n");
		FTCHARToUTF8 Bytes(*Text);
		Writer.Serialize((void*)Bytes.Get(), Bytes.Length());
	}
	virtual void OnAnalysisBegin(const FOnAnalysisContext& C) override
	{
		C.InterfaceBuilder.RouteEvent(1, "Cpu", "FRenderTargetPool_CreateTexture", true);
		C.InterfaceBuilder.RouteEvent(2, "HatchPool", "ViewFamily");
		C.InterfaceBuilder.RouteEvent(3, "HatchPool", "PoolEntry");
	}
	virtual bool OnEvent(uint16 Route, EStyle Style, const FOnEventContext& C) override
	{
		if (Style == EStyle::LeaveScope)
			return true;
		if (Route == 2 || Route == 3)
		{
			const auto& E = C.EventData;
			auto Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("type"), Route == 2 ? TEXT("view_family") : TEXT("pool_entry"));
			Row->SetNumberField(TEXT("qpc"), double(E.GetValue<uint64>("Qpc")));
			Row->SetStringField(TEXT("family"), FString::Printf(TEXT("%llx"), E.GetValue<uint64>("Family")));
			if (Route == 2)
			{
				FString World;
				E.GetString("World", World);
				Row->SetStringField(TEXT("world"), World);
				Row->SetBoolField(TEXT("begin"), E.GetValue<uint8>("Begin") != 0);
				Row->SetNumberField(TEXT("width"), E.GetValue<uint32>("Width"));
				Row->SetNumberField(TEXT("height"), E.GetValue<uint32>("Height"));
			}
			else
			{
				FString Name;
				E.GetString("Name", Name);
				Row->SetStringField(TEXT("name"), Name);
				Row->SetStringField(TEXT("rhi"), FString::Printf(TEXT("%llx"), E.GetValue<uint64>("Rhi")));
				Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%016llx"), E.GetValue<uint64>("Flags")));
				for (const auto* Field : {"Width", "Height", "Depth", "ArraySize", "Mips", "Samples", "Format", "Hash", "Refs", "Unused"})
					Row->SetNumberField(UTF8_TO_TCHAR(Field), E.GetValue<uint32>(Field));
			}
			Write(Row);
			return true;
		}
		FString Name;
		const uint64 Qpc = C.EventTime.AsCycle64();
		if (!C.EventData.GetString("Name", Name) || !Qpc)
		{
			++Missing;
			return true;
		}
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("type"), TEXT("pool_create"));
		Row->SetStringField(TEXT("name"), Name);
		Row->SetNumberField(TEXT("qpc"), double(Qpc));
		Row->SetNumberField(TEXT("seconds"), C.EventTime.AsSeconds());
		Row->SetNumberField(TEXT("thread"), C.ThreadInfo.GetId());
		Write(Row);
		++Count;
		return true;
	}
};
}

int32 ExportRenderTargetPoolTrace(const FString& Input, const FString& Output)
{
	UE::Trace::FFileDataStream File;
	if (!File.Open(*Input))
		return 2;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output), true);
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Output));
	if (!Writer)
		return 2;
	FPoolTrace Analyzer(*Writer);
	UE::Trace::FAnalysisContext Context;
	Context.AddAnalyzer(Analyzer);
	auto Worker = Context.Process(File);
	const double Start = FPlatformTime::Seconds();
	bool Complete = true;
	while (Worker.IsActive())
	{
		if (FPlatformTime::Seconds() - Start > 600 || FPlatformMemory::GetStats().UsedPhysical > 8ull * 1024 * 1024 * 1024)
		{
			Complete = false;
			Worker.Stop();
			break;
		}
		FPlatformProcess::Sleep(.01f);
	}
	Worker.Wait();
	auto Summary = MakeShared<FJsonObject>();
	Summary->SetStringField(TEXT("type"), TEXT("summary"));
	Summary->SetBoolField(TEXT("complete"), Complete && Analyzer.Count > 0 && !Analyzer.Missing);
	Summary->SetNumberField(TEXT("events"), double(Analyzer.Count));
	Summary->SetNumberField(TEXT("missing"), double(Analyzer.Missing));
	Analyzer.Write(Summary);
	Writer->Close();
	return Complete && Analyzer.Count > 0 && !Analyzer.Missing ? 0 : 2;
}
