#pragma once
#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "ShipHatchMonitorWidget.generated.h"
class AShipHatchSimulation;
class AShipHatchMonitorHost;
class UComboBoxString;
class UEditableTextBox;
class USpinBox;
class UTextBlock;
class UImage;
class UTexture2D;
class UCanvasPanelSlot;
class UBorder;
class UButton;
class USlider;
class UShipHatchAnalysisView;
class UInvalidationBox;
class UPrimitiveComponent;
namespace ShipHatch { struct FAnalysis; struct FSettings; }
struct FShipHatchMonitorWork;
class UShipHatchMonitorWidget;

/** Owned Slate grid; shares the monitor's immutable analysis without raster textures. */
UCLASS()
class MA0T10_DT_API UShipHatchAnalysisView : public UWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<UShipHatchMonitorWidget> Monitor;
    bool bHeatmap=false;
    bool bLegend=false;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS()
class MA0T10_DT_API UShipHatchMonitorWidget : public UDxWidget
{
    GENERATED_BODY()
    friend struct FHatchPresentationProbe;
    friend struct FHatchDigProbe;
public:
    void Bind(AShipHatchMonitorHost* InHost,AShipHatchSimulation* InSimulation);
    void Refresh();
    void SelectView(int32 View);
    void SelectColor(int32 Color);
    void SetFontPercent(int32 Percent);
    const FString& GetStatusText() const { return CachedStatus; }
    int32 GetView() const { return ActiveView; }
    int32 GetColor() const { return ActiveColor; }
    void SetSplitView(bool Enabled);
    bool IsSplitView() const { return bSplit; }
    float GetFontScale() const { return FontScale; }
    const ShipHatch::FAnalysis* GetAnalysis() const;
    const ShipHatch::FSettings* GetAnalysisSettings() const;
    const ShipHatch::FAnalysis* GetDisplayAnalysis() const;
    bool IsAnalysisPreview() const;
    FText CellTooltip(int32 Cell) const;
    void SetCameraPreset(int32 Preset);
    uint64 AnalysisBuilds=0;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseWheel(const FGeometry&,const FPointerEvent&) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;
private:
    TWeakObjectPtr<AShipHatchSimulation> Simulation;
    TWeakObjectPtr<AShipHatchMonitorHost> Host;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> ViewChoice;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> ColorChoice;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> CargoChoice;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> SensorChoice;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> FontChoice;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> SeedInput;
    UPROPERTY(Transient) TObjectPtr<USpinBox> MinFill;
    UPROPERTY(Transient) TObjectPtr<USpinBox> MaxFill;
    UPROPERTY(Transient) TObjectPtr<USpinBox> GridSize;
    UPROPERTY(Transient) TObjectPtr<USpinBox> SensorHeight;
    UPROPERTY(Transient) TObjectPtr<USpinBox> PointSize;
    UPROPERTY(Transient) TObjectPtr<USpinBox> PointStride;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatsText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LegendText;
    UPROPERTY(Transient) TObjectPtr<UImage> AnalysisImage;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> AnalysisTexture;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> ControlsSlot;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> AnalysisSlot;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TextBlocks;
    UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanelSlot>> LayoutSlots;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MetricValues;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ViewButtons;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ColorButtons;
    UPROPERTY(Transient) TObjectPtr<UBorder> SettingsPanel;
    UPROPERTY(Transient) TObjectPtr<UBorder> PointPanel;
    UPROPERTY(Transient) TObjectPtr<UShipHatchAnalysisView> TopView;
    UPROPERTY(Transient) TObjectPtr<UShipHatchAnalysisView> HeatView;
    UPROPERTY(Transient) TObjectPtr<UInvalidationBox> TopCache;
    UPROPERTY(Transient) TObjectPtr<UInvalidationBox> HeatCache;
    UPROPERTY(Transient) TObjectPtr<UInvalidationBox> PaletteCache;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ContextText;
    UPROPERTY(Transient) TObjectPtr<UBorder> StatusChip;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> UpdatedText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FillBadge;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PointSizeLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DensityLabel;
    UPROPERTY(Transient) TObjectPtr<UButton> DigToggle;
    UPROPERTY(Transient) TObjectPtr<UButton> ScanToggle;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> SensorBadges;
    UPROPERTY(Transient) TObjectPtr<UButton> UndoButton;
    UPROPERTY(Transient) TObjectPtr<UButton> RestoreButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DigAmounts;
    UPROPERTY(Transient) TObjectPtr<USpinBox> DigRadius;
    UPROPERTY(Transient) TObjectPtr<USpinBox> DigDepth;
    UPROPERTY(Transient) TObjectPtr<USlider> SizeSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> DensitySlider;
    TArray<int32> BaseFontSizes;
    TSharedPtr<FShipHatchMonitorWork> Work;
    FString CachedStatus;
    int32 ActiveView=0,ActiveColor=0;
    int32 LastSceneView=0;
    uint64 LastRevision=MAX_uint64;
    int64 LastMLX=-1,LastLivox=-1;
    bool bSync=false;
    bool bWasGenerating=false;
    bool bSplit=true,bSettings=false,bOrbit=false;
    bool bDigEnabled=false,bDragged=false;
    FVector2D PointerDown;
    double PointerDownTime=0;
    uint64 PointerRevision=0;
    double RadiusMeters=2,DepthMeters=1.5;
    uint64 LastDigRevision=MAX_uint64;
    bool bLastDigEnabled=false,bLastDigGenerating=false;
    TSet<TWeakObjectPtr<UPrimitiveComponent>> StalePointHides;
    void RestorePointVisibility();
    float FontScale=1;
    FVector2D LastLayout=FVector2D::ZeroVector;
    FSlateRect SceneRect;
    double OrbitYaw=48,OrbitPitch=-62,OrbitDistance=7800;
    void Layout();
    void UpdateCamera();
    bool SceneInputAt(const FGeometry&,FVector2D Absolute) const;
    bool DigAt(const FGeometry&,FVector2D Absolute);
    void SyncSettings();
    void UpdateDigControls();
    void UpdateSegments();
    void InvalidatePlots();
    double CellSize=1;
    void ApplyPreview();
    void RefreshAnalysis();
    void RefreshEditPreview();
    void UpdateSensorBadges();
    UFUNCTION() void OnHighlightMLX();
    UFUNCTION() void OnHighlightLivox();
    UFUNCTION() void OnScanToggle();
    UFUNCTION() void OnRandom();
    UFUNCTION() void OnApply();
    UFUNCTION() void OnTools();
    UFUNCTION() void OnExport();
    UFUNCTION() void OnView(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void OnColor(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void OnCargo(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void OnSensor(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void OnFont(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void OnGrid(float Value);
    UFUNCTION() void OnPointSize(float Value);
    UFUNCTION() void OnPointStride(float Value);
    UFUNCTION() void OnSizeSlider(float Value);
    UFUNCTION() void OnDensitySlider(float Value);
    UFUNCTION() void OnSettings();
    UFUNCTION() void OnDigToggle();
    UFUNCTION() void OnUndoDig();
    UFUNCTION() void OnRestoreCargo();
    UFUNCTION() void OnDigRadius(float Value){RadiusMeters=Value;}
    UFUNCTION() void OnDigDepth(float Value){DepthMeters=Value;}
    UFUNCTION() void OnSplit(){SetSplitView(true);}
    UFUNCTION() void OnRender(){SelectView(0);}
    UFUNCTION() void OnPoints(){SelectView(1);}
    UFUNCTION() void OnMax3d(){SelectView(ActiveView==1?1:0);SetSplitView(false);}
    UFUNCTION() void OnMaxTop(){SelectView(2);SetSplitView(false);}
    UFUNCTION() void OnMaxHeat(){SelectView(3);SetSplitView(false);}
    UFUNCTION() void OnHeight(){SelectColor(0);}
    UFUNCTION() void OnClass(){SelectColor(1);}
    UFUNCTION() void OnReflection(){SelectColor(2);}
    UFUNCTION() void OnDistance(){SelectColor(3);}
    UFUNCTION() void OnMono(){SelectColor(4);}
    UFUNCTION() void OnTop(){SetCameraPreset(0);}
    UFUNCTION() void OnOblique(){SetCameraPreset(1);}
    UFUNCTION() void OnReset(){SetCameraPreset(2);}
};
