#pragma once

#include "Commandlets/Commandlet.h"
#include "SetupVirtualLidarNiagaraAssetsCommandlet.generated.h"

UCLASS()
class USetupVirtualLidarNiagaraAssetsCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    USetupVirtualLidarNiagaraAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
