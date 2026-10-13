#pragma once
#include "CoreMinimal.h"

class AActor;
class ASlabActor;

/** Weak configured-Actor presence, distinct from active Widget hosting.
 * EndPlay releases activation; weak presence lasts until Actor invalidation,
 * matching the legacy iterator even before BeginPlay and during teardown.
 */
namespace SlabUiRegistry
{
MA0T10_DT_API void RegisterHost(AActor* Owner, TFunction<ASlabActor*()> ResolveTarget, bool bActive = false);
MA0T10_DT_API void UnregisterHost(AActor* Owner);
MA0T10_DT_API void RegisterActivation(AActor* Owner,TFunction<bool(ASlabActor*)> Callback);
MA0T10_DT_API bool Activate(ASlabActor* Slab);
MA0T10_DT_API bool HasHostFor(const ASlabActor* Slab, int32 SlabCount);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
MA0T10_DT_API bool IsHostActiveForTesting(const AActor* Owner);
#endif
}
