#include "SlabDataSyncComponent.h"
#include "SlabActor.h"
USlabDataSyncComponent::USlabDataSyncComponent() { PrimaryComponentTick.bCanEverTick=false; }
bool USlabDataSyncComponent::ReceiveSlabScenario(FSlabScenarioDataPtr Scenario)
{
	check(IsInGameThread());
	if(ASlabActor* Actor=Cast<ASlabActor>(GetOwner())) return Actor->ReceiveScenario(MoveTemp(Scenario));
	return false;
}
