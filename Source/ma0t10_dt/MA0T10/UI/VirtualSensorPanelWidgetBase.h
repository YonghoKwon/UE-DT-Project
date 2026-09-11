#pragma once

#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "VirtualSensorPanelWidgetBase.generated.h"

class UVirtualSensorPanelHostComponent;

UCLASS(Abstract, BlueprintType)
class MA0T10_DT_API UVirtualSensorPanelWidgetBase : public UDxWidget
{
    GENERATED_BODY()

public:
	void SetToolWorkspace(UVirtualSensorToolWorkspaceSubsystem* Owner,ESensorToolPanelRole Role);
	UVirtualSensorToolWorkspaceSubsystem* GetToolWorkspace() const { return ToolWorkspace.Get(); }
	ESensorToolPanelRole GetToolRole() const { return ToolRole; }
	bool IsWorkspaceOwned() const { return ToolWorkspace.IsValid(); }
	FVirtualSensorPanelUiState CaptureWorkspaceLayout() const;
	void ApplyWorkspaceLayout(const FVirtualSensorPanelUiState& State);
	FVector2D GetPanelLayoutViewport() const { return ResolveLogicalViewportSize(); }
	void SetWorkspacePosition(FVector2D Position) { if(IsWorkspaceOwned())SetPanelPositionInternal(Position); }
	TSharedRef<SWidget> BuildToolPanelHeader(const FText& Title);
	// PR17 compatibility names. They no longer change any global UI state.
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SensorPanel|Accessibility")
	void SetGlobalSensorUiFontScale(float InScale);
	UFUNCTION(BlueprintPure, Category="DigitalTwin|SensorPanel|Accessibility")
	float GetGlobalSensorUiFontScale() const;
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SensorPanel|Accessibility")
	void ResetGlobalSensorUiFontScale();
	UFUNCTION(BlueprintImplementableEvent, Category="DigitalTwin|SensorPanel|Accessibility")
	void OnSensorUiFontScaleChanged(float NewScale);
	static int32 CalculateScaledFontSize(int32 BaseSize, float Scale);
	void SetSensorAppearanceOwner(class AVirtualSensorUiHostActor* Host);
	void ApplySensorToolFontScale(float Scale);
	float GetSensorToolFontScale() const { return SensorToolFontScale; }
	void RegisterSensorNativeFont(TSharedRef<STextBlock> Widget, const STextBlock::FArguments& Args);
	void RegisterSensorNativeFont(TSharedRef<SButton> Widget, const SButton::FArguments& Args);
	void RegisterSensorNativeFont(TSharedRef<SEditableTextBox> Widget, const SEditableTextBox::FArguments& Args);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SensorPanel|Appearance")
	void RegisterSensorTextControl(class UTextBlock* Text);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SensorPanel|Appearance")
	void RegisterSensorInputControl(class UEditableTextBox* Input);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|SensorPanel|Appearance")
	void SetSensorToolFontScale(float Scale);
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void SetPanelPersistenceKey(FName InPanelPersistenceKey);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void ConfigurePanelLayout(EVirtualSensorPanelPlacement InPlacement, FVector2D InDesiredSize, float InMargin = 18.0f);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void ResetPanelPosition();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void TogglePanelCollapsed();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void ResetPanelUiStateToDefault();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    bool IsPanelCollapsed() const { return bPanelCollapsed; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    FVector2D GetCurrentPanelPosition() const { return CurrentViewportPosition; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    FVector2D GetEffectivePanelSize() const;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void SetPanelResizable(bool bResizable) { bPanelResizable = bResizable; }

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void SetPanelResizeLimits(FVector2D InMinimumSize, FVector2D InMaximumSize);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void SetPanelExpandedSize(FVector2D InExpandedSize, bool bPersist = true);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorPanel")
    void ResetPanelSize();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    FVector2D GetPanelExpandedSize() const { return DesiredPanelSize; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    static FVector2D ClampPanelPosition(FVector2D Position, FVector2D PanelSize, FVector2D ViewportSize, float Margin);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    static FVector2D CalculateDraggedPanelPosition(FVector2D CurrentPosition, FVector2D CursorDelta, float DpiScale);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    static FVector2D CalculateResizedPanelSize(FVector2D CurrentSize, FVector2D CursorDelta, float DpiScale, FVector2D MinimumSize, FVector2D MaximumSize);

    void SetPanelHostComponent(UVirtualSensorPanelHostComponent* InHostComponent) { PanelHostComponent = InHostComponent; }
    void RefreshHostedPanelLayout();

protected:
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

    EVisibility GetPanelBodyVisibility() const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorPanel")
    bool bPanelDraggable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorPanel")
    bool bClampPanelToViewport = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorPanel")
    bool bPanelResizable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorPanel", meta = (ClampMin = "16.0", ClampMax = "96.0"))
    float DragHandleHeight = 38.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorPanel", meta = (ClampMin = "8.0", ClampMax = "48.0"))
    float ResizeHandleSize = 18.0f;

private:
	TWeakObjectPtr<UVirtualSensorToolWorkspaceSubsystem> ToolWorkspace;
	ESensorToolPanelRole ToolRole=ESensorToolPanelRole::Monitor;
	bool bSensorPanelConstructed = false;
	TArray<TFunction<void(float)>> SensorFontSetters;
	TWeakObjectPtr<class AVirtualSensorUiHostActor> SensorAppearanceOwner;
	float SensorToolFontScale = 1.0f;
	TSet<TWeakObjectPtr<UWidget>> RegisteredSensorUmg;
    void ApplyInitialPanelLayout(FVector2D ViewportSize);
    void ResetPanelPositionInternal(bool bPersist);
    void RestorePanelUiState();
    void SavePanelUiState() const;
    FVector2D ToNormalizedPanelPosition(FVector2D Position, FVector2D ViewportSize) const;
    FVector2D FromNormalizedPanelPosition(FVector2D NormalizedPosition, FVector2D ViewportSize) const;
    FVector2D ResolveLogicalViewportSize() const;
    void SetPanelPositionInternal(FVector2D NewPosition);
    void ApplyPanelSize();
    FVector2D ResolveMaximumPanelSize() const;
    bool IsInResizeHandle(const FGeometry& Geometry, const FVector2D& ScreenPosition) const;

    EVirtualSensorPanelPlacement DefaultPlacement = EVirtualSensorPanelPlacement::RightCenter;
    FVector2D RequestedPanelSize = FVector2D(820.0f, 430.0f);
    FVector2D DefaultResolvedPanelSize = FVector2D(820.0f, 430.0f);
    FVector2D DesiredPanelSize = FVector2D(820.0f, 430.0f);
    FVector2D MinimumPanelSize = FVector2D(160.0f, 80.0f);
    FVector2D MaximumPanelSize = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
    FName PanelPersistenceKey = NAME_None;
    float ViewportMargin = 18.0f;
    float CollapsedPanelHeight = 48.0f;
    bool bDraggingPanel = false;
    bool bResizingPanel = false;
    bool bPanelCollapsed = false;
    bool bInitialLayoutPending = false;
    bool bPanelLayoutConfigured = false;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorPanelHostComponent> PanelHostComponent;
};
