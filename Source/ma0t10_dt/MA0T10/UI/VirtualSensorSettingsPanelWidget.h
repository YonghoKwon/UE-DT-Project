#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementTypes.h"
#include "VirtualSensorSettingsPanelWidget.generated.h"

class AVirtualSensorCoordinator;
class AVirtualSensorActorBase;
class AVirtualSensorUiHostActor;
class AVirtualSensorTransformGizmoActor;
class STextBlock;
class USensorPlacementInteraction;

UCLASS(BlueprintType)
class MA0T10_DT_API UVirtualSensorSettingsPanelWidget : public UVirtualSensorPanelWidgetBase
{
    GENERATED_BODY()

public:
	/** Presentation-only entry for the owned native toolbar; Designer and legacy opt out. */
	void OpenOwnedSettingsSection(bool bAdvanced);
	TSharedRef<SWidget> BuildWorkspacePlacementControls();
	void RefreshWorkspacePlacementControls();
	/** Follow the workspace selection without cycling or applying edited values. Legacy panels opt out. */
	bool SynchronizeWorkspaceSelection(bool bForceRefresh=false);
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void BindSensorManager(AVirtualSensorCoordinator* InSensorManager);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void BindHostActor(AVirtualSensorUiHostActor* InHostActor);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SelectTargetKind(EVirtualSensorTargetKind InTargetKind);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SelectNextTarget();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    bool ApplyPendingState();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    bool SetSelectedSimulationQuality(EVirtualSensorSimulationQuality NewQuality);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    bool ApplySelectedProfileAndQualityPreset();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void ResetPendingStateToMapValue();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    bool QueuePendingStateForSensorTestMap();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void NudgeSelectedSensor(FVector TranslationDelta, FRotator RotationDelta);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorControl|Placement")
    void SetPlacementInputMode(EVirtualSensorPlacementInputMode Mode);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorControl|Placement") FString RequestSaveSelectedSensorPose(const FString& Name,const FString& ExistingBookmarkId=TEXT(""));
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorControl|Placement") bool ApplySelectedSensorPoseBookmark(const FString& BookmarkId);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorControl|Placement") bool UndoSelectedSensorPlacement();
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorControl|Placement") bool RedoSelectedSensorPlacement();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetSensorManipulationEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetSensorGizmoMode(EVirtualSensorGizmoMode InMode);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetSensorCoordinateSpace(EVirtualSensorCoordinateSpace InSpace);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SetProjectionDebugEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    const FVirtualSensorEditableState& GetPendingState() const { return PendingState; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    AVirtualSensorTransformGizmoActor* GetTransformGizmoActor() const { return GizmoActor; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl")
    FString GetControlStatusText() const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl|UI")
    void ResetSettingsUiPreferencesToDefault();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl|Help")
    TArray<FVirtualSensorSettingHelpDescriptor> GetAllSettingHelpDescriptors() const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl|Help")
    bool SelectSettingHelp(FName SettingKey);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl|Help")
    FString GetSelectedSettingHelpText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorControl|Performance")
    FString GetCurrentLoadSummaryText() const;

    static bool ValidateEditableStateValues(const FVirtualSensorEditableState& State, const TArray<FString>& OtherSensorIds, FString& OutError);
    static double CalculateCameraMegaPixelsPerSecond(const FVirtualSensorEditableState& State);
    static double CalculateLidarRaysPerSecond(const FVirtualSensorEditableState& State);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
    float TranslationStepCm = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl", meta = (ClampMin = "0.1", ClampMax = "90.0"))
    float RotationStepDegrees = 5.0f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DigitalTwin|SensorControl|Placement") float TranslationSpeedCmPerSecond=125;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DigitalTwin|SensorControl|Placement") float RotationSpeedDegreesPerSecond=62.5f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	USensorPlacementInteraction* OwnedInteraction() const;
	void SynchronizeInteractionState();
	TWeakObjectPtr<USensorPlacementInteraction> SharedInteraction;
	friend class FSensorWorkflowToolbarTest;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSensorV2ManipulationLifecycleTest;
    friend class FSensorPlacementIdentityUiTest;
#endif
	TSharedRef<SWidget> BuildOwnedAdministrativeControls();
	TSharedRef<SWidget> BuildOwnedSettingsWidget();
	void RefreshOwnedSettingsText();
	void OpenOwnedSettingHelp(FName Key);
	FText OwnedSelectionText,OwnedProfileText,OwnedQualityText,OwnedHelpText,OwnedLoadText;
	FText OwnedStatusText;
	FText OwnedDebugLogText,OwnedIdentityText;
	bool bOwnedCanEdit=false;
	double NextOwnedSettingsRefresh=0;
	TSharedPtr<class SScrollBox> OwnedSettingsScroll;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSensorSettingsPresentationTest;
	friend class FSensorVocabularyTest;
	friend class FUxrSettingsRender;
#endif
	bool bOwnedSettingHelpOpen=false;
	int32 WorkspaceSettingsTab=0;
    bool ReadSelectedSensorState(FVirtualSensorEditableState& OutState) const;
    bool ApplyStateToRuntime(const FVirtualSensorEditableState& State, FString& OutError);
	bool ApplyPendingTransform();
    bool ValidateState(const FVirtualSensorEditableState& State, FString& OutError) const;
    void RefreshPendingState(bool bCaptureInitialValue);
    void RefreshNativeText();
    void SpawnGizmoIfNeeded();
    void SyncGizmoTarget();
    AActor* GetSelectedSensorActor() const;
    void HandleGizmoTransformChanged(const FTransform& Transform);
    void HandleGizmoGestureStarted();
    void RefreshPoseRows();
    void SelectPoseBookmark(const FString& BookmarkId);
    TSharedRef<SWidget> BuildPoseBookmarkControls();
    void HandleGizmoTransformCommitted(const FTransform& Transform);
    void HandleManipulationExitRequested();
    void FinishSensorManipulation(bool bRestoreMonitorView);
    void RefreshSelectedSensorNow(bool bForce);
    void BeginMonitorFollowForManipulation();
    void EndMonitorFollowForManipulation();
    void StartAllRealSensorSources();
    void StopAllRealSensorSources();
    void PushSelectedRealSensorSource();
    void RestoreSettingsUiPreferences();
    void SaveSettingsUiPreferences() const;
    FString BuildDeviceSpecText() const;
    FName ResolvePersistentActorTag(const AActor* Actor) const;
    const FVirtualSensorSettingHelpDescriptor* FindSettingHelp(FName SettingKey) const;
    TSharedRef<SWidget> MakeFloatRow(const FText& Label, TFunction<float()> Getter, TFunction<void(float)> Setter, float Min, float Max, bool bApplyOnCommit = true, FName HelpKey = NAME_None, bool bTransformOnly = false);
    TSharedRef<SWidget> MakeIntRow(const FText& Label, TFunction<int32()> Getter, TFunction<void(int32)> Setter, int32 Min, int32 Max, FName HelpKey = NAME_None);

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorCoordinator> SensorManager;

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorUiHostActor> HostActor;

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorTransformGizmoActor> GizmoActor;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorControl", meta = (AllowPrivateAccess = "true"))
    FVirtualSensorEditableState PendingState;

    TMap<FName, FVirtualSensorEditableState> InitialStates;
    TArray<TSharedPtr<EVirtualSensorSimulationQuality>> NativeQualityOptions;
    TArray<TSharedPtr<EVirtualCameraDeviceProfile>> NativeCameraProfileOptions;
    TArray<TSharedPtr<EVirtualLidarDeviceProfile>> NativeLidarProfileOptions;
    TSharedPtr<STextBlock> NativeStatusText;
    TSharedPtr<SEditableTextBox> PoseNameInput;
    TArray<FVirtualSensorPoseBookmark> PoseRows;
    FString PoseName,SelectedPoseId,PendingPoseDeleteId,LastPoseRequest;
    uint64 PoseInteractionRevision=0,PoseRequestRevision=0;
    TWeakObjectPtr<AVirtualSensorActorBase> PoseRowsSensor;
    FString PoseRowsSensorId;
    EVirtualSensorKind PoseRowsKind=EVirtualSensorKind::Camera;
    int64 PoseRowsRevision=-1;
    double NextPoseRefresh=0;
    bool bPoseEditable=false,bPoseUndoAvailable=false,bPoseRedoAvailable=false;
    FString PoseLockReason;
    FVirtualSensorPlacementPreviewStatus PlacementPreviewStatus;
    bool bShowAdvanced = false;
    bool bKeyboardHelpExpanded = false;
    bool bGizmoVisible = true;
    bool bManipulationEnabled = false;
    bool bMonitorAutoFollowingManipulation = false;
    bool bRestoreMonitorViewAfterManipulation = false;
    uint8 MonitorViewBeforeManipulation = 0;
    uint8 AutoFollowMonitorView = 0;
    bool bProjectionDebugEnabled = false;
    EVirtualSensorGizmoMode GizmoMode = EVirtualSensorGizmoMode::Translate;
    EVirtualSensorCoordinateSpace CoordinateSpace = EVirtualSensorCoordinateSpace::Local;
    double LastPreviewRefreshTime = -1.0;
	FVirtualSensorInteractionRequest InteractionRequest;
    EVirtualSensorPlacementInputMode PlacementInputMode=EVirtualSensorPlacementInputMode::Continuous;
    TWeakObjectPtr<AActor> LastSyncedSensorActor;
    TWeakObjectPtr<AVirtualSensorActorBase> ManipulationTarget;
    FString LastControlMessage = TEXT("센서를 선택하고 PIE 실행 값을 조정하세요.");
    FName SelectedSettingHelpKey = NAME_None;
};
