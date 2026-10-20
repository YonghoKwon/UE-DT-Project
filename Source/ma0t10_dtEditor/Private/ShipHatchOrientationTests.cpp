#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchViewTransform.h"
#include "ma0t10_dt/MA0T10/UI/SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SToolTip.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

// Test-only in-memory fixture. No package or personal setting is saved.
struct FHatchOrientationProbe
{
    static void Prepare(UShipHatchMonitorWidget& M)
    {
        auto& S=*M.Simulation;auto& Surface=S.Current;
        FRandomStream R(20261020);const double Peak=16+R.FRand()*.1;
        for(int32 Y=0;Y<=Surface.NY;++Y)for(int32 X=0;X<=Surface.NX;++X)
        {
            const double PX=X*Surface.DX-Surface.Settings.Length*.5,PY=Y*Surface.DY-Surface.Settings.Width*.5;
            Surface.Heights[Y*(Surface.NX+1)+X]=FMath::Max(3.+.025*PX+.01*PY,Peak-.65*FMath::Sqrt(FMath::Square(PX-5)+FMath::Square(PY-6)));
        }
        Surface.Volume=ShipHatch::Integrate(Surface);Surface.Fill=100*Surface.Volume/(S.HoldLength*S.HoldWidth*S.HoldDepth);
        S.FinishCargoEdit();M.SelectView(0);M.SetSplitView(true);int32 Font=100;FParse::Value(FCommandLine::Get(),TEXT("HatchFontPercent="),Font);M.SetFontPercent(Font);M.SetCameraPreset(0);
        M.SetSection(true,6/S.HoldWidth+.5);M.SetMapHeat(false);
    }
    static void View(UShipHatchMonitorWidget& M,int32 State)
    {M.SetMapHeat(State==1||State==2);if(State==2)M.SetSection(false,5/M.Simulation->HoldLength+.5);if(State==3)M.SetCameraPreset(1);if(State==4){M.bCoverageOverlay=true;M.UpdateSection();}}
    static void Manual(UShipHatchMonitorWidget& M)
    {M.bCoverageOverlay=false;M.SetMapHeat(true);M.SetCameraPreset(0);M.SetSection(true,6/M.Simulation->HoldWidth+.5);}
    static FVector2D Pixel(UShipHatchMonitorWidget& M,const FGeometry& G,FVector2D Local,FIntVector Size)
    {
        const auto VG=M.GetWorld()->GetGameViewport()->GetGameViewportWidget()->GetCachedGeometry();
        return VG.AbsoluteToLocal(G.LocalToAbsolute(Local))*FVector2D(Size.X,Size.Y)/VG.GetLocalSize();
    }
    static void Check(FAutomationTestBase& T,UShipHatchMonitorWidget& M,int32 State,const TArray<FColor>& Pixels,FIntVector Size,FJsonObject& Report)
    {
        auto& S=*M.Simulation;const auto& Settings=S.Surface().Settings;auto* PC=M.GetOwningPlayer();
        auto* View=State?M.HeatView.Get():M.TopView.Get();const auto G=View->GetCachedGeometry();
        const auto Map=ShipHatch::FViewTransform::Top(Settings);const auto R=Map.Fit(G.GetLocalSize(),State>0,M.GetFontScale());
        auto Actual3D=[&](FVector Local){FVector2D P;T.TestTrue(TEXT("actual top player projection"),PC->ProjectWorldLocationToScreen(S.GetActorTransform().TransformPosition(Local*100),P));return P;};
        // Independent oracle: normalize the real engine player projection at each
        // object's Z. Perspective scale/parallax differs between the two panes.
        auto Oracle=[&](FVector Local)
        {
            const auto P=Actual3D(Local),Left=Actual3D(FVector(Map.Span.X*.5,0,Local.Z)),Right=Actual3D(FVector(-Map.Span.X*.5,0,Local.Z));
            const auto Top=Actual3D(FVector(0,Map.Span.Y*.5,Local.Z)),Bottom=Actual3D(FVector(0,-Map.Span.Y*.5,Local.Z));
            return FVector2D((P.X-Left.X)/(Right.X-Left.X),(P.Y-Top.Y)/(Bottom.Y-Top.Y));
        };
        const auto Peak=Oracle(FVector(5,6,S.Surface().Height(5,6)));
        T.TestTrue(TEXT("asymmetric sea/+X peak is top-left in actual 3D and 2D"),Peak.X<.5&&Peak.Y<.5&&Map.ToUnit(FVector(5,6,0)).Equals(Peak,1.e-6));
        double MaxProjection=0,MaxIcon=0;TArray<TSharedPtr<FJsonValue>> SensorRows;
        auto At=[&](int32 X,int32 Y){return Pixels[FMath::Clamp(Y,0,Size.Y-1)*Size.X+FMath::Clamp(X,0,Size.X-1)];};
        for(int32 I=0;I<3;++I)
        {
            T.TestTrue(TEXT("three actual sensors enabled"),S.SensorEnabled(I));const auto Local=S.GetActorTransform().InverseTransformPosition(S.SensorAt(I)->GetActorLocation())*.01;const auto Unit=Oracle(Local),Mapped=Map.ToUnit(S.SensorOrigin(I));
            const double Error=((Unit-Mapped)*FVector2D(R.Right-R.Left,R.Bottom-R.Top)).Size();MaxProjection=FMath::Max(MaxProjection,Error);
            const FVector2D Expected(R.Left+Unit.X*(R.Right-R.Left),R.Top+Unit.Y*(R.Bottom-R.Top));const auto Center=Pixel(M,G,Expected,Size);
            FVector2D Sum=FVector2D::ZeroVector;int32 Count=0;
            for(int32 Y=FMath::RoundToInt(Center.Y)-6;Y<=FMath::RoundToInt(Center.Y)+6;++Y)for(int32 X=FMath::RoundToInt(Center.X)-6;X<=FMath::RoundToInt(Center.X)+6;++X)
            {
                const auto C=At(X,Y);const bool Match=I==0?(C.G>170&&C.B>170&&C.R<140):I==1?(C.R>200&&C.G>130&&C.G<225&&C.B<120):(C.R>190&&C.B>180&&C.G<155);
                if(Match){Sum+=FVector2D(X+.5,Y+.5);++Count;}
            }
            T.TestTrue(TEXT("rendered sensor square exists at independent projected position"),Count>=25);
            const double RasterError=Count?(Sum/Count-Center).Size():1000;MaxIcon=FMath::Max(MaxIcon,RasterError);
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("sensor_index"),I);Row->SetNumberField(TEXT("projection_error_logical_px"),Error);Row->SetNumberField(TEXT("raster_centroid_error_client_px"),RasterError);SensorRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        T.TestTrue(TEXT("3D-to-map sensor projection <= 1 logical pixel"),MaxProjection<=1);
        T.TestTrue(TEXT("actual sensor icon centroid <= 1.5 client pixels"),MaxIcon<=1.5);
        Report.SetArrayField(TEXT("sensors"),SensorRows);
        // Camera cannot project its own origin. Its XY ground footprint is
        // compared in the same independent, actual top-camera reference plane.
        FVector Camera,Direction;double Fov;T.TestTrue(TEXT("actual camera pose"),M.CameraInHold(Camera,Direction,Fov));
        const auto CameraUnit=Oracle(FVector(Camera.X,Camera.Y,0));const auto CameraPixel=Pixel(M,G,FVector2D(R.Left+CameraUnit.X*(R.Right-R.Left),R.Top+CameraUnit.Y*(R.Bottom-R.Top)),Size);
        int32 CameraPixels=0;FVector2D CameraSum=FVector2D::ZeroVector;const auto CameraColor=FSensorOwnedPresentationStyle::Success().ToFColorSRGB();for(int Y=-6;Y<=6;++Y)for(int X=-6;X<=6;++X){const int PX=FMath::RoundToInt(CameraPixel.X)+X,PY=FMath::RoundToInt(CameraPixel.Y)+Y;const auto C=At(PX,PY);if(FMath::Abs(int(C.R)-CameraColor.R)<5&&FMath::Abs(int(C.G)-CameraColor.G)<5&&FMath::Abs(int(C.B)-CameraColor.B)<5){++CameraPixels;CameraSum+=FVector2D(PX+.5,PY+.5);}}
        T.TestTrue(TEXT("top-down camera remains a visible square at its XY footprint"),CameraPixels>=16);
        const double CameraError=CameraPixels?(CameraSum/CameraPixels-CameraPixel).Size():1000;T.TestTrue(TEXT("actual camera square centroid <= 1.5 client pixels"),CameraError<=1.5);Report.SetNumberField(TEXT("camera_raster_centroid_error_client_px"),CameraError);
        if(State)
        {
            const auto* A=M.GetDisplayAnalysis();int32 Tested=0;bool Quadrants[4]={false,false,false,false};FVector2D Highest;double Max=-1;
            for(int32 Y=0;Y<A->NY;++Y)for(int32 X=0;X<A->NX;++X)
            {
                const auto& C=A->Cells[Y*A->NX+X];if(!C.Count)continue;
                const FVector P((X+.5)*A->DX-Settings.Length*.5,(Y+.5)*A->DY-Settings.Width*.5,0);
                if(FMath::Abs(P.X)>Settings.OpeningLength*.5-.5||FMath::Abs(P.Y)>Settings.OpeningWidth*.5-.5)continue;
                if(C.Mean()>Max){Max=C.Mean();Highest=FVector2D(P);}
                const int Q=(P.X>0?1:0)+(P.Y>0?2:0);if(Quadrants[Q])continue;
                if(FMath::Abs((M.SectionAlongLength()?P.Y:P.X)-M.SectionOffset())<1.5||FVector2D(P).Size()<2)continue;
                bool NearSensor=false;for(int I=0;I<3;++I)NearSensor|=FVector2D::Distance(FVector2D(P),FVector2D(S.SensorOrigin(I)))<2;if(NearSensor)continue;
                const auto Unit=Oracle(P);const auto Screen=Pixel(M,G,FVector2D(R.Left+Unit.X*(R.Right-R.Left),R.Top+Unit.Y*(R.Bottom-R.Top)),Size);
                const auto Color=At(FMath::FloorToInt(Screen.X),FMath::FloorToInt(Screen.Y)),Expected=ShipHatch::Jet(C.Mean()/Settings.Depth);
                T.TestTrue(TEXT("rendered heat cell matches actual row-Y/column-X observation in every quadrant"),FMath::Abs(int(Color.R)-Expected.R)<=5&&FMath::Abs(int(Color.G)-Expected.G)<=5&&FMath::Abs(int(Color.B)-Expected.B)<=5);
                Quadrants[Q]=true;++Tested;
            }
            T.TestEqual(TEXT("four non-symmetric measured quadrants sampled"),Tested,4);
            T.TestTrue(TEXT("fresh measured peak remains in sea/+X quadrant"),Highest.X>0&&Highest.Y>0);
            Report.SetNumberField(TEXT("measured_peak_x_m"),Highest.X);Report.SetNumberField(TEXT("measured_peak_y_m"),Highest.Y);
        }
        const auto SectionG=M.SectionView->GetCachedGeometry();const double Span=M.SectionAlongLength()?Settings.Length:Settings.Width;
        double Top=Settings.Depth+Settings.Coaming+2;for(int I=0;I<3;++I)Top=FMath::Max(Top,S.SensorOrigin(I).Z+2);
        int32 ProfileChecks=0;double MaxSectionError=0;
        for(const auto& Sample:M.GetSectionSamples())
        {
            if(FMath::Abs(Sample.Position)>Span*.4)continue;
            const double H=M.SectionAlongLength()?S.Surface().Height(Sample.Position,M.SectionOffset()):S.Surface().Height(M.SectionOffset(),Sample.Position);
            T.TestTrue(TEXT("section uses immutable triangle height in original SI order"),FMath::Abs(H-Sample.Height)<1.e-6);
            const double L=46*M.GetFontScale(),Right=SectionG.GetLocalSize().X-16,B=SectionG.GetLocalSize().Y-22*M.GetFontScale(),YTop=42*M.GetFontScale();
            // Right=-X for land-facing length section; right=-Y for +X-facing width.
            const auto Expected=Pixel(M,SectionG,FVector2D(L+(.5-Sample.Position/Span)*(Right-L),B-H/Top*(B-YTop)),Size);
            double Error=1000;for(int Y=-3;Y<=3;++Y)for(int X=-3;X<=3;++X){const int PX=FMath::RoundToInt(Expected.X)+X,PY=FMath::RoundToInt(Expected.Y)+Y;const auto C=At(PX,PY);if(C.R>210&&C.G>155&&C.B<150)Error=FMath::Min(Error,(FVector2D(PX+.5,PY+.5)-Expected).Size());}
            MaxSectionError=FMath::Max(MaxSectionError,Error);++ProfileChecks;
        }
        T.TestTrue(TEXT("actual section line matches reversed height profile <= 2 client pixels"),ProfileChecks>=20&&MaxSectionError<=2);
        Report.SetNumberField(TEXT("section_samples_checked"),ProfileChecks);Report.SetNumberField(TEXT("section_raster_error_client_px"),MaxSectionError);
        Report.SetNumberField(TEXT("window_dpi"),FSlateApplication::Get().FindWidgetWindow(M.TakeWidget())->GetDPIScaleFactor());Report.SetNumberField(TEXT("umg_scale"),UWidgetLayoutLibrary::GetViewportScale(&M));
    }
    static void Overlays(FAutomationTestBase& T,UShipHatchMonitorWidget& M,int32 State,const TArray<FColor>& Pixels,FIntVector Size,FJsonObject& Report)
    {
        const auto& S=M.GetSurface()->Settings;const auto G=M.TopView->GetCachedGeometry();const auto Map=ShipHatch::FViewTransform::Top(S);const auto R=Map.Fit(G.GetLocalSize(),false,M.GetFontScale());
        FVector O,D;double Fov;T.TestTrue(TEXT("oblique camera pose"),M.CameraInHold(O,D,Fov));
        // Independent canonical UE camera right/up basis, not DirectionToPixel.
        FVector2D Forward(-D.X*(R.Right-R.Left)/Map.Span.X,-D.Y*(R.Bottom-R.Top)/Map.Span.Y);Forward.Normalize();
        const double L=R.Left+(R.Right-R.Left)/22,Right=R.Right-(R.Right-R.Left)/22,Top=R.Top+(R.Bottom-R.Top)/22,Bottom=R.Bottom-(R.Bottom-R.Top)/22;
        const FVector2D Icon(FMath::Clamp(R.Left+(.5-O.X/Map.Span.X)*(R.Right-R.Left),L+5,Right-5),FMath::Clamp(R.Top+(.5-O.Y/Map.Span.Y)*(R.Bottom-R.Top),Top+5,Bottom-5));
        const auto Expected=Pixel(M,G,Icon-Forward,Size);const auto Color=FSensorOwnedPresentationStyle::Success().ToFColorSRGB();FVector2D Sum=FVector2D::ZeroVector;int32 N=0;
        const auto Center=Pixel(M,G,Icon,Size);for(int Y=FMath::RoundToInt(Center.Y)-10;Y<=FMath::RoundToInt(Center.Y)+10;++Y)for(int X=FMath::RoundToInt(Center.X)-10;X<=FMath::RoundToInt(Center.X)+10;++X)
        {const auto C=Pixels[FMath::Clamp(Y,0,Size.Y-1)*Size.X+FMath::Clamp(X,0,Size.X-1)];if(FMath::Abs(int(C.R)-Color.R)<5&&FMath::Abs(int(C.G)-Color.G)<5&&FMath::Abs(int(C.B)-Color.B)<5){Sum+=FVector2D(X+.5,Y+.5);++N;}}
        const double Error=N?(Sum/N-Expected).Size():1000;T.TestTrue(TEXT("actual oblique camera arrow centroid <= 1.5 client pixels"),N>=12&&Error<=1.5);Report.SetNumberField(TEXT("camera_arrow_centroid_error_client_px"),Error);
        const auto Tip=Pixel(M,G,Icon+Forward*5,Size);bool TipFound=false;for(int Y=-1;Y<=1;++Y)for(int X=-1;X<=1;++X){const auto C=Pixels[FMath::Clamp(FMath::RoundToInt(Tip.Y)+Y,0,Size.Y-1)*Size.X+FMath::Clamp(FMath::RoundToInt(Tip.X)+X,0,Size.X-1)];TipFound|=FMath::Abs(int(C.R)-Color.R)<5&&FMath::Abs(int(C.G)-Color.G)<5&&FMath::Abs(int(C.B)-Color.B)<5;}T.TestTrue(TEXT("camera arrow tip points along independently projected actual forward vector"),TipFound);
        if(State!=4)return;
        int32 Checked=0;TSet<uint8> Kinds;const auto& Masks=M.GetCoverageMask();
        for(int Y=1;Y<39;++Y)for(int X=1;X<39;++X)
        {
            const FVector P(((Y+.5)/40-.5)*S.OpeningLength,((X+.5)/40-.5)*S.OpeningWidth,0);if(FMath::Abs(P.X-M.SectionOffset())<1.5||FVector2D(P).Size()<2)continue;
            bool Near=false;for(int I=0;I<3;++I)Near|=FVector2D::Distance(FVector2D(P),FVector2D(M.Simulation->SensorOrigin(I)))<2;if(Near)continue;
            const FVector2D PlotP(R.Left+(.5-P.X/Map.Span.X)*(R.Right-R.Left),R.Top+(.5-P.Y/Map.Span.Y)*(R.Bottom-R.Top));
            // Foreground camera glyph/wedge/target line legitimately cover the
            // mask. Sample uncovered interiors, retaining every source mask kind.
            if(FVector2D::Distance(PlotP,Icon)<55)continue;
            if(D.Z<-.01){const auto Target=O+D*((S.Depth*.7-O.Z)/D.Z);const FVector2D End(FMath::Clamp(R.Left+(.5-Target.X/Map.Span.X)*(R.Right-R.Left),L,Right),FMath::Clamp(R.Top+(.5-Target.Y/Map.Span.Y)*(R.Bottom-R.Top),Top,Bottom));if(FMath::PointDistToSegment(FVector(PlotP,0),FVector(Icon,0),FVector(End,0))<3)continue;}
            const uint8 Mask=Masks[Y*40+X];const FLinearColor C=Mask?FLinearColor((Mask&2)?.9:0,(Mask&1)?.85:.1,(Mask&4)?.9:.1,.33):FLinearColor(.9,.08,.08,.55);
            // Slate's SDR target blends the packed sRGB tint with the sRGB
            // underlay (not a linear-light blend followed by sRGB conversion).
            const auto Tint=C.ToFColorSRGB();const FColor ExpectedColor(FMath::RoundToInt(172*(1-C.A)+Tint.R*C.A),FMath::RoundToInt(111*(1-C.A)+Tint.G*C.A),FMath::RoundToInt(75*(1-C.A)+Tint.B*C.A));
            const auto Screen=Pixel(M,G,FVector2D(R.Left+(.5-P.X/Map.Span.X)*(R.Right-R.Left),R.Top+(.5-P.Y/Map.Span.Y)*(R.Bottom-R.Top)),Size);const auto PixelColor=Pixels[FMath::Clamp(FMath::FloorToInt(Screen.Y),0,Size.Y-1)*Size.X+FMath::Clamp(FMath::FloorToInt(Screen.X),0,Size.X-1)];
            const auto* A=M.GetDisplayAnalysis();const int IX=FMath::FloorToInt((P.X+S.Length*.5)/A->DX),IY=FMath::FloorToInt((P.Y+S.Width*.5)/A->DY);if(!A->Cells[IY*A->NX+IX].Count)continue;
            if(Checked<12||!Kinds.Contains(Mask))T.TestTrue(FString::Printf(TEXT("coverage raster source=(%d,%d) mask=%d actual=%d,%d,%d expected=%d,%d,%d"),Y,X,Mask,PixelColor.R,PixelColor.G,PixelColor.B,ExpectedColor.R,ExpectedColor.G,ExpectedColor.B),FMath::Abs(int(PixelColor.R)-ExpectedColor.R)<=6&&FMath::Abs(int(PixelColor.G)-ExpectedColor.G)<=6&&FMath::Abs(int(PixelColor.B)-ExpectedColor.B)<=6);
            Kinds.Add(Mask);++Checked;
        }
        T.TestTrue(TEXT("asymmetric coverage mask has multiple observed contributions"),Checked>100&&Kinds.Num()>=2);Report.SetNumberField(TEXT("coverage_samples"),Checked);Report.SetNumberField(TEXT("coverage_mask_kinds"),Kinds.Num());
    }
    static void Input(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        const auto& Settings=M.GetSurface()->Settings;auto* View=M.HeatView.Get();const auto G=View->GetCachedGeometry();const auto Map=ShipHatch::FViewTransform::Top(Settings);const auto R=Map.Fit(G.GetLocalSize(),true,M.GetFontScale());
        const TSet<FKey> Keys{EKeys::LeftMouseButton};auto Event=[&](FVector2D P){return FPointerEvent(0,P,P,Keys,EKeys::LeftMouseButton,0,FModifierKeysState());};
        const uint32 Hash=M.GetSurface()->Hash();const uint64 Revision=M.Simulation->Revision();
        for(auto XY:{FVector2D(4,-5),FVector2D(-4,-5),FVector2D(4,5),FVector2D(-4,5)})
        {
            M.SetSection(true,.5);const auto P=G.LocalToAbsolute(Map.ToPixel(FVector(XY,0),R));
            View->TakeWidget()->OnMouseMove(G,Event(P));const auto Tooltip=View->TakeWidget()->GetToolTip();const auto* A=M.GetDisplayAnalysis();
            const int32 IX=FMath::FloorToInt((XY.X+Settings.Length*.5)/A->DX),IY=FMath::FloorToInt((XY.Y+Settings.Width*.5)/A->DY);
            T.TestTrue(TEXT("hover reverse mapping selects the original observation cell"),Tooltip.IsValid()&&StaticCastSharedRef<SToolTip>(Tooltip->AsWidget())->GetTextTooltip().EqualTo(M.CellTooltip(IY*A->NX+IX)));
            T.TestTrue(TEXT("production Slate 2D click handled"),View->TakeWidget()->OnMouseButtonDown(G,Event(P)).IsEventHandled());
            T.TestTrue(TEXT("2D click updates actual camera target in all four quadrants <= 1mm"),M.Focus.Equals(XY,.001));
            const FVector Target=M.Simulation->GetActorTransform().TransformPosition(FVector(XY.X*100,XY.Y*100,M.Simulation->HoldDepth*70));
            T.TestTrue(TEXT("actual 3D camera forward ray reaches clicked target <= 1mm"),FVector::CrossProduct(Target-M.Host->ViewCamera->GetActorLocation(),M.Host->ViewCamera->GetActorForwardVector()).Size()<.1);
        }
        for(bool Along:{true,false})
        {
            M.SetSection(Along,.5);const auto From=G.LocalToAbsolute(Map.ToPixel(FVector::ZeroVector,R)),To=G.LocalToAbsolute(Map.ToPixel(Along?FVector(0,6,0):FVector(5,0,0),R));
            View->TakeWidget()->OnMouseButtonDown(G,Event(From));View->TakeWidget()->OnMouseMove(G,Event(To));View->TakeWidget()->OnMouseButtonUp(G,Event(To));
            T.TestTrue(TEXT("section drag uses shared inverse for both axes <= 1mm"),FMath::Abs(M.SectionOffset()-(Along?6:5))<.001);
        }
        M.bDigEnabled=true;const auto Focus=M.Focus;const auto P=G.LocalToAbsolute(Map.ToPixel(FVector(4,4,0),R));View->TakeWidget()->OnMouseButtonDown(G,Event(P));
        T.TestTrue(TEXT("2D focus remains disabled during excavation"),M.Focus.Equals(Focus));M.bDigEnabled=false;
        T.TestEqual(TEXT("display transforms and clicks never edit heightfield"),M.GetSurface()->Hash(),Hash);T.TestEqual(TEXT("display transforms never change acquisition revision"),M.Simulation->Revision(),Revision);M.SetCameraPreset(0);
    }
};
namespace
{
FString OrientationRoot()
{FString P;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-05/focus"):P;}
class FHatchOrientation final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Phase=0,State=0;double Start=FPlatformTime::Seconds(),Changed=0;uint64 Builds=0;
    TUniquePtr<TGuardValue<bool>> Messages;
    void Shot(UWorld* W,UShipHatchMonitorWidget& M)
    {
        TArray<FColor> Pixels;FIntVector Size;
        if(!Test->TestTrue(TEXT("actual D3D12 viewport capture"),FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;
        const TCHAR* Names[]={TEXT("iron-top-length.png"),TEXT("iron-heat-length.png"),TEXT("iron-heat-width.png"),TEXT("iron-oblique-camera.png"),TEXT("iron-oblique-coverage.png")};const FString Path=OrientationRoot()/TEXT("captures")/Names[State];
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*Path);
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("fixture_seed"),20261020);R->SetNumberField(TEXT("surface_crc"),M.GetSurface()->Hash());R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetStringField(TEXT("kind"),TEXT("automated RHI; not direct mouse acceptance"));R->SetNumberField(TEXT("peak_local_x_m"),5);R->SetNumberField(TEXT("peak_local_y_m"),6);
        if(State<3)FHatchOrientationProbe::Check(*Test,M,State,Pixels,Size,*R);else FHatchOrientationProbe::Overlays(*Test,M,State,Pixels,Size,*R);
        FString JSON;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));FFileHelper::SaveStringToFile(JSON,*FPaths::ChangeExtension(Path,TEXT("json")));
    }
public:
    explicit FHatchOrientation(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();const bool Manual=FParse::Param(FCommandLine::Get(),TEXT("HatchOrientationManual"));if(Now-Start>(Manual?600:120)){Test->AddError(TEXT("orientation fixture timeout"));return true;}
        if(Phase==2)return IFileManager::Get().FileExists(*(OrientationRoot()/TEXT("finish.flag")));
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;
        if(!H||!H->Monitor||!H->Simulation||!H->Monitor->GetAnalysis())return false;auto& M=*H->Monitor;
        if(Phase==0){Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);Builds=M.AnalysisBuilds;FHatchOrientationProbe::Prepare(M);Changed=Now;Phase=1;return false;}
        if(Now-Changed<1||M.AnalysisBuilds<=Builds||!H->Simulation->HasFreshMeasurement())return false;
        Shot(W,M);if(State==2)FHatchOrientationProbe::Input(*Test,M);if(++State<5){FHatchOrientationProbe::View(M,State);Changed=Now;return false;}
        if(Manual){FHatchOrientationProbe::Manual(M);FFileHelper::SaveStringToFile(TEXT("ready; automated fixture checks finished; direct mouse pending"),*(OrientationRoot()/TEXT("ready.txt")));Phase=2;return false;}return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchOrientationCaptures,"HATCH05.CapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchOrientationCaptures::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchOrientation(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchOrientationRhi,"MA0T10.Hatch.ViewOrientationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchOrientationRhi::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchOrientation(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchViewTransform,"MA0T10.Hatch.ViewTransform",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchViewTransform::RunTest(const FString&)
{
    const ShipHatch::FSettings S;const auto Map=ShipHatch::FViewTransform::Top(S);const FSlateRect R(17,31,617,721);
    const FTransform Pose(FRotator(0,37,0),FVector(1200,-700,80),FVector(1.2,.8,1));
    for(int Y=-1;Y<=1;++Y)for(int X=-1;X<=1;++X)
    {
        const FVector P(X*S.OpeningLength*.4,Y*S.OpeningWidth*.4,0);const auto Pixel=Map.ToPixel(P,R);
        TestTrue(TEXT("asymmetric SI round trip"),Map.FromPixel(Pixel,R).Equals(P,1.e-9));
        const auto World=Pose.TransformPosition(P*100);TestTrue(TEXT("rotated translated scaled world round trip cm"),Map.PixelToWorld(Map.WorldToPixel(World,Pose,R),Pose,R).Equals(World,1.e-6));
        const auto Rotation=ShipHatch::FViewTransform::TopRotation();const double Right=FVector::DotProduct(P,Rotation.RotateVector(FVector::RightVector)),Down=-FVector::DotProduct(P,Rotation.RotateVector(FVector::UpVector));
        TestTrue(TEXT("UE camera basis agrees with map right/down"),FMath::Abs(Map.ToUnit(P).X-(.5+Right/Map.Span.X))<1.e-10&&FMath::Abs(Map.ToUnit(P).Y-(.5+Down/Map.Span.Y))<1.e-10);
    }
    TestTrue(TEXT("sea label belongs above center"),Map.ToUnit(FVector(0,1,0)).Y<.5);
    for(bool Along:{true,false}){const auto Section=ShipHatch::FViewTransform::Section(Along,20,30);const auto Direction=Section.DirectionToPixel(Along?FVector(1,0,0):FVector(0,1,0),R);TestTrue(TEXT("section increasing SI axis goes left; looking side is explicit"),Direction.X<0&&FMath::Abs(Direction.Y)<1.e-9);}
    return true;
}
#endif
