#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorDraggableWidgetBase.h"
#include "VirtualSensorSettingsWidget.generated.h"

class AVirtualSensorManager;
class AVirtualSensorMonitorHostActor;
class AVirtualSensorTransformGizmoActor;
class STextBlock;

UCLASS(BlueprintType)
class MA0T10_DT_API UVirtualSensorSettingsWidget : public UVirtualSensorDraggableWidgetBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void BindSensorManager(AVirtualSensorManager* InSensorManager);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void BindHostActor(AVirtualSensorMonitorHostActor* InHostActor);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SelectTargetKind(EVirtualSensorTargetKind InTargetKind);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void SelectNextTarget();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    bool ApplyPendingState();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void ResetPendingStateToMapValue();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    bool QueuePendingStateForSensorTestMap();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl")
    void NudgeSelectedSensor(FVector TranslationDelta, FRotator RotationDelta);

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
    FString GetControlStatusText() const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorControl|UI")
    void ResetSettingsUiPreferencesToDefault();

    static bool ValidateEditableStateValues(const FVirtualSensorEditableState& State, const TArray<FString>& OtherSensorIds, FString& OutError);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
    float TranslationStepCm = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorControl", meta = (ClampMin = "0.1", ClampMax = "90.0"))
    float RotationStepDegrees = 5.0f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    bool ReadSelectedSensorState(FVirtualSensorEditableState& OutState) const;
    void ApplySelectedProfileAndQualityPreset();
    bool ApplyStateToRuntime(const FVirtualSensorEditableState& State, FString& OutError);
    bool ValidateState(const FVirtualSensorEditableState& State, FString& OutError) const;
    void RefreshPendingState(bool bCaptureInitialValue);
    void RefreshNativeText();
    void SpawnGizmoIfNeeded();
    void SyncGizmoTarget();
    AActor* GetSelectedSensorActor() const;
    void HandleGizmoTransformChanged(const FTransform& Transform);
    void HandleGizmoTransformCommitted(const FTransform& Transform);
    void RefreshSelectedSensorNow(bool bForce);
    void StartAllRealSensorSources();
    void StopAllRealSensorSources();
    void PushSelectedRealSensorSource();
    void RestoreSettingsUiPreferences();
    void SaveSettingsUiPreferences() const;
    FString BuildDeviceSpecText() const;
    FName ResolvePersistentActorTag(const AActor* Actor) const;
    TSharedRef<SWidget> MakeFloatRow(const FText& Label, TFunction<float()> Getter, TFunction<void(float)> Setter, float Min, float Max, bool bApplyOnCommit = true);
    TSharedRef<SWidget> MakeIntRow(const FText& Label, TFunction<int32()> Getter, TFunction<void(int32)> Setter, int32 Min, int32 Max);

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorManager> SensorManager;

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorMonitorHostActor> HostActor;

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorTransformGizmoActor> GizmoActor;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorControl", meta = (AllowPrivateAccess = "true"))
    FVirtualSensorEditableState PendingState;

    TMap<FName, FVirtualSensorEditableState> InitialStates;
    TSharedPtr<STextBlock> NativeStatusText;
    bool bShowAdvanced = false;
    bool bKeyboardHelpExpanded = false;
    bool bGizmoVisible = true;
    bool bManipulationEnabled = false;
    bool bProjectionDebugEnabled = false;
    EVirtualSensorGizmoMode GizmoMode = EVirtualSensorGizmoMode::Translate;
    EVirtualSensorCoordinateSpace CoordinateSpace = EVirtualSensorCoordinateSpace::Local;
    double LastPreviewRefreshTime = -1.0;
    TWeakObjectPtr<AActor> LastSyncedSensorActor;
    FString LastControlMessage = TEXT("센서를 선택하고 PIE 실행 값을 조정하세요.");
};
