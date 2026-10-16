#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VirtualSensorFileSaveTypes.h"
#include "VirtualSensorFileSaveSubsystem.generated.h"
class AVirtualSensorActorBase;
class UVirtualImuComponent;
struct FVirtualImuSample;
struct FVirtualCameraLocalSaveFrame;
struct FVirtualSensorFileSaveRequest;
class AVirtualCameraSensorActor;
struct FVirtualSensorVerificationFile
{
    FString Name;
    TArray64<uint8> Bytes;
};

/** World-owned file requests. No widget lifetime, sensor selection or transport ownership. */
UCLASS()
class MA0T10_DT_API UVirtualSensorFileSaveSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
    FString RequestImuCsv(UVirtualImuComponent* Source,const FString& Id,const TArray<FVirtualImuSample>& Samples);
    /** CAM-GT-01 adapter: capture is asynchronous and uses the existing file-save admission. */
    bool RequestCameraGroundTruthSave(AVirtualCameraSensorActor* Sensor);
    /** Worker-only, immutable verification bundle. Last file is the completion marker. */
    static bool WriteCameraGroundTruthBundle(const FString& Directory, const TArray<FVirtualSensorVerificationFile>& Files, TFunctionRef<bool()> Cancelled);
    static bool WriteDatasetSceneBundle(const FString& Directory, const TArray<FVirtualSensorVerificationFile>& Files, TFunctionRef<bool()> Cancelled);
    static bool WriteTestSetSceneBundle(const FString& Directory, const TArray<FVirtualSensorVerificationFile>& Files, TFunctionRef<bool()> Cancelled);
    /** Same-directory atomic replace, restricted to Saved/TestSets. Worker thread only. */
    static bool CommitTestSetManifest(const FString& Destination,const FString& Pending);
	/** Always returns a request ID, including explicit rejected/Failed requests. Query status immediately. */
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorFiles") FString RequestSave(EVirtualSensorFileSaveMode Mode,AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection);
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorFiles") FVirtualSensorFileSaveStatus GetRequestStatus(const FString& RequestId) const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorFiles") TArray<FVirtualSensorFileSaveStatus> GetRecentRequests() const;
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorFiles") bool CancelRequest(const FString& RequestId);
	bool HasPendingSave(AVirtualSensorActorBase* Sensor) const;
	bool CanAcceptSave(AVirtualSensorActorBase* Sensor) const;
	static int64 GetAvailableFrameId(AVirtualSensorActorBase* Sensor);
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorFiles") static bool CanSaveCurrent(AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection);
	static uint64 GetSensorSaveRevision(AVirtualSensorActorBase* Sensor);
	FString RequestSaveInCaptureSession(AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection,const FString& SessionDirectory);
	UPROPERTY(BlueprintAssignable) FVirtualSensorFileSaveUpdated OnRequestUpdated;
	FVirtualSensorFileSaveUpdatedNative OnRequestUpdatedNative;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;
private:
    static bool WriteVerificationBundle(const FString& AllowedRoot,const FString& Directory,const TArray<FVirtualSensorVerificationFile>& Files,TFunctionRef<bool()> Cancelled,bool AllowEmpty=false);
	TMap<FString,TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>> Requests;
	TArray<FString> Order;
	bool bShuttingDown=false;
	int32 ActiveWrites=0;
	bool bPublishingNotifications=false;
	TArray<FVirtualSensorFileSaveStatus> PendingNotifications;
	FString RequestSaveInternal(EVirtualSensorFileSaveMode Mode,AVirtualSensorActorBase* Sensor,const FVirtualSensorCaptureSelection& Selection,const FString& SessionDirectory);
	void QueueWrite(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& Request);
	bool IsRequestCurrent(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& Request,FString& Reason) const;
	void SetState(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& Request,EVirtualSensorFileSaveState State,const FString& Message);
	void StartLidarSave(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& Request);
	void StartCameraSave(const TSharedPtr<FVirtualSensorFileSaveRequest,ESPMode::ThreadSafe>& Request,TSharedPtr<const FVirtualCameraLocalSaveFrame,ESPMode::ThreadSafe> Frame,const FString& Error);
	void FinishWrite(const FString& RequestId,TArray<FVirtualSensorExportResult> Results,const FString& Error);
	void TrimHistory();
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSensorFileSaveStateTest;
#endif
};
