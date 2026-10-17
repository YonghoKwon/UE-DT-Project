#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "SensorToolToolbarWidget.generated.h"
class UVirtualSensorToolWorkspaceSubsystem;
class AVirtualSensorActorBase;
class ASlabActor;
/** Native presentation routes only; not a serialized panel role or Blueprint enum. */
enum class ESensorToolDestination : uint8
{
	Placement, Preview, Scenario, PcdTransfer, Results, SensorSettings, FileSave, ServerSettings, Diagnostics, Compatibility, ScenarioCharts, MapCheck, TestSet
};
struct FSensorToolGlobalStatus
{
	FText Equipment, Pcd, Scenario, Server, Warning, Automation, Imu, TestSet;
	int32 EquipmentCount=0, MeasuringCount=0;
	bool bPcdSending=false, bServerConnected=false, bConnectionWarning=false, bPreflightWarning=false;
};
UCLASS()
class MA0T10_DT_API USensorToolWorkspaceRootWidget : public UUserWidget {GENERATED_BODY()};
UCLASS()
class MA0T10_DT_API USensorToolToolbarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Owned native navigation; does not start measurement, transmission or file work. */
	bool OpenOwnedDestination(ESensorToolDestination Destination);
	bool ToggleOwnedDestination(ESensorToolDestination Destination);
	bool IsOwnedDestinationVisible(ESensorToolDestination Destination) const;
	bool ShowOwnedToolsMenu();
	const FSensorToolGlobalStatus& GetGlobalStatus() const { return GlobalStatus; }
	float GetFloatingToolbarHeight() const;
	TSharedPtr<SWidget> GetSlabSummaryCard() const { return SlabSummaryCard; }
	/** UXR-13: an owned 이동·회전 is active, so the 3D key guide is shown. */
	bool IsOwnedManipulationActive() const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSensorVocabularyTest;
	friend class FSensorWorkspaceSelectionSyncTest;
	friend class FSensorWorkflowToolbarTest;
	friend class FSensorWorkflowStatusTest;
	friend class FUxrSidebarRender;
    friend class FUxr16Integration;
    friend class FInspectionUiFeedbackCommand;
#endif
	UVirtualSensorToolWorkspaceSubsystem* Workspace() const;
	TArray<TSharedPtr<FString>> SensorOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SensorCombo;
	void RefreshSensors();
	void SelectSensor(const FString& Id);
	FText SelectedSensorText() const;
	FSlateFontInfo ToolFont(int32 Size=13) const;
	TMap<FString,FText> OwnedSensorLabels;
	struct FEquipmentLabel { FText Name,State; FLinearColor Color=FLinearColor::Gray; };
	TMap<FString,FEquipmentLabel> EquipmentLabels;
	TSharedRef<SWidget> BuildEquipmentMenu();
	TSharedRef<SWidget> BuildManipulationGuide();
	TSharedRef<SWidget> BuildEquipmentLabel(TWeakObjectPtr<AVirtualSensorActorBase> Actor);
	FSensorToolGlobalStatus GlobalStatus;
	FText OwnedFontScaleText;
	float DisplayedFontScale=-1.f;
	double NextStatusRefresh=-1;
	double ServerDisconnectedSince=-1;
	TWeakObjectPtr<ASlabActor> StatusSlab;
	TWeakObjectPtr<ASlabActor> LabelExclusionSlab;
	double NextSlabLookup=0;
	int32 CachedSlabCount=0;
	void RefreshOwnedStatus(double Now);
	TSharedRef<SWidget> BuildToolsMenu();
	TSharedRef<SWidget> BuildStatusStrip();
    TSharedRef<SWidget> BuildActivityChips();
    void RefreshActivityChips();
    TArray<TWeakPtr<class SBox>> ActivityHosts;
    uint8 DisplayedActivityMask=255;
	TSharedRef<SWidget> BuildAppearanceMenu();
	TSharedRef<SWidget> BuildLegacyToolbar();
	TSharedRef<SWidget> BuildSidebarShell();
	bool IsSidebarRouteSelected(ESensorToolDestination Destination) const;
	TSharedRef<SWidget> BuildOwnedFloatingToolbar();
	TSharedPtr<SWidget> FloatingToolbar;
	TSharedPtr<SWidget> SlabSummaryCard;
	FText SlabCardTitle,SlabCardAngle,SlabCardOffset,SlabCardMargin;
	float SlabCardProgress=0;
};
