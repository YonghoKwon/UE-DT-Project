#pragma once
#include "CoreMinimal.h"
#include "TrainBatchArchive.h"
#include "TrainBatchSftp.h"
#include "Async/Future.h"
#include <atomic>

/** User-chosen delivery profile. Holds no secret: the private key stays in its own file. */
struct FTrainDeliverySettings
{
    ETrainArchiveFormat Format=ETrainArchiveFormat::Zip;
    int32 Level=3;
    FTrainSftpSettings Sftp;
};

enum class ETrainDeliveryPhase : uint8 { Idle, Collecting, Hashing, Archiving, Uploading, Done, Failed, Cancelled };

struct FTrainDeliveryRequest
{
    FString JobId;
    /** Folder whose files become "<JobId>/..." inside the archive (already limited to confirmed results). */
    FString StagingRoot;
    /** Local folder for the archive, its .sha256 and artifact.json. */
    FString OutputDir;
    FTrainDeliverySettings Settings;
    /** False: package only (user may send later with the same archive). */
    bool bUpload=false;
};

struct FTrainDeliveryStatus
{
    ETrainDeliveryPhase Phase=ETrainDeliveryPhase::Idle;
    /** Sub-step such as "probe"/"upload"/"verify"/"finalize" while uploading. */
    FString Step;
    int64 Done=0,Total=0;
    FString Error;
    FString ArchivePath;
    FString ArchiveSha256;
    int64 ArchiveBytes=0;
    FString RemoteName;
    bool bReusedArchive=false;
};

namespace TrainBatchDelivery
{
    MA0T10_DT_API FString SettingsPath();
    /** Missing file yields defaults. Never stores keys, passphrases or passwords. */
    MA0T10_DT_API bool LoadSettings(FTrainDeliverySettings& Out,FString& Error);
    MA0T10_DT_API bool SaveSettings(const FTrainDeliverySettings& Settings,FString& Error);
    /** App-owned known_hosts next to the settings file. */
    MA0T10_DT_API FString KnownHostsPath();
    MA0T10_DT_API bool IsSafeJobId(const FString& JobId);
    /** "<JobId>-<first 12 hex of sha256>.<ext>": the name changes whenever the bytes change, so a .partial is only resumed for identical content. */
    MA0T10_DT_API FString RemoteArchiveName(const FString& JobId,const FString& Sha256,ETrainArchiveFormat Format);
}

/**
 * One background delivery: collect → per-file checksums → archive → artifact.json → optional SFTP.
 * An existing archive whose artifact.json still matches its bytes is reused instead of rebuilt.
 * Status() is safe from the game thread; work never runs there.
 */
class MA0T10_DT_API FTrainBatchDelivery : public TSharedFromThis<FTrainBatchDelivery,ESPMode::ThreadSafe>
{
public:
    bool Start(const FTrainDeliveryRequest& Request,FString& Error);
    void Cancel(){bCancel.store(true);}
    bool IsRunning() const {return bRunning.load();}
    FTrainDeliveryStatus Status() const;
    /** Blocks until the worker has finished (shutdown/tests). */
    void Wait();
private:
    void Run(FTrainDeliveryRequest Request);
    void Set(ETrainDeliveryPhase Phase,const FString& Step=FString());
    void Fail(const FString& Error);
    mutable FCriticalSection Mutex;
    FTrainDeliveryStatus State;
    std::atomic<bool> bCancel{false},bRunning{false};
    TFuture<void> Worker;
};
