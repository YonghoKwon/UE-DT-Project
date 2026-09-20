#pragma once
#include "CoreMinimal.h"
#include "ActorComponent/DataSyncCompBase.h"
#include "SlabScenarioTypes.h"
#include "SlabDataSyncComponent.generated.h"
UCLASS(ClassGroup=(Slab),meta=(BlueprintSpawnableComponent))
class MA0T10_DT_API USlabDataSyncComponent : public UDataSyncCompBase
{
	GENERATED_BODY()
public:
	USlabDataSyncComponent();
	/** Typed project contract: deliberately does not extend DTCore's EDxDataType. */
	bool ReceiveSlabScenario(FSlabScenarioDataPtr Scenario);
};
