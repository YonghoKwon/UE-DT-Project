#pragma once
#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "Components/VerticalBox.h"
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
class UWidgetSwitcher;
class UShipHatchOverlay;
namespace ShipHatch { struct FAnalysis; struct FSettings; struct FSurface; struct FSectionSample; }
struct FShipHatchMonitorWork;
class UShipHatchMonitorWidget;
class UScrollBox;

/** A local details group. Explicit UMG visibility keeps collapsed contents
 * out of layout, input, accessibility and presentation refresh work. */
UCLASS(NotBlueprintable)
class MA0T10_DT_API UShipHatchDetailsSection : public UVerticalBox
{
    GENERATED_BODY()
public:
    void InitializeSection(UButton* Header,UVerticalBox* Content,bool Expanded);
private:
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Body;
    UFUNCTION() void Toggle();
};

/** Owned Slate grid; shares the monitor's immutable analysis without raster textures. */
UCLASS()
class MA0T10_DT_API UShipHatchAnalysisView : public UWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<UShipHatchMonitorWidget> Monitor;
    bool bHeatmap=false;
    bool bLegend=false;
    bool bSection=false;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** Owned, input-transparent Slate links; never a world/capture component. */
UCLASS(Transient,NotBlueprintable)
class MA0T10_DT_API UShipHatchSensorLabelView : public UWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<UShipHatchMonitorWidget> Monitor;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS()
class MA0T10_DT_API UShipHatchMonitorWidget : public UDxWidget
{
    GENERATED_BODY()
    friend struct FHatchPresentationProbe;
    friend struct FHatchDigProbe;
    friend struct FHatchFeedbackProbe;
    friend struct FHatchOrientationProbe;
    friend struct FHatchLabelProbe;
    friend struct FHatch06Probe;
    friend struct FHatchCabProbe;
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
    void SetMapHeat(bool Heat);
    bool IsMapHeat() const { return bMapHeat; }
    void SetSection(bool AlongLength,double Normalized);
    bool SectionAlongLength() const { return bSectionLength; }
    double SectionPosition() const { return SectionNormalized; }
    double SectionOffset() const;
    const ShipHatch::FSurface* GetSurface() const;
    const TArray<ShipHatch::FSectionSample>& GetSectionSamples() const;
    bool CameraInHold(FVector& Origin,FVector& Direction,double& Fov) const;
    void FocusAt(FVector2D HoldMeters);
    bool GetSensorBadgeLink(int32 Index,FVector2D& Anchor,FVector2D& End) const;
    FSlateRect OverlayExclusionRect() const;
    bool IsOverlayAnchorVisible(FVector2D Point) const;
    void SetModelPresentation(bool Clean);
    bool IsModelPresentation() const { return bModelPresentation; }
    FSlateRect GetSceneRect() const { return SceneRect; }
    const AShipHatchSimulation* GetSimulation() const { return Simulation.Get(); }
    bool ShowCoverage() const { return bCoverageOverlay; }
    bool IsSensorColoring() const { return bSensorColors; }
    void SetHatchPresentationVisible(bool Visible);
    const TArray<uint8>& GetCoverageMask() const;
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
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CoverageText;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> QuickToolScroll;
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
    UPROPERTY(Transient) TObjectPtr<UButton> PointControlsToggle;
    UPROPERTY(Transient) TObjectPtr<UShipHatchAnalysisView> TopView;
    UPROPERTY(Transient) TObjectPtr<UShipHatchAnalysisView> HeatView;
    UPROPERTY(Transient) TObjectPtr<UInvalidationBox> TopCache;
    UPROPERTY(Transient) TObjectPtr<UInvalidationBox> HeatCache;
    UPROPERTY(Transient) TObjectPtr<UInvalidationBox> PaletteCache;
    UPROPERTY(Transient) TObjectPtr<UShipHatchAnalysisView> SectionView;
    UPROPERTY(Transient) TObjectPtr<UInvalidationBox> SectionCache;
    UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> MapSwitcher;
    UPROPERTY(Transient) TObjectPtr<UButton> MapTopButton;
    UPROPERTY(Transient) TObjectPtr<UButton> MapHeatButton;
    UPROPERTY(Transient) TObjectPtr<UButton> SectionLengthButton;
    UPROPERTY(Transient) TObjectPtr<UButton> SectionWidthButton;
    UPROPERTY(Transient) TObjectPtr<USlider> SectionSlider;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SectionLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ModeLabel;
    UPROPERTY(Transient) TObjectPtr<USlider> AzimuthSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> ElevationSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> IntensitySlider;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LightLabel;
    UPROPERTY(Transient) TObjectPtr<UButton> FillToggle;
    UPROPERTY(Transient) TArray<TObjectPtr<UComboBoxString>> FieldChoices;
    UPROPERTY(Transient) TArray<TObjectPtr<USpinBox>> UnloaderInputs;
    UPROPERTY(Transient) TArray<TObjectPtr<USpinBox>> SensorPoseInputs;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> DirectionBadges;
    bool bGrabAnimation=true;
    bool bSensorColors=false,bCoverageOverlay=false;
    UPROPERTY(Transient) TObjectPtr<UShipHatchOverlay> Overlay;
    // STextBlock retains a raw pointer to UTextBlock::StrikeBrush. Keep the
    // generated UObject alive for exactly as long as its native row survives.
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ComboLabels;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ContextText;
    UPROPERTY(Transient) TObjectPtr<UBorder> StatusChip;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> UpdatedText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FillBadge;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PointSizeLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DensityLabel;
    UPROPERTY(Transient) TObjectPtr<UButton> DigToggle;
    UPROPERTY(Transient) TObjectPtr<UButton> GrabDisplayToggle;
    UPROPERTY(Transient) TObjectPtr<UButton> ScanToggle;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> SensorBadges;
    UPROPERTY(Transient) TObjectPtr<UShipHatchSensorLabelView> SensorLabelView;
    FVector2D SensorBadgeAnchors[3],SensorBadgeEnds[3];
    bool SensorBadgeLinkVisible[3]={false,false,false};
    double LastSensorBadgeUpdate=-1;
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
    int64 LastCabRight=-1;
    bool bSync=false;
    bool bWasGenerating=false;
    bool bSplit=true,bSettings=false,bOrbit=false;
    bool bFrameUnloader=false,bFrameVessel=false,bFrameSite=false;
    bool bPointControlsExpanded=false,bPresetHidesPointControls=false,bModelPresentation=false,bHudSuppressed=false,bSavedModelRays=false;
    bool bDigEnabled=false,bDragged=false;
    bool bRightOrbit=false,bMapHeat=true,bSectionLength=true;
    double SectionNormalized=.5,FeedbackUntil=0,DischargeUntil=0;
    uint64 FeedbackRevision=0;
    TWeakObjectPtr<UPrimitiveComponent> FeedbackGrabHidden;
    bool bFeedbackOwnsGrabHide=false;
    bool bPendingCabDig=false;
    FVector PendingDigOrigin,PendingDigDirection;
    uint32 PendingDigCargoHash=0;
    uint64 PendingDigCabCommand=0;
    double PendingDigRadius=0,PendingDigDepth=0;
    FVector FeedbackCenter=FVector::ZeroVector;
    FVector2D Focus=FVector2D::ZeroVector;
    bool bOverlayDirty=true,bHadFeedback=false;
    void UpdateSection();
    void UpdateOverlay();
    void UpdateOverlayControlVisibility();
    UFUNCTION() void OnPointControlsToggle();
    void RestoreFeedbackGrab();
    void ApplyLighting();
    void UpdateLightingLabel();
    void UpdateSectionLabel();
    double LastSectionLabel=-1;
    bool bSectionLabelPending=true;
    double LastLightingLabel=-1,LastPlotInvalidation=-1,LastOverlayUpdate=-1;
    bool bPlotsPending=false;
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
    bool ExecuteDigRay(FVector Origin,FVector Direction);
    bool QueueOrExecuteDigRay(FVector Origin,FVector Direction);
    UFUNCTION() void OnCabCenter();
    UFUNCTION() void OnCabStop();
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
    UFUNCTION() void OnHighlightRight();
    UFUNCTION() void OnFieldA(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void OnFieldB(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void OnFieldC(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void OnUnloaderToggle();
    UFUNCTION() void OnUnloaderApply();
    UFUNCTION() void OnSensorColors();
    UFUNCTION() void OnCoverage();
    UFUNCTION() void OnSensorPose();
    UFUNCTION() void OnGrabToggle(){bGrabAnimation=!bGrabAnimation;if(!bGrabAnimation){FeedbackUntil=DischargeUntil=0;RestoreFeedbackGrab();bOverlayDirty=true;UpdateOverlay();}}
    UFUNCTION() void OnGrabDisplayToggle();
    UFUNCTION() void OnGrabAspect(float V){if(Simulation.IsValid())SetGrabAspect(V);}
    void SetGrabAspect(float Value);
    UFUNCTION() void OnSea(){OrbitYaw=-90;OrbitPitch=-55;Focus=FVector2D::ZeroVector;UpdateCamera();}
    UFUNCTION() void OnLand(){OrbitYaw=90;OrbitPitch=-55;Focus=FVector2D::ZeroVector;UpdateCamera();}
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
    UFUNCTION() UWidget* ComboEntry(FString Value);
    UFUNCTION() void OnSectionPosition(float V){SetSection(bSectionLength,V);}
    UFUNCTION() void OnSectionLength(){SetSection(true,SectionNormalized);}
    UFUNCTION() void OnSectionWidth(){SetSection(false,SectionNormalized);}
    UFUNCTION() void OnMapTop(){SetMapHeat(false);}
    UFUNCTION() void OnMapHeat(){SetMapHeat(true);}
    UFUNCTION() void OnMaxMap(){SelectView(bMapHeat?3:2);}
    UFUNCTION() void OnMaxSection(){SelectView(4);}
    UFUNCTION() void OnLight(float Value){ApplyLighting();}
    UFUNCTION() void OnNoon();
    UFUNCTION() void OnEvening();
    UFUNCTION() void OnWorkLight();
    UFUNCTION() void OnFrameVessel(){bFrameSite=false;bFrameVessel=true;bFrameUnloader=true;OrbitYaw=35;OrbitPitch=-35;Focus=FVector2D::ZeroVector;OrbitDistance=7800;bPresetHidesPointControls=true;SetModelPresentation(true);UpdateCamera();}
    UFUNCTION() void OnSiteOverview(){SetCameraPreset(3);}
    UFUNCTION() void OnScaleReference();
    UFUNCTION() void OnWater();
    UFUNCTION() void OnFrameUnloader(){bFrameSite=false;bFrameVessel=false;bFrameUnloader=true;OrbitDistance=7800;bPresetHidesPointControls=true;SetModelPresentation(true);UpdateCamera();}
    UFUNCTION() void OnFrameCargo(){bFrameSite=false;bFrameVessel=false;bFrameUnloader=false;OrbitDistance=5000;SetModelPresentation(false);UpdateCamera();}
    UFUNCTION() void OnFill();
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
