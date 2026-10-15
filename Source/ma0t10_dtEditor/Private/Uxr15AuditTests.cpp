#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "Json.h"

// Explicit audit fixture: existing map opened in memory, no assets saved or output started.
class FUxr15Audit final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),ReadyAt=0;
    int32 Phase=0;
    bool Requested=false;
    FString Path;
    FDateTime Previous;
    TUniquePtr<TGuardValue<bool>> Debug;
    static constexpr int32 Count=20;
    const TCHAR* Names[Count]={TEXT("placement"),TEXT("preview-lidar"),TEXT("scenario"),TEXT("archive"),TEXT("charts"),TEXT("pcd"),TEXT("pcd-advanced"),TEXT("results"),TEXT("file-save"),TEXT("dataset"),TEXT("dataset-ranges"),TEXT("server"),TEXT("diagnostics"),TEXT("lidar-basic"),TEXT("lidar-fidelity"),TEXT("camera-basic"),TEXT("camera-advanced"),TEXT("tools"),TEXT("preview-camera-options"),TEXT("preview-lidar-options")};
    static void Visit(const TSharedRef<SWidget>& Item,TFunctionRef<void(const TSharedRef<SWidget>&)> Action)
    {
        Action(Item);auto* Children=Item->GetChildren();
        for(int32 I=0;I<Children->Num();++I)Visit(Children->GetChildAt(I),Action);
    }
    static void Expand(const TSharedRef<SWidget>& Root,const FString& Label)
    {
        Visit(Root,[&](const TSharedRef<SWidget>& Item)
        {
            if(Item->GetType()!=TEXT("SExpandableArea"))return;
            bool Found=false;
            Visit(Item,[&](const TSharedRef<SWidget>& Child){if(Child->GetType()==TEXT("STextBlock")&&StaticCastSharedRef<STextBlock>(Child)->GetText().ToString()==Label)Found=true;});
            if(Found)StaticCastSharedRef<SExpandableArea>(Item)->SetExpanded(true);
        });
    }
    static void Scroll(const TSharedRef<SWidget>& Root,bool End)
    {
        Visit(Root,[&](const TSharedRef<SWidget>& Item){if(Item->GetType()==TEXT("SScrollBox")){auto Box=StaticCastSharedRef<SScrollBox>(Item);if(End)Box->ScrollToEnd();else Box->ScrollToStart();}});
    }
public:
    explicit FUxr15Audit(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const bool Manual=FParse::Param(FCommandLine::Get(),TEXT("Uxr15Manual"));
        if(FPlatformTime::Seconds()-Started>(Manual?2160:240)){Test->AddError(TEXT("UXR15 audit timeout"));return true;}
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();
        auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());
        auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));
        auto* Data=Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data));
        auto* Monitor=Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));
        if(!Bar||!C||!Settings||!Data||!Monitor)return false;
        const int32 View=Phase%Count;const float Scale=Phase<Count?1.f:1.5f;
        if(!ReadyAt)
        {
            if(!Debug)Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            if(Phase==0)
            {
                W->ResetOwnedWorkspaceLayout();for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);
                if(Manual)
                {
                    // Explicit transient targets for direct UI checks. No existing Actor is moved.
                    auto* Slab=World->SpawnActor<ASlabActor>();Slab->Tags.Add(TEXT("SensorTestManaged"));Slab->SetActorLocation(FVector(1500,2500,200));Slab->ApplyDatasetGeometry(FVector(800,200,50));Slab->SetDiagnosticHelpersVisible(false);
                    const FVector Origin=Slab->GetActorLocation()+FVector(-1000,-1000,800);const FTransform Pose((Slab->GetActorLocation()-Origin).Rotation(),Origin);
                    auto* CameraActor=World->SpawnActor<AVirtualCameraSensorActor>();CameraActor->Tags.Add(TEXT("SensorTestManaged"));CameraActor->StopSensor();CameraActor->SetActorTransform(Pose);CameraActor->CaptureComponent->SensorId=TEXT("UXR15-MANUAL-CAMERA");CameraActor->CaptureComponent->CaptureResolution=FIntPoint(640,360);CameraActor->CaptureComponent->CaptureInterval=.1f;CameraActor->CaptureComponent->SimulationQuality=EVirtualSensorSimulationQuality::Custom;CameraActor->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;C->RegisterSensorActor(CameraActor);
                    auto* LidarActor=World->SpawnActor<AVirtualLidarSensorActor>();LidarActor->Tags.Add(TEXT("SensorTestManaged"));LidarActor->StopSensor();LidarActor->SetActorTransform(Pose);LidarActor->ScanComponent->SensorId=TEXT("UXR15-MANUAL-LIDAR");LidarActor->ScanComponent->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);LidarActor->ScanComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);LidarActor->ScanComponent->OutputMode=EVirtualLidarOutputMode::None;C->RegisterSensorActor(LidarActor);
                    Data->DatasetDraft.Name=TEXT("UXR15_manual");Data->DatasetDraft.Scenes=3;Data->DatasetDraft.SettleFrames=3;Data->DatasetSlab=Slab;Data->DatasetCamera=CameraActor;Data->DatasetLidar=LidarActor;
                }
            }
            FSlateApplication::Get().DismissAllMenus();W->SetSidebarMode(true);W->SetOwnedPanelFontScale(Scale);
            const bool Camera=View==15||View==16||View==18;
            for(auto* A:C->GetSensorActors())if(A&&A->GetSensorKind()==(Camera?EVirtualSensorKind::Camera:EVirtualSensorKind::Lidar)){C->SelectSensorActor(A);C->SetViewMode(Camera?EVirtualSensorViewMode::Camera:EVirtualSensorViewMode::Lidar);W->SynchronizeOwnedSelection();break;}
            const ESensorToolDestination Routes[Count]={ESensorToolDestination::Placement,ESensorToolDestination::Preview,ESensorToolDestination::Scenario,ESensorToolDestination::Scenario,ESensorToolDestination::ScenarioCharts,ESensorToolDestination::PcdTransfer,ESensorToolDestination::PcdTransfer,ESensorToolDestination::Results,ESensorToolDestination::FileSave,ESensorToolDestination::FileSave,ESensorToolDestination::FileSave,ESensorToolDestination::ServerSettings,ESensorToolDestination::Diagnostics,ESensorToolDestination::SensorSettings,ESensorToolDestination::SensorSettings,ESensorToolDestination::SensorSettings,ESensorToolDestination::SensorSettings,ESensorToolDestination::Placement,ESensorToolDestination::Preview,ESensorToolDestination::Preview};
            Bar->OpenOwnedDestination(Routes[View]);
            if(View==3)W->SetPanelOpen(ESensorToolPanelRole::Replay,true);
            if(View==6){Expand(Data->TakeWidget(),TEXT("고급"));Scroll(Data->TakeWidget(),true);}
            if(View==8){if(auto Area=Data->DatasetArea.Pin())Area->SetExpanded(false);Scroll(Data->TakeWidget(),false);}
            if(View==9||View==10){if(auto Area=Data->DatasetArea.Pin())Area->SetExpanded(true);if(View==10)Expand(Data->TakeWidget(),TEXT("랜덤화 조건 · 켜기 / 최솟값 / 최댓값"));Scroll(Data->TakeWidget(),View==10);}
            if(View>=13&&View<=16){Settings->SelectOwnedSettingsTab(View==16?1:0);if(Settings->OwnedSettingsScroll){if(View==14)Settings->OwnedSettingsScroll->ScrollToEnd();else Settings->OwnedSettingsScroll->ScrollToStart();}}
            if(View==17)Bar->ShowOwnedToolsMenu();
            if(View>=18){Monitor->bOwnedAdvancedMenuOpen=true;FSlateApplication::Get().PushMenu(Monitor->TakeWidget(),FWidgetPath(),Monitor->BuildOwnedAdvancedViewMenu(),FVector2D(550,100),FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));}
            if(Manual&&FParse::Param(FCommandLine::Get(),TEXT("Uxr15DatasetManual"))){Data->OpenOwnedTab(ESensorOwnedDataTab::FileSave);W->SetPanelOpen(ESensorToolPanelRole::Data,true);if(auto Area=Data->DatasetArea.Pin())Area->SetExpanded(true);if(FParse::Param(FCommandLine::Get(),TEXT("Uxr15RangesManual"))){Expand(Data->TakeWidget(),TEXT("랜덤화 조건 · 켜기 / 최솟값 / 최댓값"));Scroll(Data->TakeWidget(),true);}}
            ReadyAt=FPlatformTime::Seconds()+1.2;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(!Requested)
        {
            const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
            const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();
            Path=Root/FString::Printf(TEXT("%dx%d-%d-%s.png"),Size.X,Size.Y,FMath::RoundToInt(Scale*100),Names[View]);Previous=IFileManager::Get().GetTimeStamp(*Path);
            auto Record=MakeShared<FJsonObject>();Record->SetStringField(TEXT("capture"),Path);Record->SetNumberField(TEXT("width"),Size.X);Record->SetNumberField(TEXT("height"),Size.Y);Record->SetNumberField(TEXT("font_scale"),Scale);
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Record->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);
            FString Json;FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*FPaths::ChangeExtension(Path,TEXT("json")));
            FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            Test->AddInfo(FString::Printf(TEXT("UXR15 automated render %s; direct mouse not tested"),*Path));return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        // A visible, stationary session for Computer Use; only its explicit finish flag
        // or timeout ends PIE. Automatic preparation is never direct mouse acceptance.
        if(Manual)
        {
            const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
            return FPaths::FileExists(Root/TEXT("finish.flag"))||FPlatformTime::Seconds()-Started>2100;
        }
        Requested=false;ReadyAt=0;Monitor->bOwnedAdvancedMenuOpen=false;return ++Phase==2*Count;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr15AuditTest,"UXR15.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr15AuditTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT")).IsEmpty()){AddInfo(TEXT("SKIP explicit UXR15 audit root required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr15Audit(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
