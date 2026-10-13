#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Crane/CraneDataTypes.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "WebSocket/FTransactionCodeDataBase.h"
#include "HAL/PlatformProcess.h"
#include "UI/DxWidget.h"
#include "Core/DxWebSocketSubsystem.h"
#include "DTCoreContractListener.generated.h"

class UBlueprint;

UCLASS()
class UDTCoreMainAcceptanceWidget : public UDxWidget
{
    GENERATED_BODY()
protected:
    void NativeOnInitialized() override;
};

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
    UFUNCTION(BlueprintCallable, Category="DTCore|Validation")
    static void PrepareFinalAcceptanceEditor();
    UFUNCTION(BlueprintCallable, Category="DTCore|Validation")
    static FString GetFinalAcceptanceRuntimeState();
    int32 ReceivedCount = 0;
    UFUNCTION() void ReceiveMessage(const FWebSocketMessage& Message) { ++ReceivedCount; }
    int32 ReadyCount = 0;
    UFUNCTION() void ReceiveReady(FString Protocol, FString Session, FString Server) { ++ReadyCount; }
    UFUNCTION() void ReceiveHttp(bool bSuccess, int32 Code, const FString& Content) { ++ReceivedCount; }
    FCranePositionData LastPosition;
    UFUNCTION() void ReceivePosition(const FCranePositionData& Position)
    {
        ++ReceivedCount;
        LastPosition = Position;
    }
};

UCLASS()
class UDTCoreCloseOrderWidget : public UDxWidget
{
    GENERATED_BODY()
public:
    TSharedPtr<TArray<FString>> CloseTrace;
    bool bRemovedBeforeHook=false;
    void CloseWidgetAddLogic_Implementation() override
    {
        bRemovedBeforeHook=GetParent()==nullptr;
        if(CloseTrace)CloseTrace->Add(GetName());
        CloseWidget();
    }
};
