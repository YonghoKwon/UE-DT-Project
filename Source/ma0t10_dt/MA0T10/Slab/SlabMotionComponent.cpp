#include "SlabMotionComponent.h"
#include "SlabActor.h"
USlabMotionComponent::USlabMotionComponent()
{ PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false; PrimaryComponentTick.TickGroup=TG_PrePhysics; }
void USlabMotionComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
	if(ASlabActor* Actor=Cast<ASlabActor>(GetOwner())) Actor->AdvanceSimulation(DeltaTime);
}
