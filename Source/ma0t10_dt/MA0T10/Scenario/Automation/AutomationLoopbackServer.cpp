#include "AutomationLoopbackServer.h"
#include "AutomationProtocol.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"

FAutomationLoopbackServer::~FAutomationLoopbackServer(){Shutdown();}
bool FAutomationLoopbackServer::Start(uint16 Port,FString Token)
{
    if(Thread||Listener)return false;
    auto* S=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    Listener=S->CreateSocket(NAME_Stream,TEXT("MA0T10AutomationLoopback"),false);
    if(!Listener)return false;
    auto Address=S->CreateInternetAddr();bool Valid=false;Address->SetIp(TEXT("127.0.0.1"),Valid);Address->SetPort(Port);
    // No reuse: another instance/PIE must fail closed, never share a control port.
    if(!Valid||!Listener->SetNonBlocking(true)||!Listener->Bind(*Address)||!Listener->Listen(16)){Shutdown();return false;}
    ListenPort=Port;Secret=MoveTemp(Token);Stopping=false;
    Thread=FRunnableThread::Create(this,TEXT("MA0T10Automation"));
    if(!Thread){Shutdown();return false;}return true;
}
void FAutomationLoopbackServer::Shutdown()
{
    Stopping=true;
    if(Thread){Thread->WaitForCompletion();delete Thread;Thread=nullptr;}
    if(Listener){Listener->Close();ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Listener);Listener=nullptr;}
    Secret.Reset();TSharedPtr<FRequest,ESPMode::ThreadSafe> R;while(Requests.Dequeue(R)){}
}
uint32 FAutomationLoopbackServer::Run()
{
    auto* S=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    while(!Stopping)
    {
        bool Pending=false;
        if(Listener->HasPendingConnection(Pending)&&Pending)
        {if(auto* Client=Listener->Accept(TEXT("MA0T10Control"))){Client->SetNonBlocking(true);Serve(Client);Client->Close();S->DestroySocket(Client);}}
        else FPlatformProcess::SleepNoStats(.005f);
    }
    return 0;
}
void FAutomationLoopbackServer::Serve(FSocket* Client)
{
    TArray<uint8> Bytes;FString Header;int32 HeaderBytes=-1,Length=0,Code=400;bool Valid=false;
    FString Method,Path,Body;TMap<FString,FString> Headers;
    const double Deadline=FPlatformTime::Seconds()+2;
    while(!Stopping&&FPlatformTime::Seconds()<Deadline)
    {
        uint8 Buffer[8192];int32 Count=0;
        if(Client->Recv(Buffer,sizeof(Buffer),Count)&&Count>0)Bytes.Append(Buffer,Count);
        else {FPlatformProcess::SleepNoStats(.002f);continue;}
        if(Bytes.Num()>MA0T10Automation::MaxDocumentBytes+8192){Code=413;break;}
        if(HeaderBytes<0)
        {
            for(int32 I=3;I<Bytes.Num();++I)if(Bytes[I-3]==13&&Bytes[I-2]==10&&Bytes[I-1]==13&&Bytes[I]==10){HeaderBytes=I+1;break;}
            if(HeaderBytes<0){if(Bytes.Num()>8192){Code=431;break;}continue;}
            if(HeaderBytes>8192)break;
            Header=FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),HeaderBytes).Length(),FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),HeaderBytes).Get());
            TArray<FString> Lines;Header.ParseIntoArray(Lines,TEXT("\r\n"));TArray<FString> Parts;
            if(Lines.IsEmpty())break;Lines[0].ParseIntoArray(Parts,TEXT(" "));
            if(Parts.Num()!=3||Parts[2]!=TEXT("HTTP/1.1"))break;Method=Parts[0];Path=Parts[1];bool Bad=false;
            for(int32 I=1;I<Lines.Num();++I)
            {
                FString K,V;if(!Lines[I].Split(TEXT(":"),&K,&V)){Bad=true;break;}K=K.ToLower();V.TrimStartAndEndInline();
                if(Headers.Contains(K)){Bad=true;break;}Headers.Add(K,V);
            }
            if(Bad||Headers.Contains(TEXT("transfer-encoding"))||Headers.Contains(TEXT("expect"))||Headers.Contains(TEXT("origin")))break;
            if(Headers.FindRef(TEXT("host"))!=FString::Printf(TEXT("127.0.0.1:%u"),ListenPort))break;
            if(!Secret.IsEmpty())
            {
                const FString Expected=TEXT("Bearer ")+Secret;const FString Supplied=Headers.FindRef(TEXT("authorization"));
                uint32 Different=Expected.Len()^Supplied.Len();for(int32 I=0;I<Expected.Len();++I)Different|=uint32(Expected[I]^(I<Supplied.Len()?Supplied[I]:0));
                if(Different){Code=401;break;}
            }
            const FString L=Headers.FindRef(TEXT("content-length"));int64 Wide=0;
            if(!L.IsEmpty()){bool Digits=L.Len()<=8;for(TCHAR C:L)Digits&=C>=TEXT('0')&&C<=TEXT('9');if(!Digits)break;Wide=FCString::Atoi64(*L);}
            if(Wide>MA0T10Automation::MaxDocumentBytes){Code=413;break;}Length=int32(Wide);
            if(Method!=TEXT("GET")&&Method!=TEXT("POST")){Code=405;break;}
            if((Method==TEXT("GET")&&Length)||(Method==TEXT("POST")&&(Length==0||Headers.FindRef(TEXT("content-type"))!=TEXT("application/json"))))break;
        }
        if(HeaderBytes>=0&&Bytes.Num()>=HeaderBytes+Length)
        {
            if(Bytes.Num()!=HeaderBytes+Length)break; // no pipelining/ambiguous trailing bytes
            FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()+HeaderBytes),Length);Body=FString(Utf8.Length(),Utf8.Get());Valid=true;break;
        }
    }
    FString Response=TEXT("{\"ok\":false,\"error\":\"Request rejected\"}");
    if(Valid&&!Stopping)
    {
        auto R=MakeShared<FRequest,ESPMode::ThreadSafe>();R->Method=Method;R->Path=Path;R->Body=MoveTemp(Body);Requests.Enqueue(R);
        // Never time out a queued mutation and later execute it: wait for GT or shutdown.
        while(!Stopping&&!R->Complete.load(std::memory_order_acquire))FPlatformProcess::SleepNoStats(.002f);
        if(R->Complete.load(std::memory_order_acquire)){Response=MoveTemp(R->Response);Code=R->Code;}
    }
    FTCHARToUTF8 Payload(*Response);const FString H=FString::Printf(TEXT("HTTP/1.1 %d Response\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n\r\n"),Code,Payload.Length());
    FTCHARToUTF8 HU(*H);TArray<uint8> Output;Output.Append(reinterpret_cast<const uint8*>(HU.Get()),HU.Length());Output.Append(reinterpret_cast<const uint8*>(Payload.Get()),Payload.Length());
    int32 Sent=0;const double SendDeadline=FPlatformTime::Seconds()+2;
    while(!Stopping&&Sent<Output.Num()&&FPlatformTime::Seconds()<SendDeadline){int32 N=0;if(Client->Send(Output.GetData()+Sent,Output.Num()-Sent,N)&&N>0)Sent+=N;else FPlatformProcess::SleepNoStats(.002f);}
}
