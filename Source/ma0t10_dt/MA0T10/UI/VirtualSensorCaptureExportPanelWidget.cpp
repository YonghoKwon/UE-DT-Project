#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"

#define LOCTEXT_NAMESPACE "VirtualSensorCaptureExportPanelWidget"

void UVirtualSensorCaptureExportPanelWidget::BindSensorManager(AVirtualSensorCoordinator* InSensorManager)
{
    SensorManager = InSensorManager;
	if (SensorManager && SensorManager->SharedTransportComponent)
	{
		const FVirtualSensorTransportProfile& Profile = SensorManager->SharedTransportComponent->GetTransportProfile();
		DraftBrokerUrl = Profile.BrokerUrl;
		DraftCameraTopic = Profile.CameraTopic;
		DraftLidarTopic = Profile.LidarTopic;
		DraftExportTopic = Profile.ExportTopic;
		DraftUserName = Profile.UserName;
		DraftAckTopic = Profile.AckTopic;
		DraftMaxMessageBytes = Profile.MaxMessageBytes;
		DraftHttpEndpoint = Profile.HttpEndpoint;
	}
    RefreshNativeText();
}

void UVirtualSensorCaptureExportPanelWidget::BindMonitorWidget(UVirtualSensorMonitorPanelWidget* InMonitorWidget)
{
    MonitorWidget = InMonitorWidget;
    RefreshNativeText();
}

void UVirtualSensorCaptureExportPanelWidget::CaptureOnce()
{
    if (SensorManager)
    {
        SensorManager->RefreshSelectedSensorOnce(MonitorWidget && MonitorWidget->IsShowingLidar());
        AddResult(EVirtualSensorExportKind::TimedCapture, GetSelectedSensorId(), true, FString(), TEXT("선택 센서 1회 캡처를 실행했습니다"));
    }
    else
    {
        AddResult(EVirtualSensorExportKind::TimedCapture, FString(), false, FString(), TEXT("SensorManager가 연결되지 않았습니다"));
    }
}

bool UVirtualSensorCaptureExportPanelWidget::ExportServerPayload()
{
    const bool bSaved = MonitorWidget && MonitorWidget->ExportSelectedSensorServerPayload(TEXT("panel_server_payload"));
    const FString Path = MonitorWidget ? MonitorWidget->GetLastManualExportPath() : FString();
    AddResult(EVirtualSensorExportKind::ServerPayload, GetSelectedSensorId(), bSaved, Path,
        MonitorWidget ? MonitorWidget->GetLastManualExportMessage() : TEXT("모니터 Widget이 연결되지 않았습니다"));
    return bSaved;
}

bool UVirtualSensorCaptureExportPanelWidget::ExportSelectedPointCloud(EVirtualSensorExportKind Kind)
{
    UVirtualLidarScanComponent* Lidar = SensorManager ? SensorManager->GetSelectedLidar() : nullptr;
    if (!Lidar)
    {
        AddResult(Kind, FString(), false, FString(), TEXT("선택한 LiDAR가 없습니다"));
        return false;
    }
    if (Lidar->GetLastPoints().Num() <= 0)
    {
        Lidar->ScanAndSend();
    }

    bool bSaved = false;
    if (Kind == EVirtualSensorExportKind::PointCloudCsv) bSaved = Lidar->ExportLastPointCloudCsv(TEXT("panel_export"));
    else if (Kind == EVirtualSensorExportKind::PointCloudJsonLines) bSaved = Lidar->ExportLastPointCloudJsonLines(TEXT("panel_export"));
    else if (Kind == EVirtualSensorExportKind::PointCloudPcd) bSaved = Lidar->ExportLastPointCloudPcd(TEXT("panel_export"));
    else if (Kind == EVirtualSensorExportKind::PointCloudLas) bSaved = Lidar->ExportLastPointCloudLas(TEXT("panel_export"));
    else if (Kind == EVirtualSensorExportKind::PointCloudLaz) bSaved = Lidar->ExportLastPointCloudLaz(TEXT("panel_export"));

    FString Path = Lidar->GetLastPointCloudExportPath();
    if (Kind == EVirtualSensorExportKind::PointCloudLaz)
    {
        Path = Lidar->GetLastLazOutputPath().IsEmpty() ? Lidar->GetLastLazLasSourcePath() : Lidar->GetLastLazOutputPath();
    }
    AddResult(Kind, Lidar->SensorId, bSaved, Path, bSaved ? TEXT("포인트 클라우드를 저장했습니다") : TEXT("포인트 클라우드 저장에 실패했습니다"));
    return bSaved;
}

void UVirtualSensorCaptureExportPanelWidget::ToggleTimedCapture()
{
    if (!MonitorWidget)
    {
        AddResult(EVirtualSensorExportKind::TimedCapture, FString(), false, FString(), TEXT("모니터 Widget이 연결되지 않았습니다"));
        return;
    }
    MonitorWidget->ToggleLocalSensorCapture();
    AddResult(EVirtualSensorExportKind::TimedCapture, GetSelectedSensorId(), true, MonitorWidget->GetLocalCaptureSessionDirectory(),
        MonitorWidget->IsLocalSensorCaptureActive() ? TEXT("시간 지정 캡처를 시작했습니다") : TEXT("시간 지정 캡처를 중지했습니다"));
}

bool UVirtualSensorCaptureExportPanelWidget::OpenCaptureRootFolder()
{
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures")));
    IFileManager::Get().MakeDirectory(*Directory, true);
    FPlatformProcess::ExploreFolder(*Directory);
    return true;
}

bool UVirtualSensorCaptureExportPanelWidget::OpenLastResultFolder()
{
    if (RecentResults.Num() <= 0 || RecentResults[0].AbsolutePath.IsEmpty()) return false;
    const FString Path = RecentResults[0].AbsolutePath;
    const FString Directory = IFileManager::Get().DirectoryExists(*Path) ? Path : FPaths::GetPath(Path);
    if (!IFileManager::Get().DirectoryExists(*Directory)) return false;
    FPlatformProcess::ExploreFolder(*Directory);
    return true;
}

bool UVirtualSensorCaptureExportPanelWidget::CopyLastResultPath()
{
    if (RecentResults.Num() <= 0 || RecentResults[0].AbsolutePath.IsEmpty())
    {
        LastUiMessage = TEXT("복사할 최근 저장 경로가 없습니다.");
        RefreshNativeText();
        return false;
    }
    const FString& Path = RecentResults[0].AbsolutePath;
    if (!IFileManager::Get().FileExists(*Path) && !IFileManager::Get().DirectoryExists(*Path))
    {
        LastUiMessage = TEXT("최근 결과 파일 또는 폴더가 존재하지 않아 경로를 복사하지 않았습니다.");
        RefreshNativeText();
        return false;
    }
    FPlatformApplicationMisc::ClipboardCopy(*Path);
    LastUiMessage = TEXT("최근 저장 경로를 클립보드에 복사했습니다.");
    RefreshNativeText();
    return true;
}

void UVirtualSensorCaptureExportPanelWidget::SetSelectedPointCloudExportKind(EVirtualSensorExportKind Kind)
{
	if (Kind < EVirtualSensorExportKind::PointCloudCsv || Kind > EVirtualSensorExportKind::PointCloudLaz) return;
	SelectedPointCloudKind = Kind;
	if (UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate())
	{
		Preferences->SelectedPointCloudExportKind = static_cast<uint8>(Kind);
		UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
	}
	RefreshNativeText();
}

bool UVirtualSensorCaptureExportPanelWidget::ApplyTransportProfile()
{
	UVirtualSensorTransportComponent* Transport = SensorManager ? SensorManager->SharedTransportComponent : nullptr;
	if (!Transport)
	{
		LastUiMessage = TEXT("공용 Transport가 연결되지 않았습니다.");
		RefreshNativeText();
		return false;
	}
	FVirtualSensorTransportProfile Profile = Transport->GetTransportProfile();
	Profile.BrokerUrl = DraftBrokerUrl.TrimStartAndEnd();
	Profile.CameraTopic = DraftCameraTopic.TrimStartAndEnd();
	Profile.LidarTopic = DraftLidarTopic.TrimStartAndEnd();
	Profile.ExportTopic = DraftExportTopic.TrimStartAndEnd();
	Profile.UserName = DraftUserName.TrimStartAndEnd();
	Profile.AckTopic = DraftAckTopic.TrimStartAndEnd();
	Profile.MaxMessageBytes = FMath::Max(1024, DraftMaxMessageBytes);
	Profile.HttpEndpoint = DraftHttpEndpoint.TrimStartAndEnd();
	Transport->ConfigureTransportProfile(Profile);
	Transport->SetSessionCredentials(SessionPasscode, SessionBearerToken);
	Transport->TransportMode = bUseStompTransport ? EVirtualSensorTransportMode::StompWebSocket : EVirtualSensorTransportMode::HttpPost;
	if (UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate())
	{
		Preferences->StompBrokerUrl = Profile.BrokerUrl;
		Preferences->StompCameraTopic = Profile.CameraTopic;
		Preferences->StompLidarTopic = Profile.LidarTopic;
		Preferences->StompExportTopic = Profile.ExportTopic;
		Preferences->StompUserName = Profile.UserName;
		Preferences->StompAckTopic = Profile.AckTopic;
		Preferences->OutboundMaxMessageBytes = Profile.MaxMessageBytes;
		Preferences->OutboundHttpEndpoint = Profile.HttpEndpoint;
		UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
	}
	LastUiMessage = bUseStompTransport ? TEXT("Artemis STOMP 설정을 적용했습니다.") : TEXT("HTTP 전송 설정을 적용했습니다.");
	RefreshNativeText();
	return true;
}

bool UVirtualSensorCaptureExportPanelWidget::TestServerConnection()
{
	if (!ApplyTransportProfile()) return false;
	UVirtualSensorTransportComponent* Transport = SensorManager ? SensorManager->SharedTransportComponent : nullptr;
	const FVirtualSensorTransportResult Result = Transport->TestConnection();
	LastUiMessage = Result.Message;
	RefreshNativeText();
	return Result.bSubmitted;
}

bool UVirtualSensorCaptureExportPanelWidget::SendSelectedPayloadToServer()
{
	if (!ApplyTransportProfile()) return false;
	UVirtualSensorTransportComponent* Transport = SensorManager ? SensorManager->SharedTransportComponent : nullptr;
	FString Payload;
	FString SensorType;
	int64 FrameId = 0;
	if (MonitorWidget && MonitorWidget->IsShowingLidar())
	{
		if (const UVirtualLidarScanComponent* Lidar = SensorManager->GetSelectedLidar())
		{
			Payload = Lidar->GetLastJsonPayload(); SensorType = TEXT("lidar"); FrameId = Lidar->GetRuntimeStatus().FrameId;
		}
	}
	else if (const UVirtualCameraCaptureComponent* Camera = SensorManager ? SensorManager->GetSelectedCamera() : nullptr)
	{
		Payload = Camera->GetLastJsonPayload(); SensorType = TEXT("camera"); FrameId = Camera->GetRuntimeStatus().FrameId;
	}
	if (Payload.IsEmpty())
	{
		LastUiMessage = TEXT("전송할 완료 Payload가 없습니다. 먼저 1회 캡처를 실행하세요.");
		RefreshNativeText();
		return false;
	}
	const FVirtualSensorTransportResult Result = Transport->SendJsonRequest(GetSelectedSensorId(), SensorType, TEXT("manual-payload"), FrameId, Payload, true);
	LastUiMessage = Result.Message;
	RefreshNativeText();
	return Result.bSubmitted;
}

bool UVirtualSensorCaptureExportPanelWidget::SendLastExportToServer()
{
	if (!ApplyTransportProfile()) return false;
	if (RecentResults.Num() == 0 || RecentResults[0].AbsolutePath.IsEmpty())
	{
		LastUiMessage = TEXT("전송할 최근 내보내기 파일이 없습니다.");
		RefreshNativeText();
		return false;
	}
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *RecentResults[0].AbsolutePath))
	{
		LastUiMessage = TEXT("최근 내보내기 파일을 읽지 못했습니다.");
		RefreshNativeText();
		return false;
	}
	UVirtualSensorTransportComponent* Transport = SensorManager ? SensorManager->SharedTransportComponent : nullptr;
	const FString Extension = FPaths::GetExtension(RecentResults[0].AbsolutePath, true);
	const FVirtualSensorTransportResult Result = Transport->SendBinary(GetSelectedSensorId(), TEXT("lidar"), Extension, Bytes);
	LastUiMessage = Result.Message;
	RefreshNativeText();
	return Result.bSubmitted;
}

FString UVirtualSensorCaptureExportPanelWidget::GetTransportSummaryText() const
{
	const UVirtualSensorTransportComponent* Transport = SensorManager ? SensorManager->SharedTransportComponent : nullptr;
	if (!Transport) return TEXT("Transport 연결 없음");
	const FVirtualSensorTransportResult& Result = Transport->LastResult;
	return FString::Printf(TEXT("%s · %s\n요청=%s · 대상=%s · %d bytes · %.1fms\n%s"),
		bUseStompTransport ? TEXT("Artemis STOMP/WS") : TEXT("HTTP POST"),
		Transport->IsStompConnected() ? TEXT("연결됨") : TEXT("대기/미연결"),
		Result.RequestId.IsEmpty() ? TEXT("-") : *Result.RequestId,
		Result.Destination.IsEmpty() ? TEXT("-") : *Result.Destination,
		Result.DataLength, Result.LatencyMs,
		Result.Message.IsEmpty() ? TEXT("전송 결과 없음") : *Result.Message);
}

FString UVirtualSensorCaptureExportPanelWidget::GetStorageSummaryText() const
{
    const FString SensorId = GetSelectedSensorId().IsEmpty() ? TEXT("<SensorId>") : GetSelectedSensorId();
    const FString SavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    FString Text = FString::Printf(TEXT("절대 저장 루트: %s\nPayload: Saved/SensorCaptures/%s/ServerPayload\n포인트 클라우드: Saved/SensorCaptures/%s/PointCloud\n시간 지정 캡처: Saved/SensorCaptures/LocalTimedCapture/<UTC>/Camera | Lidar\nRecorder: Saved/SensorRecordings\nTransport SaveToFile: Saved/SensorCaptures/%s"), *SavedRoot, *SensorId, *SensorId, *SensorId);
    if (MonitorWidget)
    {
        Text += FString::Printf(TEXT("\n시간 지정 캡처: %s  세션: %s"), MonitorWidget->IsLocalSensorCaptureActive() ? TEXT("기록 중") : TEXT("중지됨"), *MonitorWidget->GetLocalCaptureSessionDirectory());
    }
    if (RecentResults.Num() > 0)
    {
        Text += TEXT("\n최근 결과:");
        const int32 Count = FMath::Min(RecentResults.Num(), 8);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVirtualSensorExportResult& Result = RecentResults[Index];
            Text += FString::Printf(TEXT("\n[%s] %s | %s | %lld 바이트"), Result.bSucceeded ? TEXT("성공") : TEXT("실패"), *ExportKindText(Result.Kind), Result.AbsolutePath.IsEmpty() ? *Result.Message : *Result.AbsolutePath, Result.FileSizeBytes);
        }
    }
    if (!LastUiMessage.IsEmpty()) Text += FString::Printf(TEXT("\n\n안내: %s"), *LastUiMessage);
    return Text;
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::RebuildWidget()
{
    if (WidgetTree && WidgetTree->RootWidget)
    {
        return Super::RebuildWidget();
    }
	NativeExportKindOptions.Reset();
	for (EVirtualSensorExportKind Kind : { EVirtualSensorExportKind::PointCloudCsv, EVirtualSensorExportKind::PointCloudJsonLines, EVirtualSensorExportKind::PointCloudPcd, EVirtualSensorExportKind::PointCloudLas, EVirtualSensorExportKind::PointCloudLaz })
	{
		NativeExportKindOptions.Add(MakeShared<EVirtualSensorExportKind>(Kind));
	}
	if (const UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate())
	{
		const EVirtualSensorExportKind SavedKind = static_cast<EVirtualSensorExportKind>(Preferences->SelectedPointCloudExportKind);
		if (SavedKind >= EVirtualSensorExportKind::PointCloudCsv && SavedKind <= EVirtualSensorExportKind::PointCloudLaz) SelectedPointCloudKind = SavedKind;
		DraftBrokerUrl = Preferences->StompBrokerUrl;
		DraftCameraTopic = Preferences->StompCameraTopic;
		DraftLidarTopic = Preferences->StompLidarTopic;
		DraftExportTopic = Preferences->StompExportTopic;
		DraftUserName = Preferences->StompUserName;
		DraftAckTopic = Preferences->StompAckTopic;
		DraftMaxMessageBytes = FMath::Max(1024, Preferences->OutboundMaxMessageBytes);
		DraftHttpEndpoint = Preferences->OutboundHttpEndpoint;
	}
	TSharedPtr<EVirtualSensorExportKind> InitiallySelected = NativeExportKindOptions[0];
	for (const TSharedPtr<EVirtualSensorExportKind>& Option : NativeExportKindOptions) if (Option.IsValid() && *Option == SelectedPointCloudKind) { InitiallySelected = Option; break; }

    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FVirtualSensorUiStyle::PanelBackground)
        .ForegroundColor(FVirtualSensorUiStyle::PrimaryText)
        .Padding(10.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FVirtualSensorUiStyle::HeaderBackground)
                .Padding(FMargin(8.0f, 6.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("Title", "캡처 및 내보내기  |  제목을 드래그해 이동")) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(IsPanelCollapsed() ? TEXT("펼치기") : TEXT("접기")); }).OnClicked_Lambda([this]() { TogglePanelCollapsed(); return FReply::Handled(); }) ]
                    + SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ResetUi", "위치 초기화")).OnClicked_Lambda([this]() { ResetPanelPosition(); return FReply::Handled(); }) ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(2.0f, 5.0f, 2.0f, 0.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("선택: %s · SensorId: %s"), MonitorWidget && MonitorWidget->IsShowingLidar() ? TEXT("LiDAR") : TEXT("카메라"), GetSelectedSensorId().IsEmpty() ? TEXT("없음") : *GetSelectedSensorId())); }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
            [
                SNew(SWrapBox).UseAllottedSize(true).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("CaptureOnce", "선택 센서 1회 캡처")).OnClicked_Lambda([this]() { CaptureOnce(); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ExportPayload", "서버 Payload 내보내기")).OnClicked_Lambda([this]() { ExportServerPayload(); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()
				[
					SNew(SComboBox<TSharedPtr<EVirtualSensorExportKind>>)
					.OptionsSource(&NativeExportKindOptions)
					.InitiallySelectedItem(InitiallySelected)
					.OnGenerateWidget_Lambda([this](TSharedPtr<EVirtualSensorExportKind> Item) { return SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(FText::FromString(Item.IsValid() ? ExportKindText(*Item) : TEXT("CSV"))); })
					.OnSelectionChanged_Lambda([this](TSharedPtr<EVirtualSensorExportKind> Item, ESelectInfo::Type) { if (Item.IsValid()) SetSelectedPointCloudExportKind(*Item); })
					[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("형식: %s"), *ExportKindText(SelectedPointCloudKind))); }) ]
				]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ExportPointCloud", "포인트 클라우드 내보내기")).OnClicked_Lambda([this]() { ExportSelectedPointCloud(SelectedPointCloudKind); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(MonitorWidget && MonitorWidget->IsLocalSensorCaptureActive() ? TEXT("시간 지정 캡처 중지") : TEXT("시간 지정 캡처 시작")); }).OnClicked_Lambda([this]() { ToggleTimedCapture(); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("OpenRoot", "저장 루트 열기")).OnClicked_Lambda([this]() { OpenCaptureRootFolder(); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("OpenLast", "최근 결과 위치 열기")).OnClicked_Lambda([this]() { OpenLastResultFolder(); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("CopyLast", "최근 저장 경로 복사")).OnClicked_Lambda([this]() { CopyLastResultPath(); return FReply::Handled(); }) ]
            ]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SScrollBox).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text(LOCTEXT("ServerTransportTitle", "외부 서버 전송")) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this]() { return FText::FromString(bUseStompTransport ? TEXT("방식: Artemis STOMP") : TEXT("방식: HTTP POST")); }).OnClicked_Lambda([this]() { bUseStompTransport = !bUseStompTransport; return FReply::Handled(); }) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("ApplyServer", "설정 적용")).OnClicked_Lambda([this]() { ApplyTransportProfile(); return FReply::Handled(); }) ]
						+ SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("TestServer", "연결 시험")).OnClicked_Lambda([this]() { TestServerConnection(); return FReply::Handled(); }) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("SendPayload", "현재 Payload 전송")).OnClicked_Lambda([this]() { SendSelectedPayloadToServer(); return FReply::Handled(); }) ] ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).HintText(LOCTEXT("BrokerUrl", "Broker URL (ws://127.0.0.1:61616)")).Text_Lambda([this]() { return FText::FromString(bUseStompTransport ? DraftBrokerUrl : DraftHttpEndpoint); }).OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { if (bUseStompTransport) DraftBrokerUrl = T.ToString(); else DraftHttpEndpoint = T.ToString(); }) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("LidarTopic", "LiDAR Topic")).Text_Lambda([this]() { return FText::FromString(DraftLidarTopic); }).OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { DraftLidarTopic = T.ToString(); }) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("CameraTopic", "Camera Topic")).Text_Lambda([this]() { return FText::FromString(DraftCameraTopic); }).OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { DraftCameraTopic = T.ToString(); }) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("StompUser", "사용자명")).Text_Lambda([this]() { return FText::FromString(DraftUserName); }).OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { DraftUserName = T.ToString(); }) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).IsPassword(true).HintText(LOCTEXT("StompPassword", "비밀번호 (세션에만 유지)")).Text_Lambda([this]() { return FText::FromString(SessionPasscode); }).OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { SessionPasscode = T.ToString(); }) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("AckTopic", "소비자 ACK Topic (선택)")).Text_Lambda([this]() { return FText::FromString(DraftAckTopic); }).OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { DraftAckTopic = T.ToString(); }) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Collapsed : EVisibility::Visible; }).IsPassword(true).HintText(LOCTEXT("BearerToken", "Bearer token (세션에만 유지)")).Text_Lambda([this]() { return FText::FromString(SessionBearerToken); }).OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { SessionBearerToken = T.ToString(); }) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).HintText(LOCTEXT("MaxMessageBytes", "최대 메시지 bytes")).Text_Lambda([this]() { return FText::AsNumber(DraftMaxMessageBytes); }).OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { DraftMaxMessageBytes = FMath::Max(1024, FCString::Atoi(*T.ToString())); }) ]
					+ SVerticalBox::Slot().AutoHeight()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("SendLastExport", "최근 내보내기 파일 전송")).OnClicked_Lambda([this]() { SendLastExportToServer(); return FReply::Handled(); }) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(GetTransportSummaryText()); }) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[ SAssignNew(NativeStorageText, STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text(FText::FromString(GetStorageSummaryText())) ]
				]
			]
        ];
}

void UVirtualSensorCaptureExportPanelWidget::AddResult(EVirtualSensorExportKind Kind, const FString& SensorId, bool bSucceeded, const FString& Path, const FString& Message)
{
    FVirtualSensorExportResult Result;
    Result.Kind = Kind;
    Result.SensorId = SensorId;
    Result.bSucceeded = bSucceeded;
    Result.AbsolutePath = Path.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(Path);
    Result.FileSizeBytes = Result.AbsolutePath.IsEmpty() ? 0 : IFileManager::Get().FileSize(*Result.AbsolutePath);
    Result.Message = Message;
    Result.TimestampUtc = FDateTime::UtcNow();
    RecentResults.Insert(Result, 0);
    if (RecentResults.Num() > 8) RecentResults.SetNum(8);
    OnExportCompleted.Broadcast(Result);
    RefreshNativeText();
}

void UVirtualSensorCaptureExportPanelWidget::RefreshNativeText()
{
    if (NativeStorageText.IsValid()) NativeStorageText->SetText(FText::FromString(GetStorageSummaryText()));
}

FString UVirtualSensorCaptureExportPanelWidget::GetSelectedSensorId() const
{
    if (!SensorManager) return FString();
    if (MonitorWidget && MonitorWidget->IsShowingLidar())
    {
        const UVirtualLidarScanComponent* Lidar = SensorManager->GetSelectedLidar();
        return Lidar ? Lidar->SensorId : FString();
    }
    const UVirtualCameraCaptureComponent* Camera = SensorManager->GetSelectedCamera();
    return Camera ? Camera->SensorId : FString();
}

FString UVirtualSensorCaptureExportPanelWidget::ExportKindText(EVirtualSensorExportKind Kind) const
{
    if (Kind == EVirtualSensorExportKind::ServerPayload) return TEXT("Server Payload JSON");
    if (Kind == EVirtualSensorExportKind::PointCloudCsv) return TEXT("CSV");
    if (Kind == EVirtualSensorExportKind::PointCloudJsonLines) return TEXT("JSONL");
    if (Kind == EVirtualSensorExportKind::PointCloudPcd) return TEXT("PCD");
    if (Kind == EVirtualSensorExportKind::PointCloudLas) return TEXT("LAS");
    if (Kind == EVirtualSensorExportKind::PointCloudLaz) return TEXT("LAZ");
    return TEXT("시간 지정 캡처");
}

#undef LOCTEXT_NAMESPACE

EVirtualSensorExportKind UVirtualSensorCaptureExportPanelWidget::CyclePointCloudKind(EVirtualSensorExportKind Kind) const
{
    if (Kind == EVirtualSensorExportKind::PointCloudCsv) return EVirtualSensorExportKind::PointCloudJsonLines;
    if (Kind == EVirtualSensorExportKind::PointCloudJsonLines) return EVirtualSensorExportKind::PointCloudPcd;
    if (Kind == EVirtualSensorExportKind::PointCloudPcd) return EVirtualSensorExportKind::PointCloudLas;
    if (Kind == EVirtualSensorExportKind::PointCloudLas) return EVirtualSensorExportKind::PointCloudLaz;
    return EVirtualSensorExportKind::PointCloudCsv;
}
