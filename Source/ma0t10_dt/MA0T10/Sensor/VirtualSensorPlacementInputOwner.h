#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VirtualSensorPlacementInputOwner.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UVirtualSensorPlacementInputOwner : public UInterface
{
    GENERATED_BODY()
};

class MA0T10_DT_API IVirtualSensorPlacementInputOwner
{
    GENERATED_BODY()
public:
    virtual bool IsPlacementObserverNavigationActive() const = 0;
    virtual bool HasPlacementInputFocus() const = 0;
    virtual void EndPlacementObserverNavigation() = 0;
};
