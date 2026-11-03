#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlabSimulationUiHostActor.generated.h"
class ASlabActor;
class UDxWidgetSlabChartsPanel;
class UDxWidgetSlabProgressPanel;
class UVirtualSensorPanelHostComp;

/** Explicit opt-in owner; does not discover or change unrelated widgets. */
UCLASS(BlueprintType)
class MA0T10_DT_API ASlabSimulationUiHostActor : public AActor
{
	GENERATED_BODY()
public:
	ASlabSimulationUiHostActor();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slab|UI") TObjectPtr<ASlabActor> SlabActor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slab|UI") TSubclassOf<UDxWidgetSlabChartsPanel> ChartsWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slab|UI") TSubclassOf<UDxWidgetSlabProgressPanel> ProgressWidgetClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Slab|UI") TObjectPtr<UVirtualSensorPanelHostComp> PanelHostComp;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Slab|UI") TObjectPtr<UDxWidgetSlabChartsPanel> ChartsWidget;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Slab|UI") TObjectPtr<UDxWidgetSlabProgressPanel> ProgressWidget;
	UFUNCTION(BlueprintCallable, Category="Slab|UI") void ShowSimulationPanels();
	UFUNCTION(BlueprintCallable, Category="Slab|UI") void BindSlabActor(ASlabActor* InSlab);
	UFUNCTION(BlueprintPure, Category="Slab|UI") FString GetInitializationMessage() const { return InitializationMessage; }
protected:
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostActorCreated() override;
	virtual void Destroyed() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void RegisterSlabUiHost(bool bActive=false);
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSlabUiHostRetryTest;
#endif
	FTimerHandle InitializationRetry;
	FString InitializationMessage;
	int32 InitializationAttempts=0;
	bool bEnding=false;
};
