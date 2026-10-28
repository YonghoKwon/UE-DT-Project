#pragma once
#include "Commandlets/Commandlet.h"
#include "NativeLidarLifetimeCommandlet.generated.h"

/** Explicit real-RHI diagnostic; no map/widget/assets are created or saved. */
UCLASS()
class UNativeLidarLifetimeCommandlet final:public UCommandlet
{
    GENERATED_BODY()
public:
    UNativeLidarLifetimeCommandlet();
    int32 Main(const FString& Params)override;
};
