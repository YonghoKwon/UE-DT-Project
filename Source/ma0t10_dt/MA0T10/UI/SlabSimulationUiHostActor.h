#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlabSimulationUiHostActor.generated.h"
class ASlabActor;
class USlabChartsPanelWidget;
class USlabProgressPanelWidget;
class UVirtualSensorPanelHostComponent;

/** Explicit opt-in owner; does not discover or change unrelated widgets. */
UCLASS(BlueprintType)
class MA0T10_DT_API ASlabSimulationUiHostActor : public AActor
{
	GENERATED_BODY()
public:
	ASlabSimulationUiHostActor();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slab|UI") TObjectPtr<ASlabActor> SlabActor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slab|UI") TSubclassOf<USlabChartsPanelWidget> ChartsWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slab|UI") TSubclassOf<USlabProgressPanelWidget> ProgressWidgetClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Slab|UI") TObjectPtr<UVirtualSensorPanelHostComponent> PanelHost;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Slab|UI") TObjectPtr<USlabChartsPanelWidget> ChartsWidget;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Slab|UI") TObjectPtr<USlabProgressPanelWidget> ProgressWidget;
	UFUNCTION(BlueprintCallable, Category="Slab|UI") void ShowSimulationPanels();
	UFUNCTION(BlueprintCallable, Category="Slab|UI") void BindSlabActor(ASlabActor* InSlab);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
