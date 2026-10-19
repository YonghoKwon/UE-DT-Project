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
struct FShipHatchMonitorWork;

UCLASS()
class MA0T10_DT_API UShipHatchMonitorWidget : public UDxWidget
{
    GENERATED_BODY()
public:
    void Bind(AShipHatchMonitorHost* InHost,AShipHatchSimulation* InSimulation);
    void Refresh();
    void SelectView(int32 View);
    void SelectColor(int32 Color);
    void SetFontPercent(int32 Percent);
    const FString& GetStatusText() const { return CachedStatus; }
    int32 GetView() const { return ActiveView; }
    uint64 AnalysisBuilds=0;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
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
    TArray<int32> BaseFontSizes;
    TSharedPtr<FShipHatchMonitorWork> Work;
    FString CachedStatus;
    int32 ActiveView=0,ActiveColor=0;
    uint64 LastRevision=MAX_uint64;
    int64 LastMLX=-1,LastLivox=-1;
    bool bSync=false;
    bool bWasGenerating=false;
    double CellSize=1;
    void ApplyPreview();
    void RefreshAnalysis();
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
};
