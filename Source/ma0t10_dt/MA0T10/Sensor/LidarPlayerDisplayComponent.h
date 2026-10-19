#pragma once
#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "LidarPlayerDisplayComponent.generated.h"

/** Ordinary player-view primitives, including restricted viewport rectangles. */
UCLASS(NotBlueprintable,Transient)
class MA0T10_DT_API ULidarPlayerDisplayComponent : public ULineBatchComponent
{
    GENERATED_BODY()
public:
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};
