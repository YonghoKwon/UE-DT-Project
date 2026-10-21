#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/Sensor/IndustrialLidarProfiles.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "ImageUtils.h"
#include "Json.h"
#include "ProceduralMeshComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchOverlay.h"
#include "Camera/CameraActor.h"

struct FHatch06Probe
{
 static void Cargo(UShipHatchMonitorWidget& M){M.OnFrameCargo();}
 static void Vessel(UShipHatchMonitorWidget& M){M.OnFrameVessel();}
 static bool StartReferenceGrab(UShipHatchMonitorWidget& M)
 {
  M.bDigEnabled=true;auto& S=*M.Simulation;FVector2D Pixel;auto* PC=M.GetOwningPlayer();
  const FVector P=S.GetActorTransform().TransformPosition(FVector(5,-5,S.Surface().Height(5,-5))*100);
  if(!PC->ProjectWorldLocationToScreen(P,Pixel))return false;int W=0,H=0;PC->GetViewportSize(W,H);const auto G=M.GetCachedGeometry();
  const bool Result=M.DigAt(G,G.LocalToAbsolute(Pixel*G.GetLocalSize()/FVector2D(W,H)));M.bDigEnabled=false;return Result;
 }
 static void HopperCamera(UShipHatchMonitorWidget& M)
 {
  const auto& S=*M.Simulation;const double Y=S.SeaRailY()-S.RailSpan*.5,Z=S.QuayZ()+15;
  const auto Pose=S.GetActorTransform();const auto Target=Pose.TransformPosition(FVector(0,Y,Z+1)*100),Position=Pose.TransformPosition(FVector(18,Y+12,Z+10)*100);
  M.Host->ViewCamera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());
 }
 static void GroundRigCamera(UShipHatchMonitorWidget& M)
 {
  const auto& S=*M.Simulation;const auto Pose=S.GetActorTransform();const auto P=Pose.TransformPosition(FVector(80,S.QuayEdgeY()-25,S.QuayZ()+1.8)*100),Target=Pose.TransformPosition(FVector(0,(S.BoomTipY()+S.SeaRailY()-S.RailSpan)*.5,(S.QuayZ()+S.HoldDepth+S.BoomHeight+16.2)*.5)*100);M.Host->ViewCamera->SetActorLocationAndRotation(P,(Target-P).Rotation());
 }
 static void GrabCloseCamera(UShipHatchMonitorWidget& M)
 {
  const auto& S=*M.Simulation;const auto Pose=S.GetActorTransform();const double Z=S.Surface().Height(0,4)+4;const auto P=Pose.TransformPosition(FVector(8,8,S.HoldDepth+5)*100),Target=Pose.TransformPosition(FVector(0,4,Z+.4)*100);M.Host->ViewCamera->SetActorLocationAndRotation(P,(Target-P).Rotation());
 }
 static void CheckDischarge(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
 {
  T.TestTrue(TEXT("bounded visible clamshell and mist discharge meshes"),M.Overlay&&M.Overlay->BatchedMeshes.Num()==4&&M.Overlay->BatchedMeshes[1].MeshVerts.Num()>0&&M.Overlay->BatchedMeshes[1].MeshVerts.Num()<=160&&M.Overlay->BatchedMeshes[2].MeshVerts.Num()<=32&&M.Overlay->BatchedMeshes[3].MeshVerts.Num()>0);
  T.TestEqual(TEXT("display effect adds no measurement collision"),M.Overlay->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
 }
 static void FullRig(UShipHatchMonitorWidget& M){M.OnFrameUnloader();}
 static void FloorCamera(UShipHatchMonitorWidget& M,FVector2D Target)
 {
  const auto Pose=M.Simulation->GetActorTransform();const auto P=Pose.TransformPosition(FVector(Target.X+3,Target.Y-2,15)*100),Aim=Pose.TransformPosition(FVector(Target,0)*100);
  M.Host->ViewCamera->SetActorLocationAndRotation(P,(Aim-P).Rotation());
 }
 static void Settings(UShipHatchMonitorWidget& M,bool Open){if(M.bSettings!=Open)M.OnSettings();}
 static void CheckLayout(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
 {
  const auto* D=M.LayoutSlots[8].Get();const auto P=D->GetPosition(),Z=D->GetSize();
  for(int I:{4,5}){const auto* Slot=M.LayoutSlots[I].Get();T.TestTrue(TEXT("both right cards remain visible with settings open"),Slot->Content->IsVisible());const auto Q=Slot->GetPosition(),S=Slot->GetSize();const double Intersection=FMath::Max(0.,FMath::Min(P.X+Z.X,Q.X+S.X)-FMath::Max(P.X,Q.X))*FMath::Max(0.,FMath::Min(P.Y+Z.Y,Q.Y+S.Y)-FMath::Max(P.Y,Q.Y));T.TestEqual(TEXT("drawer occlusion of each right card is exactly zero pixels (visible area100%)"),Intersection,0.);}
 }
 static void Scale(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
 {
  auto& S=*M.Simulation;FString Error;T.TestTrue(TEXT("live vessel and rig dimensions satisfy real-scale ranges"),S.ValidateSceneScale(Error));
  T.TestTrue(TEXT("rail is on land side of quay edge; boom reaches into vessel"),S.SeaRailY()<S.QuayEdgeY()&&S.QuayEdgeY()<-S.VesselBeam*.5&&S.BoomTipY()>0&&S.BoomTipY()<S.VesselBeam*.5);
  T.TestTrue(TEXT("tank top is reachable within reference25m hoist below rail"),S.QuayZ()>0&&S.QuayZ()<25);
  const double Saved=S.VesselLength;S.VesselLength=120;T.TestFalse(TEXT("unrealistic short vessel is rejected"),S.ValidateSceneScale(Error));S.VesselLength=Saved;
  for(TActorIterator<AShipHatchVessel>I(S.GetWorld());I;++I)
  {
   const auto* Hull=I->Mesh->GetProcMeshSection(0);T.TestNotNull(TEXT("actual hull mesh exists"),Hull);if(!Hull)continue;
   const auto B=Hull->SectionLocalBox.GetSize()*.01;T.TestTrue(TEXT("rendered hull actual LOA/beam/moulded depth match SI configuration within1cm"),FMath::Abs(B.X-S.VesselLength)<.01&&FMath::Abs(B.Y-S.VesselBeam)<.01&&FMath::Abs(B.Z-S.MouldedDepth)<.01);
   const auto* Covers=I->Mesh->GetProcMeshSection(3);T.TestTrue(TEXT("two real side-rolling panel meshes exist"),Covers&&Covers->ProcVertexBuffer.Num()>=16);
   if(Covers&&Covers->ProcVertexBuffer.Num()>=16)for(int Side=0;Side<2;++Side){FBox Box(ForceInit);for(int J=0;J<8;++J)Box+=FVector(Covers->ProcVertexBuffer[Side*8+J].Position)*.01;const auto Center=Box.GetCenter(),Size=Box.GetSize();T.TestTrue(TEXT("open panel travels toward land/sea Y, with no fore-aft folding"),FMath::Abs(Center.X)<.01&&FMath::Abs(Center.Y)>S.OpeningWidth*.5&&FMath::Abs(Size.Y-S.OpeningWidth*.5)<.01&&Size.Z<.5);T.TestTrue(TEXT("both open panels clear aperture and stay within ship beam"),FMath::Abs(Center.Y)-Size.Y*.5>=S.OpeningWidth*.5&&FMath::Abs(Center.Y)+Size.Y*.5<=S.VesselBeam*.5);}
   T.TestFalse(TEXT("1.8m scale reference defaults off"),I->ScaleMarker->IsVisible());const auto* Ref=I->ScaleMarker->GetProcMeshSection(0);T.TestTrue(TEXT("actual worker/scale geometry is1.8m within1cm"),Ref&&FMath::Abs(Ref->SectionLocalBox.GetSize().Z*.01-1.8)<.01);
   T.TestEqual(TEXT("scale reference cannot contaminate physical acquisition"),I->ScaleMarker->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
  }
 }
};
namespace
{
FString Root06(){FString P;FParse::Value(FCommandLine::Get(),TEXT("HatchReport="),P);return P.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/rhi"):P;}
void Save06(const FString& Name,const TSharedRef<FJsonObject>& R){FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Root06(),true);FFileHelper::SaveStringToFile(Text,*(Root06()/Name),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);}
double Mean06(const TArray<double>& A){double S=0;for(double V:A)S+=V;return A.Num()?S/A.Num():0;}
double P9506(TArray<double> A){A.Sort();return A.Num()?A[FMath::Clamp(FMath::CeilToInt(A.Num()*.95)-1,0,A.Num()-1)]:0;}
class FHatch06Run:public IAutomationLatentCommand
{
 FAutomationTestBase* T;int Phase=0,Model=0,Material=0,Edits=0,View=0;double Start=FPlatformTime::Seconds(),Changed=0,LastWall=0;uint64 LastFrame=0;
 TArray<double> Engine,Game,Wall;TArray<TSharedPtr<FJsonValue>> Rows;TUniquePtr<TGuardValue<bool>> Messages;
 FVector2D Target;uint32 OriginalHash=0;uint64 RssBefore=0;
 void Shot(UWorld* W,UShipHatchMonitorWidget& M,const FString& Name,bool Floor=false)
 {
  TArray<FColor> Pixels;FIntVector Size;if(!T->TestTrue(TEXT("real RHI pixels captured"),FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);const FString Path=Root06()/TEXT("captures")/(Name+TEXT(".png"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);FFileHelper::SaveArrayToFile(PNG,*Path);
  const FString Progress=FPaths::ProjectSavedDir()/TEXT("Reports/HATCH-06/progress")/(FString::Printf(TEXT("%02d-"),int(Rows.Num()+Material*10+View))+FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M"))+TEXT("-")+Name+TEXT(".png"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Progress),true);FFileHelper::SaveArrayToFile(PNG,*Progress);
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());R->SetStringField(TEXT("kind"),TEXT("automated D3D12, not direct UI"));R->SetNumberField(TEXT("width"),Size.X);R->SetNumberField(TEXT("height"),Size.Y);R->SetNumberField(TEXT("font_percent"),M.GetFontScale()*100);R->SetNumberField(TEXT("default_radius_m"),2);R->SetNumberField(TEXT("default_depth_m"),1.5);R->SetNumberField(TEXT("edits"),Edits);
  const auto* Sim=M.GetSimulation();R->SetNumberField(TEXT("surface_crc"),Sim->Surface().Hash());R->SetNumberField(TEXT("seed"),Sim->Seed);R->SetNumberField(TEXT("loa_m"),Sim->VesselLength);R->SetNumberField(TEXT("beam_m"),Sim->VesselBeam);R->SetNumberField(TEXT("moulded_depth_m"),Sim->MouldedDepth);R->SetNumberField(TEXT("rail_span_m"),Sim->RailSpan);R->SetNumberField(TEXT("outreach_m"),Sim->BoomLength);R->SetNumberField(TEXT("boom_sensor_above_deck_m"),Sim->SensorOrigin(0).Z-Sim->HoldDepth);R->SetNumberField(TEXT("cab_sensor_above_deck_m"),Sim->SensorOrigin(1).Z-Sim->HoldDepth);R->SetNumberField(TEXT("water_z_m"),Sim->WaterZ());R->SetNumberField(TEXT("quay_z_m"),Sim->QuayZ());R->SetNumberField(TEXT("umg_scale"),UWidgetLayoutLibrary::GetViewportScale(&M));
  const auto Window=FSlateApplication::Get().FindWidgetWindow(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef());R->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0);FString Commit;FParse::Value(FCommandLine::Get(),TEXT("HatchSourceCommit="),Commit);R->SetStringField(TEXT("source_commit"),Commit);
  if(Floor)
  {
   FHitResult First;const FVector Aim=Sim->GetActorTransform().TransformPosition(FVector(Target,-.05)*100);FCollisionQueryParams Query(SCENE_QUERY_STAT(Hatch06FloorPixel),true);
   FVector Eye;FRotator EyeRotation;W->GetFirstPlayerController()->GetPlayerViewPoint(Eye,EyeRotation);
   const bool HasHit=W->LineTraceSingleByChannel(First,Eye,Aim,ECC_Visibility,Query);
   const double FirstZ=HasHit?Sim->GetActorTransform().InverseTransformPosition(First.ImpactPoint).Z*.01:1.e9;
   const bool TankTop=HasHit&&First.GetActor()&&First.GetActor()->ActorHasTag(TEXT("HatchHold"))&&FMath::Abs(FirstZ)<.005;
   R->SetBoolField(TEXT("first_visible_hit_is_tank_top"),TankTop);R->SetNumberField(TEXT("first_hit_local_z_m"),FirstZ);
   T->TestTrue(TEXT("pixel sample sightline first hits actual Z0 tank-top, with no crane/cargo occlusion"),TankTop);
   FVector2D P;const auto* S=M.GetSimulation();W->GetFirstPlayerController()->ProjectWorldLocationToScreen(S->GetActorTransform().TransformPosition(FVector(Target,0)*100),P,false);
   int Hit=0,Total=0;for(int Y=-3;Y<=3;++Y)for(int X=-3;X<=3;++X){const int PX=FMath::RoundToInt(P.X)+X,PY=FMath::RoundToInt(P.Y)+Y;if(PX>=0&&PY>=0&&PX<Size.X&&PY<Size.Y){const auto C=Pixels[PY*Size.X+PX];++Total;Hit+=C.B>C.R*1.04&&C.G>C.R*1.04&&C.B>C.G*.98&&C.B>35;}}
   R->SetNumberField(TEXT("floor_pixel_x"),P.X);R->SetNumberField(TEXT("floor_pixel_y"),P.Y);R->SetNumberField(TEXT("blue_gray_steel_pixels"),Hit);R->SetNumberField(TEXT("sample_pixels"),Total);
   T->TestTrue(TEXT("actual rendered exposed tank-top center is blue-gray steel (>=60% of7x7 pixels)"),Total==49&&Hit>=30);
  }
  FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*FPaths::ChangeExtension(Path,TEXT("json")));
 }
public:
 explicit FHatch06Run(FAutomationTestBase* Test):T(Test){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(Now-Start>1800){T->AddError(TEXT("HATCH06 timed out"));return true;}
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
  AShipHatchMonitorHost* H=nullptr;for(TActorIterator<AShipHatchMonitorHost>I(W);I;++I)H=*I;if(!H||!H->Monitor)return false;
  auto& M=*H->Monitor;auto* S=const_cast<AShipHatchSimulation*>(M.GetSimulation());if(!S||S->IsGenerating())return false;
  if(Phase==0)
  {
   if(!M.GetAnalysis())return false;FHatch06Probe::Scale(*T,M);Messages=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);int Font=100;FParse::Value(FCommandLine::Get(),TEXT("HatchFontPercent="),Font);M.SetFontPercent(Font);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(1);FHatch06Probe::Cargo(M);Changed=Now;Phase=1;return false;
  }
  if(Phase==1)
  {
   if(Now-Changed<1)return false;Shot(W,M,TEXT("cargo"));FHatch06Probe::FullRig(M);Changed=Now;Phase=10;return false;
  }
  if(Phase==10){if(Now-Changed<1)return false;Shot(W,M,TEXT("gtsu-full"));FHatch06Probe::GroundRigCamera(M);Changed=Now;Phase=19;return false;}
  if(Phase==14){if(Now-Changed<1)return false;Shot(W,M,TEXT("gtsu-scale-1p8m"));S->ShowScaleReference(false);FHatch06Probe::Vessel(M);Changed=Now;Phase=13;return false;}
  if(Phase==13){if(Now-Changed<1)return false;Shot(W,M,TEXT("vessel-overview"));S->ShowWater(false);Changed=Now;Phase=15;return false;}
  if(Phase==15){if(Now-Changed<1)return false;Shot(W,M,TEXT("vessel-hull-water-hidden"));S->ShowWater(true);M.SetCameraPreset(1);FHatch06Probe::Settings(M,true);Changed=Now;Phase=2;return false;}
  if(Phase==2)
  {
   if(Now-Changed<1)return false;FHatch06Probe::CheckLayout(*T,M);Shot(W,M,TEXT("settings-three-views"));
   if(FParse::Param(FCommandLine::Get(),TEXT("Hatch06Manual"))){FFileHelper::SaveStringToFile(TEXT("Ready for direct Computer Use; waiting for manual.done"),*(Root06()/TEXT("manual.ready")));Phase=12;return false;}
   FHatch06Probe::Settings(M,false);FHatch06Probe::GrabCloseCamera(M);Changed=Now;Phase=20;return false;
  }
  if(Phase==12)return FPaths::FileExists(Root06()/TEXT("manual.done"));
  if(Phase==19){if(Now-Changed<1)return false;Shot(W,M,TEXT("gtsu-reference-ground"));M.SetCameraPreset(1);FHatch06Probe::FullRig(M);S->ShowScaleReference(true);Changed=Now;Phase=14;return false;}
  if(Phase==20){if(Now-Changed<1)return false;Shot(W,M,TEXT("grab-hold-close"));M.SetCameraPreset(0);FHatch06Probe::Cargo(M);Changed=Now;Phase=16;return false;}
  if(Phase==16){if(Now-Changed<1)return false;if(!T->TestTrue(TEXT("reference effect starts through production pointer path"),FHatch06Probe::StartReferenceGrab(M)))return true;FHatch06Probe::HopperCamera(M);Changed=Now;Phase=17;return false;}
  if(Phase==17){if(Now-Changed<3.8)return false;FHatch06Probe::CheckDischarge(*T,M);Shot(W,M,TEXT("grab-hopper-discharge"));Changed=Now;Phase=18;return false;}
  if(Phase==18){if(Now-Changed<1.6)return false;S->RestoreCargo();M.SetCameraPreset(1);FHatch06Probe::Cargo(M);for(int I=0;I<3;++I)S->SetFieldSensor(I,true,3+Model);M.SelectView(1);M.SetSplitView(true);Changed=Now;Phase=3;return false;}
  if(Phase==3)
  {
   if(!S->HasFreshMeasurement()||!M.GetAnalysis()||Now-Changed<10)return false;
   Engine.Reset();Game.Reset();Wall.Reset();LastWall=Now;LastFrame=GFrameCounter;Changed=Now;RssBefore=FPlatformMemory::GetStats().UsedPhysical;Phase=4;return false;
  }
  if(Phase==4)
  {
   if(GFrameCounter!=LastFrame){LastFrame=GFrameCounter;Engine.Add(W->GetDeltaSeconds()*1000);Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));Wall.Add((Now-LastWall)*1000);LastWall=Now;}
   const bool Fast=FParse::Param(FCommandLine::Get(),TEXT("Hatch06Fast"));if(Now-Changed<(Fast?2:30))return false;
   const auto& Spec=IndustrialLidar::Specs()[Model];auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("model"),Spec.Model);R->SetNumberField(TEXT("fps"),1000/Mean06(Engine));R->SetNumberField(TEXT("engine_p95_ms"),P9506(Engine));R->SetNumberField(TEXT("game_mean_ms"),Mean06(Game));R->SetNumberField(TEXT("wall_p95_ms"),P9506(Wall));R->SetNumberField(TEXT("measurement_seconds"),Now-Changed);R->SetNumberField(TEXT("rss_before_mib"),RssBefore/1048576.);R->SetNumberField(TEXT("rss_after_mib"),FPlatformMemory::GetStats().UsedPhysical/1048576.);
   T->TestTrue(TEXT("three-device measurement engine has zero >150ms frames"),Engine.Num()>0&&FMath::Max(Engine)<=150);T->TestTrue(TEXT("three-device game thread has zero >150ms frames"),Game.Num()>0&&FMath::Max(Game)<=150);
   TArray<TSharedPtr<FJsonValue>> Devices;
   for(int I=0;I<3;++I)
   {
    auto* A=S->SensorAt(I);const auto F=A->ScanComponent->GetLastFrameSnapshot();T->TestTrue(TEXT("each selected industrial model produces a fresh acquisition"),F&&F->Points&&F->Points->Num());if(!F||!F->Points)continue;
    T->TestEqual(TEXT("actual acquired ray budget matches published selected mode"),F->RequestedRayCount,Spec.Columns*Spec.Rows);T->TestEqual(TEXT("actual selected device"),A->ScanComponent->DeviceProfile,Spec.Profile);
    TArray<ShipHatch::FObservation> P;int Hits=0;for(const auto& Point:*F->Points)if(Point.bHit){++Hits;T->TestTrue(TEXT("physical hit respects model range"),Point.Distance<=Spec.RangeMeters*100+.01);T->TestTrue(TEXT("physical return respects echo capability"),Point.EchoCount<=Spec.Echoes);if(Point.SemanticLabel==TEXT("HatchCargo"))P.Add({S->GetActorTransform().InverseTransformPosition(Point.WorldLocation)*.01,Point.SemanticLabel});}
    const auto Analysis=ShipHatch::Analyze(S->Settings(),1,P);auto D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("sensor"),I);D->SetNumberField(TEXT("hit_points"),Hits);D->SetNumberField(TEXT("cargo_points"),P.Num());D->SetNumberField(TEXT("observed_percent"),100*Analysis.Coverage);D->SetNumberField(TEXT("unobserved_percent"),100*(1-Analysis.Coverage));D->SetNumberField(TEXT("measured_hz"),A->ScanComponent->GetRuntimeStatus().MeasuredAcquisitionRateHz);Devices.Add(MakeShared<FJsonValueObject>(D));T->TestTrue(TEXT("mounted industrial model sees cargo"),P.Num()>0);
   }
   R->SetArrayField(TEXT("sensors"),Devices);Rows.Add(MakeShared<FJsonValueObject>(R));Save06(TEXT("models.json"),[&](){auto J=MakeShared<FJsonObject>();J->SetArrayField(TEXT("models"),Rows);return J;}());Phase=5;Changed=Now;return false;
  }
  if(Phase==5)
  {
   if(Now-Changed<1)return false;Shot(W,M,FString::Printf(TEXT("model-%d"),Model));if(++Model<5){for(int I=0;I<3;++I)S->SetFieldSensor(I,true,3+Model);Changed=Now;Phase=3;return false;}
   for(int I=0;I<3;++I)S->SetFieldSensor(I,true,1);S->bCoal=false;S->FillMin=35;S->FillMax=65;S->Regenerate(false);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(0);Changed=Now;Phase=6;return false;
  }
  if(Phase==6)
  {
   if(!S->HasFreshMeasurement()||Now-Changed<2)return false;
   const auto& F=S->Surface();Target=FVector2D(FMath::RoundToInt((5+F.Settings.Length*.5)/F.DX)*F.DX-F.Settings.Length*.5,FMath::RoundToInt((-5+F.Settings.Width*.5)/F.DY)*F.DY-F.Settings.Width*.5);
   OriginalHash=F.Hash();Edits=0;FHatch06Probe::Cargo(M);Changed=Now;Phase=11;return false;
  }
  if(Phase==11){if(Now-Changed<1)return false;Shot(W,M,Material?TEXT("coal-full-before"):TEXT("iron-full-before"));Phase=7;return false;
  }
  if(Phase==7)
  {
   if(S->Surface().Height(Target.X,Target.Y)>1.e-4&&Edits<1500)
   {
    const FVector O=S->GetActorTransform().TransformPosition(FVector(Target,S->HoldDepth+6)*100),D=S->GetActorTransform().TransformVectorNoScale(FVector(0,0,-1));
    if(!S->Dig(O,D,2,1.5)){T->AddError(TEXT("default-radius repeated physical grab did not progress"));return true;}++Edits;return false;
   }
   T->TestTrue(TEXT("default fill and repeated default grabs reach zero thickness"),S->Surface().Height(Target.X,Target.Y)<=1.e-4);M.SetSection(true,Target.Y/S->HoldWidth+.5+.04);FHatch06Probe::FloorCamera(M,Target);Changed=Now;View=0;Phase=8;return false;
  }
  if(Phase==8)
  {
   if(!S->HasFreshMeasurement()||!M.GetAnalysis()||Now-Changed<2)return false;
   Shot(W,M,FString::Printf(TEXT("%s-full-floor-%s"),Material?TEXT("coal"):TEXT("iron"),View==0?TEXT("3d"):View==1?TEXT("points"):TEXT("section")),View==0);
   if(++View<3){M.SelectView(View==1?1:4);if(View==2)M.SetSection(true,Target.Y/S->HoldWidth+.5);Changed=Now;return false;}
   T->TestTrue(TEXT("undo restores a prior physical excavation"),S->UndoDig());T->TestTrue(TEXT("restore returns bit-exact original detail"),S->RestoreCargo()&&S->Surface().Hash()==OriginalHash);
   if(Material++==0){S->bCoal=true;S->Regenerate(false);M.SelectView(0);M.SetSplitView(true);M.SetCameraPreset(0);Changed=Now;Phase=6;return false;}
   auto Done=MakeShared<FJsonObject>();Done->SetBoolField(TEXT("valid"),!T->HasAnyErrors());Done->SetArrayField(TEXT("models"),Rows);Save06(TEXT("acceptance.json"),Done);return true;
  }
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatch06Acceptance,"HATCH06.AcceptanceRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatch06Acceptance::RunTest(const FString&){if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatch06Run(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
