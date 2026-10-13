#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "RepairTestMapMonitorReferenceCommandlet.generated.h"

/** Narrow, opt-in repair of the retired monitor reference in the legacy TestMap. */
UCLASS()
class URepairTestMapMonitorReferenceCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    URepairTestMapMonitorReferenceCommandlet();
    virtual int32 Main(const FString& Params) override;
};
