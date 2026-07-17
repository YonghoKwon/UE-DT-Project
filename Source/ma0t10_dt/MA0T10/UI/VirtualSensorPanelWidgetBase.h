#pragma once

#include "CoreMinimal.h"
#include "UI/DxWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorControlTypes.h"
#include "VirtualSensorPanelWidgetBase.generated.h"

class UVirtualSensorPanelHostComponent;

UCLASS(Abstract, BlueprintType)
class MA0T10_DT_API UVirtualSensorPanelWidgetBase : public UDxWidget
{
    GENERATED_BODY()

public:
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

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    static FVector2D ClampPanelPosition(FVector2D Position, FVector2D PanelSize, FVector2D ViewportSize, float Margin);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorPanel")
    static FVector2D CalculateDraggedPanelPosition(FVector2D CurrentPosition, FVector2D CursorDelta, float DpiScale);

    void SetPanelHostComponent(UVirtualSensorPanelHostComponent* InHostComponent) { PanelHostComponent = InHostComponent; }
    void RefreshHostedPanelLayout();

protected:
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorPanel", meta = (ClampMin = "16.0", ClampMax = "96.0"))
    float DragHandleHeight = 38.0f;

private:
    void ApplyInitialPanelLayout(FVector2D ViewportSize);
    void ResetPanelPositionInternal(bool bPersist);
    void RestorePanelUiState();
    void SavePanelUiState() const;
    FVector2D ToNormalizedPanelPosition(FVector2D Position, FVector2D ViewportSize) const;
    FVector2D FromNormalizedPanelPosition(FVector2D NormalizedPosition, FVector2D ViewportSize) const;
    FVector2D ResolveLogicalViewportSize() const;
    void SetPanelPositionInternal(FVector2D NewPosition);
    void ApplyPanelSize();

    EVirtualSensorPanelPlacement DefaultPlacement = EVirtualSensorPanelPlacement::RightCenter;
    FVector2D RequestedPanelSize = FVector2D(820.0f, 430.0f);
    FVector2D DesiredPanelSize = FVector2D(820.0f, 430.0f);
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
    FName PanelPersistenceKey = NAME_None;
    float ViewportMargin = 18.0f;
    float CollapsedPanelHeight = 48.0f;
    bool bDraggingPanel = false;
    bool bPanelCollapsed = false;
    bool bInitialLayoutPending = false;
    bool bPanelLayoutConfigured = false;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorPanelHostComponent> PanelHostComponent;
};
