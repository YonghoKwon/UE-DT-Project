#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Async/Future.h"
#include "VirtualSensorPlacementTypes.h"
#include "VirtualSensorPlacementSubsystem.generated.h"
class AVirtualSensorActorBase;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVirtualSensorPoseLibraryChanged);

/** Per-world pose history and per-map persistent pose library. Does not save a level. */
UCLASS()
class MA0T10_DT_API UVirtualSensorPlacementSubsystem:public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    virtual void Tick(float Delta) override;
    virtual TStatId GetStatId() const override;
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") FString RequestSaveSensorPose(AVirtualSensorActorBase* Sensor,const FString& Name,const FString& ExistingBookmarkId=TEXT(""));
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorPlacement") FVirtualSensorPoseSaveStatus GetPoseSaveStatus(const FString& RequestId) const;
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") TArray<FVirtualSensorPoseBookmark> GetSensorPoseBookmarks(AVirtualSensorActorBase* Sensor);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") bool ApplySensorPoseBookmark(AVirtualSensorActorBase* Sensor,const FString& BookmarkId,FString& Reason);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") FString RenameSensorPoseBookmark(AVirtualSensorActorBase* Sensor,const FString& BookmarkId,const FString& Name);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") FString DeleteSensorPoseBookmark(AVirtualSensorActorBase* Sensor,const FString& BookmarkId);
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorPlacement") bool CanEditSensorPlacement(AVirtualSensorActorBase* Sensor,FString& Reason) const;
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") bool UndoSensorPlacement(AVirtualSensorActorBase* Sensor,FString& Reason);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") bool RedoSensorPlacement(AVirtualSensorActorBase* Sensor,FString& Reason);
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorPlacement") bool CanUndoSensorPlacement(AVirtualSensorActorBase* Sensor) const;
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorPlacement") bool CanRedoSensorPlacement(AVirtualSensorActorBase* Sensor) const;
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") FVirtualSensorPlacementSnapshot GetCurrentPlacementSnapshot(AVirtualSensorActorBase* Sensor);
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorPlacement") int64 GetLibraryRevision() const {return LibraryRevision;}
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorPlacement") FString GetLibraryMessage() const {return LibraryMessage;}
    UPROPERTY(BlueprintAssignable) FVirtualSensorPoseLibraryChanged OnPoseLibraryChanged;
    void BeginGesture(AVirtualSensorActorBase* Sensor);
    void CommitGesture(AVirtualSensorActorBase* Sensor);
    void RecordPoseChange(AVirtualSensorActorBase* Sensor,const FTransform& Before,const FTransform& After);
    bool IsPreviewRefreshPending(AVirtualSensorActorBase* Sensor) const;
    static FString CanonicalMap(UWorld* World);
    static bool SamePose(const FTransform& A,const FTransform& B);
    static constexpr int32 MaxBookmarksPerSensor=20;
    static constexpr int32 MaxHistoryPerSensor=50;
#if WITH_DEV_AUTOMATION_TESTS
    bool SetAutomationStorageRoot(const FString& Root)
    {if(PendingWrite)return false;StorageRootOverrideForTests=Root;bLoaded=false;bLoadValid=true;Bookmarks.Reset();Histories.Reset();AppliedBookmarks.Reset();LoadedMap.Reset();++LibraryRevision;return true;}
#endif
private:
    struct FCommand {FTransform Before,After;FString BeforeBookmark,AfterBookmark;};
    struct FHistory {FString SensorId;TArray<FCommand> Commands;int32 Index=0;TOptional<FTransform> GestureBefore;FString GestureBookmark;};
    struct FWriteResult {bool Success=false;FString Error;};
    struct FPendingWrite {FString Request;TArray<FVirtualSensorPoseBookmark> Candidate;TFuture<FWriteResult> Future;};
    TUniquePtr<FPendingWrite> PendingWrite;
    TArray<FVirtualSensorPoseBookmark> Bookmarks;
    TMap<FString,FVirtualSensorPoseSaveStatus> SaveStatuses;
    TArray<FString> RequestOrder;
    TMap<TWeakObjectPtr<AVirtualSensorActorBase>,FHistory> Histories;
    TMap<TWeakObjectPtr<AVirtualSensorActorBase>,FString> AppliedBookmarks;
    TMap<TWeakObjectPtr<AVirtualSensorActorBase>,int64> PreviewBaselineFrames;
    bool bLoaded=false,bLoadValid=true,bEnding=false,bApplyingHistory=false;
    int64 LibraryRevision=0;
    FString LibraryMessage,LoadedMap;
    bool EnsureLoaded();
    bool CheckSensor(const AVirtualSensorActorBase* Sensor,FString& Reason) const;
    bool CheckName(const FString& Name,const AVirtualSensorActorBase* Sensor,const FString& Except,FString& Reason) const;
    FString StoragePath() const;
    FString StartWrite(TArray<FVirtualSensorPoseBookmark> Candidate,const FString& BookmarkId);
    FString Reject(const FString& Reason);
    void FinishWrite(FWriteResult Result,bool Broadcast);
    bool ApplyPose(AVirtualSensorActorBase* Sensor,const FTransform& Pose,FString& Reason);
    FHistory& HistoryFor(AVirtualSensorActorBase* Sensor);
    void RecordCommand(AVirtualSensorActorBase* Sensor,const FTransform& Before,const FTransform& After,const FString& BeforeBookmark,const FString& AfterBookmark);
    static FWriteResult WriteLibrary(const FString& Path,const FString& Json);
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSensorPoseLibraryTest;
    friend class FSensorPoseLibraryRhiTest;
    friend class FSlabRunPlacementSnapshotTest;
    FString StorageRootOverrideForTests;
#endif
};
