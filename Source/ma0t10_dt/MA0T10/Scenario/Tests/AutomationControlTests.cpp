#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationProtocol.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationLoopbackServer.h"
#include "Misc/Paths.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
using namespace MA0T10Automation;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationProtocolTest,"MA0T10.Automation.Protocol",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAutomationProtocolTest::RunTest(const FString&)
{
    FString Error;FJob Job;
    const FString Base=FPaths::ProjectSavedDir()/TEXT("Reports/AUTO-01");
    TestTrue(TEXT("minimal bounded job"),ParseJob(TEXT("{\"schema\":\"ma0t10.job.v1\",\"result_path\":\"result.json\",\"steps\":[{\"op\":\"wait\",\"seconds\":0.1}]}"),Base,Job,Error));
    TestEqual(TEXT("relative result resolved against job"),Job.ResultPath,FPaths::ConvertRelativePathToFull(Base,TEXT("result.json")));
    const TArray<FString> Invalid={TEXT("{}"),TEXT("[]"),TEXT("{\"op\":\"wait\",\"seconds\":-1}"),TEXT("{\"op\":\"wait\",\"seconds\":\"10\"}"),TEXT("{\"op\":\"pcd.run\",\"sensor_id\":\"x\",\"seconds\":0}"),TEXT("{\"op\":\"sensor.set\",\"sensor_id\":\"x\",\"interval_sec\":0}"),TEXT("{\"op\":\"sensor.set\",\"sensor_id\":\"x\",\"interval_sec\":0.1,\"quality\":\"FullSpec\"}"),TEXT("{\"op\":\"scenario.start\",\"uuid\":\"x\",\"json_path\":\"x\"}"),TEXT("{\"op\":\"scenario.start\",\"uuid\":\"x\",\"send_pcd\":true}"),TEXT("{\"op\":\"wait\",\"seconds\":1,\"bind\":\"0.0.0.0\"}"),TEXT("{\"op\":\"dataset.start\",\"name\":\"../escape\",\"scenes\":20,\"slab\":\"a\",\"lidar_id\":\"b\",\"camera_id\":\"c\"}")};
    for(const auto& Text:Invalid){TSharedPtr<FJsonObject> O;const bool Valid=ParseObject(Text,O)&&ValidateCommand(*O,Error);TestFalse(TEXT("invalid input rejected"),Valid);}
    TestFalse(TEXT("oversize rejected before parsing"),ParseJob(FString::ChrN(MaxDocumentBytes+1,TEXT(' ')),Base,Job,Error));
    TestFalse(TEXT("job must be bounded"),ParseJob(TEXT("{\"schema\":\"ma0t10.job.v1\",\"result_path\":\"r.json\",\"steps\":[{\"op\":\"pcd.start\",\"sensor_id\":\"x\"}]}"),Base,Job,Error));
    TSharedPtr<FJsonObject> Compound;
    ParseObject(TEXT("{\"op\":\"wait\",\"seconds\":1,\"op|timeout_sec\":2}"),Compound);
    TestFalse(TEXT("compound names cannot bypass exact field admission"),ValidateCommand(*Compound,Error));
    ParseObject(TEXT("{\"op\":\"dataset.start\",\"name\":\"x\",\"scenes\":1,\"slab\":\"s\",\"lidar_id\":\"l\",\"camera_id\":\"c\",\"ranges\":{\"yaw|pitch\":[-1,1]}}"),Compound);
    TestFalse(TEXT("compound range names are rejected"),ValidateCommand(*Compound,Error));
    IFileManager::Get().MakeDirectory(*Base,true);const FString Temp=FPaths::CreateTempFilename(*Base,TEXT("protocol-limit-"),TEXT(".txt"));
    TestTrue(TEXT("bounded reader fixture saved"),FFileHelper::SaveStringToFile(TEXT("abcdefghijklmnop"),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));FString Read;
    TestTrue(TEXT("exact byte admission reads full document"),ReadDocument(Temp,Read,16));TestEqual(TEXT("bounded content retained"),Read.Len(),16);
    TestFalse(TEXT("over-limit document is not read"),ReadDocument(Temp,Read,15));IFileManager::Get().Delete(*Temp);
    return true;
}

// Real loopback socket framing/auth/close tests, independent of any Broker or render fixture.
class FAutomationSocketCommand final:public IAutomationLatentCommand
{
public:
    explicit FAutomationSocketCommand(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(!Started)
        {
            Started=true;
            auto* S=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);auto Addr=S->CreateInternetAddr();bool Valid;Addr->SetIp(TEXT("127.0.0.1"),Valid);Addr->SetPort(0);
            auto* Probe=S->CreateSocket(NAME_Stream,TEXT("port-probe"),false);Probe->Bind(*Addr);Probe->GetAddress(*Addr);Port=Addr->GetPort();S->DestroySocket(Probe);
            Server=MakeUnique<FAutomationLoopbackServer>();if(!Server->Start(Port,TEXT("test-only-token"))){Test->AddError(TEXT("loopback bind failed"));return true;}
            Future=Async(EAsyncExecution::ThreadPool,[Port=Port](){return Exchange(Port);});Start=FPlatformTime::Seconds();return false;
        }
        TSharedPtr<FAutomationLoopbackServer::FRequest,ESPMode::ThreadSafe> R;
        if(Server->Dequeue(R)){++Accepted;R->Code=200;R->Response=TEXT("{\"ok\":true}");R->Complete.store(true,std::memory_order_release);}
        if(Future.IsReady())
        {const auto Responses=Future.Get();Test->TestEqual(TEXT("three responses"),Responses.Num(),3);if(Responses.Num()==3){Test->TestTrue(TEXT("auth rejected"),Responses[0].StartsWith(TEXT("HTTP/1.1 401")));Test->TestTrue(TEXT("origin rejected"),Responses[1].StartsWith(TEXT("HTTP/1.1 400")));Test->TestTrue(TEXT("authorized accepted"),Responses[2].StartsWith(TEXT("HTTP/1.1 200")));}Test->TestEqual(TEXT("only valid request reaches game thread"),Accepted,1);Server.Reset();FAutomationLoopbackServer Reopened;Test->TestTrue(TEXT("listener can be enabled again immediately"),Reopened.Start(Port,FString()));Reopened.Shutdown();return true;}
        if(FPlatformTime::Seconds()-Start>15){Test->AddError(TEXT("socket test timed out"));Server.Reset();return true;}return false;
    }
private:
    static TArray<FString> Exchange(int32 Port)
    {
        TArray<FString> Out;auto* S=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        for(int32 Case=0;Case<3;++Case)
        {
            auto Addr=S->CreateInternetAddr();bool Valid;Addr->SetIp(TEXT("127.0.0.1"),Valid);Addr->SetPort(Port);auto* C=S->CreateSocket(NAME_Stream,TEXT("automation-test"),false);
            if(!C->Connect(*Addr)){S->DestroySocket(C);continue;}C->SetNonBlocking(true);
            FString Q=FString::Printf(TEXT("GET /v1/status HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n"),Port);
            if(Case>0)Q+=TEXT("Authorization: Bearer test-only-token\r\n");if(Case==1)Q+=TEXT("Origin: http://example.invalid\r\n");Q+=TEXT("\r\n");
            FTCHARToUTF8 U(*Q);int32 Sent=0;C->Send(reinterpret_cast<const uint8*>(U.Get()),U.Length(),Sent);TArray<uint8> B;const double End=FPlatformTime::Seconds()+5;
            while(FPlatformTime::Seconds()<End){uint8 Buffer[2048];int32 N=0;if(C->Recv(Buffer,sizeof(Buffer),N)&&N>0){B.Append(Buffer,N);if(B.Num()>0&&B.Last()==TEXT('}'))break;}else FPlatformProcess::SleepNoStats(.005f);}
            FUTF8ToTCHAR T(reinterpret_cast<const ANSICHAR*>(B.GetData()),B.Num());Out.Add(FString(T.Length(),T.Get()));C->Close();S->DestroySocket(C);
        }
        return Out;
    }
    FAutomationTestBase* Test;bool Started=false;int32 Port=0,Accepted=0;double Start=0;TUniquePtr<FAutomationLoopbackServer> Server;TFuture<TArray<FString>> Future;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationSocketTest,"MA0T10.Automation.LoopbackTransport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAutomationSocketTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FAutomationSocketCommand(this));return true;}
#endif
