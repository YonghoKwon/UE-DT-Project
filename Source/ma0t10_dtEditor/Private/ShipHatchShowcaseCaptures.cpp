#include "SensorOwnedRayTracing.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "Camera/CameraActor.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "DynamicRHI.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ProceduralMeshComponent.h"
#include "Misc/Crc.h"

namespace
{
FString ShowcaseRoot()
{FString P;FParse::Value(FCommandLine::Get(),TEXT("HatchShowcaseOutput="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/FOLLOWUP-26-28/showcase"):P;}
void ShowcaseJson(const FString& Name,const TSharedRef<FJsonObject>& R)
{FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));IFileManager::Get().MakeDirectory(*ShowcaseRoot(),true);FFileHelper::SaveStringToFile(S,*(ShowcaseRoot()/Name),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);}
TArray<TSharedPtr<FJsonValue>> ShowcaseVector(const FVector& P)
{return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)};}
class FHatchShowcaseRun final:public IAutomationLatentCommand
{
 FAutomationTestBase* T;TWeakObjectPtr<AShipHatchMonitorHost> Host;
 double Started=FPlatformTime::Seconds(),Changed=0;int Shot=0,Digs=0,CapturedImages=0,FloorProbe=-1;bool Initialized=false,Ui=false,SettingsOpen=false,Digging=false,Polish=false;
 const bool Presentation=FParse::Param(FCommandLine::Get(),TEXT("HatchPresentationOnly"));
 TArray<TWeakObjectPtr<UPrimitiveComponent>> OwnedPlayerHides;
 FVector CameraLocal,AimLocal;double BeforeVolume=0;uint32 BeforeHash=0,OpenGrabHash=0;
 const TCHAR* Names[29]={TEXT("00-quay-vessel-overview"),TEXT("01-vessel-gtsu-side"),TEXT("02-ref4-composition"),TEXT("03-portal-boom-machine"),TEXT("04-covers-seaward"),TEXT("05-covers-landward"),TEXT("06-hopper-inside-seaward-leg"),TEXT("07-clamshell-open"),TEXT("08-clamshell-closed"),TEXT("09-iron-surface"),TEXT("10-coal-surface"),TEXT("11-three-sensor-colors"),TEXT("12-coverage-heatmap"),TEXT("13-measured-section"),TEXT("14-top-plan"),TEXT("15-settings-three-panels"),TEXT("16-lidar-mid360"),TEXT("17-lidar-avia"),TEXT("18-lidar-os1-64"),TEXT("19-lidar-xt32"),TEXT("20-lidar-vlp16"),TEXT("21-dig-before"),TEXT("22-dig-after"),TEXT("23-exposed-floor-close"),TEXT("24-exposed-floor-gpu-points"),TEXT("25-full-load-corner"),TEXT("27-exposed-floor-section"),TEXT("pres-01-gtsu-ship-normal"),TEXT("pres-02-gtsu-ship-points")};
 TSharedRef<FJsonObject> LidarMetrics(AShipHatchSimulation& S,int Index)
 {
  auto R=MakeShared<FJsonObject>();const auto* C=S.SensorAt(Index)->ScanComponent.Get();const auto F=C->GetLastFrameSnapshot();
  T->TestTrue(TEXT("comparison metrics use a full immutable HWRT frame"),F&&F->Points&&F->AcquisitionBackend==EVirtualLidarAcquisitionBackend::HardwareRayTracing&&F->RequestedRayCount==C->HorizontalSamples*C->VerticalChannels);
  TArray<ShipHatch::FObservation> Cargo;TSet<int32> Floor;int Hits=0,FloorPoints=0;
  const auto Settings=S.Settings();const int NX=FMath::CeilToInt(Settings.Length),NY=FMath::CeilToInt(Settings.Width);const double DX=Settings.Length/NX,DY=Settings.Width/NY;
  if(F&&F->Points)for(const auto& P:*F->Points)if(P.bHit)
  {
   ++Hits;const FVector L=S.GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01;
   if(P.SemanticLabel==TEXT("HatchCargo"))Cargo.Add({L,P.SemanticLabel});
   const double X=L.X+Settings.Length*.5,Y=L.Y+Settings.Width*.5;
   if(P.SemanticLabel==TEXT("HatchHold")&&FMath::Abs(L.Z)<=.02&&X>=0&&X<=Settings.Length&&Y>=0&&Y<=Settings.Width)
   {++FloorPoints;Floor.Add(FMath::Min(FMath::FloorToInt(Y/DY),NY-1)*NX+FMath::Min(FMath::FloorToInt(X/DX),NX-1));}
  }
  const auto Analysis=ShipHatch::Analyze(Settings,1,Cargo);
  R->SetStringField(TEXT("model"),C->DeviceSpec.Model);R->SetStringField(TEXT("sensor_id"),C->SensorId);R->SetStringField(TEXT("backend"),C->GetRuntimeStatus().ActiveAcquisitionBackend);
  R->SetNumberField(TEXT("rays"),F?F->RequestedRayCount:0);R->SetNumberField(TEXT("frame_point_count"),F&&F->Points?F->Points->Num():0);R->SetNumberField(TEXT("hit_points"),Hits);R->SetNumberField(TEXT("cargo_hit_points"),Cargo.Num());R->SetNumberField(TEXT("floor_hit_points"),FloorPoints);
  R->SetNumberField(TEXT("cargo_observed_percent"),100*Analysis.Coverage);R->SetNumberField(TEXT("floor_observed_percent"),100.*Floor.Num()/(NX*NY));R->SetNumberField(TEXT("cargo_observed_cells"),Analysis.Observed);R->SetNumberField(TEXT("floor_observed_cells"),Floor.Num());R->SetNumberField(TEXT("total_hold_cells"),NX*NY);R->SetNumberField(TEXT("cell_dx_m"),DX);R->SetNumberField(TEXT("cell_dy_m"),DY);
  R->SetStringField(TEXT("coverage_definition"),TEXT("actual semantic hits in full-hold1m-target grid; zero means unobserved, no extrapolation; floor=HatchHold absZ<=2cm"));
  R->SetNumberField(TEXT("measured_hz"),C->GetRuntimeStatus().MeasuredAcquisitionRateHz);R->SetNumberField(TEXT("requested_hz"),1./C->ScanInterval);R->SetNumberField(TEXT("max_range_m"),F?F->MaxDistanceCm*.01:0);R->SetNumberField(TEXT("horizontal_fov_deg"),C->HorizontalFov);R->SetNumberField(TEXT("vertical_min_deg"),C->MinVerticalAngle);R->SetNumberField(TEXT("vertical_max_deg"),C->MaxVerticalAngle);
  R->SetNumberField(TEXT("frame_id"),F?double(F->FrameId):0);R->SetStringField(TEXT("acquisition_utc"),F?F->GetAcquisitionStartUtc().ToIso8601():TEXT(""));R->SetNumberField(TEXT("surface_crc"),S.Surface().Hash());R->SetNumberField(TEXT("fill_percent"),S.Surface().Fill);R->SetNumberField(TEXT("production_digs"),Digs);
  if(F){R->SetArrayField(TEXT("acquisition_origin_hold_m"),ShowcaseVector(S.GetActorTransform().InverseTransformPosition(F->AcquisitionTransform.GetLocation())*.01));const auto Rot=F->AcquisitionTransform.Rotator();R->SetArrayField(TEXT("acquisition_pitch_yaw_roll"),ShowcaseVector(FVector(Rot.Pitch,Rot.Yaw,Rot.Roll)));}
  return R;
 }
 FString ShotName() const
 {const auto Size=FinalAcceptanceViewportSize();return FString(Names[Shot])+(Presentation?FString::Printf(TEXT("-%dx%d"),Size.X,Size.Y):TEXT(""));}
 void Regenerate(AShipHatchSimulation& S,bool Coal)
 {S.Seed=19019;S.FillMin=S.FillMax=65;S.bCoal=Coal;T->TestTrue(TEXT("showcase regenerates through production heightfield"),S.Regenerate(false));}
 void Colors(UShipHatchMonitorWidget& M)
 {if(!M.IsSensorColoring())M.ProcessEvent(M.FindFunctionChecked(TEXT("OnSensorColors")),nullptr);}
 void Frame(AShipHatchMonitorHost& H,AShipHatchSimulation& S,UShipHatchMonitorWidget& M)
 {
  if(auto* PC=M.GetOwningPlayer()){for(const auto& C:OwnedPlayerHides)PC->HiddenPrimitiveComponents.RemoveSingle(C);OwnedPlayerHides.Reset();}
  if(SettingsOpen){M.ProcessEvent(M.FindFunctionChecked(TEXT("OnSettings")),nullptr);SettingsOpen=false;}
  Ui=(Shot>=11&&Shot<=15)||Shot==26;M.SelectView(0);M.SetSplitView(false);M.SetModelPresentation(true);M.SetRenderOpacity(Ui?1:0);
  S.ShowUnloader(true);S.SetScanDisplay(false);S.SetGrabVisible(Shot==7||Shot==8);S.SetSensorMarkersVisible(false);
  auto* Camera=H.ViewCamera->GetCameraComponent();Camera->SetFieldOfView(58);
  if(Shot==0){CameraLocal=FVector(240,-230,185);AimLocal=FVector(0,-10,17);}
  if(Shot==1){CameraLocal=FVector(210,-145,92);AimLocal=FVector(0,-16,30);}
  if(Shot==2){CameraLocal=FVector(-100,-115,95);AimLocal=FVector(0,-18,30);}
  if(Shot==3){CameraLocal=FVector(58,-52,68);AimLocal=FVector(0,-22,42);}
  if(Shot==4){CameraLocal=FVector(32,36,45);AimLocal=FVector(0,0,24);}
  if(Shot==5){CameraLocal=FVector(28,-47,45);AimLocal=FVector(0,-5,24);}
  if(Shot==6){CameraLocal=FVector(-14,S.HopperCenterY()+16,S.HopperRimZ()+17);AimLocal=FVector(0,S.HopperCenterY(),S.HopperRimZ()-2);Camera->SetFieldOfView(57);}
  if(Shot==7||Shot==8)
  {const double Z=S.Surface().Height(0,4);T->TestTrue(TEXT("clamshell showcase pose clears independent cab and ropes"),S.TrySetGrabPose(FVector(0,4,Z+4),Shot==7?1:0));CameraLocal=FVector(13,19,Z+15);AimLocal=FVector(0,4,Z+7);Camera->SetFieldOfView(48);}
  if(Shot==9||Shot==10)
  {Regenerate(S,Shot==10);S.ShowUnloader(false);CameraLocal=FVector(1,-3,29);AimLocal=FVector(4,3,14);Camera->SetFieldOfView(48);}
  if(Shot==11){Regenerate(S,false);M.SelectView(1);Colors(M);CameraLocal=FVector(25,-5,65);AimLocal=FVector(0,2,12);}
  if(Shot==12)M.SelectView(3);
  if(Shot==13){M.SelectView(4);M.SetSection(true,.62);}
  if(Shot==14)M.SelectView(2);
  if(Shot==15){M.SelectView(0);M.SetSplitView(true);M.SetMapHeat(true);M.ProcessEvent(M.FindFunctionChecked(TEXT("OnSettings")),nullptr);SettingsOpen=true;CameraLocal=FVector(34,-35,63);AimLocal=FVector(0,0,12);}
  if(Shot>=16&&Shot<=20)
  {S.SetFieldSensor(0,true,Shot-13);M.SelectView(1);Colors(M);CameraLocal=S.SensorOrigin(0)+FVector(.6,-.9,.4);AimLocal=FVector(0,1,8);Camera->SetFieldOfView(65);}
  if(Shot==21){S.SetFieldSensor(0,true,6);Regenerate(S,false);}
  if(Shot==21||Shot==22){CameraLocal=FVector(24,-31,56);AimLocal=FVector(0,0,8);}
  if(Shot==23){S.ShowUnloader(false);CameraLocal=FVector(10,-11,12);AimLocal=FVector(1.5,2,0);Camera->SetFieldOfView(65);T->TestTrue(TEXT("close-up aims at actually excavated tank top"),S.Surface().Height(1.5,2)<=1.e-4);}
  if(Shot==24){M.SelectView(1);Colors(M);CameraLocal=FVector(25,-5,65);AimLocal=FVector(0,2,2);}
  if(Shot==26){M.SelectView(4);M.SetSection(true,.5);}
  if(Shot==25){S.Seed=19019;S.bCoal=false;S.FillMin=S.FillMax=95;S.Regenerate(false);S.ShowUnloader(false);CameraLocal=FVector(3.1,3.8,28);AimLocal=FVector(9.4,10.9,21.8851357283);Camera->SetFieldOfView(55);}
  if(Shot==27||Shot==28)
  {
   // Identical lens and pose for the normal / actual three-sensor HWRT pair.
   S.SetGrabVisible(true);
   T->TestTrue(TEXT("presentation grab clears the independent cab"),S.TrySetGrabPose(FVector(0,4,S.Surface().Height(0,4)+2),.65));
   CameraLocal=FVector(65,-80,92);AimLocal=FVector(0,-19,28);Camera->SetFieldOfView(58);
   if(Shot==28){M.SelectView(1);Colors(M);}
  }
  if(!Ui||Shot==11)M.SetModelPresentation(true);
  M.SetRenderOpacity(Ui?1:0);
  if(Shot==11||(Shot>=16&&Shot<=20)||Shot==24||Shot==28)if(auto* PC=M.GetOwningPlayer())if(auto* Cab=S.CabPrimitive())
  {
   TInlineComponentArray<UPrimitiveComponent*> Parts(Cab->GetOwner());for(auto* C:Parts)if(!PC->HiddenPrimitiveComponents.Contains(C)){PC->HiddenPrimitiveComponents.Add(C);OwnedPlayerHides.Add(C);}
   T->TestTrue(TEXT("pure point view keeps actual rig collision unchanged"),Cab->IsQueryCollisionEnabled());
  }
  const auto Pose=S.GetActorTransform();const auto P=Pose.TransformPosition(CameraLocal*100),A=Pose.TransformPosition(AimLocal*100);
  H.ViewCamera->SetActorLocationAndRotation(P,(A-P).Rotation());Changed=FPlatformTime::Seconds();
 }
 bool Capture(UWorld& W,AShipHatchMonitorHost& H,AShipHatchSimulation& S,UShipHatchMonitorWidget& M)
 {
  uint32 GrabHash=0;
  if(Shot==7||Shot==8)
  {
   auto* Mesh=Cast<UProceduralMeshComponent>(S.GrabPrimitive());const auto* Section=Mesh?Mesh->GetProcMeshSection(0):nullptr;
   if(!T->TestNotNull(TEXT("clamshell has actual measured triangles"),Section))return false;
   for(const auto& Vertex:Section->ProcVertexBuffer)GrabHash=FCrc::MemCrc32(&Vertex.Position,sizeof(Vertex.Position),GrabHash);
   if(Shot==7)OpenGrabHash=GrabHash;else T->TestTrue(TEXT("actual open and closed jaws have different geometry"),GrabHash!=OpenGrabHash);
   auto* PC=M.GetOwningPlayer();const double Z=S.Surface().Height(0,4)+4;
   for(int X:{-1,1})for(int Y:{-1,1})for(double Height:{0.,5.5})
   {FVector2D Pixel;const auto World=S.GetActorTransform().TransformPosition(FVector(X*2.2,4+Y*2.1,Z+Height)*100);T->TestTrue(TEXT("complete bucket and crosshead fit inside native frame"),PC->ProjectWorldLocationToScreen(World,Pixel)&&Pixel.X>8&&Pixel.X<1912&&Pixel.Y>8&&Pixel.Y<1072);}
  }
  TArray<FColor> Pixels;FIntVector Size;
  bool Ok=false;
  if(Ui)Ok=FSlateApplication::Get().TakeScreenshot(W.GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size);
  else{auto* V=W.GetGameViewport()->Viewport;const auto N=V->GetSizeXY();Size=FIntVector(N.X,N.Y,0);Ok=V->ReadPixels(Pixels);}
  const auto Expected=FinalAcceptanceViewportSize();
  if(!T->TestTrue(TEXT("showcase reads actual requested-resolution RHI pixels"),Ok&&Size.X==Expected.X&&Size.Y==Expected.Y))return false;
  int Visible=0;for(int I=0;I<Pixels.Num();I+=16)Visible+=Pixels[I].R>20||Pixels[I].G>20||Pixels[I].B>20;
  // A single sensor's sparse returns cover fewer pixels than a solid scene.
  // Keep a nonblank test for those frames and inspect their native PNGs too.
  const int MinimumVisible=Shot>=16&&Shot<=20?150:2000;
  if(!T->TestTrue(TEXT("showcase frame is populated"),Visible>MinimumVisible))return false;
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
  IFileManager::Get().MakeDirectory(*ShowcaseRoot(),true);
  if(!T->TestTrue(TEXT("showcase PNG written"),FFileHelper::SaveArrayToFile(Png,*(ShowcaseRoot()/(ShotName()+TEXT(".png"))))))return false;
  auto R=MakeShared<FJsonObject>();FString Commit;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Commit);
  R->SetStringField(TEXT("source_commit"),Commit);R->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());R->SetStringField(TEXT("name"),Names[Shot]);
  R->SetStringField(TEXT("kind"),TEXT("automated offscreen D3D12; no Windows input; native RHI pixels"));R->SetStringField(TEXT("rhi"),GDynamicRHI?GDynamicRHI->GetName():TEXT("none"));
  R->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);
  R->SetStringField(TEXT("rig_player_display"),OwnedPlayerHides.IsEmpty()?TEXT("normal"):TEXT("rig hidden in selected player view; collision unchanged"));
  R->SetNumberField(TEXT("visible_sampled_pixels"),Visible);R->SetStringField(TEXT("point_display_scope"),Shot>=16&&Shot<=20?TEXT("boom sensor only; cab acquisitions continue unchanged"):TEXT("three selected sensors"));
  R->SetNumberField(TEXT("umg_viewport_dpi_scale"),UWidgetLayoutLibrary::GetViewportScale(&W));R->SetNumberField(TEXT("slate_application_scale"),FSlateApplication::Get().GetApplicationScale());R->SetNumberField(TEXT("owned_font_scale"),M.GetFontScale());R->SetBoolField(TEXT("includes_slate_ui"),Ui);R->SetStringField(TEXT("material"),S.bCoal?TEXT("coal"):TEXT("iron_ore"));R->SetNumberField(TEXT("fill_percent"),S.Surface().Fill);
  R->SetNumberField(TEXT("surface_crc"),S.Surface().Hash());R->SetNumberField(TEXT("remaining_volume_m3"),S.Surface().Volume);R->SetNumberField(TEXT("production_digs"),Shot==25?0:Digs);R->SetNumberField(TEXT("session_production_digs"),Digs);R->SetNumberField(TEXT("removed_volume_m3"),S.TotalRemovedVolume());
  R->SetNumberField(TEXT("view"),M.GetView());R->SetBoolField(TEXT("sensor_colors"),M.IsSensorColoring());R->SetBoolField(TEXT("split_view"),M.IsSplitView());
  R->SetNumberField(TEXT("grab_geometry_crc"),GrabHash);
  R->SetArrayField(TEXT("actual_camera_hold_m"),ShowcaseVector(S.GetActorTransform().InverseTransformPosition(H.ViewCamera->GetActorLocation())*.01));
  R->SetArrayField(TEXT("actual_camera_pitch_yaw_roll"),ShowcaseVector(FVector(H.ViewCamera->GetActorRotation().Pitch,H.ViewCamera->GetActorRotation().Yaw,H.ViewCamera->GetActorRotation().Roll)));
  R->SetArrayField(TEXT("camera_hold_m"),ShowcaseVector(CameraLocal));R->SetArrayField(TEXT("aim_hold_m"),ShowcaseVector(AimLocal));R->SetNumberField(TEXT("horizontal_fov_deg"),H.ViewCamera->GetCameraComponent()->FieldOfView);
  TArray<TSharedPtr<FJsonValue>> Sensors;
  for(int I=0;I<3;++I)if(auto* A=S.SensorAt(I))
  {const auto* C=A->ScanComponent.Get();const auto F=C->GetLastFrameSnapshot();auto D=MakeShared<FJsonObject>();T->TestTrue(TEXT("captured sensor frame uses actual HWRT"),F&&F->AcquisitionBackend==EVirtualLidarAcquisitionBackend::HardwareRayTracing);D->SetStringField(TEXT("sensor_id"),C->SensorId);D->SetStringField(TEXT("model"),C->DeviceSpec.Model);D->SetStringField(TEXT("backend"),C->GetRuntimeStatus().ActiveAcquisitionBackend);D->SetNumberField(TEXT("measured_hz"),C->GetRuntimeStatus().MeasuredAcquisitionRateHz);D->SetNumberField(TEXT("frame_id"),F?double(F->FrameId):0);D->SetNumberField(TEXT("rays"),F?F->RequestedRayCount:0);Sensors.Add(MakeShared<FJsonValueObject>(D));}
  R->SetArrayField(TEXT("sensors"),Sensors);
  if(Shot>=16&&Shot<=20)
  {
   R->SetObjectField(TEXT("lidar_metrics"),LidarMetrics(S,0));TArray<TSharedPtr<FJsonValue>> Lines;
   auto Outline=[&](double L,double B,double Z)
   {
    const FVector P[4]={FVector(-L*.5,-B*.5,Z),FVector(L*.5,-B*.5,Z),FVector(L*.5,B*.5,Z),FVector(-L*.5,B*.5,Z)};
    for(int I=0;I<4;++I){FVector2D A,BP;if(M.GetOwningPlayer()->ProjectWorldLocationToScreen(S.GetActorTransform().TransformPosition(P[I]*100),A)&&M.GetOwningPlayer()->ProjectWorldLocationToScreen(S.GetActorTransform().TransformPosition(P[(I+1)%4]*100),BP))
    Lines.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(A.X),MakeShared<FJsonValueNumber>(A.Y),MakeShared<FJsonValueNumber>(BP.X),MakeShared<FJsonValueNumber>(BP.Y)}));}
   };
   Outline(S.OpeningLength,S.OpeningWidth,S.HoldDepth+S.CoamingHeight);R->SetArrayField(TEXT("coaming_outline_screen_px"),Lines);Lines.Reset();Outline(S.HoldLength,S.HoldWidth,0);R->SetArrayField(TEXT("hold_floor_outline_screen_px"),Lines);
  }
  ++CapturedImages;ShowcaseJson(ShotName()+TEXT(".json"),R);return true;
 }
public:
 explicit FHatchShowcaseRun(FAutomationTestBase* Test):T(Test){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Now-Started>900){T->AddError(TEXT("showcase capture timeout"));return true;}
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
  if(!Host.IsValid())for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)Host=*I;
  if(!Host.IsValid()||!Host->Monitor||!Host->Simulation||Host->Simulation->IsGenerating())return false;
  auto& S=*Host->Simulation;auto& M=*Host->Monitor;if(!S.SensorAt(0)||!S.SensorAt(1)||!S.SensorAt(2))return false;
  if(!Initialized)
  {
   if(!T->TestTrue(TEXT("showcase requires actual D3D12 HWRT"),GDynamicRHI&&FString(GDynamicRHI->GetName())==TEXT("D3D12")&&SensorOwnedRayTracing::IsAvailable()))return true;
   if(Presentation){Shot=27;for(int I=0;I<3;++I)S.SetFieldSensor(I,true,I==0?6:1);}
   Regenerate(S,false);M.SetFontPercent(100);Initialized=true;Frame(*Host,S,M);return false;
  }
  if(FloorProbe>=0)
  {
   if(Now-Changed<2.5||!S.HasFreshMeasurement())return false;
   ShowcaseJson(FString::Printf(TEXT("lidar-floor-%02d.json"),16+FloorProbe),LidarMetrics(S,0));
   if(++FloorProbe<5){S.SetFieldSensor(0,true,FloorProbe+3);Changed=Now;return false;}
   FloorProbe=-1;S.SetFieldSensor(0,true,6);Shot=23;Frame(*Host,S,M);return false;
  }
  if(Digging)
  {
   double Highest=0;FVector2D Target;
   for(double X:{-9.,-4.5,0.,4.5,9.})for(double Y:{-10.,-5.,0.,5.,10.})if(S.Surface().Height(X,Y)>Highest){Highest=S.Surface().Height(X,Y);Target=FVector2D(X,Y);}
   if(Highest<=.5&&S.Surface().Fill<=5)Polish=true;
   if(Polish){Highest=0;for(const FVector2D P:{FVector2D(0,0),FVector2D(0,4.5),FVector2D(4,0),FVector2D(4,4.5)})if(S.Surface().Height(P.X,P.Y)>Highest){Highest=S.Surface().Height(P.X,P.Y);Target=P;}}
   if(Polish&&Highest<=1.e-4)
   {T->TestTrue(TEXT("floor exposure follows real physical volume removal"),Digs>0&&S.Surface().Hash()!=BeforeHash&&S.TotalRemovedVolume()>BeforeVolume*.9);Digging=false;Frame(*Host,S,M);return false;}
   if(Digs>=1000||Highest<=1.e-4){T->AddError(TEXT("showcase real excavation did not expose floor"));return true;}
   const auto P=S.GetActorTransform();if(!S.Dig(P.TransformPosition(FVector(Target,S.HoldDepth+10)*100),P.TransformVectorNoScale(FVector(0,0,-1)),5,3)){T->AddError(TEXT("showcase production dig rejected"));return true;}++Digs;return false;
  }
  if(Shot>=16&&Shot<=20)for(int I=0;I<3;++I)
  {auto* V=S.SensorAt(I)->VisualizationComponent.Get();auto Settings=V->GetVisualizationSettings();if(Settings.bShowWorldPointCloud!=(I==0)||Settings.PointSize!=6){Settings.bShowWorldPointCloud=I==0;Settings.PointSize=6;V->SetVisualizationSettings(Settings);}}
  if(Presentation&&Shot==28)for(int I=0;I<3;++I)
  {
   auto* V=S.SensorAt(I)->VisualizationComponent.Get();auto Settings=V->GetVisualizationSettings();
   const float PointSize=4.f*FinalAcceptanceViewportSize().X/1920.f;
   if(!Settings.bShowWorldPointCloud||Settings.PointSize!=PointSize){Settings.bShowWorldPointCloud=true;Settings.PointSize=PointSize;V->SetVisualizationSettings(Settings);}
  }
  if(!Ui){const auto Size=FinalAcceptanceViewportSize();Host->SetSceneRect(FSlateRect(0,0,Size.X,Size.Y),FVector2D(Size.X,Size.Y),true);}
  if(Now-Changed<2.5||!S.HasFreshMeasurement())return false;
  if(!Capture(*W,*Host,S,M))return true;
  if(Presentation&&Shot==27){++Shot;Frame(*Host,S,M);return false;}
  if(Shot==21){BeforeHash=S.Surface().Hash();BeforeVolume=S.Surface().Volume;Digging=true;++Shot;return false;}
  if(Shot==22){FloorProbe=0;S.SetFieldSensor(0,true,3);Changed=Now;return false;}
  if(Shot==24){Shot=26;Frame(*Host,S,M);return false;}
  if(Shot==26){Shot=25;Frame(*Host,S,M);return false;}
  if(Shot<25){++Shot;Frame(*Host,S,M);return false;}
  if(auto* PC=M.GetOwningPlayer())for(const auto& C:OwnedPlayerHides)PC->HiddenPrimitiveComponents.RemoveSingle(C);OwnedPlayerHides.Reset();M.SetRenderOpacity(1);auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("valid"),!T->HasAnyErrors());R->SetNumberField(TEXT("images"),CapturedImages);R->SetNumberField(TEXT("production_digs"),Digs);const auto Size=FinalAcceptanceViewportSize();ShowcaseJson(Presentation?FString::Printf(TEXT("capture-result-%dx%d.json"),Size.X,Size.Y):TEXT("capture-result.json"),R);return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatchShowcaseCaptures,"HATCH06.ShowcaseCapturesRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatchShowcaseCaptures::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatchShowcaseRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
