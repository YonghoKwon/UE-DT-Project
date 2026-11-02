#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchArchive.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchSftp.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchDelivery.h"
#include "ma0t10_dt/MA0T10/Scenario/Training/TrainBatchCodec.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
THIRD_PARTY_INCLUDES_START
#include "zlib.h"
#include "libzip/zip.h"
THIRD_PARTY_INCLUDES_END
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
/** Fixture kept under Saved/Automation/TrainBatchIo so Tools/TrainBatchTools can re-verify it independently. */
FString FixtureRoot(){return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/TrainBatchIo"));}

TMap<FString,TArray<uint8>> MakeFixture(const FString& Source)
{
    TMap<FString,TArray<uint8>> Files;
    TArray<uint8> Big;Big.SetNumUninitialized(3*1024*1024+17);for(int32 I=0;I<Big.Num();++I)Big[I]=uint8((I*131)^(I>>7));
    Files.Add(TEXT("job.json"),TArray<uint8>((const uint8*)"{\"schema\":\"ma0t10.train.result.v1\"}",36));
    Files.Add(TEXT("scenarios/case-001/scene_00000/lidar.pcd"),Big);
    Files.Add(TEXT("scenarios/case-001/empty.bin"),TArray<uint8>());
    // >100 UTF-8 bytes and non-ASCII: needs a PAX path record in tar.
    Files.Add(TEXT("scenarios/긴_이름_")+FString::ChrN(60,TEXT('a'))+TEXT("/label.json"),TArray<uint8>((const uint8*)"{}",2));
    for(const auto& P:Files)FFileHelper::SaveArrayToFile(P.Value,*(Source/P.Key));
    return Files;
}

/** Minimal independent tar reader (ustar + PAX path) over a fully inflated gzip stream. */
bool ReadTarGz(const FString& Path,TMap<FString,TArray<uint8>>& Out,FString& Error)
{
    TArray<uint8> Packed;if(!FFileHelper::LoadFileToArray(Packed,*Path)){Error=TEXT("read");return false;}
    z_stream Z={};if(inflateInit2(&Z,15+16)!=Z_OK){Error=TEXT("inflateInit");return false;}
    TArray<uint8> Tar;uint8 Buffer[64*1024];Z.next_in=Packed.GetData();Z.avail_in=Packed.Num();int Result=Z_OK;
    while(Result!=Z_STREAM_END){Z.next_out=Buffer;Z.avail_out=sizeof(Buffer);Result=inflate(&Z,Z_NO_FLUSH);if(Result!=Z_OK&&Result!=Z_STREAM_END){inflateEnd(&Z);Error=TEXT("inflate");return false;}Tar.Append(Buffer,sizeof(Buffer)-Z.avail_out);}
    inflateEnd(&Z);
    FString PaxPath;
    for(int64 At=0;At+512<=Tar.Num();)
    {
        const char* H=(const char*)Tar.GetData()+At;bool bZero=true;for(int32 I=0;I<512;++I)bZero&=H[I]==0;
        if(bZero)return true;
        uint32 Sum=0;for(int32 I=0;I<512;++I)Sum+=(I>=148&&I<156)?uint32(' '):uint8(H[I]);
        if(Sum!=uint32(FCStringAnsi::Strtoi(H+148,nullptr,8))){Error=TEXT("checksum");return false;}
        const int64 Size=FCStringAnsi::Strtoi64(H+124,nullptr,8);const char Type=H[156];At+=512;
        if(Type=='x')
        {
            const FUTF8ToTCHAR Conv((const ANSICHAR*)Tar.GetData()+At,int32(Size));const FString Records(Conv.Length(),Conv.Get());
            int32 Index=Records.Find(TEXT(" path="));if(Index!=INDEX_NONE){PaxPath=Records.Mid(Index+6);PaxPath.LeftInline(PaxPath.Find(TEXT("\n")));}
        }
        else if(Type=='0')
        {
            FString Name=PaxPath.IsEmpty()?FString(UTF8_TO_TCHAR(H)):PaxPath;PaxPath.Reset();
            Out.Add(Name,TArray<uint8>(Tar.GetData()+At,int32(Size)));
        }
        At+=(Size+511)/512*512;
    }
    Error=TEXT("missing end blocks");return false;
}

bool ReadZip(const FString& Path,TMap<FString,TArray<uint8>>& Out,FString& Error)
{
    int Code=0;zip_t* Zip=zip_open(FTCHARToUTF8(*Path).Get(),ZIP_RDONLY|ZIP_CHECKCONS,&Code);if(!Zip){Error=TEXT("zip_open");return false;}
    for(zip_int64_t I=0;I<zip_get_num_entries(Zip,0);++I)
    {
        zip_stat_t St;zip_stat_index(Zip,I,0,&St);zip_file_t* F=zip_fopen_index(Zip,I,0);
        TArray<uint8> Data;Data.SetNumUninitialized(int32(St.size));
        if(!F||zip_fread(F,Data.GetData(),St.size)!=zip_int64_t(St.size)){if(F)zip_fclose(F);zip_discard(Zip);Error=TEXT("zip_fread");return false;}
        zip_fclose(F);Out.Add(UTF8_TO_TCHAR(St.name),MoveTemp(Data));
    }
    zip_discard(Zip);return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchArchiveTest,"MA0T10.TrainBatch.Archive",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchArchiveTest::RunTest(const FString&)
{
    using namespace TrainBatchArchive;
    TestEqual(TEXT("SHA-256 known vector"),Sha256Bytes("abc",3),FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    for(const TCHAR* Bad:{TEXT(""),TEXT("/abs"),TEXT("a/../b"),TEXT("./a"),TEXT("C:/x"),TEXT("a\\b"),TEXT("a//b"),TEXT("dir/"),TEXT("a\nb"),TEXT("trail."),TEXT("x/y*z")})
        TestFalse(*FString::Printf(TEXT("unsafe entry rejected: %s"),Bad),IsSafeEntryName(Bad));
    TestTrue(TEXT("nested relative entry"),IsSafeEntryName(TEXT("scenarios/case-001/scene_00000/lidar.pcd")));

    const FString Root=FixtureRoot(),Source=Root/TEXT("source"),Output=Root/TEXT("output");
    IFileManager::Get().DeleteDirectory(*Root,false,true);
    const TMap<FString,TArray<uint8>> Expected=MakeFixture(Source);
    TArray<FTrainArchiveEntry> Entries;FString Error;
    TestTrue(TEXT("collect entries"),CollectEntries(Source,TEXT("job-1/"),Entries,Error));
    TestEqual(TEXT("all files listed"),Entries.Num(),Expected.Num());
    for(int32 I=1;I<Entries.Num();++I)TestTrue(TEXT("entries sorted"),Entries[I-1].EntryName.Compare(Entries[I].EntryName,ESearchCase::CaseSensitive)<0);
    TestTrue(TEXT("checksum manifest"),WriteChecksums(Entries,Output/TEXT("checksums.sha256"),Error));

    for(const ETrainArchiveFormat Format:{ETrainArchiveFormat::Zip,ETrainArchiveFormat::TarGz})
    {
        FTrainArchiveOptions O;O.Format=Format;int64 LastProgress=-1;O.OnProgress=[&](int64 Done,int64){LastProgress=Done;};
        const FString Path=Output/(TEXT("job-1.")+FString(Extension(Format)));
        const FTrainArchiveResult R=Write(Entries,Path,O);
        TestTrue(*FString::Printf(TEXT("%s written: %s"),Extension(Format),*R.Error),R.bOk);
        TestFalse(TEXT("no partial left"),IFileManager::Get().FileExists(*(Path+TEXT(".partial"))));
        FString Hash;Sha256File(Path,Hash,Error);TestEqual(TEXT("reported hash matches file"),R.Sha256,Hash);
        TestEqual(TEXT("reported size matches file"),R.Bytes,IFileManager::Get().FileSize(*Path));
        TestTrue(TEXT("progress reported"),LastProgress>0);
        TMap<FString,TArray<uint8>> Read;
        TestTrue(*FString::Printf(TEXT("%s reads back: %s"),Extension(Format),*Error),Format==ETrainArchiveFormat::Zip?ReadZip(Path,Read,Error):ReadTarGz(Path,Read,Error));
        TestEqual(TEXT("entry count"),Read.Num(),Expected.Num());
        for(const auto& P:Expected){const auto* Got=Read.Find(TEXT("job-1/")+P.Key);TestTrue(*(TEXT("entry bytes equal: ")+P.Key),Got&&*Got==P.Value);}
        TestFalse(TEXT("existing archive never overwritten"),Write(Entries,Path,O).bOk);
        FTrainArchiveOptions C=O;C.IsCancelled=[](){return true;};
        const FString CancelPath=Output/(TEXT("cancel.")+FString(Extension(Format)));
        const FTrainArchiveResult Cancelled=Write(Entries,CancelPath,C);
        TestTrue(TEXT("cancel reported"),!Cancelled.bOk&&Cancelled.bCancelled);
        TestFalse(TEXT("cancel leaves no archive"),IFileManager::Get().FileExists(*CancelPath)||IFileManager::Get().FileExists(*(CancelPath+TEXT(".partial"))));
    }
    TArray<FTrainArchiveEntry> Dup=Entries;Dup.Add(Entries[0]);Dup.Last().EntryName=Dup.Last().EntryName.ToUpper();
    TestFalse(TEXT("case-insensitive duplicate rejected"),Write(Dup,Output/TEXT("dup.zip"),FTrainArchiveOptions()).bOk);
    FTrainArchiveOptions StoreGz;StoreGz.Format=ETrainArchiveFormat::TarGz;StoreGz.Level=0;
    TestFalse(TEXT("tar.gz has no store level"),Write(Entries,Output/TEXT("store.tar.gz"),StoreGz).bOk);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchDeliveryTest,"MA0T10.TrainBatch.Delivery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchDeliveryTest::RunTest(const FString&)
{
    const FString Job=TEXT("8cdf7e10-7972-4493-b5d2-a7d36c1c04dd");
    TestTrue(TEXT("uuid job id accepted"),TrainBatchDelivery::IsSafeJobId(Job));
    TestFalse(TEXT("dotted job id rejected"),TrainBatchDelivery::IsSafeJobId(TEXT("a.b")));
    TestEqual(TEXT("hash-bound remote name"),TrainBatchDelivery::RemoteArchiveName(Job,TEXT("0123456789abcdef"),ETrainArchiveFormat::TarGz),Job+TEXT("-0123456789ab.tar.gz"));

    // Settings round trip without disturbing a real user profile.
    const FString SettingsFile=TrainBatchDelivery::SettingsPath();FString Backup;const bool bHadSettings=FFileHelper::LoadFileToString(Backup,*SettingsFile);
    FTrainDeliverySettings S;S.Format=ETrainArchiveFormat::TarGz;S.Level=6;S.Sftp.Host=TEXT("train.example");S.Sftp.Port=2222;S.Sftp.User=TEXT("trainer");S.Sftp.RemoteDirectory=TEXT("/data/inbox");S.Sftp.PrivateKeyPath=TEXT("C:/Keys/id_ed25519");
    FString Error;TestTrue(TEXT("settings saved"),TrainBatchDelivery::SaveSettings(S,Error));
    FString Saved;FFileHelper::LoadFileToString(Saved,*SettingsFile);
    TestFalse(TEXT("no secret fields persisted"),Saved.Contains(TEXT("password"))||Saved.Contains(TEXT("passphrase"))||Saved.Contains(TEXT("BEGIN OPENSSH")));
    FTrainDeliverySettings L;TestTrue(TEXT("settings loaded"),TrainBatchDelivery::LoadSettings(L,Error));
    TestTrue(TEXT("settings round trip"),L.Format==S.Format&&L.Level==6&&L.Sftp.Host==S.Sftp.Host&&L.Sftp.Port==2222&&L.Sftp.User==S.Sftp.User&&L.Sftp.RemoteDirectory==S.Sftp.RemoteDirectory&&L.Sftp.PrivateKeyPath==S.Sftp.PrivateKeyPath&&L.Sftp.KnownHostsPath==TrainBatchDelivery::KnownHostsPath());
    if(bHadSettings)FFileHelper::SaveStringToFile(Backup,*SettingsFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);else IFileManager::Get().Delete(*SettingsFile,false,true);

    const FString Root=FixtureRoot()/TEXT("delivery"),Staging=Root/TEXT("staging"),Out=Root/TEXT("out");
    IFileManager::Get().DeleteDirectory(*Root,false,true);MakeFixture(Staging);
    FTrainDeliveryRequest R;R.JobId=Job;R.StagingRoot=Staging;R.OutputDir=Out;R.Settings.Format=ETrainArchiveFormat::Zip;
    auto D=MakeShared<FTrainBatchDelivery,ESPMode::ThreadSafe>();
    TestTrue(TEXT("package-only delivery starts"),D->Start(R,Error));D->Wait();
    const FTrainDeliveryStatus First=D->Status();
    TestTrue(*(TEXT("package-only delivery done: ")+First.Error),First.Phase==ETrainDeliveryPhase::Done&&!First.bReusedArchive&&First.ArchiveBytes>0);
    TestEqual(TEXT("status remote name bound to hash"),First.RemoteName,TrainBatchDelivery::RemoteArchiveName(Job,First.ArchiveSha256,ETrainArchiveFormat::Zip));
    FString Side;FFileHelper::LoadFileToString(Side,*(First.ArchivePath+TEXT(".sha256")));
    TestEqual(TEXT("sha256sum-compatible sidecar"),Side,First.ArchiveSha256+TEXT("  ")+First.RemoteName+TEXT("\n"));
    TMap<FString,TArray<uint8>> Read;TestTrue(TEXT("archive readable"),ReadZip(First.ArchivePath,Read,Error));
    TestTrue(TEXT("in-archive checksum manifest"),Read.Contains(Job+TEXT("/checksums.sha256")));
    TestTrue(TEXT("second start reuses matching archive"),D->Start(R,Error));D->Wait();
    TestTrue(TEXT("archive reused, not rebuilt"),D->Status().Phase==ETrainDeliveryPhase::Done&&D->Status().bReusedArchive&&D->Status().ArchiveSha256==First.ArchiveSha256);
    // Tamper: the recorded hash no longer matches, so it must refuse rather than overwrite.
    {TUniquePtr<FArchive> A(IFileManager::Get().CreateFileWriter(*First.ArchivePath,FILEWRITE_Append));uint8 B=0;A->Serialize(&B,1);}
    TestTrue(TEXT("tampered start"),D->Start(R,Error));D->Wait();
    TestEqual(TEXT("mismatched archive refused"),D->Status().Phase,ETrainDeliveryPhase::Failed);
    FTrainDeliveryRequest Bad=R;Bad.bUpload=true;Bad.Settings.Sftp.Host=TEXT("-x");
    TestFalse(TEXT("upload with invalid settings refused before work starts"),D->Start(Bad,Error));

    // View packaging: only confirmed runs, and partial jobs only when explicitly chosen.
    FTrainBatchJobView View;View.Spec.Id=TEXT("io-test-")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12).ToLower();
    const FString JobDir=TrainBatchCodec::JobDirectory(View.Spec.Id);
    FFileHelper::SaveStringToFile(TEXT("{}"),*(JobDir/TEXT("input/job.json")));
    FTrainBatchItemView Done;Done.Spec.Id=TEXT("case-001");Done.State=ETrainBatchItemState::Completed;Done.RunDirectory=JobDir/TEXT("scenarios/case-001/run_a");
    FFileHelper::SaveStringToFile(TEXT("{}"),*(Done.RunDirectory/TEXT("complete.json")));FFileHelper::SaveStringToFile(TEXT("pcd"),*(Done.RunDirectory/TEXT("scene_00000/lidar.pcd")));
    FFileHelper::SaveStringToFile(TEXT("stale"),*(JobDir/TEXT("scenarios/case-001/run_old/scene_00000/lidar.pcd")));
    FTrainBatchItemView Lost;Lost.Spec.Id=TEXT("case-002");Lost.State=ETrainBatchItemState::Failed;
    View.Items={Done,Lost};
    TArray<FTrainArchiveEntry> JobEntries;TArray<FString> Excluded;
    TestFalse(TEXT("failed scenario blocks default send"),TrainBatchDelivery::BuildJobEntries(View,false,JobEntries,Excluded,Error));
    TestTrue(TEXT("success-only package when chosen"),TrainBatchDelivery::BuildJobEntries(View,true,JobEntries,Excluded,Error));
    TestEqual(TEXT("excluded ids reported"),Excluded,TArray<FString>{TEXT("case-002")});
    TArray<FString> Names;for(const auto& E:JobEntries)Names.Add(E.EntryName);
    TestEqual(TEXT("input + confirmed run only"),Names,TArray<FString>{View.Spec.Id+TEXT("/input/job.json"),View.Spec.Id+TEXT("/scenarios/case-001/complete.json"),View.Spec.Id+TEXT("/scenarios/case-001/scene_00000/lidar.pcd")});
    View.Items[0].RunDirectory=JobDir/TEXT("scenarios/case-001/run_old");
    TestFalse(TEXT("run without complete.json refused"),TrainBatchDelivery::BuildJobEntries(View,true,JobEntries,Excluded,Error));
    IFileManager::Get().DeleteDirectory(*JobDir,false,true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchSftpTest,"MA0T10.TrainBatch.Sftp",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainBatchSftpTest::RunTest(const FString&)
{
    using namespace TrainBatchSftp;
    for(const TCHAR* Bad:{TEXT(""),TEXT(".hidden"),TEXT("-o"),TEXT("a b"),TEXT("a/b"),TEXT("a*b"),TEXT("이름")})TestFalse(*FString::Printf(TEXT("remote name rejected: %s"),Bad),IsSafeRemoteName(Bad));
    TestTrue(TEXT("hash-suffixed artifact name"),IsSafeRemoteName(TEXT("8cdf7e10-ab12cd34ef56.zip")));
    for(const TCHAR* Bad:{TEXT("relative"),TEXT("/a/../b"),TEXT("/a//b"),TEXT("/a b"),TEXT("/a/-x"),TEXT("/a;rm")})TestFalse(*FString::Printf(TEXT("remote dir rejected: %s"),Bad),IsSafeRemoteDirectory(Bad));
    TestTrue(TEXT("absolute dir"),IsSafeRemoteDirectory(TEXT("/data/train/inbox")));
    TestEqual(TEXT("quote converts and escapes"),Quote(TEXT("C:\\Unreal Projects\\a\"b.zip")),FString(TEXT("\"C:/Unreal Projects/a\\\"b.zip\"")));

    FTrainSftpSettings S;S.Host=TEXT("train.example");S.User=TEXT("trainer");S.RemoteDirectory=TEXT("/data/inbox");S.PrivateKeyPath=TEXT("C:/Keys/id_ed25519");S.KnownHostsPath=TEXT("C:/Saved/known_hosts");
    const FString Cmd=CommandLine(S,TEXT("C:/Work Dir/b.txt"));
    for(const TCHAR* Need:{TEXT("BatchMode=yes"),TEXT("StrictHostKeyChecking=yes"),TEXT("PasswordAuthentication=no"),TEXT("IdentitiesOnly=yes"),TEXT("\"C:/Work Dir/b.txt\""),TEXT("trainer@train.example")})
        TestTrue(*FString::Printf(TEXT("command line has %s"),Need),Cmd.Contains(Need));
    TestTrue(TEXT("first contact may accept a new key only into a pending file"),CommandLine(S,TEXT("b"),true,TEXT("C:/p/pending")).Contains(TEXT("StrictHostKeyChecking=accept-new"))&&CommandLine(S,TEXT("b"),true,TEXT("C:/p/pending")).Contains(TEXT("UserKnownHostsFile=C:/p/pending")));

    const TArray<FTrainSftpFile> Files={{TEXT("C:/out/a.zip"),TEXT("a.zip"),100},{TEXT("C:/out/a.zip.sha256"),TEXT("a.zip.sha256"),80},{TEXT("C:/out/artifact.json"),TEXT("a.artifact.json"),50}};
    const FString Fresh=UploadBatch(S,Files,{});
    TestTrue(TEXT("fresh upload uses put to .partial"),Fresh.Contains(TEXT("put \"C:/out/a.zip\" \"a.zip.partial\""))&&!Fresh.Contains(TEXT("reput")));
    const FString Resume=UploadBatch(S,Files,{{TEXT("a.zip.partial"),40},{TEXT("a.zip.sha256.partial"),80},{TEXT("a.artifact.json.partial"),70}});
    TestTrue(TEXT("shorter partial resumes"),Resume.Contains(TEXT("reput \"C:/out/a.zip\" \"a.zip.partial\"")));
    TestFalse(TEXT("complete partial skipped"),Resume.Contains(TEXT("\"a.zip.sha256.partial\"")));
    TestTrue(TEXT("oversized partial replaced"),Resume.Contains(TEXT("rm \"a.artifact.json.partial\""))&&Resume.Contains(TEXT("put \"C:/out/artifact.json\" \"a.artifact.json.partial\"")));
    const FString Final=FinalizeBatch(S,Files);
    TestTrue(TEXT("artifact renamed last"),Final.Find(TEXT("rename \"a.zip.partial\" \"a.zip\""))>Final.Find(TEXT("rename \"a.zip.sha256.partial\"")));

    const TMap<FString,int64> Listing=ParseListing(TEXT("sftp> cd \"/data/inbox\"\nsftp> ls -ln\n-rw-r--r--    1 1000     1000     12345678901 Nov  2 10:00 a.zip.partial\ndrwxr-xr-x    2 1000     1000         4096 Nov  2 10:00 sub\n-rw-r--r--    1 1000     1000           80 Nov  2 10:00 a.zip.sha256\n"));
    TestEqual(TEXT("listing parses 64-bit sizes and skips dirs"),Listing.Num(),2);
    TestEqual(TEXT("64-bit size"),Listing.FindRef(TEXT("a.zip.partial")),int64(12345678901ll));
    const FString Masked=Mask(TEXT("Permission denied for trainer@train.example using C:/Keys/id_ed25519"),S);
    TestFalse(TEXT("user/host/key path masked"),Masked.Contains(TEXT("train.example"))||Masked.Contains(TEXT("trainer"))||Masked.Contains(TEXT("id_ed25519")));
    FString Error;FTrainSftpSettings Bad=S;Bad.Host=TEXT("-oProxyCommand=x");TestFalse(TEXT("option-looking host rejected"),ValidateSettings(Bad,Error,false));
    Bad=S;Bad.User=TEXT("a b");TestFalse(TEXT("user with space rejected"),ValidateSettings(Bad,Error,false));
    TestTrue(TEXT("inbox OpenSSH client present on this Windows host"),!ClientExecutable().IsEmpty());
    return true;
}
#endif
