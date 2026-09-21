#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorFileSaveTypes.h"
#include "VirtualSensorPeriodicFileSaveSubsystem.generated.h"
class AVirtualSensorActorBase;
class UVirtualSensorFileSaveSubsystem;
struct FVirtualSensorPeriodicFileSaveSession;

UENUM(BlueprintType)
enum class EVirtualSensorPeriodicSaveWaitReason : uint8
{
	None,
	SaveBusy,
	NoCompletedFrame,
	NoNewFrame,
	SelectedOutputUnavailable
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorPeriodicFileSaveStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) FString SensorId;
	UPROPERTY(BlueprintReadOnly) float IntervalSeconds=1;
	UPROPERTY(BlueprintReadOnly) bool bActive=false;
	UPROPERTY(BlueprintReadOnly) bool bPending=false;
	UPROPERTY(BlueprintReadOnly) bool bWaitingForLatestFrame=false;
	UPROPERTY(BlueprintReadOnly) EVirtualSensorPeriodicSaveWaitReason WaitReason=EVirtualSensorPeriodicSaveWaitReason::None;
	UPROPERTY(BlueprintReadOnly) int64 CompletedCount=0;
	UPROPERTY(BlueprintReadOnly) int64 SkippedCount=0;
	UPROPERTY(BlueprintReadOnly) int64 FailedCount=0;
	UPROPERTY(BlueprintReadOnly) int64 LastFrameId=-1;
	UPROPERTY(BlueprintReadOnly) FString LastRequestId;
	UPROPERTY(BlueprintReadOnly) FString LastMessage;
	UPROPERTY(BlueprintReadOnly) FString Folder;
	UPROPERTY(BlueprintReadOnly) FVirtualSensorCaptureSelection Selection;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVirtualSensorPeriodicFileSaveUpdated,const FVirtualSensorPeriodicFileSaveStatus&,Status);

/** Latest completed frames only. No acquisition, no widget lifetime, no unbounded waiting queue. */
UCLASS()
class MA0T10_DT_API UVirtualSensorPeriodicFileSaveSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	/** Returns a diagnostic session ID even if startup is rejected. Parameters freeze at startup. */
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorFiles|Periodic") FString StartPeriodicSave(AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorFiles|Periodic") bool StopPeriodicSave(const FString& SessionId);
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorFiles|Periodic") FVirtualSensorPeriodicFileSaveStatus GetPeriodicStatus(const FString& SessionId) const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorFiles|Periodic") TArray<FVirtualSensorPeriodicFileSaveStatus> GetRecentPeriodicSessions() const;
	UPROPERTY(BlueprintAssignable) FVirtualSensorPeriodicFileSaveUpdated OnPeriodicUpdated;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;
	/** Pure monotonic cadence helper. Returns due slots, advances to the first future deadline. */
	static int64 AdvanceDeadline(double OriginSeconds,double IntervalSeconds,double NowSeconds,int64& NextDeadlineIndex);
	static bool IsNewFrame(int64 AvailableFrameId,int64 LastRequestedFrameId);
	static bool ValidateSelection(EVirtualSensorKind Kind,const FVirtualSensorCaptureSelection& Selection,FString& OutError);
	static constexpr int32 MaxConcurrentSessions=16;
	static constexpr int32 MaxRetainedSessions=32;
private:
	TMap<FString,TSharedPtr<FVirtualSensorPeriodicFileSaveSession>> Sessions;
	TArray<FString> Order;
	bool bShuttingDown=false;
	void Publish(const TSharedPtr<FVirtualSensorPeriodicFileSaveSession>& Session);
	UFUNCTION() void HandleFileSaveUpdated(const FVirtualSensorFileSaveStatus& Status);
	void CompleteRequest(const TSharedPtr<FVirtualSensorPeriodicFileSaveSession>& Session,const FVirtualSensorFileSaveStatus& Result);
	void TrimHistory();
	void ProcessSession(const TSharedPtr<FVirtualSensorPeriodicFileSaveSession>& Session,UVirtualSensorFileSaveSubsystem* Files,double Now);
};
