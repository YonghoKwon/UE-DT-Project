#include "SlabDataSyncComponent.h"
#include "SlabActor.h"
USlabDataSyncComponent::USlabDataSyncComponent() { PrimaryComponentTick.bCanEverTick=false; }
bool USlabDataSyncComponent::ReceiveSlabScenario(FSlabScenarioDataPtr Scenario)
{
	check(IsInGameThread());
	if(ASlabActor* Actor=Cast<ASlabActor>(GetOwner())) return Actor->ReceiveScenario(MoveTemp(Scenario));
	return false;
}
bool USlabDataSyncComponent::ReceiveTestSetTrigger(const FString& Material)
{
    check(IsInGameThread());auto* Actor=Cast<ASlabActor>(GetOwner());return Actor&&Actor->ReceiveTestSetTrigger(Material);
}
bool USlabDataSyncComponent::ReceiveScenarioJson(const FString& Json)
{check(IsInGameThread());auto* Actor=Cast<ASlabActor>(GetOwner());return Actor&&Actor->SubmitScenarioJson(Json);}
