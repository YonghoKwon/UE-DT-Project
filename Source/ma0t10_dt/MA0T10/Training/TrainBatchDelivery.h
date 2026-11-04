#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchArchive.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchSftp.h"
#include "ma0t10_dt/MA0T10/Contracts/TrainBatchContract.h"
#include "Async/Future.h"
#include <atomic>

/** User-chosen delivery profile. Holds no secret: the private key stays in its own file. */
struct FTrainDeliverySettings
{
	ETrainArchiveFormat Format = ETrainArchiveFormat::Zip;
	int32 Level = 3;
	FTrainSftpSettings Sftp;
};

enum class ETrainDeliveryPhase : uint8
{
	Idle,
	Collecting,
	Hashing,
	Archiving,
	Uploading,
	Done,
	Failed,
	Cancelled
};

struct FTrainDeliveryRequest
{
	FString JobId;
	/** Folder whose files become "<JobId>/..." inside the archive. Ignored when Entries is set. */
	FString StagingRoot;
	/** Explicit confirmed files (see BuildJobEntries); preferred over StagingRoot. */
	TArray<FTrainArchiveEntry> Entries;
	/** Local folder for the archive, its .sha256 and artifact.json. */
	FString OutputDir;
	FTrainDeliverySettings Settings;
	/** False: package only (user may send later with the same archive). */
	bool bUpload = false;
	/** Packages this job on the worker thread (BuildJobEntries); avoids scanning run folders on the game thread. */
	TOptional<FTrainBatchJobView> Job;
	/** With Job: package completed scenarios only and list the rest in excluded_scenarios.txt. */
	bool bAllowPartial = false;
};

struct FTrainDeliveryStatus
{
	ETrainDeliveryPhase Phase = ETrainDeliveryPhase::Idle;
	/** Sub-step such as "probe"/"upload"/"verify"/"finalize" while uploading. */
	FString Step;
	int64 Done = 0, Total = 0;
	FString Error;
	FString ArchivePath;
	FString ArchiveSha256;
	int64 ArchiveBytes = 0;
	FString RemoteName;
	bool bReusedArchive = false;
	/** Local paths of this selection's per-file manifest and (success-only) exclusion list. */
	FString ChecksumsPath, ExcludedPath;
	TArray<FString> Excluded;
};

namespace TrainBatchDelivery
{
MA0T10_DT_API FString SettingsPath();
/** Missing file yields defaults. Never stores keys, passphrases or passwords. */
MA0T10_DT_API bool LoadSettings(FTrainDeliverySettings& Out, FString& Error);
MA0T10_DT_API bool SaveSettings(const FTrainDeliverySettings& Settings, FString& Error);
/** App-owned known_hosts next to the settings file. */
MA0T10_DT_API FString KnownHostsPath();
MA0T10_DT_API bool IsSafeJobId(const FString& JobId);
/** "<JobId>-<first 12 hex of sha256>.<ext>": the name changes whenever the bytes change, so a .partial is only resumed for identical content. */
MA0T10_DT_API FString RemoteArchiveName(const FString& JobId, const FString& Sha256, ETrainArchiveFormat Format);
/**
     * Original inputs plus every completed scenario run as "<JobId>/input/..." and "<JobId>/scenarios/<id>/...".
     * Unless bAllowPartial, any scenario that is not Completed refuses the whole job (user decision 2026-11-02);
     * with it, skipped ids are returned in Excluded so the UI can show them before sending.
     */
MA0T10_DT_API bool BuildJobEntries(const FTrainBatchJobView& Job, bool bAllowPartial, TArray<FTrainArchiveEntry>& Out, TArray<FString>& Excluded, FString& Error);
/** Saved/TrainBatch/Deliveries/<JobId>. Files inside are named per selection, so different selections never collide. */
MA0T10_DT_API FString OutputDirectory(const FString& JobId);
/** Game-thread-cheap check (states only, no file IO): ids that are not Completed. */
MA0T10_DT_API TArray<FString> IncompleteScenarios(const FTrainBatchJobView& Job);
}

/**
 * One background delivery: collect → per-file checksums → archive → artifact.json → optional SFTP.
 * An existing archive whose artifact.json still matches its bytes is reused instead of rebuilt.
 * Status() is safe from the game thread; work never runs there.
 */
class MA0T10_DT_API FTrainBatchDelivery : public TSharedFromThis<FTrainBatchDelivery, ESPMode::ThreadSafe>
{
public:
	bool Start(const FTrainDeliveryRequest& Request, FString& Error);
	void Cancel() { bCancel.store(true); }
	bool IsRunning() const { return bRunning.load(); }
	FTrainDeliveryStatus Status() const;
	/** Blocks until the worker has finished (shutdown/tests). */
	void Wait();

private:
	void Run(FTrainDeliveryRequest Request);
	void Set(ETrainDeliveryPhase Phase, const FString& Step = FString());
	void Fail(const FString& Error);
	mutable FCriticalSection Mutex;
	FTrainDeliveryStatus State;
	std::atomic<bool> bCancel{false}, bRunning{false};
	TFuture<void> Worker;
};
