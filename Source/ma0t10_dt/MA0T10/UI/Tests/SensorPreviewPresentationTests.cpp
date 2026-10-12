#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorPresentationTestHelpers.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "Widgets/Text/STextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorPreviewPresentationTest,"MA0T10.UXR06.PreviewVocabularyEmptyActionsAndHiddenDiagnostics",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorPreviewPresentationTest::RunTest(const FString&)
{
    TestEqual(TEXT("uncapped warning uses equipment terms without exposing property names"),SensorToolUiText::UserReason(TEXT("Server payload is uncapped (MaxServerPayloadPoints=0)")),FString(TEXT("추가 출력 데이터의 점 수 제한이 없습니다")));
    TestEqual(TEXT("server setup guidance has no duplicated server noun"),SensorToolUiText::UserReason(TEXT("STOMP 서버 설정을 연결·진단에서 확인하세요")),FString(TEXT("서버 설정을 서버 연결 설정에서 확인하세요")));
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* C=World->SpawnActor<AVirtualSensorCoordinator>();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(C);
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Camera->StopSensor();Lidar->StopSensor();C->DiscoverSensorsInLevel();
    auto* Placement=NewObject<UVirtualSensorPlacementPanelWidget>(World);W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,Placement);
    auto* Monitor=NewObject<UVirtualSensorMonitorPanelWidget>(World);Monitor->bPersistMonitorPreferences=false;W->RegisterOwnedPanel(ESensorToolPanelRole::Monitor,Monitor);Monitor->BindSensorManager(C);Monitor->Initialize();Monitor->SetVisibility(ESlateVisibility::Visible);const auto Slate=Monitor->TakeWidget();
    for(auto* Actor:TArray<AVirtualSensorActorBase*>{Camera,Lidar})
    {
        const int64 Before=Actor->GetSensorRuntimeStatus().FrameId;
        C->SelectSensorActor(Actor);if(Actor==Camera){Monitor->BindVirtualCamera(Camera->CaptureComponent);Monitor->ShowCameraView();}else{Monitor->BindVirtualLidar(Lidar->ScanComponent);Monitor->ShowLidarView();}
        Monitor->RefreshOwnedMonitorText();
        TestFalse(TEXT("preview title never embeds the warning marker"),Monitor->OwnedMonitorStatus.ToString().Contains(TEXT("⚠")));
        TestTrue(TEXT("stopped preview clears the separate warning text"),Monitor->OwnedMonitorWarning.IsEmpty());
        TestTrue(TEXT("stopped device has explicit empty-state guidance"),Monitor->bOwnedPreviewEmpty&&Monitor->OwnedEmptyText.ToString().Contains(TEXT("측정 중지 상태"))&&Monitor->OwnedEmptyActionText.ToString()==TEXT("측정 시작"));
        TestFalse(TEXT("no measurement warning uses normal color"),Monitor->bOwnedHasWarning);TestFalse(TEXT("there is no yellow none warning row"),Monitor->NativeWarningTextBlock.IsValid());
        TArray<FString> Text;SensorPresentationTests::VisibleText(Slate,Text);const FString All=FString::Join(Text,TEXT("\n"));
        for(const auto& Word:SensorPresentationTests::ForbiddenWords())TestFalse(*FString::Printf(TEXT("preview basic has no %s"),*Word),All.Contains(Word,ESearchCase::IgnoreCase));
        TestFalse(TEXT("frame count removed from basic status"),Monitor->OwnedMonitorStatus.ToString().Contains(TEXT("프레임")));
        TestEqual(TEXT("showing stopped preview adds no acquisition"),Actor->GetSensorRuntimeStatus().FrameId,Before);
        if(Actor==Lidar)
        {
            Monitor->SetLidarProjectionMode(ELidarMonitorProjectionMode::WorldTopDown);Monitor->SetLidarColorMode(ELidarColorMode::RelativeHeight);
            TestEqual(TEXT("owned view changes never invoke the legacy synchronous scan"),Actor->GetSensorRuntimeStatus().FrameId,Before);
        }
        TestTrue(TEXT("empty action uses placement measurement start"),Monitor->StartOwnedMonitoredEquipment());TestTrue(TEXT("target actually enters measuring state"),Actor->IsSensorRunning());Monitor->RefreshOwnedMonitorText();
        const FString Warning=Monitor->BuildOwnedMeasurementWarning();TestEqual(TEXT("warning is preserved separately with full text"),Monitor->OwnedMonitorWarning.ToString(),Warning.IsEmpty()?FString():TEXT("⚠ ")+Warning);
        TestFalse(TEXT("measuring title excludes the warning marker"),Monitor->OwnedMonitorStatus.ToString().Contains(TEXT("⚠")));Actor->StopSensor();
    }
    Lidar->ScanComponent->AcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;Lidar->ScanComponent->HorizontalSamples=8;Lidar->ScanComponent->VerticalChannels=4;
    const int64 CompatibilityBefore=Lidar->GetSensorRuntimeStatus().FrameId;Monitor->RefreshLidarPreviewWithoutTransport(true);
    TestTrue(TEXT("explicit compatibility capture retains its synchronous acquisition"),Lidar->GetSensorRuntimeStatus().FrameId>CompatibilityBefore);
    Monitor->bOwnedAdvancedMenuOpen=false;Monitor->bMonitorDetailsExpanded=true;Monitor->OwnedRendererText=FText::FromString(TEXT("closed diagnostics sentinel"));Monitor->RefreshOwnedMonitorText();TestEqual(TEXT("closed view menu does not assemble diagnostics"),Monitor->OwnedRendererText.ToString(),FString(TEXT("closed diagnostics sentinel")));
    const auto Menu=Monitor->BuildOwnedAdvancedViewMenu();Monitor->bOwnedAdvancedMenuOpen=true;Monitor->bMonitorDetailsExpanded=false;Monitor->OwnedLegendText=FText::FromString(TEXT("collapsed legend sentinel"));Monitor->RefreshOwnedMonitorText();TestEqual(TEXT("collapsed legend does not assemble text"),Monitor->OwnedLegendText.ToString(),FString(TEXT("collapsed legend sentinel")));
    Monitor->bMonitorDetailsExpanded=true;Monitor->RefreshOwnedMonitorText();TestTrue(TEXT("expanded developer details retain Payload diagnostics"),Monitor->NativeDetailedStatusTextBlock&&Monitor->NativeDetailedStatusTextBlock->GetText().ToString().Contains(TEXT("Payload")));
    Monitor->OwnedMonitorStatus=FText::FromString(TEXT("hidden sentinel"));Monitor->SetVisibility(ESlateVisibility::Hidden);Monitor->NativeTick(FGeometry(),.3);TestEqual(TEXT("hidden preview does not format status"),Monitor->OwnedMonitorStatus.ToString(),FString(TEXT("hidden sentinel")));
    Monitor->SetVisibility(ESlateVisibility::Visible);Monitor->BindVirtualCamera(nullptr);Monitor->BindVirtualLidar(nullptr);Monitor->RefreshOwnedMonitorText();TestFalse(TEXT("unbound preview is explicit no equipment"),Monitor->bOwnedHasEquipment);TestEqual(TEXT("no equipment offers placement add"),Monitor->OwnedEmptyActionText.ToString(),FString(TEXT("장비 추가")));TestTrue(TEXT("add action opens existing placement instance"),Monitor->OpenOwnedEquipmentPlacement()&&W->IsPanelOpen(ESensorToolPanelRole::Placement));
    TestFalse(TEXT("stale/unbound action never starts the former target"),Monitor->StartOwnedMonitoredEquipment());TestFalse(TEXT("former camera remains stopped"),Camera->IsSensorRunning());TestFalse(TEXT("former lidar remains stopped"),Lidar->IsSensorRunning());
    W->UnregisterPanel(Monitor);W->UnregisterPanel(Placement);C->Destroy();Camera->Destroy();Lidar->Destroy();return true;
}
#endif
