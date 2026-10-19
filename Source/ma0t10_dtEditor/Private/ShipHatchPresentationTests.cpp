#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"

struct FHatchPresentationProbe
{
    static void Check(FAutomationTestBase& T,UShipHatchMonitorWidget& M,bool Geometry)
    {
        const auto* A=M.GetAnalysis();T.TestNotNull(TEXT("current immutable analysis exists"),A);if(!A)return;
        double Volume=0,Sum=0,Minimum=DBL_MAX,Maximum=0;int32 Count=0;
        for(const auto& C:A->Cells)if(C.Count){const double H=C.Sum/C.Count;Volume+=H*A->DX*A->DY;Sum+=H;Minimum=FMath::Min(Minimum,H);Maximum=FMath::Max(Maximum,H);++Count;}
        T.TestTrue(TEXT("actual measured analysis volume retains independent observed-cell integral"),FMath::Abs(Volume-A->Volume)<1.e-6);
        const double Expected[]={M.Simulation->Surface().Fill,M.Simulation->Surface().Volume,Maximum,Count?Sum/Count:0,Count?Minimum:0,100.*(1.-double(Count)/A->Cells.Num())};
        for(int32 I=0;I<6;++I)T.TestTrue(TEXT("visible metrics use full simulated fill/volume and observed height/coverage"),FMath::Abs(FCString::Atod(*M.MetricValues[I]->GetText().ToString())-Expected[I])<(I==1?.51:I==0||I==5?.051:.0051));
        int32 Hit=INDEX_NONE,Miss=INDEX_NONE;for(int32 I=0;I<A->Cells.Num();++I){if(A->Cells[I].Count)Hit=I;else Miss=I;}
        if(Hit!=INDEX_NONE)T.TestTrue(TEXT("hover reports the actual selected cell mean and meter unit"),M.CellTooltip(Hit).ToString().Contains(FString::Printf(TEXT("%.2f m"),A->Cells[Hit].Sum/A->Cells[Hit].Count)));
        if(Miss!=INDEX_NONE)T.TestTrue(TEXT("unobserved tooltip never invents a zero height"),M.CellTooltip(Miss).ToString().Contains(TEXT("측정 불가"))&&!M.CellTooltip(Miss).ToString().Contains(TEXT("0.00")));
        if(!Geometry)return;
        const auto Root=M.GetCachedGeometry();const FVector2D Size=Root.GetLocalSize();
        for(auto Slot:M.LayoutSlots)if(Slot->Content->GetVisibility()!=ESlateVisibility::Collapsed)
        {const auto P=Slot->GetPosition(),S=Slot->GetSize();T.TestTrue(TEXT("visible panel stays inside viewport"),P.X>=-.1&&P.Y>=-.1&&P.X+S.X<=Size.X+.1&&P.Y+S.Y<=Size.Y+.1);}
        for(auto Text:M.TextBlocks)
        {
            bool Visible=true,Scrolled=false;for(UWidget* W=Text;W;W=W->GetParent()){Visible&=W->GetVisibility()!=ESlateVisibility::Collapsed&&W->GetVisibility()!=ESlateVisibility::Hidden;Scrolled|=W==M.SettingsPanel||W==M.PointPanel;}
            if(!Visible||Scrolled||Text->GetText().IsEmpty())continue;
            const auto& G=Text->GetCachedGeometry();const auto P=Root.AbsoluteToLocal(G.GetAbsolutePosition());
            const FVector2D Desired=Text->GetDesiredSize(),Actual=G.GetLocalSize();
            T.TestTrue(TEXT("visible label has enough arranged width: ")+Text->GetText().ToString(),Desired.X<=Actual.X+1);
            T.TestTrue(TEXT("visible label remains within client bounds"),P.X>=-1&&P.Y>=-1&&P.X+Actual.X<=Size.X+1&&P.Y+Actual.Y<=Size.Y+1);
        }
        T.TestTrue(TEXT("3D occupies only its assigned scene rectangle"),M.SceneRect.Right<=Size.X&&M.SceneRect.Bottom<=Size.Y);
        if(M.IsSplitView()||M.GetView()<2)
        {
            auto* PC=M.GetWorld()->GetFirstPlayerController();FSceneViewProjectionData Projection;
            T.TestTrue(TEXT("actual player projection is available"),PC->GetLocalPlayer()->GetProjectionData(M.GetWorld()->GetGameViewport()->Viewport,Projection,INDEX_NONE));
            const auto Rect=Projection.GetConstrainedViewRect();const FMatrix Matrix=Projection.ComputeViewProjectionMatrix();
            bool Inside=true;int32 Corners=0;
            for(TActorIterator<AShipHatchPart> I(M.GetWorld());I;++I)
            {
                FVector Center,Extent;I->GetActorBounds(false,Center,Extent);
                for(int32 X:{-1,1})for(int32 Y:{-1,1})for(int32 Z:{-1,1})
                {
                    FVector2D Pixel;const bool Front=FSceneView::ProjectWorldToScreen(Center+Extent*FVector(X,Y,Z),Rect,Matrix,Pixel);
                    Inside&=Front&&Pixel.X>=Rect.Min.X-1&&Pixel.X<=Rect.Max.X+1&&Pixel.Y>=Rect.Min.Y-1&&Pixel.Y<=Rect.Max.Y+1;++Corners;
                }
            }
            T.TestTrue(TEXT("reset view fits actual geometry bounds in split and maximized panes"),Inside&&Corners>=32);
        }
    }
    static void Controls(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        M.ViewButtons[1]->OnClicked.Broadcast();T.TestEqual(TEXT("point segment selects point view"),M.GetView(),1);
        for(int32 I=0;I<5;++I){M.ColorButtons[I]->OnClicked.Broadcast();T.TestEqual(TEXT("each color segment routes to its palette"),M.GetColor(),I);}
        M.SizeSlider->SetValue(4);M.SizeSlider->OnValueChanged.Broadcast(4);M.DensitySlider->SetValue(25);M.DensitySlider->OnValueChanged.Broadcast(25);
        for(auto* A:{M.Simulation->MLX.Get(),M.Simulation->Livox.Get()})
        {T.TestEqual(TEXT("size slider updates owned point preview"),A->VisualizationComponent->GetVisualizationSettings().PointSize,4.f);T.TestEqual(TEXT("density slider updates existing stride policy"),A->ScanComponent->GetPreviewPolicyState().PointStride,4);}
        auto* Camera=M.Host->ViewCamera.Get();M.SetCameraPreset(0);T.TestTrue(TEXT("top preset points down"),Camera->GetActorRotation().Pitch<-88);M.SetCameraPreset(1);T.TestTrue(TEXT("oblique preset retains an inclined view"),Camera->GetActorRotation().Pitch>-80&&Camera->GetActorRotation().Pitch<-30);
        M.SelectView(3);T.TestFalse(TEXT("heatmap maximizes"),M.IsSplitView());M.SetSplitView(true);T.TestTrue(TEXT("return restores split layout and 3D"),M.IsSplitView()&&M.GetView()<2);
        M.SizeSlider->SetValue(3);M.SizeSlider->OnValueChanged.Broadcast(3);M.DensitySlider->SetValue(50);M.DensitySlider->OnValueChanged.Broadcast(50);
    }
};
void ValidateHatchPresentation(FAutomationTestBase* T,UShipHatchMonitorWidget* M){if(T&&M)FHatchPresentationProbe::Check(*T,*M,true);}
namespace
{
class FHatchPresentation final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),Changed=0;int32 Step=0;
public:
    explicit FHatchPresentation(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("product presentation timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W||!FinalAcceptanceViewportReady(W))return false;
        AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost> I(W);I;++I)H=*I;
        if(!H||!H->Monitor||!H->Monitor->GetAnalysis())return false;auto& M=*H->Monitor;
        if(!Changed){FHatchPresentationProbe::Controls(*Test,M);M.SetFontPercent(100);Changed=FPlatformTime::Seconds();return false;}
        if(FPlatformTime::Seconds()-Changed<.4)return false;
        FHatchPresentationProbe::Check(*Test,M,true);
        ++Step;if(Step==8)return true;
        M.SetFontPercent(Step>=4?150:100);M.SelectView(Step%4);M.SetSplitView(Step%4==0);Changed=FPlatformTime::Seconds();return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchProductPresentation,"MA0T10.Hatch.ProductPresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchProductPresentation::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchPresentation(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
