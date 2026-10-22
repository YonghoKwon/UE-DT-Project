#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchOverlay.h"
#include "ma0t10_dt/MA0T10/UI/SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/WidgetSwitcher.h"
#include "Camera/CameraActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"
#include "Json.h"

struct FHatchFeedbackProbe
{
    static TWeakObjectPtr<UTextBlock> ComboLifetime(FAutomationTestBase& T,UShipHatchMonitorWidget& M,TWeakPtr<SWidget>& NativeWeak)
    {
        auto* Label=CastChecked<UTextBlock>(M.ComboEntry(TEXT("GC lifetime probe")));
        TWeakObjectPtr<UTextBlock> Weak=Label;
        TSharedPtr<SWidget> Native=Label->TakeWidget();
        NativeWeak=Native;
        CollectGarbage(RF_NoFlags);
        const bool Alive=Weak.IsValid();
        T.TestTrue(TEXT("generated combo UTextBlock survives GC while Slate still owns its raw StrikeBrush"),Alive);
        if(Alive)Native->SlatePrepass(1);
        return Weak;
    }
    static void ComboReleased(FAutomationTestBase& T,UShipHatchMonitorWidget& M,TWeakObjectPtr<UTextBlock> Weak)
    {
        auto* Next=CastChecked<UTextBlock>(M.ComboEntry(TEXT("replacement lifetime probe")));
        TSharedPtr<SWidget> NextNative=Next->TakeWidget();
        CollectGarbage(RF_NoFlags);
        T.TestFalse(TEXT("expired generated combo entry is released after native content replacement"),Weak.IsValid());
    }
    static void Check(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        auto& S=*M.Simulation;T.TestTrue(TEXT("default is grab unloader field layout, three live selected sensors"),S.bFieldLayout&&S.bShowUnloader&&S.CabRight&&S.CabRight->IsSensorRunning());
        int32 Bodies=0,Unloaders=0;for(TActorIterator<AShipHatchPart>I(S.GetWorld());I;++I)if(I->ActorHasTag(TEXT("ShipHatchPreviewSensor"))){++Bodies;Unloaders+=I->ActorHasTag(TEXT("ShipHatchUnloader"));T.TestEqual(TEXT("only the physical GTSU blocks measured rays; sensor badges remain decorative"),I->Mesh->GetCollisionEnabled(),I->ActorHasTag(TEXT("ShipHatchUnloader"))?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);}
        T.TestEqual(TEXT("three bodies plus one low-poly unloader; no high mast"),Bodies,4);T.TestEqual(TEXT("one grab unloader display"),Unloaders,1);
        for(int32 I=0;I<3;++I){auto* A=S.SensorAt(I);T.TestNotNull(TEXT("selected detector exists"),A);if(!A)continue;T.TestTrue(TEXT("installation SI origin equals actual sensor transform"),S.GetActorTransform().InverseTransformPosition(A->GetActorLocation()).Equals(S.SensorOrigin(I)*100,.001));T.TestEqual(TEXT("user-approved default: OS1-64 Rev7 three devices"),A->ScanComponent->DeviceProfile,EVirtualLidarDeviceProfile::OusterOS1_64_Rev7);}
        T.TestTrue(TEXT("boom end is on sea +Y side of cab pair"),S.SensorOrigin(0).Y>S.SensorOrigin(1).Y&&S.SensorOrigin(0).Y>0);
        for(bool Along:{false,true})for(double N:{0.,.17,.5,.83,1.})
        {
            M.SetSection(Along,N);const auto& Samples=M.GetSectionSamples();T.TestTrue(TEXT("section has exact grid and diagonal intersections"),Samples.Num()>2);
            for(int32 I=0;I<Samples.Num();++I){const auto& P=Samples[I];const double H=Along?S.Surface().Height(P.Position,M.SectionOffset()):S.Surface().Height(M.SectionOffset(),P.Position);T.TestTrue(TEXT("section vertex equals rendered triangle height"),FMath::Abs(P.Height-H)<1.e-9);if(I){const auto& Q=Samples[I-1];const double X=(P.Position+Q.Position)*.5,Mid=Along?S.Surface().Height(X,M.SectionOffset()):S.Surface().Height(M.SectionOffset(),X);T.TestTrue(TEXT("section linear segment matches same triangle interior"),FMath::Abs(Mid-(Q.Height+P.Height)*.5)<1.e-5);}}
        }
        const auto HoldPose=S.GetActorTransform();const FVector CabRay=HoldPose.TransformPosition(FVector(S.CabX+.4,S.CabY+.5,S.CabRoofZ()+.2)*100),Down=HoldPose.TransformVector(FVector(0,0,-1));FVector CargoHit;
        FHitResult Physical;FCollisionQueryParams CabQuery(SCENE_QUERY_STAT(HatchCabPickPolicy),true);
        T.TestTrue(TEXT("visible cab still occludes ordinary physics/measurement rays"),S.GetWorld()->LineTraceSingleByChannel(Physical,CabRay,CabRay+Down*10000,ECC_Visibility,CabQuery)&&Physical.GetComponent()==S.CabPrimitive());
        T.TestTrue(TEXT("independent visible cab does not block cargo location selection"),S.PickCargo(CabRay,Down,CargoHit));S.ShowUnloader(false);T.TestTrue(TEXT("hidden cab leaves the same measured cargo pickable"),S.PickCargo(CabRay,Down,CargoHit));S.ShowUnloader(true);
        M.SetSection(true,.5);M.SetMapHeat(false);T.TestEqual(TEXT("map top shares one area"),M.MapSwitcher->GetActiveWidgetIndex(),0);M.SetMapHeat(true);T.TestEqual(TEXT("map heat shares same area"),M.MapSwitcher->GetActiveWidgetIndex(),1);
        M.OnCoverage();T.TestEqual(TEXT("contribution overlay has fixed 40x40 cells"),M.GetCoverageMask().Num(),1600);
        for(uint8 Mask:M.GetCoverageMask())T.TestTrue(TEXT("coverage masks contain only three sensor bits"),Mask<=7);M.OnCoverage();
        FVector O,D;double Fov;T.TestTrue(TEXT("camera marker has live 3D pose"),M.CameraInHold(O,D,Fov));T.TestTrue(TEXT("2D marker origin matches camera world-to-hold mapping"),O.Equals(S.GetActorTransform().InverseTransformPosition(M.Host->ViewCamera->GetActorLocation())*.01,1.e-8));
        T.TestTrue(TEXT("2D marker direction matches actual camera"),D.Equals(S.GetActorTransform().InverseTransformVectorNoScale(M.Host->ViewCamera->GetActorForwardVector()),1.e-8));
        M.SetFontPercent(150);auto* Entry=CastChecked<UTextBlock>(M.ComboEntry(TEXT("Select")));T.TestTrue(TEXT("generated selected/list text font is 150 percent"),Entry->GetFont().Size==FMath::RoundToInt(FSensorOwnedPresentationStyle::BodySize*1.5));
        const auto Text=Entry->GetColorAndOpacity().GetSpecifiedColor(),BG=FSensorOwnedPresentationStyle::Panel();const double L=Text.GetLuminance(),B=BG.GetLuminance();T.TestTrue(TEXT("Select text contrast exceeds 4.5 on normal list background"),(L+.05)/(B+.05)>=4.5);T.TestTrue(TEXT("Select text contrast exceeds 4.5 on hovered/selected list"),(L+.05)/(FSensorOwnedPresentationStyle::Selection().GetLuminance()+.05)>=4.5);M.SetFontPercent(100);
        const auto F=S.MLX->ScanComponent->GetLastFrameSnapshot();const uint32 CRC=FCrc::MemCrc32(F->Points->GetData(),F->Points->Num()*sizeof(FVirtualLidarPoint));const auto Rev=S.Revision();
        for(int32 I=0;I<3;++I)
        {
            if(I==0)M.OnNoon();if(I==1)M.OnEvening();if(I==2)M.OnWorkLight();T.TestEqual(TEXT("lighting never changes acquisition geometry revision"),S.Revision(),Rev);T.TestEqual(TEXT("lighting does not mutate immutable measured points"),FCrc::MemCrc32(F->Points->GetData(),F->Points->Num()*sizeof(FVirtualLidarPoint)),CRC);
            for(TActorIterator<ADirectionalLight> Light(S.GetWorld());Light;++Light){T.TestTrue(TEXT("lighting preset reaches actual directional-light orientation"),Light->GetActorQuat().Equals(FRotator(-S.LightElevation,S.LightAzimuth,0).Quaternion(),1.e-6));T.TestTrue(TEXT("lighting intensity reaches actual light component"),FMath::Abs(Light->GetLightComponent()->Intensity-S.LightIntensity)<1.e-5);}
        }
        M.OnSensorColors();T.TestEqual(TEXT("sensor tint cannot mutate measured points"),FCrc::MemCrc32(F->Points->GetData(),F->Points->Num()*sizeof(FVirtualLidarPoint)),CRC);M.OnSensorColors();
    }
    static void Configure(UShipHatchMonitorWidget& M,int32 State)
    {
        M.SetSplitView(true);if(M.bSettings)M.OnSettings();M.SetMapHeat(true);
        if(State==0){M.SelectView(0);M.SetCameraPreset(1);M.SetSection(true,.5);}
        if(State==1)M.SetSection(true,.25);if(State==2)M.SetSection(true,.75);
        if(State==3){M.SetSection(false,.5);M.SetMapHeat(false);}
        if(State==4){M.SelectView(1);if(!M.bSensorColors)M.OnSensorColors();M.OnCoverage();}
        if(State==5){M.SelectView(0);M.OnNoon();}if(State==6)M.OnEvening();if(State==7)M.OnWorkLight();
        if(State==8){M.OnSettings();CastChecked<UScrollBox>(M.SettingsPanel->GetContent())->ScrollToStart();M.CargoChoice->TakeWidget();StaticCastSharedRef<SComboBox<TSharedPtr<FString>>>(M.CargoChoice->TakeWidget())->SetIsOpen(true);}
    }
    static void CloseCombo(UShipHatchMonitorWidget& M){StaticCastSharedRef<SComboBox<TSharedPtr<FString>>>(M.CargoChoice->TakeWidget())->SetIsOpen(false);if(M.bSettings)M.OnSettings();}
    static void Profile(UShipHatchMonitorWidget& M,int32 Value){M.FieldChoices[0]->SetSelectedIndex(Value);}
    static void PrepareGrab(UShipHatchMonitorWidget& M){M.Simulation->SetGrabVisible(true);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(1);M.bDigEnabled=true;}
    static bool Grab(UShipHatchMonitorWidget& M,FAutomationTestBase& Test)
    {
        auto& S=*M.Simulation;FVector2D Pixel;auto* PC=M.GetOwningPlayer();const auto G=M.GetCachedGeometry();int32 W,H;PC->GetViewportSize(W,H);
        for(auto XY:{FVector2D(5,0),FVector2D(-5,0),FVector2D(5,5),FVector2D(-5,5),FVector2D(5,-5),FVector2D(-5,-5),FVector2D(0,-7)})
        {const FVector P=S.GetActorTransform().TransformPosition(FVector(XY,S.Surface().Height(XY.X,XY.Y))*100);if(!PC->ProjectWorldLocationToScreen(P,Pixel))continue;
         const auto Position=G.LocalToAbsolute(Pixel*G.GetLocalSize()/FVector2D(W,H));if(M.DigAt(G,Position))return true;
         FVector O,D,Hit;PC->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,O,D);FVector Analytic;const bool Math=ShipHatch::Raycast(S.Surface(),S.GetActorTransform().InverseTransformPosition(O)*.01,S.GetActorTransform().InverseTransformVector(D),Analytic);
         FHitResult Raw;FCollisionQueryParams Q(SCENE_QUERY_STAT(HatchGrabDiagnostic),true);S.GetWorld()->LineTraceSingleByChannel(Raw,O,O+D*20000,ECC_Visibility,Q);
         Test.AddInfo(FString::Printf(TEXT("grab diagnostic coal=%d target=%.1f,%.1f screen=%.1f,%.1f scene=%d pick=%d analytic=%d z=%.6f raw=%s rawZ=%.6f"),S.bCoal,XY.X,XY.Y,Pixel.X,Pixel.Y,M.SceneInputAt(G,Position),S.PickCargo(O,D,Hit),Math,Math?Analytic.Z:-1.,Raw.GetActor()?*Raw.GetActor()->GetName():TEXT("none"),S.GetActorTransform().InverseTransformPosition(Raw.ImpactPoint).Z*.01));
         FHitResult Short;const FVector End=S.GetActorTransform().TransformPosition(Analytic*100);const bool ShortHit=S.GetWorld()->LineTraceSingleByChannel(Short,O,End+D*2,ECC_Visibility,Q);
         Test.AddInfo(FString::Printf(TEXT("grab detail ending=%d generating=%d cargo=%s hardware=%d short=%d shortActor=%s shortIsCargo=%d origin=%.3f,%.3f,%.3f"),S.bEnding,S.bGenerating,S.Cargo?*S.Cargo->GetName():TEXT("none"),S.PresentationOccludes(O,End),ShortHit,Short.GetActor()?*Short.GetActor()->GetName():TEXT("none"),Short.GetActor()==S.Cargo,O.X,O.Y,O.Z));
        }
        return false;
    }
    static void FloorView(UShipHatchMonitorWidget& M,int32 State)
    {M.bSensorColors=false;M.SelectColor(1);M.SelectView(State==0?0:State==1?1:4);}
    static void DischargeWait(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        auto& S=*M.Simulation;PrepareGrab(M);
        T.TestTrue(TEXT("first explicit dig starts a cycle"),Grab(M,T));
        M.LastOverlayUpdate=0;M.UpdateOverlay();
        M.FeedbackUntil=0;M.DischargeUntil=FPlatformTime::Seconds()-1;M.LastOverlayUpdate=0;M.UpdateOverlay();
        const FVector Wait(0,S.HopperCenterY(),FMath::Max(S.HopperRimZ()+4,S.HoldDepth+9));
        T.TestTrue(TEXT("completed discharge parks open grab above hopper"),M.bFeedbackGrabParked&&S.bGrabPoseOverride&&S.GrabLipPose.Equals(Wait,.001)&&S.GrabOpenFraction==1);
        const auto Hash=S.Surface().Hash();
        for(int32 I=0;I<10;++I){M.LastOverlayUpdate=0;M.UpdateOverlay();}
        T.TestTrue(TEXT("refresh without click keeps hopper pose and does not dig"),S.GrabLipPose.Equals(Wait,.001)&&S.Surface().Hash()==Hash&&!S.IsGrabWorkActive());
        T.TestTrue(TEXT("next explicit click starts next cycle"),Grab(M,T));
        T.TestTrue(TEXT("next cycle owns grab work and leaves parked state"),S.IsGrabWorkActive()&&!M.bFeedbackGrabParked);
        T.TestTrue(TEXT("next command travels from hopper before excavating"),M.bReturningFromHopper&&S.Surface().Hash()==Hash);
        M.ReturnToHatchUntil=FPlatformTime::Seconds()-1;M.LastOverlayUpdate=0;M.UpdateOverlay();
        T.TestTrue(TEXT("excavation starts only after travel completes"),!M.bReturningFromHopper&&S.Surface().Hash()!=Hash);
        M.RestoreFeedbackGrab();T.TestFalse(TEXT("cancellation releases owned pose"),S.bGrabPoseOverride);
    }
    static void FloorMeasurements(FAutomationTestBase& T,AShipHatchSimulation& S)
    {
        int32 Floor=0,FalseCargo=0;const auto& Surface=S.Surface();
        for(int32 Device=0;Device<3;++Device)if(S.SensorEnabled(Device))
        {
            const auto Frame=S.SensorAt(Device)->ScanComponent->GetLastFrameSnapshot();T.TestTrue(TEXT("floor classification uses actual fresh detector snapshot"),Frame.IsValid()&&Frame->Points.IsValid());if(!Frame||!Frame->Points)continue;
            for(const auto& P:*Frame->Points)if(P.bHit)
            {
                const FVector L=S.GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01;
                if(FMath::Abs(L.X)>=S.HoldLength*.5||FMath::Abs(L.Y)>=S.HoldWidth*.5)continue;
                if(P.SemanticLabel==TEXT("HatchHold")&&FMath::Abs(L.Z)<.15)++Floor;
                if(P.SemanticLabel!=TEXT("HatchCargo"))continue;
                const int32 X=FMath::FloorToInt((L.X+S.HoldLength*.5)/Surface.DX),Y=FMath::FloorToInt((L.Y+S.HoldWidth*.5)/Surface.DY);bool Empty=true;
                for(int32 J=FMath::Max(0,Y-1);J<=FMath::Min(Surface.NY,Y+2);++J)for(int32 I=FMath::Max(0,X-1);I<=FMath::Min(Surface.NX,X+2);++I)Empty&=Surface.At(I,J)<=1.e-4;
                FalseCargo+=Empty;
            }
        }
        T.TestTrue(TEXT("actual point clouds classify exposed tank top as HatchHold"),Floor>0);T.TestEqual(TEXT("no cargo points in interior of zero-thickness grid region"),FalseCargo,0);
    }
    static void Input(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        const auto G=M.GetCachedGeometry();const auto P=G.LocalToAbsolute(FVector2D((M.SceneRect.Left+M.SceneRect.Right)*.5,(M.SceneRect.Top+M.SceneRect.Bottom)*.6));
        auto Event=[](FVector2D P,FVector2D Last,bool Held,FKey Key){TSet<FKey> Keys;if(Held)Keys.Add(Key);return FPointerEvent(0,P,Last,Keys,Key,0,FModifierKeysState());};
        M.bDigEnabled=true;const auto Pose=M.Host->ViewCamera->GetActorTransform();const auto Hash=M.Simulation->Surface().Hash();
        M.NativeOnMouseButtonDown(G,Event(P,P,true,EKeys::LeftMouseButton));M.NativeOnMouseMove(G,Event(P+FVector2D(30,20),P,true,EKeys::LeftMouseButton));M.NativeOnMouseButtonUp(G,Event(P+FVector2D(30,20),P,false,EKeys::LeftMouseButton));
        T.TestTrue(TEXT("excavation left drag preserves full camera transform"),Pose.Equals(M.Host->ViewCamera->GetActorTransform()));T.TestEqual(TEXT("drag cannot scoop"),M.Simulation->Surface().Hash(),Hash);
        M.NativeOnMouseButtonDown(G,Event(P,P,true,EKeys::RightMouseButton));M.NativeOnMouseMove(G,Event(P+FVector2D(30,20),P,true,EKeys::RightMouseButton));M.NativeOnMouseButtonUp(G,Event(P+FVector2D(30,20),P,false,EKeys::RightMouseButton));T.TestFalse(TEXT("explicit right drag remains navigation"),Pose.Equals(M.Host->ViewCamera->GetActorTransform()));
        M.SetCameraPreset(0);M.bDigEnabled=false;
    }
};
namespace
{
FString FeedbackRoot(){FString P;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-04/focus"):P;}
class FHatchFeedback final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;bool Captures;int32 Phase=0,State=0,Material=0,FloorEdits=0;double Start=FPlatformTime::Seconds(),Changed=0;FVector2D FloorTarget=FVector2D::ZeroVector;bool bFloorTarget=false;TUniquePtr<TGuardValue<bool>> Messages;
    void Shot(UWorld* W,UShipHatchMonitorWidget& M,const FString& Name,bool Progress=false)
    {
        TArray<FColor> Pixels;FIntVector Size;if(!Test->TestTrue(TEXT("actual viewport RHI capture"),FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);const FString Path=FeedbackRoot()/TEXT("captures")/Name;IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);FFileHelper::SaveArrayToFile(PNG,*Path);
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetNumberField(TEXT("font_percent"),M.GetFontScale()*100);R->SetNumberField(TEXT("section_offset_m"),M.SectionOffset());R->SetBoolField(TEXT("section_along_length"),M.SectionAlongLength());R->SetNumberField(TEXT("remaining_m3"),M.GetSurface()->Volume);R->SetNumberField(TEXT("surface_crc"),M.GetSurface()->Hash());R->SetStringField(TEXT("capture_kind"),TEXT("automated RHI; not direct Computer Use"));FString JSON;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));FFileHelper::SaveStringToFile(JSON,*FPaths::ChangeExtension(Path,TEXT("json")));
        const auto* Sim=M.GetSimulation();double Opening,Floor,Area;Sim->Coverage(Opening,Floor,Area);R->SetNumberField(TEXT("opening_fov_percent"),Opening);R->SetNumberField(TEXT("visible_floor_fov_percent"),Floor);R->SetNumberField(TEXT("structurally_visible_floor_m2"),Area);R->SetNumberField(TEXT("removed_m3"),Sim->TotalRemovedVolume());
        const auto Window=FSlateApplication::Get().FindWidgetWindow(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef());R->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);R->SetNumberField(TEXT("umg_scale"),UWidgetLayoutLibrary::GetViewportScale(&M));
        TArray<TSharedPtr<FJsonValue>> Sensors;
        for(int32 I=0;I<3;++I)if(auto* A=Sim->SensorAt(I)){auto Row=MakeShared<FJsonObject>();const auto P=Sim->SensorOrigin(I);Row->SetNumberField(TEXT("index"),I);Row->SetNumberField(TEXT("x_m"),P.X);Row->SetNumberField(TEXT("y_m"),P.Y);Row->SetNumberField(TEXT("z_above_floor_m"),P.Z);Row->SetStringField(TEXT("profile"),StaticEnum<EVirtualLidarDeviceProfile>()->GetNameStringByValue(int64(A->ScanComponent->DeviceProfile)));Row->SetNumberField(TEXT("pitch_degrees"),A->GetActorRotation().Pitch);Row->SetNumberField(TEXT("yaw_degrees"),A->GetActorRotation().Yaw);Sensors.Add(MakeShared<FJsonValueObject>(Row));}
        R->SetArrayField(TEXT("sensors"),Sensors);R->SetStringField(TEXT("site_model"),TEXT("unconfirmed; screen-selectable"));R->SetNumberField(TEXT("display_hardware_measured_ray_occlusion_percent"),0);R->SetStringField(TEXT("hardware_measurement_policy"),TEXT("player-only decoration: no collision, SceneCapture, shadows or lidar response"));JSON.Reset();FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));FFileHelper::SaveStringToFile(JSON,*FPaths::ChangeExtension(Path,TEXT("json")));
        if(Progress&&Size.X==1920&&Size.Y==1080){const FString P=FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-04/progress")/FString::Printf(TEXT("%02d-%s-%s"),State+1,*FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M")),*Name);IFileManager::Get().MakeDirectory(*FPaths::GetPath(P),true);FFileHelper::SaveArrayToFile(PNG,*P);}
    }
public:
    FHatchFeedback(FAutomationTestBase* T,bool C):Test(T),Captures(C){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Start>240){Test->AddError(TEXT("HATCH04 feedback timeout"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
        AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;if(!H||!H->Monitor||!H->Simulation||(!H->Monitor->GetAnalysis()&&Phase!=8))return false;
        auto& M=*H->Monitor;auto& S=*H->Simulation;if(!Messages)Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
        if(Phase==0){FHatchFeedbackProbe::Check(*Test,M);FHatchFeedbackProbe::Input(*Test,M);int32 Font=100;FParse::Value(FCommandLine::Get(),TEXT("HatchFontPercent="),Font);M.SetFontPercent(Font);FHatchFeedbackProbe::Configure(M,0);Changed=Now;Phase=1;return false;}
        if(Phase==1)
        {
            if(Now-Changed<1)return false;
            if(Captures){const TCHAR* Names[]={TEXT("split"),TEXT("section-quarter"),TEXT("section-three-quarter"),TEXT("section-width-top"),TEXT("three-points-coverage"),TEXT("light-noon"),TEXT("light-evening"),TEXT("light-work"),TEXT("select-open")};Shot(W,M,FString::Printf(TEXT("%s-%s.png"),Material?TEXT("coal"):TEXT("iron"),Names[State]),Material==0&&State<3);}
            if(Captures&&++State<9){FHatchFeedbackProbe::Configure(M,State);Changed=Now;return false;}
            FHatchFeedbackProbe::CloseCombo(M);if(Material==0){FHatchFeedbackProbe::Profile(M,1);Phase=4;return false;}FHatchFeedbackProbe::PrepareGrab(M);State=-1;Changed=Now;Phase=8;return false;
        }
        if(Phase==4){Test->TestEqual(TEXT("screen profile selection immediately measures with ML-X80 Integration"),S.MLX->ScanComponent->DeviceProfile,EVirtualLidarDeviceProfile::IYOBOT_MLX80);FHatchFeedbackProbe::Profile(M,3);Phase=5;return false;}
        if(Phase==5){Test->TestEqual(TEXT("screen profile selection immediately measures with ML-X80 Native"),S.MLX->ScanComponent->DeviceProfile,EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);FHatchFeedbackProbe::Profile(M,2);S.SensorOffsets[2].X=.5;S.SensorTiltTrim[2]=5;S.SensorYaw[2]=7;S.RefreshFieldLayout();Phase=6;return false;}
        if(Phase==6)
        {
            Test->TestTrue(TEXT("per-sensor installation offset reaches actual sensor"),S.GetActorTransform().InverseTransformPosition(S.CabRight->GetActorLocation()).Equals(S.SensorOrigin(2)*100,.001));
            const auto Expected=S.GetActorQuat()*FRotator(-107.5,7,0).Quaternion();Test->TestTrue(TEXT("per-sensor tilt and yaw reach actual acquisition pose"),S.CabRight->GetActorQuat().Equals(Expected,1.e-6));
            S.SetFieldSensor(2,false,1);Test->TestFalse(TEXT("individual third-device off stops only its selection"),S.CabRight->IsSensorRunning());S.SensorOffsets[2]=FVector::ZeroVector;S.SensorTiltTrim[2]=S.SensorYaw[2]=0;S.SetFieldSensor(2,true,1);Phase=7;return false;
        }
        if(Phase==7){Test->TestTrue(TEXT("individual third-device on resumes acquisition"),S.CabRight->IsSensorRunning());FHatchFeedbackProbe::PrepareGrab(M);State=-1;Changed=Now;Phase=8;return false;}
        if(Phase==8)
        {
            if(State<0){if(Now-Changed<.3)return false;Test->TestTrue(TEXT("production click starts grab animation"),FHatchFeedbackProbe::Grab(M,*Test));State=0;Changed=Now;return false;}
            const double Times[]={.08,.55,1.};if(Now-Changed<Times[State])return false;
            if(Captures)Shot(W,M,FString::Printf(TEXT("%s-grab-%s.png"),Material?TEXT("coal"):TEXT("iron"),State==0?TEXT("descend"):State==1?TEXT("close"):TEXT("lift")));
            if(++State<3)return false;S.FillMin=S.FillMax=1;S.Regenerate(false);Phase=2;Changed=Now;return false;
        }
        if(Phase==2)
        {
            if(Now-Changed<.5)return false;if(!bFloorTarget){double Best=0;for(double X=-8;X<=8;X+=.5)for(double Y=-9;Y<=9;Y+=.5){const double Ht=S.Surface().Height(X,Y);if(Ht>Best&&FMath::Abs(X)>3){Best=Ht;FloorTarget=FVector2D(X,Y);}}bFloorTarget=true;}
            const FVector O=S.GetActorTransform().TransformPosition(FVector(FloorTarget,S.HoldDepth+.5)*100),D=S.GetActorTransform().TransformVector(FVector(0,0,-1));
            if(S.Surface().Height(FloorTarget.X,FloorTarget.Y)>1.e-4&&FloorEdits++<30){S.Dig(O,D,5,3);return false;}
            FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(HatchFloor),true);const bool Floor=W->LineTraceSingleByChannel(Hit,O,O+D*6000,ECC_Visibility,Q);
            Test->AddInfo(FString::Printf(TEXT("floor diagnostic cargo=%s target=(%.3f,%.3f) height=%.9f edits=%d removed=%.9f hitZ=%.9f tagFloor=%d"),Material?TEXT("coal"):TEXT("iron"),FloorTarget.X,FloorTarget.Y,S.Surface().Height(FloorTarget.X,FloorTarget.Y),FloorEdits,S.TotalRemovedVolume(),S.GetActorTransform().InverseTransformPosition(Hit.ImpactPoint).Z*.01,Floor&&Hit.GetActor()&&Hit.GetActor()->ActorHasTag(TEXT("HatchHold"))));
            Test->TestTrue(TEXT("zero-thickness region exposes actual tank top collision/classification"),Floor&&Hit.GetActor()&&Hit.GetActor()->ActorHasTag(TEXT("HatchHold")));
            Test->TestTrue(TEXT("floor fixture performed real grab edits"),FloorEdits>0);
            Test->TestTrue(TEXT("zero-thickness center has no cargo hit"),S.Surface().Height(FloorTarget.X,FloorTarget.Y)<=1.e-4);
            FHatchFeedbackProbe::FloorView(M,0);M.SetSplitView(true);M.SetCameraPreset(0);M.SetSection(true,FloorTarget.Y/S.HoldWidth+.5);Changed=Now;State=0;Phase=3;return false;
        }
        if(Phase==3)
        {
            if(Now-Changed<1)return false;FHatchFeedbackProbe::FloorMeasurements(*Test,S);if(Captures)Shot(W,M,FString::Printf(TEXT("%s-floor-%s.png"),Material?TEXT("coal"):TEXT("iron"),State==0?TEXT("3d"):State==1?TEXT("points"):TEXT("section")));
            if(Captures&&++State<3){FHatchFeedbackProbe::FloorView(M,State);Changed=Now;return false;}
            if(Material++==0){State=0;FloorEdits=0;bFloorTarget=false;S.bCoal=true;S.FillMin=35;S.FillMax=65;S.Regenerate(false);FHatchFeedbackProbe::Configure(M,0);Phase=1;Changed=Now;return false;}
            return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchFeedbackRhi,"MA0T10.Hatch.UserFeedbackRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchFeedbackRhi::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchFeedback(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchFeedbackCaptures,"HATCH04.CapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchFeedbackCaptures::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchFeedback(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
namespace
{
class FHatchComboLifetimeCommand final:public IAutomationLatentCommand
{
public:
    explicit FHatchComboLifetimeCommand(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>40){Test->AddError(TEXT("combo lifetime fixture timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        for(TActorIterator<AShipHatchMonitorHost> I(World);I;++I)if(I->Monitor&&I->Monitor->GetAnalysis())
        {
            if(!Began){Generated=FHatchFeedbackProbe::ComboLifetime(*Test,*I->Monitor,Native);Began=true;return !Generated.IsValid();}
            // The caller's native reference ended with the preceding update.
            // UTextBlock itself still owns MyTextBlock until the owner is pruned.
            FHatchFeedbackProbe::ComboReleased(*Test,*I->Monitor,Generated);
            Test->TestFalse(TEXT("native combo row is released with its expired generated owner"),Native.IsValid());return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test;double Start=FPlatformTime::Seconds();bool Began=false;TWeakObjectPtr<UTextBlock> Generated;TWeakPtr<SWidget> Native;
};
class FHatchDischargeWaitCommand final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Start=FPlatformTime::Seconds();
public:
    explicit FHatchDischargeWaitCommand(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>45){Test->AddError(TEXT("discharge wait fixture timeout"));return true;}
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&FinalAcceptanceViewportReady(C.World()))
            for(TActorIterator<AShipHatchMonitorHost> H(C.World());H;++H)if(H->Monitor&&H->Monitor->GetAnalysis())
            {FHatchFeedbackProbe::DischargeWait(*Test,*H->Monitor);return true;}
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchComboLifetimeRhi,"MA0T10.Hatch.GeneratedComboLifetimeRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchComboLifetimeRhi::RunTest(const FString&)
{if(GUsingNullRHI){AddInfo(TEXT("[OPT-IN SKIP] rendered combo lifetime requires RHI"));return true;}if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchComboLifetimeCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchDischargeWaitRhi,"MA0T10.Hatch.DischargeWaitForClickRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchDischargeWaitRhi::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchDischargeWaitCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
