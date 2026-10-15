#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Dom/JsonObject.h"
#include "Containers/Ticker.h"
#include "AutomationControlSubsystem.generated.h"
struct FAutomationControlState;

/** Explicit opt-in facade over existing project services. No listener/ticker/metrics when off. */
UCLASS()
class MA0T10_DT_API UAutomationControlSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
    bool SetApiEnabled(bool Enabled);
    bool IsApiEnabled() const;
    FString GetPresentationStatus() const;
    TSharedRef<FJsonObject> GetStatus() const;
    /** Game-thread only. Also used by the job runner and integration tests. */
    TSharedRef<FJsonObject> Dispatch(const FString& Method,const FString& Path,const TSharedPtr<FJsonObject>& Body,int32& Code);
    bool StartJob(const FString& Path,bool ExitWhenDone=true);
private:
    TSharedPtr<FAutomationControlState> State;
    FTSTicker::FDelegateHandle Ticker;
    FDelegateHandle FrameHook;
    bool Poll(float Delta);
    void ObserveFrame(UWorld* World,ELevelTick Tick,float Delta);
    void EnsurePolling();
    bool StartCommand(const TSharedRef<FJsonObject>& Command,FString& Error);
    void CompleteOperation(bool Success,const FString& Error);
    void FinishJob(bool Success,const FString& Error);
    void StopOwnedOperation();
};
