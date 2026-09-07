#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SensorToolToolbarWidget.generated.h"
class UVirtualSensorToolWorkspaceSubsystem;
UCLASS()
class MA0T10_DT_API USensorToolWorkspaceRootWidget : public UUserWidget {GENERATED_BODY()};
UCLASS()
class MA0T10_DT_API USensorToolToolbarWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UVirtualSensorToolWorkspaceSubsystem* Workspace() const;
	TArray<TSharedPtr<FString>> SensorOptions;
	void RefreshSensors();
	void SelectSensor(const FString& Id);
	FText SelectedSensorText() const;
};
