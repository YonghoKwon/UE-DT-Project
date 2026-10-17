#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorLidarHelp.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "Json.h"

// Opt-in captures use existing assets in memory. No Actor/map/user setting is saved.
class FInspectionUiCapture final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),ReadyAt=0;
    int32 Phase=0;
    bool Requested=false,Prepared=false;
    FString Path;
    FDateTime Previous;
    TUniquePtr<TGuardValue<bool>> Debug;
    static constexpr int32 Count=8;
    const TCHAR* Names[Count]={TEXT("rail-preview"),TEXT("lidar-normal"),TEXT("lidar-points"),TEXT("camera-dual"),TEXT("imu-owner"),TEXT("lidar-fidelity"),TEXT("lidar-detail-end"),TEXT("numeric-basic")};
    static void Visit(const TSharedRef<SWidget>& Item,TFunctionRef<void(const TSharedRef<SWidget>&)> Action)
    {
        Action(Item);auto* Children=Item->GetChildren();
        for(int32 I=0;I<Children->Num();++I)Visit(Children->GetChildAt(I),Action);
    }
    static void Scroll(const TSharedRef<SWidget>& Root,bool End)
    {
        Visit(Root,[&](const TSharedRef<SWidget>& Item){if(Item->GetType()==TEXT("SScrollBox")){auto Box=StaticCastSharedRef<SScrollBox>(Item);if(End)Box->ScrollToEnd();else Box->ScrollToStart();}});
    }
    static void Expand(const TSharedRef<SWidget>& Root,const FString& Prefix)
    {
        Visit(Root,[&](const TSharedRef<SWidget>& Item){if(Item->GetType()!=TEXT("SExpandableArea"))return;
            bool Found=false;Visit(Item,[&](const TSharedRef<SWidget>& Child){if(Child->GetType()==TEXT("STextBlock")&&StaticCastSharedRef<STextBlock>(Child)->GetText().ToString().StartsWith(Prefix))Found=true;});
            if(Found)StaticCastSharedRef<SExpandableArea>(Item)->SetExpanded(true);
        });
    }
public:
    explicit FInspectionUiCapture(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const bool Manual=FParse::Param(FCommandLine::Get(),TEXT("Insp4Manual"));
        const bool Baseline=FParse::Param(FCommandLine::Get(),TEXT("Insp4Baseline"));
        if(FPlatformTime::Seconds()-Started>(Manual?1800:420)){Test->AddError(TEXT("INSP4 capture timeout"));return true;}
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();
        auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());
        auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));
        if(!Bar||!C||!Settings)return false;
        const int32 View=Phase%Count;const bool Wide=(Phase/Count)%2!=0;const float Scale=Phase<Count*2?1.f:1.5f;
        if(FParse::Param(FCommandLine::Get(),TEXT("Insp4DualOnly"))&&View!=3)return ++Phase==4*Count;
        if(!ReadyAt)
        {
            if(!Debug)Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            if(!Prepared){Prepared=true;W->ResetOwnedWorkspaceLayout();for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(Manual);
                auto* Extra=World->SpawnActor<AVirtualCameraSensorActor>();Extra->Tags.Add(TEXT("SensorTestManaged"));Extra->StopSensor();Extra->CaptureComponent->SensorId=TEXT("INSP4-CAMERA-B");Extra->CaptureComponent->CaptureResolution=FIntPoint(640,360);Extra->CaptureComponent->CaptureInterval=.1f;Extra->CaptureComponent->CaptureMode=EVirtualCameraCaptureMode::PreviewOnly;C->RegisterSensorActor(Extra);Extra->StartSensor();}
            FSlateApplication::Get().DismissAllMenus();W->SetSidebarMode(true);W->SetOwnedPanelFontScale(Scale);
            if(W->IsSidebarWide()!=Wide)W->ToggleSidebarWidth();
            const bool Camera=View==3;
            if(Camera)for(auto* A:C->GetSensorActors())if(A&&A->GetSensorKind()==EVirtualSensorKind::Camera)A->StartSensor();
            for(auto* A:C->GetSensorActors())if(A&&A->GetSensorKind()==(Camera?EVirtualSensorKind::Camera:EVirtualSensorKind::Lidar)){C->SelectSensorActor(A);W->SynchronizeOwnedSelection();break;}
            Bar->OpenOwnedDestination(View<4?ESensorToolDestination::Preview:View==4?ESensorToolDestination::Placement:ESensorToolDestination::SensorSettings);
            if(View<4){auto* M=Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));M->SetDualCameraModeEnabled(Camera);if(!Camera)M->SetMonitorPresentation(View==2?EVirtualSensorMonitorPresentation::PointCloudOnly:EVirtualSensorMonitorPresentation::TwoDimensional);}
            if(View==4){auto* P=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));P->AttachSelectedImu();P->OpenOwnedImuSettings();}
            if(View>=5){Settings->SelectOwnedSettingsTab(View==7?0:1);Scroll(Settings->TakeWidget(),View==6);}
            ReadyAt=FPlatformTime::Seconds()+1.2;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
        if(!Requested)
        {
            if(!Baseline&&View==5)
            {
                int32 Explanations=0;
                Visit(Settings->TakeWidget(),[&](const TSharedRef<SWidget>& Item){if(Item->GetType()!=TEXT("STextBlock"))return;
                    const auto Text=StaticCastSharedRef<STextBlock>(Item);for(const TCHAR* Key:{TEXT("PointTime"),TEXT("MotionDistortion"),TEXT("MaterialResponse"),TEXT("MultiHit")})
                    if(Text->GetText().EqualTo(SensorLidarHelp::Summary(Key))){++Explanations;const auto Actual=Text->GetCachedGeometry().GetLocalSize(),Desired=Text->GetDesiredSize();Test->AddInfo(FString::Printf(TEXT("F6 %s actual=%s desired=%s absolute=%s"),Key,*Actual.ToString(),*Desired.ToString(),*Text->GetCachedGeometry().GetAbsolutePosition().ToString()));Test->TestTrue(TEXT("fidelity explanation fits width and wrapped height at actual font/sidebar"),Actual.X+1>=Desired.X&&Actual.Y+1>=Desired.Y);}
                });
                Test->TestEqual(TEXT("four adjacent fidelity explanations rendered"),Explanations,4);
            }
            const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();
            Path=Root/FString::Printf(TEXT("%dx%d-%d-%s-%s.png"),Size.X,Size.Y,FMath::RoundToInt(Scale*100),Wide?TEXT("wide"):TEXT("narrow"),Names[View]);Previous=IFileManager::Get().GetTimeStamp(*Path);
            auto Record=MakeShared<FJsonObject>();Record->SetStringField(TEXT("capture"),Path);Record->SetNumberField(TEXT("width"),Size.X);Record->SetNumberField(TEXT("height"),Size.Y);Record->SetNumberField(TEXT("font_scale"),Scale);Record->SetNumberField(TEXT("sidebar_width"),W->GetSidebarWidth());Record->SetBoolField(TEXT("wide"),Wide);Record->SetBoolField(TEXT("baseline"),Baseline);Record->SetStringField(TEXT("sensor"),C->GetSelectedSensorActor()?C->GetSelectedSensorActor()->GetSensorId():FString());
            if(View==3){auto* M=Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));Test->TestTrue(TEXT("capture contains dual Camera controls"),M->IsDualCameraModeEnabled());Record->SetNumberField(TEXT("camera_count"),C->GetCameraCount());Record->SetBoolField(TEXT("dual"),M->IsDualCameraModeEnabled());}
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Record->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);
            FString Json;FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*FPaths::ChangeExtension(Path,TEXT("json")));
            FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        if(Manual)return FPaths::FileExists(Root/TEXT("finish.flag"));
        Requested=false;ReadyAt=0;return ++Phase==4*Count;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspectionUiCaptureTest,"INSP4.CaptureRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInspectionUiCaptureTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT")).IsEmpty()){AddInfo(TEXT("SKIP explicit UXR18 report root required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FInspectionUiCapture(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
