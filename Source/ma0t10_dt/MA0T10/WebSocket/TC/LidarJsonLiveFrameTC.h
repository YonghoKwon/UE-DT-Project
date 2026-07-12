#pragma once

#include "CoreMinimal.h"
#include "WebSocket/FTransactionCodeDataBase.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "LidarJsonLiveFrameTC.generated.h"

struct FLidarJsonLiveFrameTCData : FTransactionCodeDataBase
{
    FString SourceId;
    FString JsonLines;
    bool bSendTransport = true;
    bool bPushFrame = true;
};

UCLASS()
class MA0T10_DT_API ULidarJsonLiveFrameTC : public UTransactionCodeMessage, public FTransactionCodeDataBase
{
    GENERATED_BODY()

public:
    ULidarJsonLiveFrameTC();

    virtual TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString& JsonString) const override;
    virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) override;
};
