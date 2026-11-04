#pragma once
#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "ShipHatchOverlayComp.generated.h"

/** Map-owned player presentation only; no collision, capture, or measurement. */
UCLASS(Transient, NotBlueprintable)
class MA0T10_DT_API UShipHatchOverlayComp : public ULineBatchComponent
{
	GENERATED_BODY()
public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};
