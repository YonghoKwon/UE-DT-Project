#pragma once
#include "CoreMinimal.h"
#include "ActorComponent/DataSyncCompBase.h"
#include "SlabScenarioTypes.h"
#include "SlabDataSyncComp.generated.h"
UCLASS(ClassGroup=(Slab),meta=(BlueprintSpawnableComponent))
class MA0T10_DT_API USlabDataSyncComp : public UDataSyncCompBase
{
	GENERATED_BODY()
public:
	USlabDataSyncComp();
	/** Typed project contract: deliberately does not extend DTCore's EDxDataType. */
	bool ReceiveSlabScenario(FSlabScenarioDataPtr Scenario);
    bool ReceiveTestSetTrigger(const FString& Material);
    bool ReceiveScenarioJson(const FString& Json);
};
