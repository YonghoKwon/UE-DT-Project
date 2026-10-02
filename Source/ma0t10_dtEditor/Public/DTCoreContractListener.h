#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Crane/CraneDataTypes.h"
#include "DTCoreContractListener.generated.h"

class UBlueprint;

UCLASS()
class UDTCoreContractListener : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="DTCore|Validation")
    static bool CompileBlueprintForValidation(UBlueprint* Blueprint);
    int32 ReceivedCount = 0;
    FCranePositionData LastPosition;
    UFUNCTION() void ReceivePosition(const FCranePositionData& Position)
    {
        ++ReceivedCount;
        LastPosition = Position;
    }
};
