#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorPresentationTestHelpers.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorSettingsPresentationTest,"MA0T10.UXR05.ThreeSectionsPreserveStateAndVocabulary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorSettingsPresentationTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* C=World->SpawnActor<AVirtualSensorCoordinator>();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(C);
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();Camera->StopSensor();Lidar->StopSensor();C->RegisterSensorActor(Camera);C->RegisterSensorActor(Lidar);
    auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings);Settings->BindSensorManager(C);Settings->SetVisibility(ESlateVisibility::Visible);Settings->Initialize();const auto Slate=Settings->TakeWidget();
    for(auto* Actor:TArray<AVirtualSensorActorBase*>{Camera,Lidar})
    {
        C->SelectSensorActor(Actor);Settings->SynchronizeWorkspaceSelection(true);
        FVirtualSensorEditableState Before;Actor->ReadEditableState(Before);
        for(int32 Tab=0;Tab<3;++Tab)
        {
            Settings->WorkspaceSettingsTab=Tab;Settings->bOwnedSettingHelpOpen=false;Settings->RefreshOwnedSettingsText();Slate->SlatePrepass();TArray<FString> Text;SensorPresentationTests::VisibleText(Slate,Text);
            const FString All=FString::Join(Text,TEXT("\n"));
            AddInfo(FString::Printf(TEXT("Settings visible tree kind=%d section=%d: %s"),int32(Settings->PendingState.TargetKind),Tab,*All));
            TestTrue(TEXT("three section entries remain reachable"),Text.Contains(TEXT("기본"))&&Text.Contains(TEXT("고급"))&&Text.Contains(TEXT("고급·호환")));
            if(Tab<2)
            {
                for(const auto& Word:SensorPresentationTests::ForbiddenWords())TestFalse(*FString::Printf(TEXT("basic/advanced have no %s"),*Word),All.Contains(Word,ESearchCase::IgnoreCase));
                for(const auto& Word:{TEXT("다중 검출 사용"),TEXT("자동 파일 저장"),TEXT("식별 정보 변경"),TEXT("저장 예약"),TEXT("배치 작업공간"),TEXT("위치 X")})TestFalse(TEXT("basic/advanced contain no compatibility or placement controls"),All.Contains(Word));
                if(Tab==0)TestFalse(TEXT("resolution edits moved to advanced"),All.Contains(TEXT("가로 해상도")));
                if(Tab==1&&Actor==Camera)TestTrue(TEXT("camera advanced exposes resolution and FOV"),All.Contains(TEXT("가로 해상도 (px)"))&&All.Contains(TEXT("수평 FOV")));
                if(Tab==1&&Actor==Lidar)TestTrue(TEXT("lidar advanced exposes samples and preview controls"),All.Contains(TEXT("수평 샘플 수"))&&All.Contains(TEXT("미리보기 최대 점 수"))&&All.Contains(TEXT("미리보기에서 검출점만 표시")));
            }
            else if(Actor==Lidar)TestTrue(TEXT("compatibility keeps legacy output and administrative access"),All.Contains(TEXT("서버 Payload"))&&All.Contains(TEXT("MultiHit"))&&All.Contains(TEXT("LiDAR 호환 JSON 생성"))&&All.Contains(TEXT("외부 측정 프레임 입력")));
        }
        FVirtualSensorEditableState After;Actor->ReadEditableState(After);
        TestEqual(TEXT("browsing does not change server stride"),After.ServerPayloadStride,Before.ServerPayloadStride);
        TestEqual(TEXT("browsing does not change compatibility policy"),After.CompatibilityOutputPolicy,Before.CompatibilityOutputPolicy);
        TestEqual(TEXT("browsing does not change nominal measurement period"),Actor==Camera?After.CameraCaptureInterval:After.LidarScanInterval,Actor==Camera?Before.CameraCaptureInterval:Before.LidarScanInterval);
        TestFalse(TEXT("browsing does not start stopped equipment"),Actor->IsSensorRunning());
    }
    Settings->OpenOwnedSettingsSection(true);TestEqual(TEXT("toolbar compatibility route still uses section two"),Settings->WorkspaceSettingsTab,2);Settings->OpenOwnedSettingsSection(false);TestEqual(TEXT("detail route starts at basic"),Settings->WorkspaceSettingsTab,0);
    Settings->OwnedHelpText=FText::FromString(TEXT("closed help sentinel"));Settings->bOwnedSettingHelpOpen=false;Settings->RefreshOwnedSettingsText();TestEqual(TEXT("closed help does not build detail strings"),Settings->OwnedHelpText.ToString(),FString(TEXT("closed help sentinel")));
    Settings->OwnedSelectionText=FText::FromString(TEXT("hidden sentinel"));Settings->SetVisibility(ESlateVisibility::Hidden);Settings->NativeTick(FGeometry(),.3);TestEqual(TEXT("hidden panel does not build captions"),Settings->OwnedSelectionText.ToString(),FString(TEXT("hidden sentinel")));
    Settings->SetVisibility(ESlateVisibility::Visible);Settings->OpenOwnedSettingHelp(TEXT("PreviewStride"));TestTrue(TEXT("info action opens existing help area"),Settings->bOwnedSettingHelpOpen);TestFalse(TEXT("owned detailed help uses current vocabulary"),Settings->OwnedHelpText.ToString().Contains(TEXT("Payload")));
    W->UnregisterPanel(Settings);
    auto* Designer=NewObject<UVirtualSensorSettingsPanelWidget>(World);Designer->Initialize();Designer->WidgetTree->RootWidget=Designer->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("DesignerSentinel"));W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Designer);UWidget* Root=Designer->WidgetTree->RootWidget.Get();Designer->OpenOwnedSettingsSection(true);Designer->TakeWidget();TestTrue(TEXT("Designer tree remains unchanged"),Designer->WidgetTree->RootWidget.Get()==Root);
    W->UnregisterPanel(Designer);W->UnregisterPanel(Settings);Settings->RemoveFromParent();C->Destroy();Camera->Destroy();Lidar->Destroy();return true;
}
#endif
