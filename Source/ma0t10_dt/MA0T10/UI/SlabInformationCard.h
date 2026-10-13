#pragma once
#include "CoreMinimal.h"
class ASlabActor;

/** UI-only formatting of one immutable source row plus existing geometric calculations. */
namespace SlabInformationCard
{
    struct FRow
    {
        FText Section;
        FText Label;
        FString Value;
        bool bSourceDetail=false;
    };
    TArray<FRow> BuildRows(const ASlabActor* Slab, bool bIncludeSourceDetail);
    FString Build(const ASlabActor* Slab);
}
