#pragma once
#include "CoreMinimal.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "FactoryAgentScenarioTC.generated.h"

/** Uses the existing DTCore MESSAGE_ID routing; never owns/disconnects the shared socket. */
UCLASS()
class MA0T10_DT_API UFactoryAgentScenarioTC : public UTransactionCodeMessage
{
	GENERATED_BODY()
public:
	UFactoryAgentScenarioTC();
	virtual TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString& JsonString) const override;
	virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) override;
	UPROPERTY(EditDefaultsOnly,Category="Slab|Routing") FString ReceiverId=TEXT("SlabScenario.Main");
};
