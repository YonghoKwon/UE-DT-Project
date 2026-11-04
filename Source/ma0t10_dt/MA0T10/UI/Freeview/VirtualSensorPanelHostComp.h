#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualSensorPanelHostComp.generated.h"

class UCanvasPanel;
class UDxWidget;
class UDxWidgetVirtualSensorPanelBase;

USTRUCT()
struct FVirtualSensorHostedPanel
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UDxWidgetVirtualSensorPanelBase> Panel;

	UPROPERTY()
	int32 DefaultZOrder = 0;
};

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualSensorPanelHostComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualSensorPanelHostComp();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	void RegisterPanel(UDxWidgetVirtualSensorPanelBase* Panel, int32 DefaultZOrder);
	void UnregisterAllPanels();
	void RefreshHosting();
	void BringPanelToFront(UDxWidgetVirtualSensorPanelBase* Panel);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorUI")
	int32 GetHostedPanelCount() const { return HostedPanels.Num(); }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorUI")
	bool IsHostingInMainCanvas() const { return CurrentMainCanvas != nullptr; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorUI")
	bool bPreferMainWidgetPanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorUI")
	bool bAllowViewportFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorUI", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MainWidgetPollInterval = 0.25f;

private:
	UCanvasPanel* ResolveMainCanvas() const;
	void AttachPanel(FVirtualSensorHostedPanel& Entry, UCanvasPanel* TargetCanvas);

	UPROPERTY(Transient)
	TArray<FVirtualSensorHostedPanel> HostedPanels;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CurrentMainCanvas;

	FTimerHandle MainWidgetPollTimer;
	int32 FrontZOrder = 100;
};
