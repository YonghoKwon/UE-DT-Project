#pragma once
#include "CoreMinimal.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "SlabScenarioReplayPanelWidget.generated.h"
class USlabScenarioReplaySubsystem;
class SVerticalBox;
UCLASS(BlueprintType)
class MA0T10_DT_API USlabScenarioReplayPanelWidget : public UVirtualSensorPanelWidgetBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool SelectScenario(const FString& UUID);
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|ScenarioReplay") bool ReplaySelected();
	UFUNCTION(BlueprintPure,Category="DigitalTwin|ScenarioReplay") FString GetSelectedScenarioUUID() const { return SelectedUUID; }
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DigitalTwin|ScenarioReplay") bool bSendPcd=false;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DigitalTwin|ScenarioReplay") TArray<FString> TargetSensorIds;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
	UFUNCTION() void RefreshList();
	USlabScenarioReplaySubsystem* Manager() const;
	FString SelectedUUID;
	TSharedPtr<SVerticalBox> List;
};
