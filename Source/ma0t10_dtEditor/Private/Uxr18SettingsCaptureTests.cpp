#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
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
class FUxr18SettingsCapture final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),ReadyAt=0;
    int32 Phase=0;
    bool Requested=false;
    FString Path;
    FDateTime Previous;
    TUniquePtr<TGuardValue<bool>> Debug;
    static constexpr int32 Count=10;
    const TCHAR* Names[Count]={TEXT("lidar-basic"),TEXT("lidar-fidelity"),TEXT("lidar-detail-end"),TEXT("camera-basic"),TEXT("camera-truth"),TEXT("automation"),TEXT("automation-end"),TEXT("active-features"),TEXT("imu-owner"),TEXT("dataset-owner")};
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
    explicit FUxr18SettingsCapture(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        const bool Manual=FParse::Param(FCommandLine::Get(),TEXT("Uxr18Manual"));
        const bool Baseline=FParse::Param(FCommandLine::Get(),TEXT("Uxr18Baseline"));
        if(FPlatformTime::Seconds()-Started>(Manual?1800:420)){Test->AddError(TEXT("UXR18 capture timeout"));return true;}
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();
        auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());
        auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));
        if(!Bar||!C||!Settings)return false;
        const int32 View=Phase%Count;const bool Wide=(Phase/Count)%2!=0;const float Scale=Phase<Count*2?1.f:1.5f;
        if(!ReadyAt)
        {
            if(!Debug)Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            if(Phase==0){W->ResetOwnedWorkspaceLayout();for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);}
            FSlateApplication::Get().DismissAllMenus();W->SetSidebarMode(true);W->SetOwnedPanelFontScale(Scale);
            if(W->IsSidebarWide()!=Wide)W->ToggleSidebarWidth();
            const bool Camera=View==3||View==4;
            for(auto* A:C->GetSensorActors())if(A)
            {
                FVirtualSensorEditableState State;FString Error;
                if(A->ReadEditableState(State)){State.bCameraGroundTruth=false;State.bLidarPointTime=State.bLidarMotionDistortion=State.bLidarMaterialResponse=View==7;A->ApplyEditableState(State,Error);}
            }
            for(auto* A:C->GetSensorActors())if(A&&A->GetSensorKind()==(Camera?EVirtualSensorKind::Camera:EVirtualSensorKind::Lidar)){C->SelectSensorActor(A);W->SynchronizeOwnedSelection();break;}
            Bar->OpenOwnedDestination(View==8?ESensorToolDestination::Placement:View==9?ESensorToolDestination::FileSave:ESensorToolDestination::SensorSettings);
            if(View<8){Settings->SelectOwnedSettingsTab(View==5||View==6?2:View==1?(Baseline?0:1):View==2?1:View==4?(Baseline?0:1):0);Scroll(Settings->TakeWidget(),View==2||View==6);}
            if(View==8){auto* P=Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));if(Baseline){Expand(P->TakeWidget(),TEXT("IMU ·"));Scroll(P->TakeWidget(),true);}else P->OpenOwnedImuSettings();}
            if(View==9){auto* P=Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data));if(Baseline){Expand(P->TakeWidget(),TEXT("데이터셋"));Scroll(P->TakeWidget(),false);}else P->OpenOwnedDatasetSettings();}
            ReadyAt=FPlatformTime::Seconds()+1.2;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));
        if(!Requested)
        {
            if(!Baseline&&View==7)
            {
                bool Found=false;
                Visit(Settings->TakeWidget(),[&](const TSharedRef<SWidget>& Item){if(Item->GetType()!=TEXT("STextBlock"))return;const auto Text=StaticCastSharedRef<STextBlock>(Item);if(Text->GetText().ToString()!=TEXT("3개 켜짐"))return;Found=true;
                    Test->TestTrue(TEXT("enabled badge fits on one line at actual narrow/wide geometry"),Text->GetDesiredSize().X<=Text->GetCachedGeometry().GetLocalSize().X+1&&Text->GetCachedGeometry().GetLocalSize().Y<45*Scale);
                });
                Test->TestTrue(TEXT("actual rendered tab shows three enabled fidelity flags"),Found);
            }
            const auto Size=World->GetGameViewport()->Viewport->GetSizeXY();
            Path=Root/FString::Printf(TEXT("%dx%d-%d-%s-%s.png"),Size.X,Size.Y,FMath::RoundToInt(Scale*100),Wide?TEXT("wide"):TEXT("narrow"),Names[View]);Previous=IFileManager::Get().GetTimeStamp(*Path);
            auto Record=MakeShared<FJsonObject>();Record->SetStringField(TEXT("capture"),Path);Record->SetNumberField(TEXT("width"),Size.X);Record->SetNumberField(TEXT("height"),Size.Y);Record->SetNumberField(TEXT("font_scale"),Scale);Record->SetNumberField(TEXT("sidebar_width"),W->GetSidebarWidth());Record->SetBoolField(TEXT("wide"),Wide);Record->SetBoolField(TEXT("baseline"),Baseline);Record->SetStringField(TEXT("sensor"),C->GetSelectedSensorActor()?C->GetSelectedSensorActor()->GetSensorId():FString());
            auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Record->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);
            FString Json;FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*FPaths::ChangeExtension(Path,TEXT("json")));
            FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        if(Manual)return FPaths::FileExists(Root/TEXT("finish.flag"));
        Requested=false;ReadyAt=0;return ++Phase==4*Count;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr18SettingsCaptureTest,"UXR18.SettingsCaptureRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr18SettingsCaptureTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT")).IsEmpty()){AddInfo(TEXT("SKIP explicit UXR18 report root required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxr18SettingsCapture(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
