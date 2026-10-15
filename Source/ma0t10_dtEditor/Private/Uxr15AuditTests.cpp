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
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
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
        if(FPlatformTime::Seconds()-Started>240){Test->AddError(TEXT("UXR15 audit timeout"));return true;}
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
            if(Phase==0){W->ResetOwnedWorkspaceLayout();for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);}
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
