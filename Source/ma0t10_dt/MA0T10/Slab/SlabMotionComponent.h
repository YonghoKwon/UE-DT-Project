#pragma once
#include "CoreMinimal.h"
#include "ActorComponent/MechDriverCompBase.h"
#include "SlabMotionComponent.generated.h"
UCLASS(ClassGroup=(Slab),meta=(BlueprintSpawnableComponent))
class MA0T10_DT_API USlabMotionComponent : public UMechDriverCompBase
{
	GENERATED_BODY()
public:
	USlabMotionComponent();
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
};
