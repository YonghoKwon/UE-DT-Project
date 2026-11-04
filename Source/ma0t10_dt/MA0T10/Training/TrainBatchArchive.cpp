#include "ma0t10_dt/MA0T10/Training/TrainBatchArchive.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
THIRD_PARTY_INCLUDES_START
#include "zlib.h"
#include "libzip/zip.h"
// OpenSSL typedefs UI, which collides with Unreal's UI namespace (same workaround as the engine).
#define UI UI_ST
#include "openssl/evp.h"
#undef UI
THIRD_PARTY_INCLUDES_END
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
constexpr int64 ChunkBytes = 1024 * 1024;
constexpr int32 MaxEntryNameBytes = 1024;
constexpr int64 UstarMaxSize = 077777777777ll;

bool Cancelled(const TFunction<bool()>& IsCancelled)
{
	return IsCancelled && IsCancelled();
}

/** Streaming SHA-256; OpenSSL is already linked through the engine's libzip dependency. */
struct FSha256
{
	EVP_MD_CTX* Ctx = EVP_MD_CTX_new();
	FSha256()
	{
		if (Ctx)
			EVP_DigestInit_ex(Ctx, EVP_sha256(), nullptr);
	}
	~FSha256()
	{
		if (Ctx)
			EVP_MD_CTX_free(Ctx);
	}
	void Update(const void* Data, int64 Size)
	{
		if (Ctx && Size > 0)
			EVP_DigestUpdate(Ctx, Data, size_t(Size));
	}
	FString Final()
	{
		uint8 Digest[EVP_MAX_MD_SIZE];
		unsigned int Length = 0;
		if (!Ctx || EVP_DigestFinal_ex(Ctx, Digest, &Length) != 1)
			return FString();
		return BytesToHex(Digest, int32(Length)).ToLower();
	}
};

/** Output archive that also hashes every byte it writes. */
struct FHashedWriter
{
	TUniquePtr<FArchive> Out;
	FSha256 Hash;
	int64 Bytes = 0;
	bool Write(const void* Data, int64 Size)
	{
		if (Size <= 0)
			return true;
		Out->Serialize(const_cast<void*>(Data), Size);
		Hash.Update(Data, Size);
		Bytes += Size;
		return !Out->IsError();
	}
};

/** gzip (RFC 1952) member around raw deflate, streamed in bounded buffers. */
class FGzipSink
{
public:
	explicit FGzipSink(FHashedWriter& InOut)
	    : Out(InOut) {}
	~FGzipSink()
	{
		if (bInit)
			deflateEnd(&Z);
	}
	bool Init(int32 Level)
	{
		bInit = deflateInit2(&Z, FMath::Clamp(Level, 0, 9), Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) == Z_OK;
		return bInit;
	}
	bool Write(const void* Data, int64 Size) { return Pump(Data, Size, Z_NO_FLUSH); }
	bool Finish() { return Pump(nullptr, 0, Z_FINISH); }

private:
	bool Pump(const void* Data, int64 Size, int Flush)
	{
		Z.next_in = (Bytef*)Data;
		Z.avail_in = uInt(Size);
		if (Buffer.IsEmpty())
			Buffer.SetNumUninitialized(256 * 1024);
		for (;;)
		{
			Z.next_out = Buffer.GetData();
			Z.avail_out = uInt(Buffer.Num());
			const int Result = deflate(&Z, Flush);
			if (Result == Z_STREAM_ERROR)
				return false;
			if (!Out.Write(Buffer.GetData(), int64(Buffer.Num()) - Z.avail_out))
				return false;
			if (Flush == Z_FINISH)
			{
				if (Result == Z_STREAM_END)
					return true;
				continue;
			}
			if (Z.avail_in == 0 && Z.avail_out != 0)
				return true;
		}
	}
	FHashedWriter& Out;
	TArray<uint8> Buffer;
	z_stream Z = {};
	bool bInit = false;
};

void PutOctal(char* Field, int32 Width, int64 Value)
{
	FCStringAnsi::Snprintf(Field, Width, "%0*llo", Width - 1, (long long)Value);
}

/** One 512-byte ustar header. Long/non-ASCII names and >8GiB sizes go in a preceding PAX record. */
void BuildHeader(uint8 (&Block)[512], const FTCHARToUTF8& Name, bool bAsciiFits, int64 Size, int64 MTime, char Type, int32 Index)
{
	FMemory::Memzero(Block);
	char* H = (char*)Block;
	if (bAsciiFits)
		FMemory::Memcpy(H, Name.Get(), Name.Length());
	else
		FCStringAnsi::Snprintf(H, 100, "PaxEntry/%08d", Index);
	PutOctal(H + 100, 8, 0644);
	PutOctal(H + 108, 8, 0);
	PutOctal(H + 116, 8, 0);
	PutOctal(H + 124, 12, Size <= UstarMaxSize ? Size : 0);
	PutOctal(H + 136, 12, MTime);
	FMemory::Memset(H + 148, ' ', 8);
	H[156] = Type;
	FMemory::Memcpy(H + 257, "ustar", 6);
	H[263] = '0';
	H[264] = '0';
	uint32 Sum = 0;
	for (int32 I = 0; I < 512; ++I)
		Sum += Block[I];
	FCStringAnsi::Snprintf(H + 148, 7, "%06o", Sum);
	H[154] = '\0';
	H[155] = ' ';
}

void AppendPaxRecord(TArray<uint8>& Out, const ANSICHAR* Key, const FTCHARToUTF8& Value)
{
	// "<len> key=value\n" where len counts its own digits.
	const int32 Body = 1 + FCStringAnsi::Strlen(Key) + 1 + Value.Length() + 1;
	int32 Length = Body + 1;
	for (;;)
	{
		const int32 Next = Body + FString::FromInt(Length).Len();
		if (Next == Length)
			break;
		Length = Next;
	}
	const FTCHARToUTF8 Prefix(*FString::Printf(TEXT("%d %s="), Length, ANSI_TO_TCHAR(Key)));
	Out.Append((const uint8*)Prefix.Get(), Prefix.Length());
	Out.Append((const uint8*)Value.Get(), Value.Length());
	Out.Add('\n');
}

bool IsAscii(const FString& S)
{
	for (TCHAR C : S)
		if (C > 127)
			return false;
	return true;
}

bool CopyEntry(const FTrainArchiveEntry& E, const TFunction<bool(const uint8*, int64)>& Sink, const FTrainArchiveOptions& O, int64& Done, int64 Total, FString& Error)
{
	TUniquePtr<FArchive> In(IFileManager::Get().CreateFileReader(*E.SourcePath));
	if (!In)
	{
		Error = TEXT("cannot open ") + E.EntryName;
		return false;
	}
	if (In->TotalSize() != E.Size)
	{
		Error = TEXT("file changed after listing: ") + E.EntryName;
		return false;
	}
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(int32(FMath::Min<int64>(ChunkBytes, FMath::Max<int64>(E.Size, 1))));
	for (int64 Left = E.Size; Left > 0;)
	{
		if (Cancelled(O.IsCancelled))
		{
			Error = TEXT("cancelled");
			return false;
		}
		const int64 N = FMath::Min<int64>(Left, Buffer.Num());
		In->Serialize(Buffer.GetData(), N);
		if (In->IsError())
		{
			Error = TEXT("read failed: ") + E.EntryName;
			return false;
		}
		if (!Sink(Buffer.GetData(), N))
		{
			Error = TEXT("write failed");
			return false;
		}
		Left -= N;
		Done += N;
		if (O.OnProgress)
			O.OnProgress(Done, Total);
	}
	return true;
}

bool WriteTarGz(const TArray<FTrainArchiveEntry>& Entries, FHashedWriter& Out, const FTrainArchiveOptions& O, int64 Total, FString& Error)
{
	FGzipSink Gz(Out);
	if (!Gz.Init(O.Level))
	{
		Error = TEXT("zlib init failed");
		return false;
	}
	const auto Sink = [&Gz](const uint8* D, int64 N)
	{
		return Gz.Write(D, N);
	};
	static const uint8 Zero[512] = {};
	int64 Done = 0;
	for (int32 I = 0; I < Entries.Num(); ++I)
	{
		const auto& E = Entries[I];
		const FTCHARToUTF8 Name(*E.EntryName);
		const bool bAsciiFits = IsAscii(E.EntryName) && Name.Length() < 100;
		uint8 Block[512];
		if (!bAsciiFits || E.Size > UstarMaxSize)
		{
			TArray<uint8> Pax;
			if (!bAsciiFits)
				AppendPaxRecord(Pax, "path", Name);
			if (E.Size > UstarMaxSize)
				AppendPaxRecord(Pax, "size", FTCHARToUTF8(*LexToString(E.Size)));
			BuildHeader(Block, Name, false, Pax.Num(), O.MTimeUnix, 'x', I);
			if (!Gz.Write(Block, 512) || !Gz.Write(Pax.GetData(), Pax.Num()) || !Gz.Write(Zero, (512 - Pax.Num() % 512) % 512))
			{
				Error = TEXT("write failed");
				return false;
			}
		}
		BuildHeader(Block, Name, bAsciiFits, E.Size, O.MTimeUnix, '0', I);
		if (!Gz.Write(Block, 512) || !CopyEntry(E, Sink, O, Done, Total, Error))
			return false;
		if (!Gz.Write(Zero, (512 - E.Size % 512) % 512))
		{
			Error = TEXT("write failed");
			return false;
		}
	}
	if (!Gz.Write(Zero, 512) || !Gz.Write(Zero, 512) || !Gz.Finish())
	{
		Error = TEXT("gzip finish failed");
		return false;
	}
	return true;
}

struct FZipCallbacks
{
	const FTrainArchiveOptions* Options;
	int64 Total;
};

bool WriteZip(const TArray<FTrainArchiveEntry>& Entries, const FString& Partial, const FTrainArchiveOptions& O, int64 Total, FString& Error)
{
	int ErrorCode = 0;
	zip_t* Zip = zip_open(FTCHARToUTF8(*Partial).Get(), ZIP_CREATE | ZIP_TRUNCATE, &ErrorCode);
	if (!Zip)
	{
		zip_error_t E;
		zip_error_init_with_code(&E, ErrorCode);
		Error = FString(TEXT("zip open failed: ")) + UTF8_TO_TCHAR(zip_error_strerror(&E));
		zip_error_fini(&E);
		return false;
	}
	const auto Fail = [&](const FString& Reason)
	{
		Error = Reason + TEXT(": ") + UTF8_TO_TCHAR(zip_strerror(Zip));
		zip_discard(Zip);
		return false;
	};
	for (const auto& E : Entries)
	{
		zip_source_t* Source = zip_source_file(Zip, FTCHARToUTF8(*E.SourcePath).Get(), 0, 0);
		if (!Source)
			return Fail(TEXT("zip source failed for ") + E.EntryName);
		const zip_int64_t Index = zip_file_add(Zip, FTCHARToUTF8(*E.EntryName).Get(), Source, ZIP_FL_ENC_UTF_8);
		if (Index < 0)
		{
			zip_source_free(Source);
			return Fail(TEXT("zip add failed for ") + E.EntryName);
		}
		if (zip_set_file_compression(Zip, Index, O.Level <= 0 ? ZIP_CM_STORE : ZIP_CM_DEFLATE, zip_uint32_t(FMath::Clamp(O.Level, 0, 9))) < 0)
			return Fail(TEXT("zip compression rejected"));
		if (zip_file_set_mtime(Zip, Index, time_t(O.MTimeUnix), 0) < 0)
			return Fail(TEXT("zip mtime rejected"));
	}
	// libzip reads sources during zip_close; progress/cancel are reported from there.
	auto* State = new FZipCallbacks{&O, Total};
	zip_register_progress_callback_with_state(Zip, 0.01, [](zip_t*, double Fraction, void* S)
	                                          {auto* C=(FZipCallbacks*)S;if(C->Options->OnProgress)C->Options->OnProgress(int64(Fraction*double(C->Total)),C->Total); }, [](void* S)
	                                          { delete (FZipCallbacks*)S; }, State);
	zip_register_cancel_callback_with_state(Zip, [](zip_t*, void* S) -> int
	                                        { return Cancelled(((FZipCallbacks*)S)->Options->IsCancelled) ? 1 : 0; }, nullptr, State);
	if (zip_close(Zip) < 0)
	{
		Error = Cancelled(O.IsCancelled) ? FString(TEXT("cancelled")) : FString(TEXT("zip close failed: ")) + UTF8_TO_TCHAR(zip_strerror(Zip));
		zip_discard(Zip);
		return false;
	}
	return true;
}
}

const TCHAR* TrainBatchArchive::Extension(ETrainArchiveFormat Format)
{
	return Format == ETrainArchiveFormat::Zip ? TEXT("zip") : TEXT("tar.gz");
}

bool TrainBatchArchive::IsSafeEntryName(const FString& Name)
{
	if (Name.IsEmpty() || Name.StartsWith(TEXT("/")) || Name.EndsWith(TEXT("/")) || FTCHARToUTF8(*Name).Length() > MaxEntryNameBytes)
		return false;
	for (TCHAR C : Name)
		if (C < 32 || C == 127 || C == TEXT('\\') || C == TEXT(':') || C == TEXT('*') || C == TEXT('?') || C == TEXT('"') || C == TEXT('<') || C == TEXT('>') || C == TEXT('|'))
			return false;
	TArray<FString> Parts;
	Name.ParseIntoArray(Parts, TEXT("/"), false);
	for (const auto& P : Parts)
		if (P.IsEmpty() || P == TEXT(".") || P == TEXT("..") || P.EndsWith(TEXT(".")) || P.EndsWith(TEXT(" ")))
			return false;
	return true;
}

bool TrainBatchArchive::CollectEntries(const FString& Root, const FString& Prefix, TArray<FTrainArchiveEntry>& Out, FString& Error)
{
	FString Base = FPaths::ConvertRelativePathToFull(Root);
	FPaths::NormalizeDirectoryName(Base);
	Base += TEXT("/");
	if (!IFileManager::Get().DirectoryExists(*Base))
	{
		Error = TEXT("result folder not found");
		return false;
	}
	bool bOk = true;
	IFileManager::Get().IterateDirectoryStatRecursively(*Base, [&](const TCHAR* Path, const FFileStatData& Stat)
	                                                    {
        if(Stat.bIsDirectory)return true;
        FString Full=Path;FPaths::NormalizeFilename(Full);
        if(!Full.StartsWith(Base,ESearchCase::IgnoreCase)){Error=TEXT("file outside result folder");bOk=false;return false;}
        FTrainArchiveEntry E;E.SourcePath=Full;E.EntryName=Prefix+Full.Mid(Base.Len());E.Size=Stat.FileSize;
        if(!IsSafeEntryName(E.EntryName)){Error=TEXT("unsafe entry name: ")+E.EntryName;bOk=false;return false;}
        Out.Add(MoveTemp(E));return true; });
	Out.Sort([](const FTrainArchiveEntry& A, const FTrainArchiveEntry& B)
	         { return A.EntryName.Compare(B.EntryName, ESearchCase::CaseSensitive) < 0; });
	return bOk;
}

FString TrainBatchArchive::Sha256Bytes(const void* Data, int64 Size)
{
	FSha256 H;
	H.Update(Data, Size);
	return H.Final();
}

bool TrainBatchArchive::Sha256File(const FString& Path, FString& OutHex, FString& Error, const TFunction<bool()>& IsCancelled)
{
	TUniquePtr<FArchive> In(IFileManager::Get().CreateFileReader(*Path));
	if (!In)
	{
		Error = TEXT("cannot open file for hashing");
		return false;
	}
	FSha256 H;
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(int32(ChunkBytes));
	for (int64 Left = In->TotalSize(); Left > 0;)
	{
		if (Cancelled(IsCancelled))
		{
			Error = TEXT("cancelled");
			return false;
		}
		const int64 N = FMath::Min<int64>(Left, ChunkBytes);
		In->Serialize(Buffer.GetData(), N);
		if (In->IsError())
		{
			Error = TEXT("read failed while hashing");
			return false;
		}
		H.Update(Buffer.GetData(), N);
		Left -= N;
	}
	OutHex = H.Final();
	if (OutHex.IsEmpty())
	{
		Error = TEXT("hash failed");
		return false;
	}
	return true;
}

bool TrainBatchArchive::WriteChecksums(const TArray<FTrainArchiveEntry>& Entries, const FString& OutputPath, FString& Error, const TFunction<bool()>& IsCancelled)
{
	FString Text;
	for (const auto& E : Entries)
	{
		FString Hex;
		if (!Sha256File(E.SourcePath, Hex, Error, IsCancelled))
			return false;
		Text += Hex + TEXT("  ") + E.EntryName + TEXT("\n");
	}
	const FTCHARToUTF8 Utf8(*Text);
	const FString Partial = OutputPath + TEXT(".partial");
	TUniquePtr<FArchive> Out(IFileManager::Get().CreateFileWriter(*Partial));
	if (!Out)
	{
		Error = TEXT("cannot create checksum file");
		return false;
	}
	Out->Serialize((void*)Utf8.Get(), Utf8.Length());
	const bool bWritten = !Out->IsError() && Out->Close();
	Out.Reset();
	if (!bWritten || !IFileManager::Get().Move(*OutputPath, *Partial, false))
	{
		IFileManager::Get().Delete(*Partial, false, true);
		Error = TEXT("checksum file not finalized");
		return false;
	}
	return true;
}

FTrainArchiveResult TrainBatchArchive::Write(const TArray<FTrainArchiveEntry>& Entries, const FString& OutputPath, const FTrainArchiveOptions& O)
{
	FTrainArchiveResult R;
	R.OutputPath = OutputPath;
	R.Entries = Entries.Num();
	if (Entries.IsEmpty())
	{
		R.Error = TEXT("nothing to archive");
		return R;
	}
	if (O.Level < 0 || O.Level > 9 || (O.Format == ETrainArchiveFormat::TarGz && O.Level == 0))
	{
		R.Error = TEXT("compression level out of range");
		return R;
	}
	if (IFileManager::Get().FileExists(*OutputPath))
	{
		R.Error = TEXT("archive already exists; not overwritten");
		return R;
	}
	TSet<FString> Seen;
	int64 Total = 0;
	for (const auto& E : Entries)
	{
		if (!IsSafeEntryName(E.EntryName))
		{
			R.Error = TEXT("unsafe entry name: ") + E.EntryName;
			return R;
		}
		bool bDuplicate = false;
		Seen.Add(E.EntryName.ToLower(), &bDuplicate);
		if (bDuplicate)
		{
			R.Error = TEXT("duplicate entry name: ") + E.EntryName;
			return R;
		}
		if (E.Size < 0)
		{
			R.Error = TEXT("invalid size");
			return R;
		}
		Total += E.Size;
	}
	const FString Partial = OutputPath + TEXT(".partial");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
	IFileManager::Get().Delete(*Partial, false, true);
	bool bWritten = false;
	if (O.Format == ETrainArchiveFormat::TarGz)
	{
		FHashedWriter Out;
		Out.Out.Reset(IFileManager::Get().CreateFileWriter(*Partial));
		if (!Out.Out)
			R.Error = TEXT("cannot create archive");
		else
		{
			bWritten = WriteTarGz(Entries, Out, O, Total, R.Error) && Out.Out->Close();
			R.Sha256 = Out.Hash.Final();
			R.Bytes = Out.Bytes;
		}
	}
	else
	{
		bWritten = WriteZip(Entries, Partial, O, Total, R.Error) && Sha256File(Partial, R.Sha256, R.Error, O.IsCancelled);
		R.Bytes = IFileManager::Get().FileSize(*Partial);
	}
	R.bCancelled = Cancelled(O.IsCancelled);
	if (!bWritten || R.bCancelled || !IFileManager::Get().Move(*OutputPath, *Partial, false))
	{
		IFileManager::Get().Delete(*Partial, false, true);
		if (R.Error.IsEmpty())
			R.Error = R.bCancelled ? TEXT("cancelled") : TEXT("archive not finalized");
		R.Sha256.Reset();
		R.Bytes = 0;
		return R;
	}
	R.bOk = true;
	return R;
}
