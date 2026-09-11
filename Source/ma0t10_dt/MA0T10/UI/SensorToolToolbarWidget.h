#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Input/SComboBox.h"
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
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSensorWorkspaceSelectionSyncTest;
#endif
	UVirtualSensorToolWorkspaceSubsystem* Workspace() const;
	TArray<TSharedPtr<FString>> SensorOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SensorCombo;
	void RefreshSensors();
	void SelectSensor(const FString& Id);
	FText SelectedSensorText() const;
};
