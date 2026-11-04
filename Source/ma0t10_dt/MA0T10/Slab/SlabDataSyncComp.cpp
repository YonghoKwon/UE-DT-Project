#include "ma0t10_dt/MA0T10/Slab/SlabDataSyncComp.h"
#include "SlabActor.h"
USlabDataSyncComp::USlabDataSyncComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}
bool USlabDataSyncComp::ReceiveSlabScenario(FSlabScenarioDataPtr Scenario)
{
	check(IsInGameThread());
	if (ASlabActor* Actor = Cast<ASlabActor>(GetOwner()))
		return Actor->ReceiveScenario(MoveTemp(Scenario));
	return false;
}
bool USlabDataSyncComp::ReceiveTestSetTrigger(const FString& Material)
{
	check(IsInGameThread());
	auto* Actor = Cast<ASlabActor>(GetOwner());
	return Actor && Actor->ReceiveTestSetTrigger(Material);
}
bool USlabDataSyncComp::ReceiveScenarioJson(const FString& Json)
{
	check(IsInGameThread());
	auto* Actor = Cast<ASlabActor>(GetOwner());
	return Actor && Actor->SubmitScenarioJson(Json);
}
