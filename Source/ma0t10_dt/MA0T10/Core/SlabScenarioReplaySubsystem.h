#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Interface.h"
#include "Containers/Ticker.h"
#include "SlabScenarioReplaySubsystem.generated.h"

UINTERFACE(BlueprintType)
class MA0T10_DT_API USlabScenarioPlaybackAdapter : public UInterface { GENERATED_BODY() };
class MA0T10_DT_API ISlabScenarioPlaybackAdapter
{
	GENERATED_BODY()
public:
	/** Accept only; notify actual start/finish through the replay subsystem. Never rewrite Json. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="DigitalTwin|ScenarioReplay")
	bool StartScenarioPlayback(const FString& Json, const FString& ScenarioUUID, const FString& RunUUID);
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabScenarioSummary
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString UUID;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString CreateTimestamp;
	UPROPERTY(BlueprintReadOnly) FString MaterialSummary;
	UPROPERTY(BlueprintReadOnly) FDateTime ReceivedUtc;
	UPROPERTY(BlueprintReadOnly) int32 RowCount=0;
	UPROPERTY(BlueprintReadOnly) double LastElapsedSec=0;
};
UENUM(BlueprintType)
enum class ESlabScenarioReplayState : uint8 { Idle, Starting, Playing, Draining, Completed, Failed };
USTRUCT(BlueprintType)
struct MA0T10_DT_API FSlabScenarioReplayStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ESlabScenarioReplayState State=ESlabScenarioReplayState::Idle;
	UPROPERTY(BlueprintReadOnly) FString ScenarioUUID;
	UPROPERTY(BlueprintReadOnly) FString RunUUID;
	UPROPERTY(BlueprintReadOnly) FString Message;
	UPROPERTY(BlueprintReadOnly) bool bSendPcd=false;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlabScenarioListChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSlabScenarioRegistrationFinished,const FString&,UUID,bool,bStored,const FString&,Message);

/** Per-PIE/GameInstance memory only. Owns JSON snapshots, never moves production Slab actors. */
UCLASS()
class MA0T10_DT_API USlabScenarioReplaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool RegisterScenarioJson(const FString& OriginalJson);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool RegisterPlaybackAdapter(UObject* Adapter);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") void UnregisterPlaybackAdapter(UObject* Adapter);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool RequestScenarioReplay(const FString& ScenarioUUID,bool bSendPcd,const TArray<FString>& TargetSensorIds);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool NotifyPlaybackStarted(const FString& RunUUID);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool NotifyPlaybackFinished(const FString& RunUUID,bool bAborted);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool SetLivePlaybackActive(bool bActive);
	UFUNCTION(BlueprintPure,Category="DigitalTwin|ScenarioReplay") bool CanReplay() const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|ScenarioReplay") bool IsReplayBusy() const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|ScenarioReplay") TArray<FSlabScenarioSummary> GetScenarios() const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|ScenarioReplay") FSlabScenarioReplayStatus GetReplayStatus() const { return Status; }
	UFUNCTION(BlueprintPure,Category="DigitalTwin|ScenarioReplay") FString GetRegistrationMessage() const { return RegistrationMessage; }
	UFUNCTION(BlueprintPure,Category="DigitalTwin|ScenarioReplay") int32 GetPendingRegistrationCount() const { return PendingJson.Num()+(bParsing?1:0); }
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool GetScenarioJson(const FString& UUID,FString& Json) const;
	UPROPERTY(BlueprintAssignable) FSlabScenarioListChanged OnScenariosChanged;
	UPROPERTY(BlueprintAssignable) FSlabScenarioRegistrationFinished OnRegistrationFinished;
	static bool ValidateScenario(const FString& Json,FSlabScenarioSummary& Summary,FString& Error);
private:
	struct FStoredScenario { FSlabScenarioSummary Summary; FString Json; };
	TArray<FStoredScenario> Entries;
	TArray<FString> PendingJson;
	TWeakObjectPtr<UObject> PlaybackAdapter;
	TWeakObjectPtr<UWorld> PlaybackWorld;
	FSlabScenarioReplayStatus Status;
	FString RegistrationMessage;
	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle CleanupHandle;
	uint64 Generation=0;
	bool bParsing=false;
	bool bInitialized=false;
	bool bLivePlaybackActive=false;
	double StartRequestedSeconds=0;
	void StartNextRegistration();
	void StoreValidated(FSlabScenarioSummary Summary,FString Json);
	bool Poll(float Delta);
	void OnWorldCleanup(UWorld* World,bool bSessionEnded,bool bCleanupResources);
	void AbortReplay(const FString& Reason);
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSlabReplayCatalogTest;
#endif
};
