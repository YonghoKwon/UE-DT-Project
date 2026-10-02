#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Crane/CraneDataTypes.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "WebSocket/FTransactionCodeDataBase.h"
#include "HAL/PlatformProcess.h"
#include "DTCoreContractListener.generated.h"

class UBlueprint;

struct FDTCoreParseProbe
{
    TAtomic<bool> Started{false};
    TAtomic<int32> Applied{0};
};

UCLASS()
class UDTCoreSlowTransaction : public UTransactionCodeMessage
{
    GENERATED_BODY()
public:
    TSharedPtr<FDTCoreParseProbe,ESPMode::ThreadSafe> Probe;
    TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString&) const override
    {
        Probe->Started.Store(true);
        FPlatformProcess::Sleep(0.05f);
        return MakeShared<FTransactionCodeDataBase>();
    }
    void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>&) override { ++Probe->Applied; }
};

UCLASS()
class UDTCoreContractListener : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="DTCore|Validation")
    static bool CompileBlueprintForValidation(UBlueprint* Blueprint);
    int32 ReceivedCount = 0;
    int32 ReadyCount = 0;
    UFUNCTION() void ReceiveReady(FString Protocol, FString Session, FString Server) { ++ReadyCount; }
    FCranePositionData LastPosition;
    UFUNCTION() void ReceivePosition(const FCranePositionData& Position)
    {
        ++ReceivedCount;
        LastPosition = Position;
    }
};
