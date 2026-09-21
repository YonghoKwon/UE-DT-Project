#include "FactoryAgentScenarioTC.h"
#include "WebSocket/FTransactionCodeDataBase.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Slab/SlabDataSyncComponent.h"
#include "Core/DxProcessSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace { struct FFactoryAgentScenarioMessage : FTransactionCodeDataBase { FSlabScenarioDataPtr Scenario; FString Error; }; }
UFactoryAgentScenarioTC::UFactoryAgentScenarioTC() { TransactionCode=TEXT("IFactory-agent"); }
TSharedPtr<FTransactionCodeDataBase> UFactoryAgentScenarioTC::ParseToStruct(const FString& JsonString) const
{
	// No UObject access: this method is invoked by DTCore's background worker.
	auto Parsed=MakeShared<FFactoryAgentScenarioMessage>();
	FSlabScenarioCodec::Parse(JsonString,Parsed->Scenario,Parsed->Error,true); return Parsed;
}
void UFactoryAgentScenarioTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	check(IsInGameThread()); if(!Data.IsValid()) return;
	const auto Parsed=StaticCastSharedPtr<FFactoryAgentScenarioMessage>(Data);
	if(!Parsed->Scenario.IsValid()) { UE_LOG(LogTemp,Warning,TEXT("[IFactory-agent] %s"),*Parsed->Error); return; }
	UGameInstance* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;
	auto* Process=GI?GI->GetSubsystem<UDxProcessSubsystem>():nullptr;
	auto* Sync=Process?Cast<USlabDataSyncComponent>(Process->FindComponent(ReceiverId)):nullptr;
	if(!Sync||Sync->GetWorld()!=GetWorld()) { UE_LOG(LogTemp,Warning,TEXT("[IFactory-agent] Slab 수신 컴포넌트를 찾지 못했습니다: %s"),*ReceiverId); return; }
	Sync->ReceiveSlabScenario(Parsed->Scenario);
}
