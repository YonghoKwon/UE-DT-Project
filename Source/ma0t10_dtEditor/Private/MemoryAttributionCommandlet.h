#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MemoryAttributionCommandlet.generated.h"

/** Offline UE5.3 TraceServices live-allocation comparison; never changes the recorded process. */
UCLASS()
class UMemoryAttributionCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UMemoryAttributionCommandlet();
    virtual int32 Main(const FString& Params) override;
};
