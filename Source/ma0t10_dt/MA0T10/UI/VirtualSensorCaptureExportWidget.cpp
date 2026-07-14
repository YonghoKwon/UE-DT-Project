#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportWidget.h"

#include "Blueprint/WidgetTree.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorComp.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorManager.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiStyle.h"

#define LOCTEXT_NAMESPACE "VirtualSensorCaptureExportWidget"

void UVirtualSensorCaptureExportWidget::BindSensorManager(AVirtualSensorManager* InSensorManager)
{
    SensorManager = InSensorManager;
    RefreshNativeText();
}

void UVirtualSensorCaptureExportWidget::BindMonitorWidget(UVirtualSensorMonitorWidget* InMonitorWidget)
{
    MonitorWidget = InMonitorWidget;
    RefreshNativeText();
}

void UVirtualSensorCaptureExportWidget::CaptureOnce()
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

bool UVirtualSensorCaptureExportWidget::ExportServerPayload()
{
    const bool bSaved = MonitorWidget && MonitorWidget->ExportSelectedSensorServerPayload(TEXT("panel_server_payload"));
    const FString Path = MonitorWidget ? MonitorWidget->GetLastManualExportPath() : FString();
    AddResult(EVirtualSensorExportKind::ServerPayload, GetSelectedSensorId(), bSaved, Path,
        MonitorWidget ? MonitorWidget->GetLastManualExportMessage() : TEXT("모니터 Widget이 연결되지 않았습니다"));
    return bSaved;
}

bool UVirtualSensorCaptureExportWidget::ExportSelectedPointCloud(EVirtualSensorExportKind Kind)
{
    UVirtualLidarSensorComp* Lidar = SensorManager ? SensorManager->GetSelectedLidar() : nullptr;
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

void UVirtualSensorCaptureExportWidget::ToggleTimedCapture()
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

bool UVirtualSensorCaptureExportWidget::OpenCaptureRootFolder()
{
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorCaptures")));
    IFileManager::Get().MakeDirectory(*Directory, true);
    FPlatformProcess::ExploreFolder(*Directory);
    return true;
}

bool UVirtualSensorCaptureExportWidget::OpenLastResultFolder()
{
    if (RecentResults.Num() <= 0 || RecentResults[0].AbsolutePath.IsEmpty()) return false;
    const FString Path = RecentResults[0].AbsolutePath;
    const FString Directory = IFileManager::Get().DirectoryExists(*Path) ? Path : FPaths::GetPath(Path);
    if (!IFileManager::Get().DirectoryExists(*Directory)) return false;
    FPlatformProcess::ExploreFolder(*Directory);
    return true;
}

bool UVirtualSensorCaptureExportWidget::CopyLastResultPath()
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

FString UVirtualSensorCaptureExportWidget::GetStorageSummaryText() const
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

TSharedRef<SWidget> UVirtualSensorCaptureExportWidget::RebuildWidget()
{
    if (WidgetTree && WidgetTree->RootWidget)
    {
        return Super::RebuildWidget();
    }
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
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("형식: %s"), *ExportKindText(SelectedPointCloudKind))); }).OnClicked_Lambda([this]() { SelectedPointCloudKind = CyclePointCloudKind(SelectedPointCloudKind); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("ExportPointCloud", "포인트 클라우드 내보내기")).OnClicked_Lambda([this]() { ExportSelectedPointCloud(SelectedPointCloudKind); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text_Lambda([this]() { return FText::FromString(MonitorWidget && MonitorWidget->IsLocalSensorCaptureActive() ? TEXT("시간 지정 캡처 중지") : TEXT("시간 지정 캡처 시작")); }).OnClicked_Lambda([this]() { ToggleTimedCapture(); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("OpenRoot", "저장 루트 열기")).OnClicked_Lambda([this]() { OpenCaptureRootFolder(); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("OpenLast", "최근 결과 위치 열기")).OnClicked_Lambda([this]() { OpenLastResultFolder(); return FReply::Handled(); }) ]
                + SWrapBox::Slot()[ SNew(SButton).ButtonStyle(&FVirtualSensorUiStyle::ButtonStyle()).ForegroundColor(FVirtualSensorUiStyle::PrimaryText).Text(LOCTEXT("CopyLast", "최근 저장 경로 복사")).OnClicked_Lambda([this]() { CopyLastResultPath(); return FReply::Handled(); }) ]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [ SNew(SScrollBox).Visibility_Lambda([this]() { return GetPanelBodyVisibility(); }) + SScrollBox::Slot()[ SAssignNew(NativeStorageText, STextBlock).ColorAndOpacity(FVirtualSensorUiStyle::SecondaryText).AutoWrapText(true).Text(FText::FromString(GetStorageSummaryText())) ] ]
        ];
}

void UVirtualSensorCaptureExportWidget::AddResult(EVirtualSensorExportKind Kind, const FString& SensorId, bool bSucceeded, const FString& Path, const FString& Message)
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

void UVirtualSensorCaptureExportWidget::RefreshNativeText()
{
    if (NativeStorageText.IsValid()) NativeStorageText->SetText(FText::FromString(GetStorageSummaryText()));
}

FString UVirtualSensorCaptureExportWidget::GetSelectedSensorId() const
{
    if (!SensorManager) return FString();
    if (MonitorWidget && MonitorWidget->IsShowingLidar())
    {
        const UVirtualLidarSensorComp* Lidar = SensorManager->GetSelectedLidar();
        return Lidar ? Lidar->SensorId : FString();
    }
    const UVirtualCameraComp* Camera = SensorManager->GetSelectedCamera();
    return Camera ? Camera->SensorId : FString();
}

FString UVirtualSensorCaptureExportWidget::ExportKindText(EVirtualSensorExportKind Kind) const
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

EVirtualSensorExportKind UVirtualSensorCaptureExportWidget::CyclePointCloudKind(EVirtualSensorExportKind Kind) const
{
    if (Kind == EVirtualSensorExportKind::PointCloudCsv) return EVirtualSensorExportKind::PointCloudJsonLines;
    if (Kind == EVirtualSensorExportKind::PointCloudJsonLines) return EVirtualSensorExportKind::PointCloudPcd;
    if (Kind == EVirtualSensorExportKind::PointCloudPcd) return EVirtualSensorExportKind::PointCloudLas;
    if (Kind == EVirtualSensorExportKind::PointCloudLas) return EVirtualSensorExportKind::PointCloudLaz;
    return EVirtualSensorExportKind::PointCloudCsv;
}
