#pragma once
#include "CoreMinimal.h"

/** TRAIN-BATCH-01 result packaging. tar.zst is deferred by user decision (no zstd source). */
enum class ETrainArchiveFormat : uint8
{
	Zip,
	TarGz
};

struct FTrainArchiveEntry
{
	/** Absolute local file. */
	FString SourcePath;
	/** Normalized relative UTF-8 path inside the archive ('/' separators). */
	FString EntryName;
	int64 Size = 0;
};

struct FTrainArchiveOptions
{
	ETrainArchiveFormat Format = ETrainArchiveFormat::Zip;
	/** 0 = store (ZIP only), 1..9 = deflate level. */
	int32 Level = 3;
	/** Fixed entry time so identical inputs produce identical metadata. ZIP (DOS) time starts at 1980-01-01. */
	int64 MTimeUnix = 315532800;
	TFunction<bool()> IsCancelled;
	/** Source bytes consumed / total source bytes. Called from the writing thread. */
	TFunction<void(int64, int64)> OnProgress;
};

struct FTrainArchiveResult
{
	bool bOk = false;
	bool bCancelled = false;
	FString Error;
	FString OutputPath;
	FString Sha256;
	int64 Bytes = 0;
	int32 Entries = 0;
};

namespace TrainBatchArchive
{
MA0T10_DT_API const TCHAR* Extension(ETrainArchiveFormat Format);
/** ZIP is available on Windows; tar.gz remains available on all supported targets. */
MA0T10_DT_API bool IsFormatSupported(ETrainArchiveFormat Format);
MA0T10_DT_API const TCHAR* ZipUnavailableReason();
/** Rejects empty, absolute, drive, '\\', '.', '..', control characters and over-long names. */
MA0T10_DT_API bool IsSafeEntryName(const FString& Name);
/** Every regular file under Root, sorted ordinally by entry name. Fails on any unsafe name. */
MA0T10_DT_API bool CollectEntries(const FString& Root, const FString& Prefix, TArray<FTrainArchiveEntry>& Out, FString& Error);
/** Streams a file in bounded chunks. Hex is lowercase. */
MA0T10_DT_API bool Sha256File(const FString& Path, FString& OutHex, FString& Error, const TFunction<bool()>& IsCancelled = nullptr);
MA0T10_DT_API FString Sha256Bytes(const void* Data, int64 Size);
/**
     * Writes OutputPath via OutputPath.partial, then renames. Never loads whole files.
     * Blocking: call from a worker thread. Duplicate entry names (case-insensitive) are rejected.
     */
MA0T10_DT_API FTrainArchiveResult Write(const TArray<FTrainArchiveEntry>& Entries, const FString& OutputPath, const FTrainArchiveOptions& Options);
/** "sha256  name\n" lines for sorted entries (sha256sum -c compatible). */
MA0T10_DT_API bool WriteChecksums(const TArray<FTrainArchiveEntry>& Entries, const FString& OutputPath, FString& Error, const TFunction<bool()>& IsCancelled = nullptr);
}
