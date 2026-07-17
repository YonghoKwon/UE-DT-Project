#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "VirtualSensorCaptureExportPanelWidget.generated.h"

class AVirtualSensorCoordinator;
class UVirtualSensorMonitorPanelWidget;
class STextBlock;
struct FVirtualSensorTransportProfile;

UCLASS(BlueprintType)
class MA0T10_DT_API UVirtualSensorCaptureExportPanelWidget : public UVirtualSensorPanelWidgetBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    void BindSensorManager(AVirtualSensorCoordinator* InSensorManager);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
    void BindMonitorWidget(UVirtualSensorMonitorPanelWidget* InMonitorWidget);

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

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport")
	void SetSelectedPointCloudExportKind(EVirtualSensorExportKind Kind);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport")
	EVirtualSensorExportKind GetSelectedPointCloudExportKind() const { return SelectedPointCloudKind; }

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Transport")
	bool ApplyTransportProfile();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Transport")
	bool TestServerConnection();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Transport")
	bool SendSelectedPayloadToServer();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Transport")
	FString GetTransportSummaryText() const;

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
    TObjectPtr<AVirtualSensorCoordinator> SensorManager;

    UPROPERTY(Transient)
    TObjectPtr<UVirtualSensorMonitorPanelWidget> MonitorWidget;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport", meta = (AllowPrivateAccess = "true"))
    TArray<FVirtualSensorExportResult> RecentResults;

    EVirtualSensorExportKind SelectedPointCloudKind = EVirtualSensorExportKind::PointCloudCsv;
	TArray<TSharedPtr<EVirtualSensorExportKind>> NativeExportKindOptions;
    TSharedPtr<STextBlock> NativeStorageText;
    FString LastUiMessage;
	FString DraftBrokerUrl = TEXT("ws://127.0.0.1:61616");
	FString DraftCameraTopic = TEXT("topic.virtual.sensor.camera.0");
	FString DraftLidarTopic = TEXT("topic.virtual.sensor.lidar.0");
	FString DraftExportTopic = TEXT("topic.virtual.sensor.export.0");
	FString DraftUserName = TEXT("artemis");
	FString SessionPasscode;
	FString DraftHttpEndpoint;
	bool bUseStompTransport = true;
};
