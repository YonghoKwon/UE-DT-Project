#pragma once
#include "Commandlets/Commandlet.h"
#include "EnsureSlabScenarioTransactionCommandlet.generated.h"
UCLASS()
class UEnsureSlabScenarioTransactionCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UEnsureSlabScenarioTransactionCommandlet();
    virtual int32 Main(const FString& Params) override;
};
