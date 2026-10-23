#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchViewTransform.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ComboBoxString.h"
#include "Components/WrapBox.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "DynamicRHI.h"

struct FHatch150Probe
{
    static TArray<UWidget*> Instances(UShipHatchMonitorWidget& M){TArray<UWidget*> R{M.CargoChoice.Get(),M.HeatView.Get(),M.SectionView.Get()};for(auto C:M.FieldChoices)R.Add(C.Get());return R;}
    static void Prepare(UShipHatchMonitorWidget& M,int32 Step)
    {
        if(Step%4==0){M.SetFontPercent(Step<4?100:150);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(1);}
        if(Step%4==1)M.MoreToggle->OnClicked.Broadcast();
        if(Step%4==2){M.MoreToggle->OnClicked.Broadcast();M.SectionToggle->OnClicked.Broadcast();}
        if(Step%4==3){M.SectionToggle->OnClicked.Broadcast();M.SelectView(3);M.SetSplitView(true);}
    }
    static TSharedRef<FJsonObject> Check(FAutomationTestBase& T,UShipHatchMonitorWidget& M,int32 Step)
    {
        auto O=MakeShared<FJsonObject>();const auto Size=M.GetCachedGeometry().GetLocalSize();const auto R=M.GetSceneRect();
        const auto MapSize=M.HeatView->GetCachedGeometry().GetLocalSize();const auto Plot=ShipHatch::FViewTransform::Top(*M.GetAnalysisSettings()).Fit(MapSize,true,M.GetFontScale());
        const double SceneRatio=(R.Bottom-R.Top)/Size.Y,HeatRatio=(Plot.Right-Plot.Left)/M.LayoutSlots[4]->GetSize().X;
        O->SetNumberField(TEXT("font_percent"),M.GetFontScale()*100);O->SetNumberField(TEXT("scene_height_ratio"),SceneRatio);O->SetNumberField(TEXT("heat_plot_card_width_ratio"),HeatRatio);
        O->SetNumberField(TEXT("scene_height_ui"),R.Bottom-R.Top);O->SetNumberField(TEXT("heat_plot_width_ui"),Plot.Right-Plot.Left);O->SetNumberField(TEXT("right_card_width_ui"),M.LayoutSlots[4]->GetSize().X);
        O->SetNumberField(TEXT("phase"),Step%4);O->SetBoolField(TEXT("more_open"),M.bMoreExpanded);O->SetBoolField(TEXT("section_open"),M.bSectionExpanded);
        if(Step%4!=2){T.TestTrue(TEXT("3D height >=45% of client at 100/150 percent"),SceneRatio>=.45);T.TestTrue(TEXT("actual heat plot width >=60% of right card"),HeatRatio>=.60);}
        for(auto Slot:M.LayoutSlots)if(Slot->Content->IsVisible())
        {const auto P=Slot->GetPosition(),S=Slot->GetSize();T.TestTrue(TEXT("visible panel stays inside client with positive area"),P.X>=0&&P.Y>=0&&S.X>0&&S.Y>0&&P.X+S.X<=Size.X+.1&&P.Y+S.Y<=Size.Y+.1);}
        if(M.bMoreExpanded)
        {
            const auto G=M.MoreDrawerSlot->Content->GetCachedGeometry();const auto DrawerPosition=M.GetCachedGeometry().AbsoluteToLocal(G.GetAbsolutePosition());
            T.TestTrue(TEXT("drawer blank surface cannot trigger scene input"),!M.SceneInputAt(M.GetCachedGeometry(),G.LocalToAbsolute(G.GetLocalSize()*.5)));
            T.TestTrue(TEXT("drawer remains inside client"),DrawerPosition.Y+G.GetLocalSize().Y<=Size.Y+.1);
            T.TestTrue(TEXT("drawer uses bounded vertical scrolling"),M.QuickToolScroll->GetOrientation()==Orient_Vertical);
            const double Width=M.QuickToolScroll->GetCachedGeometry().GetLocalSize().X;
            for(auto Row:M.ToolbarRows)
            {
                bool Visible=true,InDrawer=false;for(UWidget* P=Row;P;P=P->GetParent()){Visible&=P->IsVisible();InDrawer|=P==M.QuickToolScroll;}
                if(!Visible||!InDrawer)continue;
                for(auto Child:Row->GetAllChildren())
                {
                    const auto CG=Child->GetCachedGeometry();const auto P=Row->GetCachedGeometry().AbsoluteToLocal(CG.GetAbsolutePosition());
                    T.TestTrue(TEXT("drawer control retains its full desired width"),Child->GetDesiredSize().X<=CG.GetLocalSize().X+1);
                    T.TestTrue(TEXT("drawer control fits without horizontal scrolling"),P.X>=-1&&P.X+CG.GetLocalSize().X<=Width+1);
                }
            }
        }
        T.TestEqual(TEXT("section body exposure follows the drawer"),M.SectionBody->IsVisible(),M.bSectionExpanded);
        return O;
    }
};
namespace
{
class FHatch150Layout final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Step=0;bool Prepared=false;double Start=FPlatformTime::Seconds(),Changed=0;
    TWeakObjectPtr<UShipHatchMonitorWidget> Original;TArray<UWidget*> Instances;uint32 SurfaceHash=0;uint64 Revision=0;
    TUniquePtr<TGuardValue<bool>> Messages;
public:
    explicit FHatch150Layout(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Start>120){Test->AddError(TEXT("H150 layout timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!FinalAcceptanceViewportReady(W))return false;
        AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;
        if(!H||!H->Monitor||!H->Monitor->GetAnalysis())return false;auto& M=*H->Monitor;
        if(!Original.IsValid()){Original=&M;Instances=FHatch150Probe::Instances(M);SurfaceHash=M.GetSurface()->Hash();Revision=H->Simulation->Revision();Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);}
        if(!Prepared){FHatch150Probe::Prepare(M,Step);Changed=Now;Prepared=true;return false;}if(Now-Changed<1)return false;
        Test->TestTrue(TEXT("actual D3D12 required"),GDynamicRHI&&FString(GDynamicRHI->GetName())==TEXT("D3D12"));
        Test->TestTrue(TEXT("monitor, cargo, sensor selectors and analysis widget instances retained"),Original.Get()==&M&&Instances==FHatch150Probe::Instances(M));
        Test->TestEqual(TEXT("layout does not mutate cargo"),M.GetSurface()->Hash(),SurfaceHash);Test->TestEqual(TEXT("layout does not regenerate acquisition"),H->Simulation->Revision(),Revision);
        auto O=FHatch150Probe::Check(*Test,M,Step);auto* View=W->GetGameViewport();const auto Size=View->Viewport->GetSizeXY();
        O->SetNumberField(TEXT("client_width"),Size.X);O->SetNumberField(TEXT("client_height"),Size.Y);O->SetNumberField(TEXT("window_dpi"),View->GetWindow()->GetDPIScaleFactor());O->SetNumberField(TEXT("umg_scale"),UWidgetLayoutLibrary::GetViewportScale(&M));
        O->SetStringField(TEXT("evidence"),TEXT("automatic D3D12 geometry and capture; not direct mouse acceptance"));
        FString Root;FParse::Value(FCommandLine::Get(),TEXT("H150Report="),Root);if(Root.IsEmpty())Root=FPaths::ProjectSavedDir()/TEXT("Reports/UXR-H150/captures");IFileManager::Get().MakeDirectory(*Root,true);
        const FString Name=FString::Printf(TEXT("hatch-%dx%d-%d-phase%d"),Size.X,Size.Y,Step<4?100:150,Step%4);
        FString Json;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Root/(Name+TEXT(".json"))));
        TArray<FColor> Pixels;FIntVector ImageSize;FSlateApplication::Get().CloseToolTip();if(Test->TestTrue(TEXT("actual client screenshot"),FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,ImageSize)))
        {TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(ImageSize.X,ImageSize.Y,Pixels,Png);Test->TestTrue(TEXT("PNG saved"),FFileHelper::SaveArrayToFile(Png,*(Root/(Name+TEXT(".png")))));}
        Prepared=false;return ++Step==8;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatch150LayoutTest,"MA0T10.Hatch.Layout150Rhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatch150LayoutTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatch150Layout(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
