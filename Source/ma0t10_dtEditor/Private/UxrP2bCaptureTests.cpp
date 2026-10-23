#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/CoilLoadingMonitor.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "EngineUtils.h"
#include "DynamicRHI.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ShaderCompiler.h"
#include "Widgets/Text/STextBlock.h"
#include "Types/SlateAttributeMetaData.h"

namespace
{
FString P2bReport(){FString P;FParse::Value(FCommandLine::Get(),TEXT("UxrP2bReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/UXR-P2b"):P;}
UButton* P2bButton(UUserWidget& M,const FString& Prefix)
{
    TArray<UWidget*> Widgets;M.WidgetTree->GetAllWidgets(Widgets);
    for(auto* W:Widgets)if(auto* B=Cast<UButton>(W))if(const auto* T=Cast<UTextBlock>(B->GetContent()))if(T->GetText().ToString().StartsWith(Prefix))return B;
    return nullptr;
}
void P2bGroup(UShipHatchMonitorWidget& M,const FString& Prefix,bool Expanded)
{
    if(auto* B=P2bButton(M,Prefix))if(auto* Group=Cast<UShipHatchDetailsSection>(B->GetParent()))if(Group->GetChildAt(1)->IsVisible()!=Expanded)B->OnClicked.Broadcast();
}
UScrollBox* P2bToolbarScroll(UShipHatchMonitorWidget& M)
{const auto* Card=Cast<UBorder>(M.WidgetTree->FindWidget(TEXT("Hatch3dToolbar")));return Card?Cast<UScrollBox>(Card->GetContent()):nullptr;}
void P2bSlateText(const TSharedRef<SWidget>& W,TArray<FString>& Text)
{
    FSlateAttributeMetaData::UpdateOnlyVisibilityAttributes(W.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);if(!W->GetVisibility().IsVisible())return;
    FSlateAttributeMetaData::UpdateExceptVisibilityAttributes(W.Get(),FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
    if(W->GetType()==TEXT("STextBlock"))Text.Add(StaticCastSharedRef<STextBlock>(W)->GetText().ToString());
    auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)P2bSlateText(Children->GetChildAt(I),Text);
}
class FP2bCapture final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;bool Hatch,Prepared=false;int32 Step=0;double Started=FPlatformTime::Seconds(),Ready=0;
    TWeakObjectPtr<UUserWidget> Original;TUniquePtr<TGuardValue<bool>> Debug;FString Scene;
    bool Shot(UWorld& W,UUserWidget& M)
    {
        FSlateApplication::Get().CloseToolTip();
        TArray<FColor> Pixels;FIntVector Size;const auto View=W.GetGameViewport()->GetGameViewportWidget().ToSharedRef();
        if(!Test->TestTrue(TEXT("P2b captures the actual D3D12 client"),FSlateApplication::Get().TakeScreenshot(View,Pixels,Size)))return false;
        const auto Expected=FinalAcceptanceViewportSize();if(!Test->TestTrue(TEXT("Requested client dimensions are exact"),Size.X==Expected.X&&Size.Y==Expected.Y))return false;
        const FString Name=FString::Printf(TEXT("%s-%dx%d"),*Scene,Size.X,Size.Y),Root=P2bReport()/TEXT("progress");IFileManager::Get().MakeDirectory(*Root,true);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("PNG saved"),FFileHelper::SaveArrayToFile(Png,*(Root/(Name+TEXT(".png")))));
        auto Meta=MakeShared<FJsonObject>();FString Commit;FParse::Value(FCommandLine::Get(),TEXT("UxrSourceCommit="),Commit);Meta->SetStringField(TEXT("commit"),Commit);
        Meta->SetStringField(TEXT("scene"),Scene);Meta->SetStringField(TEXT("map"),Hatch?TEXT("ShipHatchTestMap"):TEXT("CoilLoadingTestMap"));Meta->SetStringField(TEXT("rhi"),GDynamicRHI->GetName());
        Meta->SetStringField(TEXT("dtcore"),TEXT("b2504d1dfea5659f832ff0d750515edee146a28e"));Meta->SetNumberField(TEXT("settle_min_seconds"),2);Meta->SetBoolField(TEXT("capture_has_no_submit_or_file_save_calls"),true);
        Meta->SetNumberField(TEXT("client_width"),Size.X);Meta->SetNumberField(TEXT("client_height"),Size.Y);const auto Window=FSlateApplication::Get().FindWidgetWindow(View);
        Meta->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);Meta->SetNumberField(TEXT("umg_scale"),UWidgetLayoutLibrary::GetViewportScale(&M));
        Meta->SetNumberField(TEXT("font_percent"),100*(Hatch?CastChecked<UShipHatchMonitorWidget>(&M)->GetFontScale():CastChecked<UCoilLoadingMonitor>(&M)->FontScale()));
        Meta->SetBoolField(TEXT("automatic_capture_not_mouse_acceptance"),true);Meta->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
        Meta->SetBoolField(TEXT("same_monitor_instance"),Original.Get()==&M);Test->TestTrue(TEXT("Same monitor instance through every font and layout change"),Original.Get()==&M);
        TArray<TSharedPtr<FJsonValue>> Labels;TArray<UWidget*> Widgets;M.WidgetTree->GetAllWidgets(Widgets);
        for(auto* Item:Widgets)if(auto* Text=Cast<UTextBlock>(Item))
        {
            bool Visible=true;for(UWidget* P=Text;P;P=P->GetParent())Visible&=P->IsVisible();
            if(Visible&&!Text->GetText().IsEmpty())Labels.Add(MakeShared<FJsonValueString>(Text->GetText().ToString()));
        }
        Meta->SetArrayField(TEXT("visible_umg_text"),Labels);
        if(Hatch)
        {
            auto& Monitor=*CastChecked<UShipHatchMonitorWidget>(&M);const auto* S=Monitor.GetSimulation();Meta->SetStringField(TEXT("status"),Monitor.GetStatusText());Meta->SetBoolField(TEXT("site_visualization"),Monitor.IsSiteVisualization());
            Meta->SetNumberField(TEXT("selected_sensor"),S->HighlightedSensor());Meta->SetNumberField(TEXT("surface_hash"),S->Surface().Hash());Meta->SetNumberField(TEXT("revision"),S->Revision());
            TArray<TSharedPtr<FJsonValue>> Sensors;bool AnyRunning=false;
            for(int32 I=0;I<3;++I)if(const auto* A=S->SensorAt(I)){const auto Status=A->GetSensorRuntimeStatus();auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),I);Row->SetBoolField(TEXT("running"),A->IsSensorRunning());Row->SetNumberField(TEXT("frame_id"),Status.FrameId);Row->SetStringField(TEXT("backend"),Status.ActiveAcquisitionBackend);Row->SetNumberField(TEXT("cached_acquisition_hz"),Status.MeasuredAcquisitionRateHz);Sensors.Add(MakeShared<FJsonValueObject>(Row));AnyRunning|=A->IsSensorRunning();}
            Meta->SetArrayField(TEXT("sensors"),Sensors);
            if(Scene.StartsWith(TEXT("H04-stopped"))){Test->TestFalse(TEXT("Stopped capture has no running sensors"),AnyRunning);Test->TestTrue(TEXT("Stopped capture explicitly identifies retained acquisition rates"),Monitor.GetStatusText().Contains(TEXT("측정 정지 · 마지막 획득")));}
            if(const auto* A=Monitor.GetDisplayAnalysis())
            {
                Meta->SetNumberField(TEXT("unobserved_percent"),(1-A->Coverage)*100);
                for(int32 I=0;I<A->Cells.Num();++I)if(!A->Cells[I].Count){Meta->SetStringField(TEXT("unobserved_cell_detail"),Monitor.CellTooltip(I).ToString());break;}
            }
            if(auto* Toolbar=M.WidgetTree->FindWidget(TEXT("Hatch3dToolbar"))){const auto G=Toolbar->GetCachedGeometry();Meta->SetNumberField(TEXT("toolbar_height_ui"),G.GetLocalSize().Y);}
        }
        else
        {
            auto& Monitor=*CastChecked<UCoilLoadingMonitor>(&M);const int32 Font=Monitor.FontScale()>1?18:12;int32 Rows=0;
            for(auto* Item:Widgets)if(auto* B=Cast<UCoilLoadingWarningButton>(Item))if(auto* T=Cast<UTextBlock>(B->GetContent())){++Rows;Test->TestEqual(TEXT("Every regenerated actual coil issue row uses the current font"),T->GetFont().Size,float(Font));}
            Meta->SetNumberField(TEXT("issue_rows"),Rows);Meta->SetNumberField(TEXT("issue_font_size"),Font);Meta->SetNumberField(TEXT("unknown_groups"),Monitor.DataNeededCount);Meta->SetNumberField(TEXT("revision"),Monitor.Simulation()->Revision());
            Meta->SetStringField(TEXT("backend"),Monitor.Simulation()->Lidar->GetSensorRuntimeStatus().ActiveAcquisitionBackend);
            auto* Workspace=W.GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();Test->TestFalse(TEXT("Coil world never enables Slab capability"),Workspace->AreSlabToolsEnabled());
            Meta->SetBoolField(TEXT("slab_tools_enabled"),Workspace->AreSlabToolsEnabled());TArray<TSharedPtr<FJsonValue>> Values;
            for(auto Role:{ESensorToolPanelRole::Data,ESensorToolPanelRole::Monitor})if(auto* Panel=Workspace->GetOwnedPanel(Role))if(Panel->IsVisible()&&!Panel->IsPanelCollapsed())
            {
                TArray<FString> Text;P2bSlateText(Panel->TakeWidget(),Text);for(const auto& T:Text){Values.Add(MakeShared<FJsonValueString>(T));Test->TestFalse(TEXT("Visible coil panel contains no Slab/scenario/replay text: ")+T,T.Contains(TEXT("Slab"))||T.Contains(TEXT("시나리오"))||T.Contains(TEXT("재생")));}
            }
            Meta->SetArrayField(TEXT("visible_tool_text"),Values);
        }
        FString Json;FJsonSerializer::Serialize(Meta,TJsonWriterFactory<>::Create(&Json));Test->TestTrue(TEXT("Capture metadata saved"),FFileHelper::SaveStringToFile(Json,*(Root/(Name+TEXT(".json")))));
        UE_LOG(LogTemp,Display,TEXT("[UXR-P2b] Captured %s"),*Name);return true;
    }
public:
    FP2bCapture(FAutomationTestBase* T,bool H):Test(T),Hatch(H){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Started>420){Test->AddError(TEXT("P2b capture timeout"));return true;}
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!FinalAcceptanceViewportReady(W))return false;
        AShipHatchMonitorHost* Ship=nullptr;ACoilLoadingMonitorHost* Coil=nullptr;UUserWidget* Monitor=nullptr;
        if(Hatch){for(TActorIterator<AShipHatchMonitorHost> I(W);I;++I)Ship=*I;if(Ship)Monitor=Ship->Monitor;}
        else{for(TActorIterator<ACoilLoadingMonitorHost> I(W);I;++I)Coil=*I;if(Coil)Monitor=Coil->Monitor;}
        if(!Monitor)return false;
        const int32 Count=Hatch?10:6,Phase=Step%Count,Font=Step<Count?100:150;if(Step>=Count*2)return true;
        if(!Original.IsValid())
        {
            Test->TestTrue(TEXT("Capture requires real D3D12"),GDynamicRHI&&FString(GDynamicRHI->GetName())==TEXT("D3D12"));Original=Monitor;Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            if(Coil){Coil->Monitor->LoadMixedExample();auto* Workspace=W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();Workspace->SetSidebarMode(true);Workspace->ResetOwnedWorkspaceLayout();Workspace->CollapseSidebar();Coil->Coordinator->StreamPublisherComponent->StopAllStreams(TEXT(""));}
        }
        if(!Prepared)
        {
            if(Hatch)
            {
                auto& M=*Ship->Monitor;auto* S=Ship->Simulation.Get();if(!M.GetDisplayAnalysis())return false;
                if(Phase==0){for(int32 I=0;I<3;++I)if(auto* Sensor=S->SensorAt(I))Sensor->StartSensor();M.SetFontPercent(Font);M.SetSiteVisualization(false);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(1);}
                if(Phase==1)M.SetSplitView(false);
                if(Phase==2){M.SetSplitView(true);P2bGroup(M,TEXT("원료 · 센서 선택"),true);if(auto* Scroll=P2bToolbarScroll(M))Scroll->ScrollToEnd();}
                if(Phase==3){P2bGroup(M,TEXT("원료 · 센서 선택"),false);P2bGroup(M,TEXT("보기 설정"),true);if(auto* Scroll=P2bToolbarScroll(M))Scroll->ScrollToEnd();}
                if(Phase>=4&&Phase<=6){P2bGroup(M,TEXT("보기 설정"),false);if(auto* Scroll=P2bToolbarScroll(M))Scroll->ScrollToStart();M.SetSiteVisualization(true);M.SetSplitView(false);M.SetSiteCameraPreset(2);S->HighlightSensor(Phase-4);}
                if(Phase==7)M.SetSiteCameraPreset(0);
                if(Phase==8){M.SetSiteVisualization(false);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(1);M.SetMapHeat(true);}
                if(Phase==9)for(int32 I=0;I<3;++I)if(auto* Sensor=S->SensorAt(I))Sensor->StopSensor();
                const TCHAR* Names[]={TEXT("H01-split-presets"),TEXT("H01-single-presets"),TEXT("H01-selectors-expanded"),TEXT("H01-view-settings"),TEXT("H03-site-selected-boom"),TEXT("H03-site-selected-left"),TEXT("H03-site-selected-right"),TEXT("H03-site-overview"),TEXT("H04-unobserved-partial"),TEXT("H04-stopped-last-values")};Scene=FString::Printf(TEXT("%s-%d"),Names[Phase],Font);
            }
            else
            {
                auto& M=*Coil->Monitor;M.SetFontPercent(Font);auto* Workspace=W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();Workspace->SetOwnedPanelFontScale(Font/100.f);
                if(Phase==0)
                {
                    for(auto Role:{ESensorToolPanelRole::Monitor,ESensorToolPanelRole::Settings,ESensorToolPanelRole::Data,ESensorToolPanelRole::Replay,ESensorToolPanelRole::SlabProgress,ESensorToolPanelRole::SlabCharts})Workspace->SetPanelOpen(Role,false);
                    Workspace->CollapseSidebar();if(!M.DataExpanded())if(auto* B=P2bButton(M,TEXT("추가 확인")))B->OnClicked.Broadcast();
                }
                if(Phase==1){const auto Before=Coil->Simulation->Revision();Test->TestTrue(TEXT("Existing planning API creates a new revision for C02"),Coil->Simulation->PlaceNext(false));Test->TestTrue(TEXT("Actual coil revision advances"),Coil->Simulation->Revision()!=Before);}
                if(Phase>=2){Coil->ShowTools();Coil->Coordinator->SelectSensorActor(Coil->Simulation->Lidar);Workspace->SynchronizeOwnedSelection();auto* Nav=CastChecked<USensorToolToolbarWidget>(Workspace->GetOwnedNavigation());const ESensorToolDestination Destinations[]={ESensorToolDestination::PcdTransfer,ESensorToolDestination::Results,ESensorToolDestination::Diagnostics,ESensorToolDestination::Preview};Nav->OpenOwnedDestination(Destinations[Phase-2]);}
                M.Refresh();
                if(Phase<2)
                {
                    auto* Footer=M.WidgetTree->FindWidget(TEXT("CoilAnalysis"));TArray<UWidget*> Widgets;M.WidgetTree->GetAllWidgets(Widgets);
                    for(auto* Item:Widgets)if(auto* Scroll=Cast<UScrollBox>(Item))for(auto* Parent=Scroll->GetParent();Parent;Parent=Parent->GetParent())if(Parent==Footer){Scroll->ScrollToEnd();break;}
                }
                const TCHAR* Names[]={TEXT("C02-expanded-issues"),TEXT("C02-after-revision"),TEXT("coil-no-slab-transfer"),TEXT("coil-no-slab-results"),TEXT("coil-no-slab-diagnostics"),TEXT("coil-no-slab-preview")};Scene=FString::Printf(TEXT("%s-%d"),Names[Phase],Font);
            }
            Prepared=true;Ready=Now+2;return false;
        }
        if(Now<Ready)return false;if(!Shot(*W,*Monitor))return true;
        if(Coil&&Phase==1){Test->TestTrue(TEXT("Undo restores the capture plan via the existing API"),Coil->Simulation->Undo());Coil->Monitor->SyncInputs();Coil->Monitor->Refresh();}
        ++Step;Prepared=false;return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bCoilCapture,"UXRP2B.CoilCapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP2bCoilCapture::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("UxrP2bCapture"))){AddInfo(TEXT("[OPT-IN SKIP] -UxrP2bCapture required"));return true;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FP2bCapture(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bHatchCapture,"UXRP2B.HatchCapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP2bHatchCapture::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("UxrP2bCapture"))){AddInfo(TEXT("[OPT-IN SKIP] -UxrP2bCapture required"));return true;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FP2bCapture(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
