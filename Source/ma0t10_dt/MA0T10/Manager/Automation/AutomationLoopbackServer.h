#pragma once
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"
#include <atomic>
class FSocket;
class FRunnableThread;

/** Small bounded HTTP/1.1 transport. Socket work never accesses UObjects. One outstanding
 * request is delivered to the game thread; subsequent connections remain in the bounded OS backlog. */
class FAutomationLoopbackServer final:public FRunnable
{
public:
    struct FRequest
    {
        FString Method,Path,Body,Response;
        int32 Code=503;
        std::atomic<bool> Complete{false};
    };
    ~FAutomationLoopbackServer();
    bool Start(uint16 Port,FString Token);
    void Shutdown();
    bool Dequeue(TSharedPtr<FRequest,ESPMode::ThreadSafe>& Request){return Requests.Dequeue(Request);}
    uint32 Run() override;
    void Stop() override {Stopping=true;}
private:
    FSocket* Listener=nullptr;
    FRunnableThread* Thread=nullptr;
    uint16 ListenPort=0;
    FString Secret;
    std::atomic<bool> Stopping{false};
    TQueue<TSharedPtr<FRequest,ESPMode::ThreadSafe>,EQueueMode::Spsc> Requests;
    void Serve(FSocket* Client);
};
