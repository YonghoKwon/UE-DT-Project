#include "TrainBatchDelivery.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString SettingsDir(){return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("TrainBatch/Settings"));}

FString EncodeJson(const TSharedRef<FJsonObject>& O)
{
    FString S;const auto W=TJsonWriterFactory<>::Create(&S);FJsonSerializer::Serialize(O,W);return S;
}

bool SaveAtomically(const FString& Path,const FString& Text)
{
    const FString Partial=Path+TEXT(".partial");
    return FFileHelper::SaveStringToFile(Text,*Partial,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)&&IFileManager::Get().Move(*Path,*Partial,true);
}

bool ReadArtifact(const FString& Path,FString& Sha256,FString& Format,int64& Bytes)
{
    FString Text;TSharedPtr<FJsonObject> O;
    if(!FFileHelper::LoadFileToString(Text,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O)||!O)return false;
    double Size=0;return O->TryGetStringField(TEXT("sha256"),Sha256)&&O->TryGetStringField(TEXT("format"),Format)&&O->TryGetNumberField(TEXT("bytes"),Size)&&(Bytes=int64(Size),true);
}
}

FString TrainBatchDelivery::SettingsPath(){return SettingsDir()/TEXT("server.json");}
FString TrainBatchDelivery::KnownHostsPath(){return SettingsDir()/TEXT("known_hosts");}

bool TrainBatchDelivery::IsSafeJobId(const FString& JobId){return JobId.Len()<=64&&TrainBatchSftp::IsSafeRemoteName(JobId)&&!JobId.Contains(TEXT("."));}

FString TrainBatchDelivery::RemoteArchiveName(const FString& JobId,const FString& Sha256,ETrainArchiveFormat Format)
{
    return JobId+TEXT("-")+Sha256.Left(12)+TEXT(".")+TrainBatchArchive::Extension(Format);
}

bool TrainBatchDelivery::LoadSettings(FTrainDeliverySettings& Out,FString& Error)
{
    Out=FTrainDeliverySettings();Out.Sftp.KnownHostsPath=KnownHostsPath();
    FString Text;if(!FFileHelper::LoadFileToString(Text,*SettingsPath()))return true;
    TSharedPtr<FJsonObject> O;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O)||!O){Error=TEXT("server settings file is not valid JSON");return false;}
    FString Format;O->TryGetStringField(TEXT("format"),Format);Out.Format=Format==TEXT("tar.gz")?ETrainArchiveFormat::TarGz:ETrainArchiveFormat::Zip;
    double Number=0;
    if(O->TryGetNumberField(TEXT("level"),Number))Out.Level=FMath::Clamp(int32(Number),0,9);
    if(O->TryGetNumberField(TEXT("port"),Number))Out.Sftp.Port=int32(Number);
    if(O->TryGetNumberField(TEXT("connect_timeout_sec"),Number))Out.Sftp.ConnectTimeoutSec=int32(Number);
    O->TryGetStringField(TEXT("host"),Out.Sftp.Host);O->TryGetStringField(TEXT("user"),Out.Sftp.User);
    O->TryGetStringField(TEXT("remote_dir"),Out.Sftp.RemoteDirectory);O->TryGetStringField(TEXT("private_key_path"),Out.Sftp.PrivateKeyPath);
    return true;
}

bool TrainBatchDelivery::SaveSettings(const FTrainDeliverySettings& S,FString& Error)
{
    const auto O=MakeShared<FJsonObject>();
    O->SetStringField(TEXT("schema"),TEXT("ma0t10.train.delivery-settings.v1"));
    O->SetStringField(TEXT("format"),TrainBatchArchive::Extension(S.Format));O->SetNumberField(TEXT("level"),S.Level);
    O->SetStringField(TEXT("host"),S.Sftp.Host);O->SetNumberField(TEXT("port"),S.Sftp.Port);O->SetStringField(TEXT("user"),S.Sftp.User);
    O->SetStringField(TEXT("remote_dir"),S.Sftp.RemoteDirectory);O->SetNumberField(TEXT("connect_timeout_sec"),S.Sftp.ConnectTimeoutSec);
    // Only the key file's location; the key itself and any passphrase never enter this file.
    O->SetStringField(TEXT("private_key_path"),S.Sftp.PrivateKeyPath);
    IFileManager::Get().MakeDirectory(*SettingsDir(),true);
    if(!SaveAtomically(SettingsPath(),EncodeJson(O))){Error=TEXT("cannot save server settings");return false;}
    return true;
}

bool FTrainBatchDelivery::Start(const FTrainDeliveryRequest& Request,FString& Error)
{
    if(bRunning.load()){Error=TEXT("delivery already running");return false;}
    if(!TrainBatchDelivery::IsSafeJobId(Request.JobId)){Error=TEXT("invalid job id");return false;}
    if(Request.Settings.Level<0||Request.Settings.Level>9){Error=TEXT("compression level out of range");return false;}
    if(Request.bUpload)
    {
        FTrainSftpSettings Sftp=Request.Settings.Sftp;if(Sftp.KnownHostsPath.IsEmpty())Sftp.KnownHostsPath=TrainBatchDelivery::KnownHostsPath();
        if(!TrainBatchSftp::ValidateSettings(Sftp,Error))return false;
    }
    {FScopeLock Lock(&Mutex);State=FTrainDeliveryStatus();}
    bCancel.store(false);bRunning.store(true);
    Worker=Async(EAsyncExecution::Thread,[Self=AsShared(),Request](){Self->Run(Request);Self->bRunning.store(false);});
    return true;
}

FTrainDeliveryStatus FTrainBatchDelivery::Status() const{FScopeLock Lock(&Mutex);return State;}
void FTrainBatchDelivery::Wait(){if(Worker.IsValid())Worker.Wait();}
void FTrainBatchDelivery::Set(ETrainDeliveryPhase Phase,const FString& Step){FScopeLock Lock(&Mutex);State.Phase=Phase;State.Step=Step;}
void FTrainBatchDelivery::Fail(const FString& Error)
{
    FScopeLock Lock(&Mutex);State.Phase=bCancel.load()?ETrainDeliveryPhase::Cancelled:ETrainDeliveryPhase::Failed;State.Error=Error;
}

void FTrainBatchDelivery::Run(FTrainDeliveryRequest R)
{
    const TFunction<bool()> Cancelled=[this](){return bCancel.load();};
    const FTrainDeliverySettings& S=R.Settings;
    const FString Ext=TrainBatchArchive::Extension(S.Format);
    const FString Archive=R.OutputDir/(R.JobId+TEXT(".")+Ext),Artifact=R.OutputDir/(R.JobId+TEXT(".artifact.json"));
    const FString Sidecar=Archive+TEXT(".sha256"),Checksums=R.OutputDir/(R.JobId+TEXT(".checksums.sha256"));
    IFileManager::Get().MakeDirectory(*R.OutputDir,true);
    FString Error,Sha;int64 Bytes=0;bool bReuse=false;
    {
        FString OldSha,OldFormat;int64 OldBytes=0;
        if(IFileManager::Get().FileExists(*Archive))
        {
            // Never overwrite: reuse only when the recorded hash still describes these exact bytes.
            Set(ETrainDeliveryPhase::Hashing);
            if(!ReadArtifact(Artifact,OldSha,OldFormat,OldBytes)||OldFormat!=Ext||!TrainBatchArchive::Sha256File(Archive,Sha,Error,Cancelled)||Sha!=OldSha||IFileManager::Get().FileSize(*Archive)!=OldBytes)
            {Fail(Error.IsEmpty()?TEXT("an archive with this name exists but does not match its record; remove it to rebuild"):Error);return;}
            bReuse=true;Bytes=OldBytes;
        }
    }
    if(!bReuse)
    {
        Set(ETrainDeliveryPhase::Collecting);
        TArray<FTrainArchiveEntry> Entries;
        if(!TrainBatchArchive::CollectEntries(R.StagingRoot,R.JobId+TEXT("/"),Entries,Error)){Fail(Error);return;}
        if(Entries.IsEmpty()){Fail(TEXT("no confirmed result files to package"));return;}
        Set(ETrainDeliveryPhase::Hashing);
        IFileManager::Get().Delete(*Checksums,false,true);
        if(!TrainBatchArchive::WriteChecksums(Entries,Checksums,Error,Cancelled)){Fail(Error);return;}
        FTrainArchiveEntry Manifest;Manifest.SourcePath=Checksums;Manifest.EntryName=R.JobId+TEXT("/checksums.sha256");Manifest.Size=IFileManager::Get().FileSize(*Checksums);
        if(Entries.ContainsByPredicate([&](const FTrainArchiveEntry& E){return E.EntryName.Equals(Manifest.EntryName,ESearchCase::IgnoreCase);})){Fail(TEXT("staging folder already contains checksums.sha256"));return;}
        Entries.Add(Manifest);
        Set(ETrainDeliveryPhase::Archiving);
        FTrainArchiveOptions O;O.Format=S.Format;O.Level=S.Level;O.IsCancelled=Cancelled;
        O.OnProgress=[this](int64 Done,int64 Total){FScopeLock Lock(&Mutex);State.Done=Done;State.Total=Total;};
        const FTrainArchiveResult A=TrainBatchArchive::Write(Entries,Archive,O);
        if(!A.bOk){Fail(A.Error);return;}
        Sha=A.Sha256;Bytes=A.Bytes;
        const auto J=MakeShared<FJsonObject>();
        J->SetStringField(TEXT("schema"),TEXT("ma0t10.train.artifact.v1"));J->SetStringField(TEXT("job_id"),R.JobId);
        J->SetStringField(TEXT("format"),Ext);J->SetNumberField(TEXT("level"),S.Level);J->SetStringField(TEXT("sha256"),Sha);
        J->SetNumberField(TEXT("bytes"),double(Bytes));J->SetNumberField(TEXT("entries"),Entries.Num());
        J->SetStringField(TEXT("remote_name"),TrainBatchDelivery::RemoteArchiveName(R.JobId,Sha,S.Format));
        J->SetStringField(TEXT("created_utc"),FDateTime::UtcNow().ToIso8601());
        if(!SaveAtomically(Artifact,EncodeJson(J))){Fail(TEXT("cannot write artifact.json"));return;}
    }
    const FString RemoteName=TrainBatchDelivery::RemoteArchiveName(R.JobId,Sha,S.Format);
    if(!SaveAtomically(Sidecar,Sha+TEXT("  ")+RemoteName+TEXT("\n"))){Fail(TEXT("cannot write .sha256"));return;}
    {FScopeLock Lock(&Mutex);State.ArchivePath=Archive;State.ArchiveSha256=Sha;State.ArchiveBytes=Bytes;State.RemoteName=RemoteName;State.bReusedArchive=bReuse;}
    if(!R.bUpload){Set(ETrainDeliveryPhase::Done);return;}
    Set(ETrainDeliveryPhase::Uploading);
    FTrainSftpSettings Sftp=S.Sftp;if(Sftp.KnownHostsPath.IsEmpty())Sftp.KnownHostsPath=TrainBatchDelivery::KnownHostsPath();
    const TArray<FTrainSftpFile> Files={{Archive,RemoteName,Bytes},{Sidecar,RemoteName+TEXT(".sha256"),IFileManager::Get().FileSize(*Sidecar)},
        {Artifact,R.JobId+TEXT("-")+Sha.Left(12)+TEXT(".artifact.json"),IFileManager::Get().FileSize(*Artifact)}};
    const FTrainSftpUploadResult U=TrainBatchSftp::Upload(Sftp,Files,FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("TrainBatch/Work")),Cancelled,
        [this](const FString& Step){FScopeLock Lock(&Mutex);State.Step=Step;});
    if(U.Outcome!=ETrainSftpOutcome::Uploaded){Fail(U.Error+(U.Detail.IsEmpty()?FString():TEXT("\n")+U.Detail.Right(2000)));return;}
    Set(ETrainDeliveryPhase::Done);
}
