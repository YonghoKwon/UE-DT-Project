#pragma once

#include "Commandlets/Commandlet.h"
#include "EnsureLidarJsonLiveFrameTransactionCommandlet.generated.h"

UCLASS()
class UEnsureLidarJsonLiveFrameTransactionCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UEnsureLidarJsonLiveFrameTransactionCommandlet();

    virtual int32 Main(const FString& Params) override;
};