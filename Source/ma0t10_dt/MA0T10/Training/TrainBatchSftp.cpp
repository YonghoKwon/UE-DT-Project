#include "ma0t10_dt/MA0T10/Training/TrainBatchSftp.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace
{
constexpr int32 MaxCapturedOutput=256*1024;

bool AllChars(const FString& S,const TCHAR* Extra)
{
    for(TCHAR C:S)if(!FChar::IsAlnum(C)||C>127)if(!FCString::Strchr(Extra,C))return false;
    return true;
}

/** Windows command-line quoting for CreateProc arguments. */
FString Arg(const FString& Value)
{
    FString Out=TEXT("\"");int32 Slashes=0;
    for(TCHAR C:Value)
    {
        if(C==TEXT('\\')){++Slashes;continue;}
        if(C==TEXT('"')){Out+=FString::ChrN(Slashes*2+1,TEXT('\\'));Slashes=0;Out+=C;continue;}
        Out+=FString::ChrN(Slashes,TEXT('\\'));Slashes=0;Out+=C;
    }
    return Out+FString::ChrN(Slashes*2,TEXT('\\'))+TEXT("\"");
}

FString Partial(const FString& Name){return Name+TEXT(".partial");}

bool WriteBatch(const FString& WorkDir,const FString& Text,FString& OutPath)
{
    IFileManager::Get().MakeDirectory(*WorkDir,true);
    OutPath=FPaths::ConvertRelativePathToFull(WorkDir/(TEXT("sftp-")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".txt")));
    return FFileHelper::SaveStringToFile(Text,*OutPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FTrainSftpProcessResult RunBatch(const FTrainSftpSettings& S,const FString& WorkDir,const FString& Text,const TFunction<bool()>& IsCancelled,bool bAcceptNew=false,const FString& KnownHosts=FString())
{
    FString BatchPath;FTrainSftpProcessResult R;
    if(!WriteBatch(WorkDir,Text,BatchPath)){R.Output=TEXT("cannot write sftp batch file");return R;}
    R=TrainBatchSftp::RunProcess(TrainBatchSftp::ClientExecutable(),TrainBatchSftp::CommandLine(S,BatchPath,bAcceptNew,KnownHosts),S,IsCancelled,S.ProcessTimeoutSec);
    IFileManager::Get().Delete(*BatchPath,false,true);
    return R;
}
}

FString TrainBatchSftp::ClientExecutable(const TCHAR* Tool)
{
#if PLATFORM_WINDOWS
    // Fixed inbox location only; PATH is never searched so a planted sftp.exe cannot run.
    const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot"));
    const FString Path=(Root.IsEmpty()?FString(TEXT("C:/Windows")):Root)/TEXT("System32/OpenSSH")/Tool;
    return IFileManager::Get().FileExists(*Path)?FPaths::ConvertRelativePathToFull(Path):FString();
#else
    return FString();
#endif
}

bool TrainBatchSftp::IsSafeRemoteName(const FString& Name)
{
    return !Name.IsEmpty()&&Name.Len()<=200&&!Name.StartsWith(TEXT("."))&&!Name.StartsWith(TEXT("-"))&&AllChars(Name,TEXT("._-"));
}

bool TrainBatchSftp::IsSafeRemoteDirectory(const FString& Directory)
{
    if(!Directory.StartsWith(TEXT("/"))||Directory.Len()>1024||Directory.Contains(TEXT("//"))||!AllChars(Directory,TEXT("._-/")))return false;
    TArray<FString> Parts;Directory.ParseIntoArray(Parts,TEXT("/"),true);
    for(const auto& P:Parts)if(P==TEXT(".")||P==TEXT("..")||P.StartsWith(TEXT("-")))return false;
    return true;
}

bool TrainBatchSftp::ValidateSettings(const FTrainSftpSettings& S,FString& Error,bool bRequireKnownHosts)
{
    if(ClientExecutable().IsEmpty()){Error=TEXT("Windows OpenSSH client not found (System32/OpenSSH/sftp.exe)");return false;}
    if(S.Host.IsEmpty()||S.Host.Len()>253||S.Host.StartsWith(TEXT("-"))||!AllChars(S.Host,TEXT(".-"))){Error=TEXT("invalid host");return false;}
    if(S.Port<1||S.Port>65535){Error=TEXT("invalid port");return false;}
    if(S.User.IsEmpty()||S.User.Len()>64||S.User.StartsWith(TEXT("-"))||!AllChars(S.User,TEXT("._-"))){Error=TEXT("invalid user");return false;}
    if(!IsSafeRemoteDirectory(S.RemoteDirectory)){Error=TEXT("remote directory must be absolute and use [A-Za-z0-9._-/]");return false;}
    if(S.PrivateKeyPath.IsEmpty()||!IFileManager::Get().FileExists(*S.PrivateKeyPath)){Error=TEXT("private key file not found");return false;}
    if(S.ConnectTimeoutSec<5||S.ConnectTimeoutSec>600||S.ProcessTimeoutSec<30){Error=TEXT("timeout out of range");return false;}
    if(bRequireKnownHosts&&(S.KnownHostsPath.IsEmpty()||!IFileManager::Get().FileExists(*S.KnownHostsPath))){Error=TEXT("server host key not registered yet");return false;}
    return true;
}

FString TrainBatchSftp::Quote(const FString& Value)
{
    FString V=Value.Replace(TEXT("\\"),TEXT("/"));
    return TEXT("\"")+V.Replace(TEXT("\""),TEXT("\\\""))+TEXT("\"");
}

FString TrainBatchSftp::CommandLine(const FTrainSftpSettings& S,const FString& BatchPath,bool bAcceptNewHostKey,const FString& KnownHostsOverride)
{
    const FString Known=KnownHostsOverride.IsEmpty()?S.KnownHostsPath:KnownHostsOverride;
    // ssh splits -o values on whitespace, so the path is quoted inside the option ("C:/Unreal Projects/...").
    TArray<FString> A={TEXT("-b"),Arg(BatchPath),TEXT("-P"),FString::FromInt(S.Port),TEXT("-i"),Arg(S.PrivateKeyPath),
        TEXT("-o"),TEXT("BatchMode=yes"),TEXT("-o"),TEXT("IdentitiesOnly=yes"),TEXT("-o"),TEXT("PasswordAuthentication=no"),
        TEXT("-o"),bAcceptNewHostKey?TEXT("StrictHostKeyChecking=accept-new"):TEXT("StrictHostKeyChecking=yes"),
        TEXT("-o"),Arg(TEXT("UserKnownHostsFile=\"")+Known.Replace(TEXT("\\"),TEXT("/"))+TEXT("\"")),TEXT("-o"),TEXT("GlobalKnownHostsFile=NUL"),
        TEXT("-o"),FString::Printf(TEXT("ConnectTimeout=%d"),S.ConnectTimeoutSec),TEXT("-o"),TEXT("ServerAliveInterval=15"),TEXT("-o"),TEXT("ServerAliveCountMax=4"),
        TEXT("-o"),TEXT("LogLevel=ERROR"),S.User+TEXT("@")+S.Host};
    return FString::Join(A,TEXT(" "));
}

FString TrainBatchSftp::ProbeBatch(const FTrainSftpSettings& S)
{
    // '-' lets an existing directory pass; cd then fails loudly if creation was not possible.
    return TEXT("-mkdir ")+Quote(S.RemoteDirectory)+TEXT("\ncd ")+Quote(S.RemoteDirectory)+TEXT("\nls -ln\n");
}

FString TrainBatchSftp::UploadBatch(const FTrainSftpSettings& S,const TArray<FTrainSftpFile>& Files,const TMap<FString,int64>& Remote)
{
    FString T=TEXT("cd ")+Quote(S.RemoteDirectory)+TEXT("\n");
    for(const auto& F:Files)
    {
        const FString Name=Partial(F.RemoteName);const int64* Have=Remote.Find(Name);
        if(Have&&*Have==F.Size)continue;
        if(Have&&*Have>0&&*Have<F.Size){T+=TEXT("reput ")+Quote(F.LocalPath)+TEXT(" ")+Quote(Name)+TEXT("\n");continue;}
        if(Have)T+=TEXT("rm ")+Quote(Name)+TEXT("\n");
        T+=TEXT("put ")+Quote(F.LocalPath)+TEXT(" ")+Quote(Name)+TEXT("\n");
    }
    return T;
}

FString TrainBatchSftp::FinalizeBatch(const FTrainSftpSettings& S,const TArray<FTrainSftpFile>& Files)
{
    FString T=TEXT("cd ")+Quote(S.RemoteDirectory)+TEXT("\n");
    for(int32 I=Files.Num()-1;I>=0;--I)T+=TEXT("rename ")+Quote(Partial(Files[I].RemoteName))+TEXT(" ")+Quote(Files[I].RemoteName)+TEXT("\n");
    return T;
}

TMap<FString,int64> TrainBatchSftp::ParseListing(const FString& Output)
{
    TMap<FString,int64> Out;TArray<FString> Lines;Output.ParseIntoArrayLines(Lines);
    for(const auto& Line:Lines)
    {
        if(Line.StartsWith(TEXT("sftp>"))||Line.IsEmpty()||Line[0]!=TEXT('-'))continue;
        // -rw-r--r--    1 1000     1000      12345 Nov  2 10:00 name
        TArray<FString> F;Line.ParseIntoArrayWS(F);
        if(F.Num()<9||!F[4].IsNumeric())continue;
        FString Name=F[8];for(int32 I=9;I<F.Num();++I)Name+=TEXT(" ")+F[I];
        if(IsSafeRemoteName(Name))Out.Add(Name,FCString::Atoi64(*F[4]));
    }
    return Out;
}

FString TrainBatchSftp::Mask(const FString& Text,const FTrainSftpSettings& S)
{
    FString T=Text;
    for(const FString& Secret:{S.User+TEXT("@")+S.Host,S.Host,S.User,S.PrivateKeyPath,S.PrivateKeyPath.Replace(TEXT("/"),TEXT("\\"))})
        if(Secret.Len()>=3)T.ReplaceInline(*Secret,TEXT("***"),ESearchCase::IgnoreCase);
    return T;
}

FTrainSftpProcessResult TrainBatchSftp::RunProcess(const FString& Executable,const FString& Arguments,const FTrainSftpSettings& S,const TFunction<bool()>& IsCancelled,int32 TimeoutSec)
{
    FTrainSftpProcessResult R;
    if(Executable.IsEmpty()){R.Output=TEXT("OpenSSH client not found");return R;}
    void* Read=nullptr;void* Write=nullptr;
    if(!FPlatformProcess::CreatePipe(Read,Write)){R.Output=TEXT("cannot create pipe");return R;}
    FProcHandle Proc=FPlatformProcess::CreateProc(*Executable,*Arguments,false,true,true,nullptr,0,nullptr,Write,nullptr);
    R.bLaunched=Proc.IsValid();
    FString Output;const double Deadline=FPlatformTime::Seconds()+TimeoutSec;
    const auto Drain=[&](){const FString Chunk=FPlatformProcess::ReadPipe(Read);if(Output.Len()<MaxCapturedOutput)Output+=Chunk.Left(MaxCapturedOutput-Output.Len());};
    while(R.bLaunched&&FPlatformProcess::IsProcRunning(Proc))
    {
        Drain();
        if(IsCancelled&&IsCancelled()){R.bCancelled=true;FPlatformProcess::TerminateProc(Proc,true);break;}
        if(FPlatformTime::Seconds()>Deadline){R.bTimedOut=true;FPlatformProcess::TerminateProc(Proc,true);break;}
        FPlatformProcess::Sleep(0.05f);
    }
    if(R.bLaunched)
    {
        FPlatformProcess::WaitForProc(Proc);Drain();
        int32 Code=-1;if(FPlatformProcess::GetProcReturnCode(Proc,&Code))R.ExitCode=Code;
        FPlatformProcess::CloseProc(Proc);
    }
    FPlatformProcess::ClosePipe(Read,Write);
    R.Output=Mask(R.bLaunched?Output:FString(TEXT("cannot start OpenSSH client")),S);
    return R;
}

FTrainSftpUploadResult TrainBatchSftp::Upload(const FTrainSftpSettings& S,const TArray<FTrainSftpFile>& Files,const FString& WorkDir,const TFunction<bool()>& IsCancelled,const TFunction<void(const FString&)>& OnPhase)
{
    FTrainSftpUploadResult R;
    const auto Phase=[&](const TCHAR* P){if(OnPhase)OnPhase(P);};
    const auto Done=[&](ETrainSftpOutcome O,const FString& Error,const FTrainSftpProcessResult* P=nullptr)
    {R.Outcome=(IsCancelled&&IsCancelled())?ETrainSftpOutcome::Cancelled:O;R.Error=Error;if(P)R.Detail=P->Output;return R;};
    if(!ValidateSettings(S,R.Error))return Done(ETrainSftpOutcome::Rejected,R.Error);
    if(Files.IsEmpty())return Done(ETrainSftpOutcome::Rejected,TEXT("nothing to upload"));
    TSet<FString> Names;
    for(const auto& F:Files)
    {
        bool bDuplicate=false;Names.Add(F.RemoteName,&bDuplicate);
        if(bDuplicate||!IsSafeRemoteName(F.RemoteName))return Done(ETrainSftpOutcome::Rejected,TEXT("unsafe or duplicate remote name: ")+F.RemoteName);
        // sftp put expands local globs; refuse them instead of escaping.
        if(F.LocalPath.Contains(TEXT("*"))||F.LocalPath.Contains(TEXT("?"))||F.LocalPath.Contains(TEXT("["))||F.LocalPath.Contains(TEXT("\n")))return Done(ETrainSftpOutcome::Rejected,TEXT("unsupported character in local path"));
        if(IFileManager::Get().FileSize(*F.LocalPath)!=F.Size)return Done(ETrainSftpOutcome::Rejected,TEXT("local file missing or changed: ")+F.RemoteName);
    }
    Phase(TEXT("probe"));
    FTrainSftpProcessResult P=RunBatch(S,WorkDir,ProbeBatch(S),IsCancelled);
    if(P.ExitCode!=0)return Done(ETrainSftpOutcome::Failed,TEXT("cannot open remote directory"),&P);
    TMap<FString,int64> Remote=ParseListing(P.Output);
    for(const auto& F:Files)if(Remote.Contains(F.RemoteName))return Done(ETrainSftpOutcome::Conflict,TEXT("already on server, not overwritten: ")+F.RemoteName,&P);
    Phase(TEXT("upload"));
    P=RunBatch(S,WorkDir,UploadBatch(S,Files,Remote),IsCancelled);
    if(P.ExitCode!=0)return Done(ETrainSftpOutcome::Failed,TEXT("upload interrupted; partial files kept for resume"),&P);
    Phase(TEXT("verify"));
    P=RunBatch(S,WorkDir,ProbeBatch(S),IsCancelled);
    if(P.ExitCode!=0)return Done(ETrainSftpOutcome::Failed,TEXT("cannot verify upload"),&P);
    Remote=ParseListing(P.Output);
    for(const auto& F:Files)
    {
        const int64* Size=Remote.Find(Partial(F.RemoteName));
        if(!Size||*Size!=F.Size)return Done(ETrainSftpOutcome::Failed,TEXT("remote size mismatch: ")+F.RemoteName,&P);
        if(Remote.Contains(F.RemoteName))return Done(ETrainSftpOutcome::Conflict,TEXT("appeared on server during upload: ")+F.RemoteName,&P);
    }
    Phase(TEXT("finalize"));
    P=RunBatch(S,WorkDir,FinalizeBatch(S,Files),IsCancelled);
    if(P.ExitCode!=0)return Done(ETrainSftpOutcome::Failed,TEXT("rename failed; files remain as .partial"),&P);
    for(const auto& F:Files)R.Uploaded.Add(F.RemoteName);
    return Done(ETrainSftpOutcome::Uploaded,FString(),&P);
}

bool TrainBatchSftp::FetchHostFingerprint(const FTrainSftpSettings& S,const FString& PendingKnownHosts,const FString& WorkDir,FString& OutFingerprint,FString& Error)
{
    if(!ValidateSettings(S,Error,false))return false;
    IFileManager::Get().Delete(*PendingKnownHosts,false,true);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(PendingKnownHosts),true);
    const FTrainSftpProcessResult P=RunBatch(S,WorkDir,TEXT("bye\n"),nullptr,true,PendingKnownHosts);
    if(P.ExitCode!=0||!IFileManager::Get().FileExists(*PendingKnownHosts)){Error=TEXT("connection or key authentication failed: ")+P.Output;return false;}
    const FTrainSftpProcessResult K=RunProcess(ClientExecutable(TEXT("ssh-keygen.exe")),TEXT("-l -f ")+Arg(PendingKnownHosts),S,nullptr,30);
    if(K.ExitCode!=0||K.Output.TrimStartAndEnd().IsEmpty()){Error=TEXT("cannot read host key fingerprint");return false;}
    OutFingerprint=K.Output.TrimStartAndEnd();return true;
}

bool TrainBatchSftp::ConfirmHostKey(const FTrainSftpSettings& S,const FString& PendingKnownHosts,FString& Error)
{
    if(S.KnownHostsPath.IsEmpty()||!IFileManager::Get().FileExists(*PendingKnownHosts)){Error=TEXT("no pending host key");return false;}
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(S.KnownHostsPath),true);
    if(!IFileManager::Get().Move(*S.KnownHostsPath,*PendingKnownHosts,true)){Error=TEXT("cannot store host key");return false;}
    return true;
}
