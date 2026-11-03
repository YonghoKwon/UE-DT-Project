#include "TrainBatchScenarioTC.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchSubsystem.h"
#include "Engine/World.h"

UTrainBatchScenarioTC::UTrainBatchScenarioTC(){TransactionCode=TrainBatchContract::MessageId;}
TSharedPtr<FTrainBatchTransaction> UTrainBatchScenarioTC::ParsePayload(const FString& Json)
{auto P=MakeShared<FTrainBatchTransaction>();TrainBatchCodec::Parse(Json,P->Message,P->Error);return P;}
TSharedPtr<FTransactionCodeDataBase> UTrainBatchScenarioTC::ParseToStruct(const FString& Json) const
{return ParsePayload(Json);}
void UTrainBatchScenarioTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
    check(IsInGameThread());if(!Data||!GetWorld())return;
    const auto P=StaticCastSharedPtr<FTrainBatchTransaction>(Data);
    if(auto* S=GetWorld()->GetSubsystem<UTrainBatchSubsystem>())S->AcceptTransaction(*P);
}
void UTrainBatchScenarioTC::OnSubscriptionMessage(const FWebSocketMessage& Message)
{
    if(GetWorld())if(auto* S=GetWorld()->GetSubsystem<UTrainBatchSubsystem>())
        if(S->IsSubscriptionCurrent(SubscriptionEpoch))S->OnMessage(Message);
}
void UTrainBatchScenarioTC::OnSubscriptionComplete(bool Success,FString Error)
{
    if(GetWorld())if(auto* S=GetWorld()->GetSubsystem<UTrainBatchSubsystem>())
        if(S->IsSubscriptionCurrent(SubscriptionEpoch))S->OnSubscribed(Success,Error);
}
