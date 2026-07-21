#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "VirtualSensorCaptureExportPanelWidget.generated.h"

class AVirtualSensorCoordinator;
class UVirtualSensorMonitorPanelWidget;
class STextBlock;
struct FVirtualSensorTransportProfile;

UENUM(BlueprintType)
enum class EVirtualSensorCaptureExportTab : uint8
{
	LiveStream UMETA(DisplayName = "실시간 전송"),
	Capture UMETA(DisplayName = "캡처"),
	Export UMETA(DisplayName = "내보내기"),
	ConnectionLog UMETA(DisplayName = "연결 및 로그")
};

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

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Transport")
	bool SendLastExportToServer();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Transport")
	FString GetTransportSummaryText() const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void SetActiveTab(EVirtualSensorCaptureExportTab NewTab);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Stream")
	EVirtualSensorCaptureExportTab GetActiveTab() const { return ActiveTab; }

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void ToggleSelectedStream(EVirtualSensorStreamKind StreamKind);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void ToggleAllStreams();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Stream")
	FString GetLiveStreamSummaryText() const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	bool ExportTransportDiagnosticReport();

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport")
    FString GetStorageSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport")
    const TArray<FVirtualSensorExportResult>& GetRecentResults() const { return RecentResults; }

    UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|SensorExport")
    FOnVirtualSensorExportCompleted OnExportCompleted;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void AddResult(EVirtualSensorExportKind Kind, const FString& SensorId, bool bSucceeded, const FString& Path, const FString& Message);
    void RefreshNativeText();
    FString GetSelectedSensorId() const;
    FString ExportKindText(EVirtualSensorExportKind Kind) const;
    EVirtualSensorExportKind CyclePointCloudKind(EVirtualSensorExportKind Kind) const;
	FString GetSelectedSensorIdForStream(EVirtualSensorStreamKind StreamKind) const;
	void ApplyStreamConfig(EVirtualSensorStreamKind StreamKind, const FString& SensorId, bool bEnabled);
	TSharedRef<SWidget> BuildLiveStreamTab();
	TSharedRef<SWidget> BuildCaptureTab();
	TSharedRef<SWidget> BuildExportTab(TSharedPtr<EVirtualSensorExportKind> InitiallySelected);
	TSharedRef<SWidget> BuildConnectionLogTab();
	FString BuildTransportLogText() const;
	FText TabLabel(EVirtualSensorCaptureExportTab Tab) const;

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
	FString DraftAckTopic;
	int32 DraftMaxMessageBytes = 8388608;
	FString SessionPasscode;
	FString SessionBearerToken;
	FString DraftHttpEndpoint;
	bool bUseStompTransport = true;
	EVirtualSensorCaptureExportTab ActiveTab = EVirtualSensorCaptureExportTab::LiveStream;
	int32 StreamFrameStride = 1;
	int32 StreamReceiptInterval = 10;
	FString CachedLiveStreamSummary;
	FString CachedTransportLog;
	double LastNativeStatusRefreshSeconds = -1.0;
};
