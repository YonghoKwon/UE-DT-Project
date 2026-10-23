#pragma once
#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "GameFramework/Actor.h"
#include "Components/Button.h"
#include "CoilLoadingMonitor.generated.h"
class ACoilLoadingSimulation;
class ACoilLoadingMonitorHost;
class AVirtualSensorCoordinator;
class AVirtualSensorUiHostActor;
class UTextBlock;
class USpinBox;
class UEditableTextBox;
class UCheckBox;
class UComboBoxString;
class UBorder;
class UVerticalBox;
class UCanvasPanel;
class UCanvasPanelSlot;
class ACameraActor;
class FCoilLoadingViewExtension;
class UCoilLoadingMonitor;

UCLASS()
class MA0T10_DT_API UCoilLoadingWarningButton : public UButton
{
    GENERATED_BODY()
public:
    void Bind(ACoilLoadingSimulation* Simulation,const TArray<int32>& CoilIndices);
private:
    TWeakObjectPtr<ACoilLoadingSimulation> Sim;
    TArray<int32> Indices;
    UFUNCTION() void Highlight();
};

UCLASS()
class MA0T10_DT_API UCoilLoadingPlanView : public UWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<UCoilLoadingMonitor> Monitor;
    bool PickAt(FVector2D Point,FVector2D Size);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
UCLASS()
class MA0T10_DT_API UCoilLoadingMonitor : public UDxWidget
{
    GENERATED_BODY()
public:
    void Bind(ACoilLoadingMonitorHost* H,ACoilLoadingSimulation* S);
    void Refresh();
    void SelectView(int32 View);
    void SyncInputs();
    void LoadMixedExample();
    UCoilLoadingPlanView* PlanCanvas() const { return PlanView; }
    ACoilLoadingSimulation* Simulation() const { return Sim.Get(); }
    FString WarningText() const { return CachedWarnings; }
    FSlateRect SceneRect() const { return ViewRect; }
    bool ApplyInputs();
    float FontScale() const { return TextScale; }
    void SetFontPercent(int32 Percent);
    int32 DangerCount=0,CautionCount=0,DataNeededCount=0;
    bool DataExpanded() const;
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
private:
    TWeakObjectPtr<ACoilLoadingMonitorHost> Host;
    TWeakObjectPtr<ACoilLoadingSimulation> Sim;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Badge;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Metrics;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> WarningRows;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> DataRows;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DataNeededLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Comparison;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Operation;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Details;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> IdInput;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> FileInput;
    UPROPERTY(Transient) TArray<TObjectPtr<USpinBox>> Numbers;
    UPROPERTY(Transient) TArray<TObjectPtr<UCheckBox>> Flags;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> InnerChoice;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> GearChoice;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> MotionChoice;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> TypeChoice;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> RowDirection;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> SceneInputSlot;
    UPROPERTY(Transient) TObjectPtr<UButton> RemoveButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SelectionText;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ComboLabels;
    UFUNCTION() UWidget* ComboEntry(FString Value);
    float TextScale=1;
    UPROPERTY(Transient) TObjectPtr<UCoilLoadingPlanView> PlanView;
    UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanelSlot>> Slots;
    UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanelSlot>> SceneMasks;
    UPROPERTY(Transient) TObjectPtr<UBorder> PlotCard;
    uint64 LastRevision=MAX_uint64;
    int32 LastPlanCount=INDEX_NONE;
    FString CachedWarnings;
    FString LastShownPlanPath;
    FSlateRect ViewRect;
    bool bSync=false;
    UFUNCTION() void OnApply();
    UFUNCTION() void OnPlace();
    UFUNCTION() void OnPlanOnly();
    UFUNCTION() void OnTier();
    UFUNCTION() void OnRemove();
    UFUNCTION() void OnFontSize();
    UFUNCTION() void OnRowDirection(FString Value,ESelectInfo::Type Selection);
    UFUNCTION() void OnUndo();
    UFUNCTION() void OnCancel();
    UFUNCTION() void OnMotion();
    UFUNCTION() void OnSave();
    UFUNCTION() void OnLoad();
    UFUNCTION() void OnSaveSlot();
    UFUNCTION() void OnLoadSlot();
    UFUNCTION() void OnImport();
    UFUNCTION() void OnMixedExample();
    UFUNCTION() void OnDetails();
    UFUNCTION() void OnDataNeeded();
    UFUNCTION() void OnTypeChanged(FString Value,ESelectInfo::Type Selection);
    UFUNCTION() void OnOverview();
    UFUNCTION() void OnInterior();
    UFUNCTION() void OnSection();
    UFUNCTION() void OnTop();
    UFUNCTION() void OnPoints();
    UFUNCTION() void OnTools();
    UFUNCTION() void OnCraneClose();
    UFUNCTION() void OnCoilClose();
    UFUNCTION() void OnShed();
};
UCLASS()
class MA0T10_DT_API ACoilLoadingMonitorHost : public AActor
{
    GENERATED_BODY()
public:
    ACoilLoadingMonitorHost();
    UPROPERTY(EditAnywhere,Category="Coil") TObjectPtr<ACoilLoadingSimulation> Simulation;
    UPROPERTY(EditAnywhere,Category="Coil") TObjectPtr<ACameraActor> ViewCamera;
    UPROPERTY(Transient) TObjectPtr<UCoilLoadingMonitor> Monitor;
    UPROPERTY(Transient) TObjectPtr<AVirtualSensorCoordinator> Coordinator;
    UPROPERTY(Transient) TObjectPtr<AVirtualSensorUiHostActor> Tools;
    void SetSceneRect(FSlateRect Rect,FVector2D Full,bool Visible);
    void SetView(int32 View);
    void ShowTools();
    FVector2D GetToolsInset() const;
    void Focus(int32 Preset);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Refresh();
    FTimerHandle Timer;
    TSharedPtr<FCoilLoadingViewExtension,ESPMode::ThreadSafe> Extension;
    TWeakObjectPtr<AActor> PreviousView;
    bool bPreviousCursor=false;
    bool bPreviousDebugText=true;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenByView;
};
