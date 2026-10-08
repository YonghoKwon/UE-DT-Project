#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "VirtualSensorPlacementPanelWidget.generated.h"
class UVirtualSensorSettingsPanelWidget;
class AVirtualSensorActorBase;
class UVirtualSensorLayoutSubsystem;
template<typename T> class SComboBox;
UCLASS(BlueprintType)
class MA0T10_DT_API UVirtualSensorPlacementPanelWidget : public UVirtualSensorPanelWidgetBase
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") void BindPlacementControls(UVirtualSensorSettingsPanelWidget* Controls);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") void EndPlacement();
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") bool AddSensor(EVirtualSensorKind Kind);
    UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorPlacement") bool DeleteSelectedSensor();
    UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorPlacement") FString GetPlacementStatusText() const {return Status;}
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    virtual void NativeDestruct() override;
private:
    UPROPERTY(Transient) TObjectPtr<UVirtualSensorSettingsPanelWidget> Controls;
    struct FOption {TWeakObjectPtr<AVirtualSensorActorBase> Actor;FString Label;};
    TArray<TSharedPtr<FOption>> SensorOptions;
    TArray<TSharedPtr<FString>> LayoutOptions;
    TSharedPtr<SComboBox<TSharedPtr<FOption>>> SensorCombo;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> LayoutCombo;
    FString LayoutName,SelectedLayout,OperationId,Status=TEXT("장비를 추가하거나 저장 위치를 선택하세요. PCD 송신은 별도입니다.");
    TWeakObjectPtr<AVirtualSensorActorBase> PendingDelete;
    int64 InventoryRevision=-1;
    double NextRefresh=0;
    UVirtualSensorLayoutSubsystem* Layout() const;
    AVirtualSensorActorBase* Selected() const;
    void RefreshInventory();
    void RefreshLayouts();
    FText SelectedText() const;
};
