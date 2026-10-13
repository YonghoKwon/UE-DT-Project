#pragma once
#include "CoreMinimal.h"
#include "SlabExecutionTypes.generated.h"

UENUM(BlueprintType)
enum class ESlabExecutionPolicy : uint8 { ObservationAllowed, RequireData };
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabExecutionOptions
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) ESlabExecutionPolicy Policy=ESlabExecutionPolicy::ObservationAllowed;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1",ClampMax="120")) float PreparationTimeoutSeconds=10;
};
