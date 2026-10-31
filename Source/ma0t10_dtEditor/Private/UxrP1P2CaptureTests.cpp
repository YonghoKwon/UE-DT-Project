#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "EngineUtils.h"
#include "DynamicRHI.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Types/SlateAttributeMetaData.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"

namespace
{
FString UxrCaptureRoot(){return FPaths::ProjectSavedDir()/TEXT("Reports/UXR-P1P2a");}
void UxrVisibleSlateText(const TSharedRef<SWidget>& Widget,TArray<FString>& Text)
{
    FSlateAttributeMetaData::UpdateOnlyVisibilityAttributes(Widget.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
    if(!Widget->GetVisibility().IsVisible())return;
    FSlateAttributeMetaData::UpdateExceptVisibilityAttributes(Widget.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
    if(Widget->GetType()==TEXT("STextBlock"))Text.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)UxrVisibleSlateText(Children->GetChildAt(I),Text);
}
bool UxrHasText(const TSharedRef<SWidget>& W,const FString& Prefix)
{
    FSlateAttributeMetaData::UpdateExceptVisibilityAttributes(W.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
    if(W->GetType()==TEXT("STextBlock")&&StaticCastSharedRef<STextBlock>(W)->GetText().ToString().StartsWith(Prefix))return true;
    auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)if(UxrHasText(Children->GetChildAt(I),Prefix))return true;
    return false;
}
void UxrExpandFields(const TSharedRef<SWidget>& W)
{
    if(W->GetType()==TEXT("SExpandableArea")&&UxrHasText(W,TEXT("필드 상세")))StaticCastSharedRef<SExpandableArea>(W)->SetExpanded(true);
    auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)UxrExpandFields(Children->GetChildAt(I));
}
bool UxrAdditionalChecks(UCoilLoadingMonitor& M)
{
    TArray<UWidget*> Widgets;M.WidgetTree->GetAllWidgets(Widgets);
    for(auto* W:Widgets)if(auto* B=Cast<UButton>(W))if(auto* T=Cast<UTextBlock>(B->GetContent()))if(T->GetText().ToString().StartsWith(TEXT("추가 확인"))){B->OnClicked.Broadcast();return true;}
    return false;
}
class FUxrCaptureCommand final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;bool Hatch;int32 Step=0;bool Prepared=false,WaitingManual=false;
    double Started=FPlatformTime::Seconds(),Ready=0;
    TWeakObjectPtr<UUserWidget> OriginalMonitor;TWeakObjectPtr<UUserWidget> OriginalNavigation;
    TUniquePtr<TGuardValue<bool>> Debug;
    FString Scene;
    bool Capture(UWorld& W,UUserWidget& Monitor)
    {
        const auto View=W.GetGameViewport()->GetGameViewportWidget().ToSharedRef();
        TArray<FColor> Pixels;FIntVector Size;
        if(!Test->TestTrue(TEXT("Actual D3D12 game viewport Slate screenshot"),FSlateApplication::Get().TakeScreenshot(View,Pixels,Size)))return false;
        const auto Expected=FinalAcceptanceViewportSize();
        if(!Test->TestTrue(TEXT("Capture uses the requested actual client pixels"),Size.X==Expected.X&&Size.Y==Expected.Y))return false;
        const FString Name=FString::Printf(TEXT("%s-%dx%d"),*Scene,Size.X,Size.Y);
        const FString Root=UxrCaptureRoot()/TEXT("progress");IFileManager::Get().MakeDirectory(*Root,true);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Capture PNG saved"),FFileHelper::SaveArrayToFile(Png,*(Root/(Name+TEXT(".png")))));
        auto Meta=MakeShared<FJsonObject>();FString Commit;FParse::Value(FCommandLine::Get(),TEXT("UxrSourceCommit="),Commit);
        Meta->SetStringField(TEXT("commit"),Commit);Meta->SetStringField(TEXT("dtcore"),TEXT("bc60f05e2d1e64f4ffe1410cf3239fe24cb4d4c9"));
        Meta->SetStringField(TEXT("scene"),Scene);Meta->SetStringField(TEXT("map"),Hatch?TEXT("ShipHatchTestMap"):TEXT("CoilLoadingTestMap"));
        Meta->SetStringField(TEXT("rhi"),GDynamicRHI?GDynamicRHI->GetName():TEXT("None"));
        Meta->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
        Meta->SetBoolField(TEXT("automatic_capture_not_mouse_acceptance"),true);
        Meta->SetNumberField(TEXT("client_width"),Size.X);Meta->SetNumberField(TEXT("client_height"),Size.Y);
        const auto Window=FSlateApplication::Get().FindWidgetWindow(View);
        Meta->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);
        Meta->SetNumberField(TEXT("umg_scale"),UWidgetLayoutLibrary::GetViewportScale(&Monitor));
        Meta->SetNumberField(TEXT("monitor_font_scale"),Hatch?CastChecked<UShipHatchMonitorWidget>(&Monitor)->GetFontScale():CastChecked<UCoilLoadingMonitor>(&Monitor)->FontScale());
        Meta->SetBoolField(TEXT("same_monitor_instance"),OriginalMonitor.Get()==&Monitor);
        Test->TestTrue(TEXT("Capture does not recreate the monitor instance"),OriginalMonitor.Get()==&Monitor);
        auto* Workspace=W.GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        Meta->SetNumberField(TEXT("tools_font_scale"),Workspace->GetOwnedPanelFontScale());Meta->SetBoolField(TEXT("sidebar_wide"),Workspace->IsSidebarWide());
        Meta->SetBoolField(TEXT("slab_tools_enabled"),Workspace->AreSlabToolsEnabled());
        Meta->SetBoolField(TEXT("same_navigation_instance"),!OriginalNavigation.IsValid()||OriginalNavigation==Workspace->GetOwnedNavigation());
        if(!Hatch)
        {
            auto* Coil=CastChecked<UCoilLoadingMonitor>(&Monitor);Meta->SetNumberField(TEXT("unknown_reason_groups"),Coil->DataNeededCount);
            Meta->SetNumberField(TEXT("danger"),Coil->DangerCount);Meta->SetNumberField(TEXT("caution"),Coil->CautionCount);
            ACoilLoadingMonitorHost* Host=nullptr;for(TActorIterator<ACoilLoadingMonitorHost> It(&W);It;++It)Host=*It;
            const auto Inset=Host->GetToolsInset();Meta->SetNumberField(TEXT("tools_inset_left_ui"),Inset.X);Meta->SetNumberField(TEXT("tools_inset_bottom_ui"),Inset.Y);
            if(auto* Card=Monitor.WidgetTree->FindWidget(TEXT("CoilWorkflow")))if(auto* Slot=Cast<UCanvasPanelSlot>(Card->Slot))
            {Meta->SetNumberField(TEXT("coil_card_left_ui"),Slot->GetPosition().X);Test->TestTrue(TEXT("Coil controls remain to the right of the owned rail/sidebar"),Slot->GetPosition().X+1>=Inset.X);}
            if(auto* Data=Workspace->GetOwnedPanel(ESensorToolPanelRole::Data))if(Data->IsVisible()&&!Data->IsPanelCollapsed())
            {
                TArray<FString> Labels;UxrVisibleSlateText(Data->TakeWidget(),Labels);TArray<TSharedPtr<FJsonValue>> Values;
                for(const auto& Label:Labels){Values.Add(MakeShared<FJsonValueString>(Label));Test->TestFalse(TEXT("Actual coil data viewport has no Slab scenario/replay labels: ")+Label,Label.Contains(TEXT("Slab"))||Label.Contains(TEXT("시나리오"))||Label.Contains(TEXT("재생")));}
                Meta->SetArrayField(TEXT("visible_data_text"),Values);
            }
            if(const auto* Navigation=Cast<USensorToolToolbarWidget>(Workspace->GetOwnedNavigation()))
            {
                const auto& Status=Navigation->GetGlobalStatus();Meta->SetStringField(TEXT("warning"),Status.Warning.ToString());
                Test->TestTrue(TEXT("Actual coil status strip contains no scenario state or warning"),Status.Scenario.IsEmpty()&&!Status.Warning.ToString().Contains(TEXT("시나리오")));
            }
        }
        TArray<TSharedPtr<FJsonValue>> Text;TArray<UWidget*> Widgets;Monitor.WidgetTree->GetAllWidgets(Widgets);
        for(auto* Item:Widgets)if(auto* Label=Cast<UTextBlock>(Item))
        {
            bool Visible=true;for(UWidget* Parent=Label;Parent;Parent=Parent->GetParent())Visible&=Parent->IsVisible();
            if(Visible&&!Label->GetText().IsEmpty())Text.Add(MakeShared<FJsonValueString>(Label->GetText().ToString()));
            if(!Hatch&&Visible)
            {
                auto* Header=Monitor.WidgetTree->FindWidget(TEXT("CoilViewHeader"));bool InHeader=false;
                for(UWidget* Parent=Label;Parent;Parent=Parent->GetParent())InHeader|=Parent==Header;
                if(Header&&InHeader)
                {
                    const auto& Geometry=Label->GetCachedGeometry();const auto& HeaderGeometry=Header->GetCachedGeometry();
                    const FVector2D Bottom=HeaderGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
                    Test->TestTrue(TEXT("Every visible coil header label stays inside its card: ")+Label->GetText().ToString(),Bottom.Y<=HeaderGeometry.GetLocalSize().Y+1);
                }
            }
            if(!Hatch&&Label->GetText().ToString().StartsWith(TEXT("코일 글자")))
            {
                const FVector2D Actual=Label->GetCachedGeometry().GetLocalSize(),Desired=Label->GetDesiredSize();
                Test->TestTrue(TEXT("Current coil font caption fits its arranged button"),Desired.X<=Actual.X+1&&Desired.Y<=Actual.Y+1);
                Meta->SetNumberField(TEXT("coil_font_caption_actual_width"),Actual.X);Meta->SetNumberField(TEXT("coil_font_caption_desired_width"),Desired.X);
            }
        }
        Meta->SetArrayField(TEXT("visible_umg_text"),Text);
        FString Json;FJsonSerializer::Serialize(Meta,TJsonWriterFactory<>::Create(&Json));
        Test->TestTrue(TEXT("Capture metadata saved"),FFileHelper::SaveStringToFile(Json,*(Root/(Name+TEXT(".json")))));
        UE_LOG(LogTemp,Display,TEXT("[UXR-P1P2a] Captured %s"),*Name);return true;
    }
public:
    FUxrCaptureCommand(FAutomationTestBase* InTest,bool InHatch):Test(InTest),Hatch(InHatch){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();const bool Manual=FParse::Param(FCommandLine::Get(),TEXT("UxrP1P2Manual"));
        if(Now-Started>(Manual?1800:300)){Test->AddError(TEXT("UXR-P1P2a capture fixture timeout"));return true;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())return false;
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        UUserWidget* Monitor=nullptr;ACoilLoadingMonitorHost* Coil=nullptr;AShipHatchMonitorHost* Ship=nullptr;
        if(Hatch){for(TActorIterator<AShipHatchMonitorHost> It(World);It;++It)Ship=*It;if(Ship)Monitor=Ship->Monitor;}
        else{for(TActorIterator<ACoilLoadingMonitorHost> It(World);It;++It)Coil=*It;if(Coil)Monitor=Coil->Monitor;}
        if(!Monitor)return false;
        auto* Workspace=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        if(!OriginalMonitor.IsValid())
        {
            if(!Test->TestTrue(TEXT("Capture requires actual D3D12"),GDynamicRHI&&FString(GDynamicRHI->GetName())==TEXT("D3D12")))return true;
            OriginalMonitor=Monitor;Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            Workspace->SetSidebarMode(true);Workspace->ResetOwnedWorkspaceLayout();Workspace->CollapseSidebar();
            if(Coil&&Coil->Coordinator&&Coil->Coordinator->StreamPublisherComponent)Coil->Coordinator->StreamPublisherComponent->StopAllStreams(TEXT(""));
        }
        if(WaitingManual)return FPaths::FileExists(UxrCaptureRoot()/(Hatch?TEXT("manual-hatch-finish.flag"):TEXT("manual-finish.flag")));
        if(!Prepared)
        {
            if(Hatch)
            {
                if(!Ship->Monitor->GetAnalysis())return false;
                if(Step>=4)
                {
                    if(Manual){FFileHelper::SaveStringToFile(TEXT("Hatch source cards ready for direct mouse verification."),*(UxrCaptureRoot()/TEXT("manual-hatch-ready.txt")));WaitingManual=true;return false;}
                    return true;
                }
                Ship->Monitor->SetFontPercent(Step%2?150:100);Ship->Monitor->SetSplitView(true);
                if(Step>=2)for(auto* Sensor:Ship->Coordinator->GetSensorActors())if(Sensor)Sensor->StopSensor();
                Scene=FString::Printf(TEXT("H02-%s-%d"),Step<2?TEXT("measured-split"):TEXT("stopped-last-values"),Step%2?150:100);
            }
            else
            {
                if(Step>=16)
                {
                    if(Manual){FFileHelper::SaveStringToFile(TEXT("Coil tools are ready; direct mouse actions are separate from automatic captures."),*(UxrCaptureRoot()/TEXT("manual-ready.txt")));WaitingManual=true;return false;}
                    return true;
                }
                const int32 Font=Step%2?150:100;Coil->Monitor->SetFontPercent(Font);
                Workspace->SetOwnedPanelFontScale(Font/100.f);
                if(Step<2){Workspace->CollapseSidebar();Scene=FString::Printf(TEXT("C01-C03-summary-%d"),Font);}
                else
                {
                    Coil->ShowTools();auto* Navigation=Cast<USensorToolToolbarWidget>(Workspace->GetOwnedNavigation());if(!Navigation)return false;
                    if(!OriginalNavigation.IsValid())
                    {
                        Coil->Coordinator->SelectSensorActor(Coil->Simulation->Lidar.Get());Workspace->SynchronizeOwnedSelection();
                        for(const auto Role:{ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabProgress,ESensorToolPanelRole::SlabCharts})Workspace->SetPanelOpen(Role,false);
                        OriginalNavigation=Navigation;
                    }
                    if(Step<4){Workspace->CollapseSidebar();Scene=FString::Printf(TEXT("T01-rail-%d"),Font);}
                    else if(Step<6){Navigation->OpenOwnedDestination(ESensorToolDestination::Placement);Scene=FString::Printf(TEXT("T01-placement-%d"),Font);}
                    else if(Step<12)
                    {
                        auto* Publisher=Coil->Coordinator->StreamPublisherComponent.Get();Publisher->StopAllStreams(TEXT(""));
                        FVirtualPcdSendOptions Options;Options.bCompressed=true;Options.Profile=Step<8?EVirtualPcdFieldProfile::Full:Step<10?EVirtualPcdFieldProfile::Compact:EVirtualPcdFieldProfile::Custom;
                        FString Error;Test->TestTrue(TEXT("Transient capture uses existing PCD option setter"),Publisher->SetPcdSendOptions(TEXT(""),Options,true,Error));
                        Navigation->OpenOwnedDestination(ESensorToolDestination::PcdTransfer);
                        if(Step>=10)if(auto* Data=Workspace->GetOwnedPanel(ESensorToolPanelRole::Data))UxrExpandFields(Data->TakeWidget());
                        Scene=FString::Printf(TEXT("D01-P2a-pcd-%s-%d"),Step<8?TEXT("full"):Step<10?TEXT("compact"):TEXT("custom-fields"),Font);
                    }
                    else if(Step<14)
                    {
                        Workspace->CollapseSidebar();if(!Coil->Monitor->DataExpanded())Test->TestTrue(TEXT("Existing additional-check action opens its details"),UxrAdditionalChecks(*Coil->Monitor));
                        Scene=FString::Printf(TEXT("C01-expanded-reasons-%d"),Font);
                    }
                    else
                    {
                        Navigation->OpenOwnedDestination(ESensorToolDestination::PcdTransfer);if(Step==14)Workspace->ToggleSidebarWidth();
                        Scene=FString::Printf(TEXT("D01-P2a-wide-%d"),Font);
                    }
                }
                Coil->Monitor->Refresh();
            }
            Ready=Now+(Hatch?2.:1.);Prepared=true;return false;
        }
        if(Now<Ready)return false;
        if(!Capture(*World,*Monitor))return true;
        ++Step;Prepared=false;return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP1P2CoilCapture,"MA0T10.UXRP1P2.CoilCaptureRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP1P2CoilCapture::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("UxrP1P2Capture"))){AddInfo(TEXT("[OPT-IN SKIP] -UxrP1P2Capture is required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrCaptureCommand(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxrP1P2HatchCapture,"MA0T10.UXRP1P2.HatchCaptureRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxrP1P2HatchCapture::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("UxrP1P2Capture"))){AddInfo(TEXT("[OPT-IN SKIP] -UxrP1P2Capture is required"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrCaptureCommand(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
