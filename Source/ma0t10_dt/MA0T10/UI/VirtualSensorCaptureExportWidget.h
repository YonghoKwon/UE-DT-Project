#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorDraggableWidgetBase.h"
#include "VirtualSensorCaptureExportWidget.generated.h"

class AVirtualSensorManager;
class UVirtualSensorMonitorWidget;
class STextBlock;

UCLASS(BlueprintType)
class MA0T10_DT_API UVirtualSensorCaptureExportWidget : public UVirtualSensorDraggableWidgetBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    void BindSensorManager(AVirtualSensorManager* InSensorManager);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    void BindMonitorWidget(UVirtualSensorMonitorWidget* InMonitorWidget);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    void CaptureOnce();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    bool ExportServerPayload();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    bool ExportSelectedPointCloud(EVirtualSensorExportKind Kind);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    void ToggleTimedCapture();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    bool OpenCaptureRootFolder();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    bool OpenLastResultFolder();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    bool CopyLastResultPath();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport")
    FString GetStorageSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport")
    const TArray<FVirtualSensorExportResult>& GetRecentResults() const { return RecentResults; }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|SensorExport")
    FOnVirtualSensorExportCompleted OnExportCompleted;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void AddResult(EVirtualSensorExportKind Kind, const FString& SensorId, bool bSucceeded, const FString& Path, const FString& Message);
    void RefreshNativeText();
    FString GetSelectedSensorId() const;
    FString ExportKindText(EVirtualSensorExportKind Kind) const;
    EVirtualSensorExportKind CyclePointCloudKind(EVirtualSensorExportKind Kind) const;

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorManager> SensorManager;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorMonitorWidget> MonitorWidget;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport", meta = (AllowPrivateAccess = "true"))
    TArray<FVirtualSensorExportResult> RecentResults;

    EVirtualSensorExportKind SelectedPointCloudKind = EVirtualSensorExportKind::PointCloudCsv;
    TSharedPtr<STextBlock> NativeStorageText;
    FString LastUiMessage;
};
