#pragma once
#include "CoreMinimal.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "WebSocket/FTransactionCodeDataBase.h"
#include "Core/DxWebSocketSubsystem.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchCodec.h"
#include "TrainBatchScenarioTC.generated.h"

struct FTrainBatchTransaction : FTransactionCodeDataBase
{
	FTrainBatchMessage Message;
	FString Error;
};
/** Project-owned TC; explicit DTCore Subscribe adapter avoids mutating its immutable handler map. */
UCLASS()
class MA0T10_DT_API UTrainBatchScenarioTC : public UTransactionCodeMessage
{
	GENERATED_BODY()
public:
	UTrainBatchScenarioTC();
	uint64 SubscriptionEpoch = 0;
	UFUNCTION()
	void OnSubscriptionMessage(const FWebSocketMessage& Message);
	UFUNCTION()
	void OnSubscriptionComplete(bool Success, FString Error);
	static TSharedPtr<FTrainBatchTransaction> ParsePayload(const FString& Json);
	virtual TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString& Json) const override;
	virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) override;
};
