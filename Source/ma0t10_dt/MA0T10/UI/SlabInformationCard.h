#pragma once
#include "CoreMinimal.h"
class ASlabActor;

/** UI-only formatting of one immutable source row plus existing geometric calculations. */
namespace SlabInformationCard
{
    FString Build(const ASlabActor* Slab);
}
