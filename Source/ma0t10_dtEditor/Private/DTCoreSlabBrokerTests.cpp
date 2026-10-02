#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Core/DTCoreSettings.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"

namespace
{
// 실제 Broker에서 보낸 전문만 기다린다. EnqueueWebSocketData 직접 주입은 하지 않는다.
class FDTCoreSlabBrokerCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    FString OldUrl,OldLogin,OldPassword;
    TArray<FString> OldTopics;
    double Start=FPlatformTime::Seconds();
    bool bReady=false;
public:
    explicit FDTCoreSlabBrokerCheck(FAutomationTestBase* In):Test(In)
    {
        auto* S=GetMutableDefault<UDTCoreSettings>();
        OldUrl=S->WebSocketUrl;OldLogin=S->WebSocketLogin;OldPassword=S->WebSocketPasscode;OldTopics=S->WebSocketTopics;
        S->WebSocketUrl=TEXT("ws://127.0.0.1:61616");
        S->WebSocketLogin=TEXT("artemis");S->WebSocketPasscode=TEXT("artemis");S->WebSocketTopics={TEXT("topic.scenario")};
    }
    ~FDTCoreSlabBrokerCheck()
    {
        auto* S=GetMutableDefault<UDTCoreSettings>();
        S->WebSocketUrl=OldUrl;S->WebSocketLogin=OldLogin;S->WebSocketPasscode=OldPassword;S->WebSocketTopics=OldTopics;
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>60){Test->AddError(TEXT("Actual DTCore Slab Broker test timed out; publish synthetic JSON after DTCORE_SLAB_BROKER_READY."));return true;}
        UWorld* W=nullptr;
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||!W->GetGameInstance())return false;
        ASlabActor* Slab=nullptr;for(TActorIterator<ASlabActor> It(W);It;++It){Slab=*It;break;}
        if(!Slab)return false;
        auto* WS=W->GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>();
        if(!bReady)
        {
            if(!WS->AreConfiguredSubscriptionsReady())return false;
            Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
            bReady=true;UE_LOG(LogTemp,Display,TEXT("DTCORE_SLAB_BROKER_READY topic.scenario"));
        }
        if(!Slab->IsSimulationActive()||Slab->GetSimulationStatus().ElapsedSec<0.25)return false;
        const auto S=Slab->GetSimulationStatus();
        Test->TestEqual(TEXT("real Broker MESSAGE reached TC/DataSync/Slab"),S.MtlNo,FString(TEXT("SQ83521 047")));
        Test->TestTrue(TEXT("scenario frame advances independently of sensor frames"),S.FrameNo>=4);
        Test->TestTrue(TEXT("real transport remains connected"),WS->IsTransportConnected());
        Slab->StopSimulation();WS->DisconnectStompClient({});
        Test->TestFalse(TEXT("manual disconnect clears transport readiness"),WS->IsTransportConnected());
        UE_LOG(LogTemp,Display,TEXT("DTCORE_SLAB_BROKER_PASS frame=%lld"),S.FrameNo);
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreSlabBrokerTest,"MA0T10.DTCoreIntegration.SlabBroker",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDTCoreSlabBrokerTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_DTCORE_SLAB_BROKER"))!=TEXT("1"))
    {AddInfo(TEXT("SKIP: opt-in real local Broker requires MA0T10_DTCORE_SLAB_BROKER=1 and external synthetic publisher."));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    auto Command=MakeShared<FDTCoreSlabBrokerCheck>(this);
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(Command);
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
