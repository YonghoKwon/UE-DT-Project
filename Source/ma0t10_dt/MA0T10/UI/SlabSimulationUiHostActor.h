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
	UFUNCTION(BlueprintPure, Category="Slab|UI") FString GetInitializationMessage() const { return InitializationMessage; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSlabUiHostRetryTest;
#endif
	FTimerHandle InitializationRetry;
	FString InitializationMessage;
	int32 InitializationAttempts=0;
	bool bEnding=false;
};
