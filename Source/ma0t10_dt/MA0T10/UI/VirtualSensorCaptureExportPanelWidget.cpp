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
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
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
	if (UVirtualSensorStreamPublisherComponent* Publisher = SensorManager ? SensorManager->StreamPublisherComponent : nullptr)
	{
		const FString LidarId = GetSelectedSensorIdForStream(EVirtualSensorStreamKind::PointCloud);
		if (!LidarId.IsEmpty() && Publisher->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud, LidarId))
		{
			ApplyStreamConfig(EVirtualSensorStreamKind::PointCloud, LidarId, true);
		}
		if (Publisher->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud, FString()))
		{
			ApplyStreamConfig(EVirtualSensorStreamKind::PointCloud, FString(), true);
		}
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
		ActiveTab = static_cast<EVirtualSensorCaptureExportTab>(FMath::Clamp<int32>(Preferences->CaptureExportActiveTab, 0, 3));
		StreamFrameStride = FMath::Max(1, Preferences->SensorStreamFrameStride);
		StreamReceiptInterval = FMath::Max(1, Preferences->SensorStreamReceiptInterval);
	}
	TSharedPtr<EVirtualSensorExportKind> InitiallySelected = NativeExportKindOptions[0];
	for (const TSharedPtr<EVirtualSensorExportKind>& Option : NativeExportKindOptions) if (Option.IsValid() && *Option == SelectedPointCloudKind) { InitiallySelected = Option; break; }

	#if 0
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
	#endif
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
					+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("TitleV2", "캡처 및 내보내기  |  제목을 드래그해 이동 · 우하단을 드래그해 크기 조절")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this]() { return FText::FromString(IsPanelCollapsed() ? TEXT("펼치기") : TEXT("접기")); }).OnClicked_Lambda([this]() { TogglePanelCollapsed(); return FReply::Handled(); }) ]
					+ SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("ResetUiV2", "위치·크기 초기화")).OnClicked_Lambda([this]() { ResetPanelPosition(); ResetPanelSize(); return FReply::Handled(); }) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 5.0f)[ SNew(STextBlock).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); }).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("선택: %s · SensorId: %s"), MonitorWidget && MonitorWidget->IsShowingLidar() ? TEXT("LiDAR") : TEXT("카메라"), GetSelectedSensorId().IsEmpty() ? TEXT("없음") : *GetSelectedSensorId())); }) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 6.0f)
			[
				SNew(SWrapBox).UseAllottedSize(true).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this]() { return TabLabel(EVirtualSensorCaptureExportTab::LiveStream); }).OnClicked_Lambda([this]() { SetActiveTab(EVirtualSensorCaptureExportTab::LiveStream); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this]() { return TabLabel(EVirtualSensorCaptureExportTab::Capture); }).OnClicked_Lambda([this]() { SetActiveTab(EVirtualSensorCaptureExportTab::Capture); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this]() { return TabLabel(EVirtualSensorCaptureExportTab::Export); }).OnClicked_Lambda([this]() { SetActiveTab(EVirtualSensorCaptureExportTab::Export); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this]() { return TabLabel(EVirtualSensorCaptureExportTab::ConnectionLog); }).OnClicked_Lambda([this]() { SetActiveTab(EVirtualSensorCaptureExportTab::ConnectionLog); return FReply::Handled(); }) ]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SWidgetSwitcher)
				.Visibility_Lambda([this]() { return GetPanelBodyVisibility(); })
				.WidgetIndex_Lambda([this]() { return static_cast<int32>(ActiveTab); })
				+ SWidgetSwitcher::Slot()[ BuildLiveStreamTab() ]
				+ SWidgetSwitcher::Slot()[ BuildCaptureTab() ]
				+ SWidgetSwitcher::Slot()[ BuildExportTab(InitiallySelected) ]
				+ SWidgetSwitcher::Slot()[ BuildConnectionLogTab() ]
			]
		];
}

void UVirtualSensorCaptureExportPanelWidget::SetActiveTab(EVirtualSensorCaptureExportTab NewTab)
{
	ActiveTab = NewTab;
	if (UVirtualSensorUiPreferencesSaveGame* Preferences = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate())
	{
		Preferences->CaptureExportActiveTab = static_cast<uint8>(NewTab);
		UVirtualSensorUiPreferencesSaveGame::Save(Preferences);
	}
}

FText UVirtualSensorCaptureExportPanelWidget::TabLabel(EVirtualSensorCaptureExportTab Tab) const
{
	const TCHAR* Label = Tab == EVirtualSensorCaptureExportTab::LiveStream ? TEXT("실시간 전송")
		: Tab == EVirtualSensorCaptureExportTab::Capture ? TEXT("캡처")
		: Tab == EVirtualSensorCaptureExportTab::Export ? TEXT("내보내기")
		: TEXT("연결·로그");
	return FText::FromString(FString::Printf(TEXT("%s%s"), ActiveTab == Tab ? TEXT("● ") : TEXT("○ "), Label));
}

FString UVirtualSensorCaptureExportPanelWidget::GetSelectedSensorIdForStream(EVirtualSensorStreamKind StreamKind) const
{
	if (!SensorManager) return FString();
	if (StreamKind == EVirtualSensorStreamKind::CameraImage)
	{
		const UVirtualCameraCaptureComponent* Camera = SensorManager->GetSelectedCamera();
		return Camera ? Camera->SensorId : FString();
	}
	const UVirtualLidarScanComponent* Lidar = SensorManager->GetSelectedLidar();
	return Lidar ? Lidar->SensorId : FString();
}

void UVirtualSensorCaptureExportPanelWidget::ApplyStreamConfig(EVirtualSensorStreamKind StreamKind, const FString& SensorId, bool bEnabled)
{
	UVirtualSensorStreamPublisherComponent* Publisher = SensorManager ? SensorManager->StreamPublisherComponent : nullptr;
	if (!Publisher)
	{
		LastUiMessage = TEXT("실시간 스트림 발행기가 연결되지 않았습니다.");
		return;
	}
	FVirtualSensorStreamConfig Config;
	Config.SensorId = SensorId;
	Config.StreamKind = StreamKind;
	Config.bEnabled = bEnabled;
	Config.FrameStride = FMath::Max(1, StreamFrameStride);
	Config.ReceiptSampleInterval = FMath::Max(1, StreamReceiptInterval);
	if (SelectedPointCloudKind == EVirtualSensorExportKind::PointCloudJsonLines) Config.PointCloudFormat = EVirtualPointCloudStreamFormat::JSONL;
	else if (SelectedPointCloudKind == EVirtualSensorExportKind::PointCloudPcd) Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
	else if (SelectedPointCloudKind == EVirtualSensorExportKind::PointCloudLas) Config.PointCloudFormat = EVirtualPointCloudStreamFormat::LAS;
	else if (SelectedPointCloudKind == EVirtualSensorExportKind::PointCloudLaz) Config.PointCloudFormat = EVirtualPointCloudStreamFormat::LAZ;
	else Config.PointCloudFormat = EVirtualPointCloudStreamFormat::CSV;
	if (const UVirtualLidarScanComponent* Lidar = SensorManager->GetSelectedLidar())
	{
		Config.LazCompressorPath = Lidar->ExternalLazCompressorPath;
		Config.LazCompressorArguments = Lidar->ExternalLazCompressorArguments;
	}
	Publisher->ConfigureStream(Config);
	LastUiMessage = FString::Printf(TEXT("%s %s 스트림을 %s했습니다."), SensorId.IsEmpty() ? TEXT("전체") : *SensorId,
		StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("Camera") : StreamKind == EVirtualSensorStreamKind::PointCloud ? TEXT("Point Cloud") : TEXT("LiDAR Payload"),
		bEnabled ? TEXT("시작") : TEXT("중지"));
}

void UVirtualSensorCaptureExportPanelWidget::ToggleSelectedStream(EVirtualSensorStreamKind StreamKind)
{
	UVirtualSensorStreamPublisherComponent* Publisher = SensorManager ? SensorManager->StreamPublisherComponent : nullptr;
	const FString SensorId = GetSelectedSensorIdForStream(StreamKind);
	if (!Publisher || SensorId.IsEmpty())
	{
		LastUiMessage = StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("선택된 Camera가 없습니다.") : TEXT("선택된 LiDAR가 없습니다.");
		return;
	}
	ApplyStreamConfig(StreamKind, SensorId, !Publisher->IsStreamEnabled(StreamKind, SensorId));
}

void UVirtualSensorCaptureExportPanelWidget::ToggleAllStreams()
{
	UVirtualSensorStreamPublisherComponent* Publisher = SensorManager ? SensorManager->StreamPublisherComponent : nullptr;
	if (!Publisher)
	{
		LastUiMessage = TEXT("실시간 스트림 발행기가 연결되지 않았습니다.");
		return;
	}
	bool bAnyEnabled = false;
	for (const FVirtualSensorStreamStatus& Status : Publisher->GetStreamStatuses()) bAnyEnabled |= Status.bEnabled;
	if (bAnyEnabled)
	{
		Publisher->StopAllStreams(FString());
		LastUiMessage = TEXT("모든 실시간 스트림을 중지했습니다.");
		return;
	}
	ApplyStreamConfig(EVirtualSensorStreamKind::LidarPayload, FString(), true);
	ApplyStreamConfig(EVirtualSensorStreamKind::CameraImage, FString(), true);
	ApplyStreamConfig(EVirtualSensorStreamKind::PointCloud, FString(), true);
}

FString UVirtualSensorCaptureExportPanelWidget::GetLiveStreamSummaryText() const
{
	const UVirtualSensorStreamPublisherComponent* Publisher = SensorManager ? SensorManager->StreamPublisherComponent : nullptr;
	if (!Publisher) return TEXT("스트림 발행기 연결 없음");
	FString Text = TEXT("스트림은 센서 측정을 막지 않습니다. 처리 중일 때는 가장 최신 프레임 하나만 남기고 이전 대기 프레임을 교체합니다.\n");
	const TArray<FVirtualSensorStreamStatus> Statuses = Publisher->GetStreamStatuses();
	if (Statuses.IsEmpty()) return Text + TEXT("아직 시작한 스트림이 없습니다.");
	for (const FVirtualSensorStreamStatus& Status : Statuses)
	{
		const FString Kind = Status.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("Camera")
			: Status.StreamKind == EVirtualSensorStreamKind::PointCloud ? TEXT("Point Cloud") : TEXT("LiDAR Payload");
		Text += FString::Printf(TEXT("\n[%s] %s / %s · 입력 %.1fHz · 전송 %.1fHz · frame %lld · 교체 %lld · 대역폭대기 %lld · receipt timeout %lld\n  %s"),
			Status.bEnabled ? TEXT("실행") : TEXT("중지"), *Kind, Status.SensorId.IsEmpty() ? TEXT("전체 센서") : *Status.SensorId,
			Status.InputHz, Status.SubmittedHz, Status.LastSubmittedFrameId, Status.ReplacedPendingFrameCount,
			Status.BandwidthDeferredFrameCount, Status.ReceiptTimeoutCount, *Status.Message);
	}
	return Text;
}

FString UVirtualSensorCaptureExportPanelWidget::BuildTransportLogText() const
{
	const UVirtualSensorStreamPublisherComponent* Publisher = SensorManager ? SensorManager->StreamPublisherComponent : nullptr;
	if (!Publisher) return TEXT("전송 로그 없음");
	FString Text = TEXT("최근 전송 이벤트 (최대 200개, 화면에는 최근 40개 표시)\n");
	const TArray<FVirtualSensorTransportLogEntry> Entries = Publisher->GetRecentLogEntries();
	for (int32 Index = 0; Index < FMath::Min(40, Entries.Num()); ++Index)
	{
		const FVirtualSensorTransportLogEntry& Entry = Entries[Index];
		Text += FString::Printf(TEXT("\n%s [%s] sensor=%s frame=%lld bytes=%d %.1fms\n  request=%s topic=%s\n  %s"),
			*Entry.TimestampUtc.ToString(TEXT("%H:%M:%S")), *Entry.State, Entry.SensorId.IsEmpty() ? TEXT("-") : *Entry.SensorId,
			Entry.FrameId, Entry.Bytes, Entry.LatencyMs, Entry.RequestId.IsEmpty() ? TEXT("-") : *Entry.RequestId,
			Entry.Destination.IsEmpty() ? TEXT("-") : *Entry.Destination, *Entry.Message);
	}
	return Text;
}

void UVirtualSensorCaptureExportPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const double Now = FPlatformTime::Seconds();
	if (LastNativeStatusRefreshSeconds < 0.0 || Now - LastNativeStatusRefreshSeconds >= 0.2)
	{
		LastNativeStatusRefreshSeconds = Now;
		CachedLiveStreamSummary = GetLiveStreamSummaryText();
		CachedTransportLog = BuildTransportLogText();
		RefreshNativeText();
	}
}

bool UVirtualSensorCaptureExportPanelWidget::ExportTransportDiagnosticReport()
{
	UVirtualSensorStreamPublisherComponent* Publisher = SensorManager ? SensorManager->StreamPublisherComponent : nullptr;
	FString JsonPath, MarkdownPath;
	const bool bSaved = Publisher && Publisher->ExportDiagnosticReport(JsonPath, MarkdownPath);
	LastUiMessage = bSaved
		? FString::Printf(TEXT("전송 진단 보고서를 저장했습니다.\nJSON: %s\nMarkdown: %s"), *JsonPath, *MarkdownPath)
		: TEXT("전송 진단 보고서 저장에 실패했습니다.");
	RefreshNativeText();
	return bSaved;
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildLiveStreamTab()
{
	auto StreamButtonText = [this](EVirtualSensorStreamKind Kind)
	{
		const UVirtualSensorStreamPublisherComponent* Publisher = SensorManager ? SensorManager->StreamPublisherComponent : nullptr;
		const FString SensorId = GetSelectedSensorIdForStream(Kind);
		const bool bEnabled = Publisher && !SensorId.IsEmpty() && Publisher->IsStreamEnabled(Kind, SensorId);
		const TCHAR* KindText = Kind == EVirtualSensorStreamKind::CameraImage ? TEXT("Camera 이미지") : Kind == EVirtualSensorStreamKind::PointCloud ? TEXT("Point Cloud") : TEXT("LiDAR 값");
		return FText::FromString(FString::Printf(TEXT("%s %s"), KindText, bEnabled ? TEXT("중지") : TEXT("시작")));
	};
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text(LOCTEXT("LiveTitle", "세 가지 독립 실시간 스트림")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text(LOCTEXT("LiveHelp", "LiDAR 값은 virtual-lidar.v1 JSON, Camera는 virtual-camera.v1 JSON 안의 Base64 JPEG, Point Cloud는 선택한 CSV/JSONL/PCD/LAS/LAZ를 virtual-pointcloud.v1 봉투에 담아 스캔 완료 때 전송합니다.")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)[ SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([StreamButtonText]() { return StreamButtonText(EVirtualSensorStreamKind::LidarPayload); }).OnClicked_Lambda([this]() { ToggleSelectedStream(EVirtualSensorStreamKind::LidarPayload); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([StreamButtonText]() { return StreamButtonText(EVirtualSensorStreamKind::CameraImage); }).OnClicked_Lambda([this]() { ToggleSelectedStream(EVirtualSensorStreamKind::CameraImage); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([StreamButtonText]() { return StreamButtonText(EVirtualSensorStreamKind::PointCloud); }).OnClicked_Lambda([this]() { ToggleSelectedStream(EVirtualSensorStreamKind::PointCloud); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("ToggleAllStreams", "전체 스트림 시작/중지")).OnClicked_Lambda([this]() { ToggleAllStreams(); return FReply::Handled(); }) ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[ SNew(SEditableTextBox).HintText(LOCTEXT("StreamStride", "전송 간격(프레임), 기본 1")).Text_Lambda([this]() { return FText::AsNumber(StreamFrameStride); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { StreamFrameStride = FMath::Max(1, FCString::Atoi(*Text.ToString())); if (UVirtualSensorUiPreferencesSaveGame* P = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate()) { P->SensorStreamFrameStride = StreamFrameStride; UVirtualSensorUiPreferencesSaveGame::Save(P); } }) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[ SNew(SEditableTextBox).HintText(LOCTEXT("ReceiptInterval", "자동 receipt 표본 간격, 기본 10")).Text_Lambda([this]() { return FText::AsNumber(StreamReceiptInterval); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { StreamReceiptInterval = FMath::Max(1, FCString::Atoi(*Text.ToString())); if (UVirtualSensorUiPreferencesSaveGame* P = UVirtualSensorUiPreferencesSaveGame::LoadOrCreate()) { P->SensorStreamReceiptInterval = StreamReceiptInterval; UVirtualSensorUiPreferencesSaveGame::Save(P); } }) ]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 6)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(CachedLiveStreamSummary.IsEmpty() ? GetLiveStreamSummaryText() : CachedLiveStreamSummary); }) ]
		];
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildCaptureTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text(LOCTEXT("CaptureTitle", "수동 및 시간 지정 캡처")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)[ SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("CaptureOnceV2", "선택 센서 1회 캡처")).OnClicked_Lambda([this]() { CaptureOnce(); return FReply::Handled(); }) ]
			+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this]() { return FText::FromString(MonitorWidget && MonitorWidget->IsLocalSensorCaptureActive() ? TEXT("시간 지정 캡처 중지") : TEXT("시간 지정 캡처 시작")); }).OnClicked_Lambda([this]() { ToggleTimedCapture(); return FReply::Handled(); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text(LOCTEXT("CaptureHelp", "1회 캡처는 현재 센서 프레임을 즉시 요청합니다. 시간 지정 캡처는 로컬 Saved/SensorCaptures/LocalTimedCapture 아래에 기록하며 서버 스트림과 별개입니다.")) ];
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildExportTab(TSharedPtr<EVirtualSensorExportKind> InitiallySelected)
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text(LOCTEXT("ExportTitleV2", "파일 내보내기 및 최근 파일 전송")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)[ SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("ExportPayloadV2", "Server Payload JSON 저장")).OnClicked_Lambda([this]() { ExportServerPayload(); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SComboBox<TSharedPtr<EVirtualSensorExportKind>>).OptionsSource(&NativeExportKindOptions).InitiallySelectedItem(InitiallySelected).OnGenerateWidget_Lambda([this](TSharedPtr<EVirtualSensorExportKind> Item) { return SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text(FText::FromString(Item.IsValid() ? ExportKindText(*Item) : TEXT("CSV"))); }).OnSelectionChanged_Lambda([this](TSharedPtr<EVirtualSensorExportKind> Item, ESelectInfo::Type) { if (Item.IsValid()) SetSelectedPointCloudExportKind(*Item); })[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("형식: %s"), *ExportKindText(SelectedPointCloudKind))); }) ] ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("ExportPointV2", "Point Cloud 저장")).OnClicked_Lambda([this]() { ExportSelectedPointCloud(SelectedPointCloudKind); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("SendRecentV2", "최근 파일 수동 전송")).OnClicked_Lambda([this]() { SendLastExportToServer(); return FReply::Handled(); }) ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[ SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("OpenRootV2", "저장 루트 열기")).OnClicked_Lambda([this]() { OpenCaptureRootFolder(); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("OpenLastV2", "최근 결과 위치 열기")).OnClicked_Lambda([this]() { OpenLastResultFolder(); return FReply::Handled(); }) ]
				+ SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("CopyLastV2", "최근 경로 복사")).OnClicked_Lambda([this]() { CopyLastResultPath(); return FReply::Handled(); }) ]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 6)[ SAssignNew(NativeStorageText, STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text(FText::FromString(GetStorageSummaryText())) ]
		];
}

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildConnectionLogTab()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::Accent).Text(LOCTEXT("ConnectionTitle", "Artemis STOMP / HTTP 연결 및 상세 로그")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[ SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text_Lambda([this]() { return FText::FromString(bUseStompTransport ? TEXT("방식: Artemis STOMP") : TEXT("방식: HTTP POST")); }).OnClicked_Lambda([this]() { bUseStompTransport = !bUseStompTransport; return FReply::Handled(); }) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("ApplyConnectionV2", "설정 적용")).OnClicked_Lambda([this]() { ApplyTransportProfile(); return FReply::Handled(); }) ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("TestConnectionV2", "연결 시험")).OnClicked_Lambda([this]() { TestServerConnection(); return FReply::Handled(); }) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("ManualPayloadV2", "현재 Payload 수동 전송")).OnClicked_Lambda([this]() { SendSelectedPayloadToServer(); return FReply::Handled(); }) ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).Text(LOCTEXT("SaveDiagnostic", "진단 보고서 저장")).OnClicked_Lambda([this]() { ExportTransportDiagnosticReport(); return FReply::Handled(); }) ]
			]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).HintText(LOCTEXT("BrokerUrlV2", "Broker URL (ws://127.0.0.1:61616)")).Text_Lambda([this]() { return FText::FromString(bUseStompTransport ? DraftBrokerUrl : DraftHttpEndpoint); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { if (bUseStompTransport) DraftBrokerUrl = Text.ToString(); else DraftHttpEndpoint = Text.ToString(); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("LidarTopicV2", "LiDAR 값 Topic")).Text_Lambda([this]() { return FText::FromString(DraftLidarTopic); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { DraftLidarTopic = Text.ToString(); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("CameraTopicV2", "Camera 이미지 Topic")).Text_Lambda([this]() { return FText::FromString(DraftCameraTopic); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { DraftCameraTopic = Text.ToString(); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("PointTopicV2", "Point Cloud Topic")).Text_Lambda([this]() { return FText::FromString(DraftExportTopic); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { DraftExportTopic = Text.ToString(); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("UserV2", "사용자명")).Text_Lambda([this]() { return FText::FromString(DraftUserName); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { DraftUserName = Text.ToString(); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).IsPassword(true).HintText(LOCTEXT("PasswordV2", "비밀번호 (세션에서만 유지)")).Text_Lambda([this]() { return FText::FromString(SessionPasscode); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { SessionPasscode = Text.ToString(); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Visible : EVisibility::Collapsed; }).HintText(LOCTEXT("AckTopicV2", "소비자 ACK Topic (선택)")).Text_Lambda([this]() { return FText::FromString(DraftAckTopic); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { DraftAckTopic = Text.ToString(); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).Visibility_Lambda([this]() { return bUseStompTransport ? EVisibility::Collapsed : EVisibility::Visible; }).IsPassword(true).HintText(LOCTEXT("BearerV2", "Bearer token (세션에서만 유지)")).Text_Lambda([this]() { return FText::FromString(SessionBearerToken); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { SessionBearerToken = Text.ToString(); }) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(SEditableTextBox).HintText(LOCTEXT("MaxBytesV2", "최대 메시지 bytes")).Text_Lambda([this]() { return FText::AsNumber(DraftMaxMessageBytes); }).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { DraftMaxMessageBytes = FMath::Max(1024, FCString::Atoi(*Text.ToString())); }) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(GetTransportSummaryText()); }) ]
			+ SVerticalBox::Slot().FillHeight(1.0f)[ SNew(STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(CachedTransportLog.IsEmpty() ? BuildTransportLogText() : CachedTransportLog); }) ]
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
