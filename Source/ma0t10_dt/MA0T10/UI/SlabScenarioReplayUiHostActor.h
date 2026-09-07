#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlabScenarioReplayUiHostActor.generated.h"
class USlabScenarioReplayPanelWidget;
class UVirtualSensorPanelHostComponent;
UCLASS()
class MA0T10_DT_API ASlabScenarioReplayUiHostActor : public AActor
{
	GENERATED_BODY()
public:
	ASlabScenarioReplayUiHostActor();
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="ScenarioReplay") TSubclassOf<USlabScenarioReplayPanelWidget> ReplayWidgetClass;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="ScenarioReplay") TObjectPtr<UVirtualSensorPanelHostComponent> PanelHost;
	UPROPERTY(Transient,BlueprintReadOnly,Category="ScenarioReplay") TObjectPtr<USlabScenarioReplayPanelWidget> ReplayWidget;
	UFUNCTION(BlueprintCallable,Category="ScenarioReplay") void ShowReplayPanel();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
