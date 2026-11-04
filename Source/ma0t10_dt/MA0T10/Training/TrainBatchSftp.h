#pragma once
#include "CoreMinimal.h"

/**
 * TRAIN-BATCH-01 delivery to a Linux receiver over SFTP with no server-side code.
 * Uses the Windows inbox OpenSSH client at a fixed system path; key authentication only.
 * Files land as <name>.partial and are renamed after size checks; the artifact is renamed last,
 * so a receiver may treat "<artifact> present and <artifact>.sha256 present" as complete.
 */
struct FTrainSftpSettings
{
	FString Host;
	int32 Port = 22;
	FString User;
	/** Absolute POSIX directory. Only its last component is created when missing. */
	FString RemoteDirectory;
	FString PrivateKeyPath;
	/** App-owned known_hosts (never the user's ~/.ssh file). */
	FString KnownHostsPath;
	int32 ConnectTimeoutSec = 30;
	/** Per sftp/ssh process; uploads of large archives need a generous value. */
	int32 ProcessTimeoutSec = 6 * 3600;
};

/** RemoteName must identify the exact bytes (e.g. "<JobId>-<sha256 prefix>.zip") because an existing .partial is resumed. */
struct FTrainSftpFile
{
	FString LocalPath;
	FString RemoteName;
	int64 Size = 0;
};

struct FTrainSftpProcessResult
{
	bool bLaunched = false;
	bool bCancelled = false;
	bool bTimedOut = false;
	int32 ExitCode = -1;
	/** Already masked; safe for UI/log. */
	FString Output;
};

enum class ETrainSftpOutcome : uint8
{
	Uploaded,
	Conflict,
	Rejected,
	Failed,
	Cancelled
};

struct FTrainSftpUploadResult
{
	ETrainSftpOutcome Outcome = ETrainSftpOutcome::Failed;
	FString Error;
	/** Last masked client output for diagnosis. */
	FString Detail;
	TArray<FString> Uploaded;
};

namespace TrainBatchSftp
{
/** Fixed inbox client path; empty when not installed (feature then refuses to run). */
MA0T10_DT_API FString ClientExecutable(const TCHAR* Tool = TEXT("sftp.exe"));
MA0T10_DT_API bool ValidateSettings(const FTrainSftpSettings& Settings, FString& Error, bool bRequireKnownHosts = true);
/** [A-Za-z0-9._-], no leading '.' or '-', max 200 chars. */
MA0T10_DT_API bool IsSafeRemoteName(const FString& Name);
MA0T10_DT_API bool IsSafeRemoteDirectory(const FString& Directory);
/** sftp batch quoting: forward slashes, double quotes, escaped backslash/quote. */
MA0T10_DT_API FString Quote(const FString& Value);
MA0T10_DT_API FString CommandLine(const FTrainSftpSettings& Settings, const FString& BatchPath, bool bAcceptNewHostKey = false, const FString& KnownHostsOverride = FString());
MA0T10_DT_API FString ProbeBatch(const FTrainSftpSettings& Settings);
/** put/reput each file to <name>.partial based on the remote listing. */
MA0T10_DT_API FString UploadBatch(const FTrainSftpSettings& Settings, const TArray<FTrainSftpFile>& Files, const TMap<FString, int64>& Remote);
/** Sidecars first, the first file (the artifact) last. */
MA0T10_DT_API FString FinalizeBatch(const FTrainSftpSettings& Settings, const TArray<FTrainSftpFile>& Files);
/** Parses `ls -ln` output (lines echoed with "sftp>" are ignored). */
MA0T10_DT_API TMap<FString, int64> ParseListing(const FString& Output);
MA0T10_DT_API FString Mask(const FString& Text, const FTrainSftpSettings& Settings);
/** Blocking. Captures stdout/stderr (bounded), honours cancel and timeout. */
MA0T10_DT_API FTrainSftpProcessResult RunProcess(const FString& Executable, const FString& Arguments, const FTrainSftpSettings& Settings, const TFunction<bool()>& IsCancelled, int32 TimeoutSec);
/**
     * Blocking end-to-end upload: probe → conflict check → put/reput → size check → rename.
     * Files[0] is the artifact. Call from a worker thread. WorkDir holds generated batch files.
     */
MA0T10_DT_API FTrainSftpUploadResult Upload(const FTrainSftpSettings& Settings, const TArray<FTrainSftpFile>& Files, const FString& WorkDir, const TFunction<bool()>& IsCancelled, const TFunction<void(const FString&)>& OnPhase = nullptr);
/**
     * First connection: accepts the host key into PendingKnownHosts (not the trusted file) and
     * returns its fingerprint for the user to confirm. Requires working key authentication.
     */
MA0T10_DT_API bool FetchHostFingerprint(const FTrainSftpSettings& Settings, const FString& PendingKnownHosts, const FString& WorkDir, FString& OutFingerprint, FString& Error);
/** Moves a confirmed pending known_hosts into Settings.KnownHostsPath. */
MA0T10_DT_API bool ConfirmHostKey(const FTrainSftpSettings& Settings, const FString& PendingKnownHosts, FString& Error);
}
