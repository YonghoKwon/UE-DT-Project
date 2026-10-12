#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementTypes.h"
#include "VirtualSensorPlacementPanelWidget.generated.h"
class UVirtualSensorSettingsPanelWidget;
class AVirtualSensorActorBase;
class UVirtualSensorLayoutSubsystem;
class USensorPlacementInteraction;
template<typename T> class SListView;
template<typename T> class SComboBox;
UCLASS(BlueprintType)
class MA0T10_DT_API UVirtualSensorPlacementPanelWidget : public UVirtualSensorPanelWidgetBase
{
    GENERATED_BODY()
public:
    bool SelectEquipment(AVirtualSensorActorBase* Actor);
    bool SetSelectedMeasuring(bool Measuring);
    bool SetSelectedManipulating(bool Enabled);
    FString SaveCurrentPose(const FString& Name);
    bool MoveToBookmark(const FString& Id);
    FString SaveWholeLayout(const FString& Name,bool Overwrite=false);
    void CancelSelectedSensorDeletion();
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") void BindPlacementControls(UVirtualSensorSettingsPanelWidget* Controls);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") void EndPlacement();
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") bool AddSensor(EVirtualSensorKind Kind);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") bool DeleteSelectedSensor();
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorPlacement") FString GetPlacementStatusText() const {return Status;}
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") void SelectLayoutForRestore(const FString& Name);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") FString PreviewSelectedLayoutRestore();
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") bool ApplySelectedLayoutRestore();
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") void CancelLayoutOperation();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    virtual void NativeDestruct() override;
private:
    UPROPERTY(Transient) TObjectPtr<UVirtualSensorSettingsPanelWidget> Controls;
    struct FOption {TWeakObjectPtr<AVirtualSensorActorBase> Actor;FString Label;FText NameText,OriginText,StateText;FLinearColor StateColor=FLinearColor::Gray;};
    TArray<TSharedPtr<FOption>> SensorOptions;
    TArray<TSharedPtr<FString>> LayoutOptions;
    TSharedPtr<SComboBox<TSharedPtr<FOption>>> SensorCombo;
    TSharedPtr<SListView<TSharedPtr<FOption>>> SensorList;
    TSharedPtr<class SVerticalBox> PoseList;
    TWeakObjectPtr<AVirtualSensorActorBase> PoseSensor;
    FString PoseSensorId,PoseName,SelectedPoseId,PoseRequest,PoseDeleteId;
    int64 PoseRevision=-1;
    uint64 PoseSelectionRevision=0,PoseRequestRevision=0;
    FString PlacementLock,DeleteReason,EquipmentCountText;
    FString CachedLockSource,CachedDeleteSource;
    FText CachedSelectedText,CachedCountText,CachedLockText,CachedDeleteText,CachedStatusText;
    FText CachedPreviewText;
    FVirtualSensorPlacementPreviewStatus LastPreview;
    bool bPreviousManipulating=false;
    bool bSelectedMeasuring=false;
    FText CachedMeasureCaption,CachedManipulationCaption;
    FString PreviousStatus;
    mutable TWeakObjectPtr<class AVirtualSensorCoordinator> CachedCoordinator;
    mutable double NextCoordinatorLookup=0;
    bool bCanMove=false,bCanDelete=false;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> LayoutCombo;
    FString LayoutName,SelectedLayout,OperationId,Status=TEXT("장비를 선택하거나 추가한 뒤 배치하세요.");
    TWeakObjectPtr<AVirtualSensorActorBase> PendingDelete;
    int64 InventoryRevision=-1;
    double NextRefresh=0;
    UVirtualSensorLayoutSubsystem* Layout() const;
    AVirtualSensorActorBase* Selected() const;
    void RefreshInventory();
    void RefreshLayouts();
    FText SelectedText() const;
    USensorPlacementInteraction* Interaction() const;
    void RefreshPoses();
    TSharedRef<SWidget> BuildEquipmentSection();
    TSharedRef<SWidget> BuildSelectedSection();
    TSharedRef<SWidget> BuildPoseSection();
    TSharedRef<SWidget> BuildLayoutSaveMenu();
    TSharedRef<SWidget> BuildLayoutSection();
};
