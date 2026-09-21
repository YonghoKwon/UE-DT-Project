#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSlabFrameContext.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveTypes.h"
#include "VirtualSensorCaptureExportPanelWidget.generated.h"

class AVirtualSensorCoordinator;
class AVirtualSensorExternalSourceHostActor;
class UVirtualSensorMonitorPanelWidget;
class STextBlock;
class ASlabActor;
class AVirtualSensorActorBase;
class UVirtualSensorFileSaveSubsystem;
struct FVirtualSensorTransportProfile;

UENUM(BlueprintType)
enum class EVirtualSensorCaptureExportTab : uint8
{
	LiveStream UMETA(DisplayName = "실시간 전송"),
	Capture UMETA(DisplayName = "캡처"),
	Export UMETA(DisplayName = "내보내기"),
	ConnectionLog UMETA(DisplayName = "연결 및 로그")
};

UENUM(BlueprintType)
enum class EVirtualPointCloudStreamFilterPreset : uint8
{
	AllHits UMETA(DisplayName = "전체 검출점"),
	TargetActorTag UMETA(DisplayName = "대상 물체만"),
	TagOrSemantic UMETA(DisplayName = "Tag·Semantic"),
	SensorLocalRoi UMETA(DisplayName = "센서 로컬 ROI")
};

UCLASS(BlueprintType)
class MA0T10_DT_API UVirtualSensorCaptureExportPanelWidget : public UVirtualSensorPanelWidgetBase
{
    GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Outputs") void BindScenarioSlabActor(ASlabActor* InSlab);
	UFUNCTION(BlueprintCallable, Category="DigitalTwin|Slab|Outputs") bool SetLiveScenarioOutputs(const FVirtualSlabSensorOutputSelection& Outputs);
	UFUNCTION(BlueprintPure, Category="DigitalTwin|Slab|Outputs") FVirtualSlabSensorOutputSelection GetLiveScenarioOutputs() const;
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

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Capture")
	void SetCaptureIntervalSeconds(float IntervalSeconds);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Capture")
	void UseSelectedSensorCaptureInterval();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Capture")
	void SetCaptureSelection(const FVirtualSensorCaptureSelection& Selection);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Capture")
	FVirtualSensorCaptureSelection GetCaptureSelection() const { return CaptureSelection; }

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

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Receiver")
	void StopTopicReceivers();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Receiver")
	void ReconnectTopicReceivers();

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Receiver")
	FString GetTopicReceiverSummaryText() const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void SetActiveTab(EVirtualSensorCaptureExportTab NewTab);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Stream")
	EVirtualSensorCaptureExportTab GetActiveTab() const { return ActiveTab; }

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void ToggleSelectedStream(EVirtualSensorStreamKind StreamKind);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void ToggleAllStreams();

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void SetSelectedPointCloudStreamFormat(EVirtualPointCloudStreamFormat Format);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Stream")
	EVirtualPointCloudStreamFormat GetSelectedPointCloudStreamFormat() const { return SelectedPointCloudStreamFormat; }

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void SetPointCloudStreamFilterPreset(EVirtualPointCloudStreamFilterPreset Preset);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|SensorExport|Stream")
	void SetPointCloudStreamFilterConfig(const FVirtualPointCloudFilterConfig& Filter);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorExport|Stream")
	FVirtualPointCloudFilterConfig GetPointCloudStreamFilterConfig() const { return PointCloudStreamFilter; }

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

	/** Additive asynchronous UI path. Legacy synchronous Blueprint methods above retain their contract. */
	UFUNCTION(BlueprintCallable,Category="DigitalTwin|SensorExport|Async") FString RequestFileSave(EVirtualSensorFileSaveMode Mode);
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorExport|Async") FVirtualSensorFileSaveStatus GetActiveFileSaveStatus() const;
	UFUNCTION(BlueprintPure,Category="DigitalTwin|SensorExport|Stream") bool IsSelectedStreamScenarioControlled(EVirtualSensorStreamKind Kind) const;
	static int32 ResolveOwnedTabIndex(EVirtualSensorCaptureExportTab Tab);

protected:
	ASlabActor* ResolveScenarioSlabActor() const;
	UPROPERTY(Transient) TObjectPtr<ASlabActor> ScenarioSlabActor;
	mutable TWeakObjectPtr<ASlabActor> AutoScenarioSlabActor;
	mutable double NextScenarioSlabLookup = 0;
    virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TSharedRef<SWidget> BuildOwnedWidget();
	TSharedRef<SWidget> BuildOwnedLiveTab();
	TSharedRef<SWidget> BuildOwnedFileTab();
	TSharedRef<SWidget> BuildOwnedConnectionTab();
	void BindFileService();
	void TickOwnedUi(double Now);
	void ToggleOwnedPeriodicSave();
	void ToggleOwnedStream(EVirtualSensorStreamKind Kind);
	bool IsOwnedCompatibilityTransport() const;
	AVirtualSensorActorBase* ResolveOwnedSelectedActor() const;
	FVirtualSensorCaptureSelection ResolveOwnedFileSelection() const;
	UFUNCTION() void HandleFileSaveUpdated(const FVirtualSensorFileSaveStatus& Status);
	TWeakObjectPtr<UVirtualSensorFileSaveSubsystem> FileSaveService;
	FString OwnedFileRequestId,OwnedPeriodicSessionId;
	TSet<FString> OwnedFileRequests;
	TArray<FString> DeliveredFileRequests;
	int32 OwnedFileAction=0; // 0=new, 1=current, 2=periodic; no new persistent enum/slot.
	bool bOwnedScenarioStreamView=false,bOwnedExtraScenarioOutputs=false;
	bool bOwnedStorageExpanded=false,bOwnedReceiverExpanded=false,bOwnedTransportLogExpanded=false;
	bool bOwnedCanSaveCurrent=false,bOwnedSaveBusy=false,bOwnedPeriodicActive=false;
	FString OwnedSelectionText,OwnedFileStatusText,OwnedPeriodicStatusText,OwnedConnectionText;
	double NextReceiverLookup=0,ReceiverLookupDelay=.5;
	bool bWorkspaceLiveDetails=false;
	TMap<EVirtualSensorStreamKind,FString> WorkspaceStreamCards;
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
	void ApplyCaptureSelectionToMonitor();
	void SaveCapturePreferences() const;
	void ReconfigureEnabledPointCloudStreams();

    UPROPERTY(Transient)
    TObjectPtr<AVirtualSensorCoordinator> SensorManager;

	UPROPERTY(Transient)
	TObjectPtr<UVirtualSensorMonitorPanelWidget> MonitorWidget;

	UPROPERTY(Transient)
	TObjectPtr<AVirtualSensorExternalSourceHostActor> TopicReceiverHost;

    UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorExport", meta = (AllowPrivateAccess = "true"))
    TArray<FVirtualSensorExportResult> RecentResults;

    EVirtualSensorExportKind SelectedPointCloudKind = EVirtualSensorExportKind::PointCloudCsv;
	TArray<TSharedPtr<EVirtualSensorExportKind>> NativeExportKindOptions;
	TArray<TSharedPtr<EVirtualPointCloudStreamFormat>> NativeStreamFormatOptions;
	TArray<TSharedPtr<EVirtualPointCloudStreamFilterPreset>> NativeStreamFilterOptions;
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
	EVirtualPointCloudStreamFormat SelectedPointCloudStreamFormat = EVirtualPointCloudStreamFormat::PCD;
	EVirtualPointCloudStreamFilterPreset SelectedPointCloudStreamFilterPreset = EVirtualPointCloudStreamFilterPreset::AllHits;
	FVirtualPointCloudFilterConfig PointCloudStreamFilter;
	FVirtualSensorCaptureSelection CaptureSelection;
	FString CachedLiveStreamSummary;
	FString CachedTransportLog;
	FString CachedTopicReceiverSummary;
	double LastNativeStatusRefreshSeconds = -1.0;
};
